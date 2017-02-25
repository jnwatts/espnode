#!/bin/bash

echo -n > index.txt || exit
echo 1000 > serial || exit
chmod 700 private

echo -n > intermediate/index.txt || exit
echo 1000 > intermediate/serial || exit
chmod 700 intermediate/private

echo
echo "Create root CA key"
openssl genrsa -aes256 -out private/ca.key.pem 4096 || exit

echo
echo "Generate self-signed root CA cert"
openssl req -config root.cnf -key private/ca.key.pem -new -x509 -days 7300 -sha256 -extensions v3_ca -out certs/ca.cert.pem || exit
openssl x509 -noout -text -in certs/ca.cert.pem || exit

echo
echo "Create intermediate key"
openssl genrsa -aes256 -out intermediate/private/intermediate.key.pem 4096 || exit

echo
echo "Generate intermediate CSR"
openssl req -config intermediate.cnf -new -sha256 -key intermediate/private/intermediate.key.pem -out intermediate/csr/intermediate.csr.pem || exit

echo
echo "Generate intermediate cert from CSR using root CA key"
openssl ca -config root.cnf -extensions v3_intermediate_ca -days 3650 -notext -md sha256 -in intermediate/csr/intermediate.csr.pem -out intermediate/certs/intermediate.cert.pem || exit
openssl x509 -noout -text -in intermediate/certs/intermediate.cert.pem || exit

echo
echo "Verify intermediate cert against root CA cert"
openssl verify -CAfile certs/ca.cert.pem intermediate/certs/intermediate.cert.pem || exit

echo
echo "Create ca-chain cert file (root CA last!)"
cat intermediate/certs/intermediate.cert.pem > intermediate/certs/ca-chain.cert.pem || exit
cat certs/ca.cert.pem >> intermediate/certs/ca-chain.cert.pem || exit
