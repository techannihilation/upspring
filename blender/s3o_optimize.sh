#!/bin/bash

if [[ -z "$1" ]]; then
    echo "Usage: ./s3o_optimize.sh <folder_with_.s3o>"
    exit 1
fi

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

PROCS=$(expr $(nproc) / 2)

find "${1}" -iname '*.s3o' -print0 | xargs -0 -I {} -P ${PROCS} blender -b -P ${SCRIPT_DIR}/s3o_optimize.py -- "{}"
