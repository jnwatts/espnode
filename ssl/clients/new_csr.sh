#!/bin/sh

if [ $# -lt 1 ]; then
    echo "Usage ${0} <client-id>" >&2
    exit 1
fi

. ./csr.conf

CLIENT_ID="${1}"
SUBJECT="${SUBJECT_PREFIX:-/CN=}${CLIENT_ID}"
openssl req -out csr/"${CLIENT_ID}".csr.pem -key private/"${CLIENT_ID}".key.pem -new -subj "${SUBJECT}"
