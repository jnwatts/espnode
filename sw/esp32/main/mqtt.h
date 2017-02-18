#ifndef MQTT_H
#define MQTT_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <MQTTPacket.h>
#include <mbedtls/config.h>
#include "mbedtls/platform.h"
#include "mbedtls/net.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/certs.h"
#include "mbedtls/x509.h"
#include "mbedtls/error.h"
#include "mbedtls/debug.h"
#include "mbedtls/timing.h"
#include <esp_err.h>

#define MQTT_CLIENT_ID_LEN 32
#define MQTT_HOSTNAME_LEN 64
#define MQTT_USERNAME_LEN 32
#define MQTT_PASSWORD_LEN 32

typedef struct mqtt_client_t {
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    uint32_t flags;
    mbedtls_x509_crt cacert;
    mbedtls_x509_crt clicert;
    mbedtls_pk_context pkey;
    mbedtls_net_context ctx;

    MQTTTransport transport;
    MQTTPacket_connectData data;

    TaskHandle_t task;

    unsigned char client_id[MQTT_CLIENT_ID_LEN];
    unsigned char *ca_cert_str;
    unsigned char *client_cert_str;
    unsigned char *client_key_str;
    unsigned char hostname[MQTT_HOSTNAME_LEN];
    unsigned char port[6];
    unsigned char username[MQTT_USERNAME_LEN];
    unsigned char password[MQTT_PASSWORD_LEN];
} mqtt_client_t;

/**
 * Initialize resources
 */
esp_err_t mqtt_init(mqtt_client_t *client);
/**
 * Free resources
 */
esp_err_t mqtt_close(mqtt_client_t *client);

/**
 * Start background task and connect to server
 */
esp_err_t mqtt_start(mqtt_client_t *client);
/**
 * Disconnect from server and stop background task
 */
esp_err_t mqtt_stop(mqtt_client_t *client);

/**
 * Fills buf with client-id
 * \param buf Buffer to be filled: Must be at least MQTT_CLIENT_ID_LEN characters long. Will be null terminated.
 */
esp_err_t mqtt_client_id(char *buf);

/**
 * Wraps MQTTSerialize_publish
 * /see MQTTSerialize_publish
 */
esp_err_t mqtt_publish(mqtt_client_t *client, unsigned char dup, int qos, unsigned char retained, unsigned short packetid,
        const unsigned char *topicName, unsigned char* payload, int payloadlen);

#endif // MQTT_H