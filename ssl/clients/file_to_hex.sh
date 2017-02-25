#!/bin/bash

if [ $# -lt 1 ]; then
    echo "Usage: ${0} <filename> [<varname>]" >&2
    exit 1
fi

filename="${1}"
varname="${2:-$(basename ${filename} | tr '.' '_')}"

echo "${varname}"
while IFS='' read -r line || [[ -n "${line}" ]]; do
    printf "%s\r\n" "${line}" | od -A n -t x1 | tr -d " \t\n\r"
done < "${filename}"
echo