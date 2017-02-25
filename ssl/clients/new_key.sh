#!/bin/sh

if [ $# -lt 1 ]; then
    echo "Usage ${0} <client-id> [<key-size>]" >&2
    exit 1
fi

CLIENT_ID="${1}"
KEY_SIZE="${2:-2048}"

openssl genrsa -out private/"${CLIENT_ID}".key.pem "${SIZE}"
