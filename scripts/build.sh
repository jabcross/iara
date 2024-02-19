SCRIPT_DIR=$(dirname "$(readlink -f "$0")")

source $SCRIPT_DIR/../.env

# if build directory does not exist, create it
if [ ! -d "$IARA_DIR/build" ]; then
  mkdir -p $IARA_DIR/build
fi

cd build

cmake -G Ninja .. -DMLIR_DIR=$LLVM_DIR/build/lib/cmake/mlir -DLLVM_EXTERNAL_LIT=$LLVM_DIR/build/bin/llvm-lit
cmake --build . --target check-iara
