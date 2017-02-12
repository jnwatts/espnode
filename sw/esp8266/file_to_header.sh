#!/bin/bash

if [ $# -lt 1 ]; then
    echo "Usage: ${0} <filename> [<varname>]" >&2
    exit 1
fi

filename="${1}"
varname="${2:-$(basename ${filename} | tr '.' '_')}"

echo -n "const char *${varname} = "
while IFS='' read -r line || [[ -n "${line}" ]]; do
    echo
    echo -n "   \"${line}\r\n\""
done < "${filename}"

echo ";"