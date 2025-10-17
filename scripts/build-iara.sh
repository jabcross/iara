#!/bin/sh
SCRIPT_DIR=$(dirname "$(readlink -f "$0")")

source $SCRIPT_DIR/../.env

# if build directory does not exist, create it
if [ ! -d "$IARA_DIR/build" ]; then
	mkdir -p $IARA_DIR/build
fi

cd $IARA_DIR/build

# rm -rf .ninja_deps .ninja_log build.ninja CMakeCache.txt


export CC=$LLVM_INSTALL/bin/clang
export CXX=$LLVM_INSTALL/bin/clang++

# Set default build type to Debug if not specified
if [ -z "$CMAKE_BUILD_TYPE" ]; then
	export CMAKE_BUILD_TYPE="Debug"
fi

# cmake -G Ninja .. -DCMAKE_BUILD_TYPE=Debug -DMLIR_DIR=$LLVM_DIR/build/lib/cmake/mlir -DLLVM_EXTERNAL_LIT=$LLVM_DIR/build/bin/llvm-lit -DLLVM_PARALLEL_LINK_JOBS=1 -DLLVM_DIR=$LLVM_DIR/lib/cmake/llvm -DLLVM_USE_LINKER=mold
cmake -G "Ninja" .. \
	-DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE \
	-DMLIR_DIR=$LLVM_SOURCES/build/lib/cmake/mlir \
	-DLLVM_EXTERNAL_LIT=$LLVM_SOURCES/build/bin/llvm-lit \
	-DLLVM_PARALLEL_LINK_JOBS=1 \
	-DLLVM_DIR=$LLVM_INSTALL/lib/cmake/llvm \ -DLLVM_USE_LINKER=mold

sed -i 's/ -fno-lifetime-dse//' compile_commands.json

ninja
