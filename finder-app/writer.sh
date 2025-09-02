#!/bin/sh

writefile=$1
writestr=$2

if [ -z "$writefile" ] || [ -z "$writestr" ]; then #arguments not specified
    echo "Error: One or both parameters not specified"
    exit 1
fi

mkdir -p "$(dirname "$writefile")" || exit 1
echo "$writestr" > "$writefile" || exit 1