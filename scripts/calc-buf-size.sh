#!/bin/sh

SCRIPT_DIR=$(dirname "$(readlink -f "$0")")

cd $SCRIPT_DIR

python calc-buf-size.py "$@"
