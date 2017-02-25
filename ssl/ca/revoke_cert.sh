#!/bin/bash

if [ $# -lt 1 ]; then
    echo "Usage ${0} <csr.pem>" >&2
    exit 1
fi

CERT="${1}"
openssl ca -config intermediate.cnf -revoke "${CERT}"
