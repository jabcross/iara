#!/bin/sh
SCRIPT_DIR=$(dirname "$(readlink -f "$0")")

source $SCRIPT_DIR/../.env

# if build directory does not exist, create it
if [ ! -d "$IARA_DIR/build" ]; then
	mkdir -p $IARA_DIR/build

fi

cd $IARA_DIR
cd build

cmake -G Ninja .. -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_BUILD_TYPE=Debug -DMLIR_DIR=$LLVM_DIR/build/lib/cmake/mlir -DLLVM_EXTERNAL_LIT=$LLVM_DIR/build/bin/llvm-lit -DCMAKE_EXPORT_COMPILE_COMMANDS=YES -DLLVM_CCACHE_BUILD=ON -DLLVM_PARALLEL_LINK_JOBS=1 -DLLVM_DIR=$LLVM_DIR/lib/cmake/llvm -DLLVM_USE_LINKER=mold
cmake -E env IARA_DIR=$IARA_DIR SCHEDULER_MODE=ooo-scheduler cmake --build . --target check-iara
sed 's/ -fno-lifetime-dse//' <compile_commands.json >$IARA_DIR/compile_commands.json
