export PATH_TO_APP_SOURCES=$IARA_DIR/applications/05-cholesky/
export MATRIX_SIZE=2048
export NUM_BLOCKS=4

sh -x $IARA_DIR/applications/05-cholesky/codegen.sh

iara-opt --flatten --iara-canonicalize --virtual-fifo=main-actor=run topology.mlir > schedule.mlir
$IARA_DIR/scripts/mlir-to-llvmir.sh schedule.mlir

FLAGS="-fopenmp -stdlib=libstdc++ -DSCHEDULER_IARA -I$IARA_DIR/external -I $IARA_DIR/include -I$IARA_DIR/external" 

LINKFLAGS="-L${SPACK_VIEW_PATH}/lib -Wl,-rpath,${SPACK_VIEW_PATH}/lib -lm -lomp -lopenblas"

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++")
# set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -v -L${SPACK_VIEW_PATH}/lib -Wl,--disable-new-dtags -Wl,-rpath,${SPACK_VIEW_PATH}/lib ${SPACK_VIEW_PATH}/lib/libstdc++.so.6")


rm -rf *.o
rm -f a.out
clang++ $FLAGS -g -std=c++26 -c $IARA_DIR/runtime/virtual-fifo/*.cpp $IARA_DIR/runtime/virtual-fifo/*.cpp $IARA_DIR/runtime/common/*.cpp $IARA_DIR/external/enkiTS/*.cpp
clang++ $FLAGS -g -c -std=c++26 schedule.ll
clang $FLAGS -g -c -DMATRIX_SIZE=$MATRIX_SIZE -DNUM_BLOCKS=$NUM_BLOCKS $IARA_DIR/applications/05-cholesky/src/*.c
clang++ $LINKFLAGS -fuse-ld=lld -g -lm -Wl,--disable-new-dtags *.o
