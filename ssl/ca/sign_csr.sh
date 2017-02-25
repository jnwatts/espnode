#!/bin/bash

if [ $# -lt 1 ]; then
    echo "Usage ${0} <csr.pem>" >&2
    exit 1
fi

CSR="${1}"
CN="$(openssl req -noout -subject -in "${CSR}" | sed -e 's/.*CN=\(.*\)/\1/')"

if [[ ! "${CN}" == "ESP-"* ]]; then
    echo "CSR subject must end with CN=ESP-*" >&2
    exit 1
fi
openssl ca -config intermediate.cnf -extensions usr_cert -days 375 -notext -md sha256 -in "${CSR}" -out intermediate/certs/"${CN}".cert.pem
