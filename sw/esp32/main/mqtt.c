#include <freertos/FreeRTOS.h>
#include <esp_system.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>

#include "app_config.h"
#include "mqtt.h"


#define TASK_STACK_SIZE 1024 * 30
#define TASK_PRIORITY tskIDLE_PRIORITY

#define MQTT_READ_TIMEOUT 10 * 1000

#define MQTT_CHECK_ERROR(x) do { int err = (x); if (err != ESP_OK) { printf("CHECK FAILED: %s:%d " #x " returned %d\n", __FILE__, __LINE__, err); return ESP_FAIL; } } while (0);
#define SSL_CHECK_ERROR(x) do { int rc = (x); if (rc) { char buf[32]; mbedtls_strerror(rc, buf, sizeof(buf)); printf("CHECK FAILED: %s:%d " #x ": %d, %s\n", __FILE__, __LINE__, rc, buf); return ESP_FAIL; } } while (0);

esp_err_t mqtt_init(mqtt_client_t *client)
{
    nvs_handle nvs;
    MQTTPacket_connectData connectData = MQTTPacket_connectData_initializer;

    memcpy(&client->data, &connectData, sizeof(client->data));
    ESPNODE_ERROR_CHECK(mqtt_client_id((char*)&client->client_id[0]));

    ESPNODE_ERROR_CHECK(nvs_open(APP_NAMESPACE, NVS_READONLY, &nvs));

    ESPNODE_ERROR_CHECK(nvs_get_str_static(nvs, MQTT_PREFIX "hostname", (char*)client->hostname, sizeof(client->hostname)));
    ESPNODE_ERROR_CHECK(nvs_get_str_static(nvs, MQTT_PREFIX "port", (char*)client->port, sizeof(client->port)));
    nvs_get_str_static(nvs, MQTT_PREFIX "username", (char*)client->username, sizeof(client->username));
    nvs_get_str_static(nvs, MQTT_PREFIX "password", (char*)client->password, sizeof(client->password));
    nvs_get_str_heap(nvs, SSL_PREFIX "ca_cert", (char**)&client->ca_cert_str);
    nvs_get_str_heap(nvs, SSL_PREFIX "client_cert", (char**)&client->client_cert_str);
    nvs_get_str_heap(nvs, SSL_PREFIX "client_key", (char**)&client->client_key_str);

    nvs_close(nvs);

    return ESP_OK;
}

esp_err_t mqtt_close(mqtt_client_t *client)
{
    free(client->ca_cert_str);
    free(client->client_cert_str);
    free(client->client_key_str);

    return ESP_OK;
}

static esp_err_t ssl_start(mqtt_client_t *client)
{
    int ret;
    mbedtls_net_context *ctx = &client->ctx;
    mbedtls_ssl_config *conf = &client->conf;

    // Init
    mbedtls_net_init(ctx);
    mbedtls_ssl_init(&client->ssl);
    mbedtls_ssl_config_init(&client->conf);
    mbedtls_ctr_drbg_init(&client->ctr_drbg);
    mbedtls_x509_crt_init(&client->cacert);
    mbedtls_x509_crt_init(&client->clicert);
    mbedtls_pk_init(&client->pkey);
    mbedtls_entropy_init(&client->entropy);

    SSL_CHECK_ERROR(mbedtls_ctr_drbg_seed(&client->ctr_drbg, mbedtls_entropy_func, &client->entropy, client->client_id, strlen((char*)client->client_id)));
    if (client->ca_cert_str)
        SSL_CHECK_ERROR(mbedtls_x509_crt_parse(&client->cacert, client->ca_cert_str, strlen((char*)client->ca_cert_str) + 1));
    if (client->client_cert_str && client->client_key_str) {
        SSL_CHECK_ERROR(mbedtls_x509_crt_parse(&client->clicert, client->client_cert_str, strlen((char*)client->client_cert_str) + 1));
        SSL_CHECK_ERROR(mbedtls_pk_parse_key(&client->pkey, client->client_key_str, strlen((char*)client->client_key_str) + 1, NULL, 0));
    }

    // Connect
    printf("Connecting to %s:%s...\n", client->hostname, client->port);
    SSL_CHECK_ERROR(mbedtls_net_connect(ctx, (char*)client->hostname, (char*)client->port, MBEDTLS_NET_PROTO_TCP));
    SSL_CHECK_ERROR(mbedtls_net_set_block(ctx));

    // Configure SSL
    SSL_CHECK_ERROR(mbedtls_ssl_config_defaults(conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT));
    mbedtls_ssl_conf_authmode(conf, MBEDTLS_SSL_VERIFY_OPTIONAL); //TODO: Back to required
    mbedtls_ssl_conf_rng(conf, mbedtls_ctr_drbg_random, &client->ctr_drbg);
    if (client->ca_cert_str)
        mbedtls_ssl_conf_ca_chain(conf, &client->cacert, NULL);
    if (client->client_cert_str && client->client_key_str)
        SSL_CHECK_ERROR(mbedtls_ssl_conf_own_cert(conf, &client->clicert, &client->pkey));
    // mbedtls_ssl_conf_read_timeout() ?
    SSL_CHECK_ERROR(mbedtls_ssl_setup(&client->ssl, conf));
    SSL_CHECK_ERROR(mbedtls_ssl_set_hostname(&client->ssl, (char*)client->hostname));
    mbedtls_ssl_set_bio(&client->ssl, ctx, mbedtls_net_send, NULL,
                        mbedtls_net_recv_timeout);

    printf("Negotiating SSL...\n");
    while((ret = mbedtls_ssl_handshake(&client->ssl)) != 0) {
        if(ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            printf(" failed\n  ! mbedtls_ssl_handshake returned -0x%x\n", -ret);
            if(ret == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED) {
                printf("    Unable to verify the server's certificate. "
                       "Either it is invalid,\n"
                       "    or you didn't set ca_file or ca_path "
                       "to an appropriate value.\n"
                       "    Alternatively, you may want to use "
                       "auth_mode=optional for testing purposes.\n");
            }
            return ESP_FAIL;
        }
    }

    printf("    [ Protocol is %s ]\n    [ Ciphersuite is %s ]\n", mbedtls_ssl_get_version(&client->ssl), mbedtls_ssl_get_ciphersuite(&client->ssl));
    if((ret = mbedtls_ssl_get_record_expansion(&client->ssl)) >= 0) {
        printf("    [ Record expansion is %d ]\n", ret);
    } else {
        printf("    [ Record expansion is unknown (compression) ]\n");
    }

    if (client->ca_cert_str) {
        printf("Verifying server certificate...");
        if((client->flags = mbedtls_ssl_get_verify_result(&client->ssl)) != 0) {
            char buf[512];
            mbedtls_x509_crt_verify_info(buf, sizeof(buf), "  ! ", client->flags);
            printf("Fail: %s\n", buf);
            return ESP_FAIL;
        } else {
            printf("Pass\n");
        }
    }

    mbedtls_ssl_conf_read_timeout(&client->conf, MQTT_READ_TIMEOUT);

    return ESP_OK;
}

static int ssl_send(mqtt_client_t *client, const unsigned char *buf, int len)
{
    size_t written_so_far;
    int ret;

    ret = 0;
    for(written_so_far = 0; written_so_far < len; written_so_far += ret) {
        while((ret = mbedtls_ssl_write(&client->ssl, buf + written_so_far, len - written_so_far)) <= 0) {
            if(ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
                printf("write failed\n  ! mbedtls_ssl_write returned -0x%x\n\n", -ret);
                /* All other negative return values indicate connection needs to be reset.
                * Will be caught in ping request so ignored here */
                return -1;
            }
        }
    }

    return written_so_far;
}

static int ssl_read(void *sck, unsigned char *buf, int len)
{
    mqtt_client_t *client = (mqtt_client_t *)sck;
    mbedtls_ssl_context *ssl = &client->ssl;
    size_t rxLen = 0;
    int ret;

    while (len > 0) {
        // This read will timeout after IOT_SSL_READ_TIMEOUT if there's no data to be read
        ret = mbedtls_ssl_read(ssl, buf, len);
        if (ret > 0) {
            rxLen += ret;
            buf += ret;
            len -= ret;
        } else if (ret == MBEDTLS_ERR_SSL_TIMEOUT) {
            printf("SSL_READ TIMEOUT\n");
            goto err;
        } else if (ret == 0) {
            printf("SSL_READ NULL READ\n");
            goto err;
        } else if (ret == MBEDTLS_ERR_SSL_WANT_READ) {
            printf("SSL_READ WANT READ\n");
            continue;
        } else if (ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            printf("SSL_READ WANT WRITE\n");
            continue;
        } else {
            goto err;
        }
    }


    return rxLen;

err:
    printf("Failed: ret=%d\n", ret);
    return -1;
}

esp_err_t _mqtt_start(mqtt_client_t *client)
{
    unsigned char buf[200];
    int len;
    int ret;
    MQTTTransport *transport;
    MQTTPacket_connectData *data;

    printf("Starting MQTT...\n");

    ESPNODE_ERROR_CHECK(ssl_start(client));

    transport = &client->transport;
    transport->getfn = ssl_read;
    transport->sck = client;
    transport->state = 0;

    data = &client->data;
    data->clientID.cstring = (char*)client->client_id;
    data->keepAliveInterval = 20;
    data->cleansession = 1;
    data->username.cstring = (char*)client->username;
    data->password.cstring = (char*)client->password;
    data->MQTTVersion = 4;

    // Send connect
    printf("MQTT connecting...\n");
    len = MQTTSerialize_connect(buf, sizeof(buf), data);
    int sent_len = ssl_send(client, buf, len);
    if (sent_len != len) {
        printf("Short write??\n");
        return ESP_FAIL;
    }

    // Wait for CONNACK
    if ((ret = MQTTPacket_readnb(buf, sizeof(buf), transport)) == CONNACK) {
        unsigned char sessionPresent, connack_rc;

        if (MQTTDeserialize_connack(&sessionPresent, &connack_rc, buf, sizeof(buf)) != 1 || connack_rc != 0)
        {
            printf("Unable to connect, return code %d\n", connack_rc);
            return ESP_FAIL;
        }
    } else {
        printf("Unexpected response from server: %d\n", ret);
        return ESP_FAIL;
    }

    printf("MQTT connected\n");

    return ESP_OK;
}

void mqtt_task(void *param)
{
    mqtt_client_t *client = (mqtt_client_t *)param;

    _mqtt_start(client);

    printf("MQTT task started\n");
    unsigned char buf[64];
    const int buf_len = 32;
    int len, ret;
    unsigned short packet_id = 0;
    MQTTTransport *transport = &client->transport;
    while (1) {
        //TODO: Read from publish queue, send to host
        //TODO: Maintain packet id

        unsigned char payload[32];
        int payload_len = snprintf((char*)payload, sizeof(payload), "Hello, World! %d", packet_id);
        MQTTString topic = MQTTString_initializer;
        topic.cstring = "test";

        printf("Publish: %s\n", payload);
        len = MQTTSerialize_publish(buf, buf_len, 0, 1/*qos*/, 0, packet_id, topic, payload, payload_len);
        if ((ret = ssl_send(client, buf, len)) < 0 || ret != len)
            printf("ssl_send failed: %d\n", ret);
        if ((ret = MQTTPacket_readnb(buf, sizeof(buf), transport)) == PUBACK) {
            unsigned char dummy;
            unsigned short packet_id_ret;
            if (MQTTDeserialize_ack(&dummy, &dummy, &packet_id_ret, buf, sizeof(buf)) == 1) {
                if (packet_id == packet_id_ret) {
                    printf("Publish succeeded\n");
                } else {
                    printf("Puback for wrong packet_id: %d\n", packet_id_ret);
                }
            } else {
                printf("Unable to deserialize PUBACK\n");
            }
        } else {
            printf("Unexpected response to publish: %d\n", ret);
        }
        packet_id++;

        vTaskDelay(5 * 1000 * portTICK_PERIOD_MS);
    }
}

esp_err_t mqtt_start(mqtt_client_t *client)
{
    // Start background
    xTaskCreate(&mqtt_task, "mqtt_task", TASK_STACK_SIZE, client, TASK_PRIORITY, &client->task);

    return ESP_OK;
}

esp_err_t mqtt_stop(mqtt_client_t *client)
{
    // Send disconnect?
    // Unhandshake?
    // Close
    return ESP_OK;
}

esp_err_t mqtt_client_id(char *buf)
{
    esp_err_t err;
    const int mac_id_len = 6;
    int len;
    uint8_t mac_id[mac_id_len];

    err = esp_efuse_read_mac(mac_id);
    if (err != ESP_OK)
        return err;

    len = snprintf(buf, MQTT_CLIENT_ID_LEN, "ESP-");
    for (int i = 0; i < mac_id_len; i++)
        len += snprintf(buf + len, MQTT_CLIENT_ID_LEN - len, "%02X", mac_id[i]);
    buf[len++] = '\0';

    return ESP_OK;
}

esp_err_t mqtt_publish(mqtt_client_t *client, unsigned char dup, int qos, unsigned char retained, unsigned short packetid,
                       const unsigned char *topicName, unsigned char* payload, int payloadlen)
{
    return ESP_OK;
}
