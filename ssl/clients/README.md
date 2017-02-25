# SSL scripts

After flashing the device, use command "client_id" to retreive the device's client ID. Now generate a new key and csr:
```
$ ./new_key.sh ESP-112233445566
$ ./new_csr.sh ESP-112233445566
```
This creates the following:
```
private/ESP-112233445566.key.pem
csr/ESP-112233445566.csr.pem
```
Submit the csr file to the certificate signing authority, who will return to you a signed certificate and their root certificate. Ideally, place these in the certs folder:
```
certs/ESP-112233445566.cert.pem
certs/ca_root.cert.pem
```
Finally you need to import the CA root cert, client cert and client key into the device. To do so you need to convert each file into HEX:
```
host $ ./file_to_hex.sh certs/ca_root.cert.pem
<lots of hex data, copy all of it>
device $ ssl ca_cert
<paste the hex data>
```
Repeat for client_cert and client_key. SSL configuration should now be complete!
