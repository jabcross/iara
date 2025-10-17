#!/bin/sh
SCRIPT_DIR=$(dirname "$(readlink -f "$0")")

source $SCRIPT_DIR/../.env

# if build directory does not exist, create it
if [ ! -d "$IARA_DIR/build" ]; then
	mkdir -p $IARA_DIR/build
fi

cd $IARA_DIR
cd build

if [ -z $SCHEDULER_MODE ]; then
	echo "Must specify SCHEDULER_MODE"
	exit 1
fi

export SCHEDULER_MODE=$SCHEDULER_MODE

for i in $IARA_DIR/test/Iara/* ; do
	cd $i
	run-lit-test.sh .
done
