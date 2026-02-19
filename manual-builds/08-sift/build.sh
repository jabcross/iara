sh -x codegen.sh
iara-opt --flatten --iara-canonicalize --virtual-fifo=main-actor=run $IARA_DIR/applications/08-sift/topology.mlir > schedule.mlir
$IARA_DIR/scripts/mlir-to-llvmir.sh schedule.mlir
FLAGS="-fopenmp -lomp -stdlib=libstdc++ -DSCHEDULER_IARA -I$IARA_DIR/external -I $IARA_DIR/include -I$IARA_DIR/external "
FLAGS=$FLAGS -I$IARA_DIR/applications/08-sift/include

rm -rf *.o
rm -f a.out
clang++ $FLAGS -g -std=c++26 -c $IARA_DIR/runtime/virtual-fifo/*.cpp $IARA_DIR/runtime/virtual-fifo/*.cpp $IARA_DIR/runtime/common/*.cpp $IARA_DIR/external/enkiTS/*.cpp
clang++ $FLAGS -g -c -std=c++26 schedule.ll
clang $FLAGS -g -c  $IARA_DIR/applications/08-sift/*.c
clang++ -std=c++26 $FLAGS -g -c  $IARA_DIR/applications/08-sift/*.cpp
clang++ $FLAGS -g -lm *.o

