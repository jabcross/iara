# This file is executed by build-single-test.sh

# export PATH_TO_TEST_SOURCES=$IARA_DIR/test/Iara/$FOLDER_NAME
# export PATH_TO_TEST_BUILD_DIR=$IARA_DIR/build/test/Iara/$FOLDER_NAME

echo Generating topology and IO kernels

if [[ -z $PATH_TO_TEST_SOURCES ]]; then
  echo "Must specify PATH_TO_TEST_SOURCES"
  exit 1
fi

if [[ -z $PATH_TO_TEST_BUILD_DIR ]]; then
  echo "Must specify PATH_TO_TEST_BUILD_DIR"
  exit 1
fi

if [[ -z $NUM_BLOCKS ]]; then
  echo "Must specify NUM_BLOCKS"
  exit 1
fi

if [[ -z $MATRIX_SIZE ]]; then
  echo "Must specify MATRIX_SIZE"
  exit 1
fi

echo MATRIX_SIZE = $MATRIX_SIZE
echo NUM_BLOCKS = $NUM_BLOCKS

python $PATH_TO_TEST_SOURCES/generate_split_join.py $MATRIX_SIZE $NUM_BLOCKS > kernel_split_join.inc.h

python $PATH_TO_TEST_SOURCES/generate_topology.py $MATRIX_SIZE $NUM_BLOCKS > topology.mlir

echo End generating topology and IO kernels
