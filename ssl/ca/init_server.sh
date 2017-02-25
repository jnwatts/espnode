#!/bin/bash

if [ $# -lt 1 ]; then
    echo "Usage: ${0} <mqtt server FQDN>" >&2
    exit 1
fi

SERVER=${1:-mqtt.example.tld}

echo
echo "Create (unencrypted) server key"
openssl genrsa -out intermediate/private/"${SERVER}".key.pem 2048 || exit

echo
echo "Generate server CSR"
openssl req -config server.cnf -key intermediate/private/"${SERVER}".key.pem -new -sha256 -out intermediate/csr/"${SERVER}".csr.pem || exit

echo
echo "Generate server cert from CSR using intermediate key"
openssl ca -config server.cnf -extensions server_cert -days 375 -notext -md sha256 -in intermediate/csr/"${SERVER}".csr.pem -out intermediate/certs/"${SERVER}".cert.pem || exit
openssl x509 -noout -text -in intermediate/certs/"${SERVER}".cert.pem || exit

echo
echo "Verify server cert using ca-chain cert"
openssl verify -CAfile intermediate/certs/ca-chain.cert.pem intermediate/certs/"${SERVER}".cert.pem || exit
