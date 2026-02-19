FLAGS="-fopenmp -lomp -stdlib=libstdc++ -DSCHEDULER_IARA -I$IARA_DIR/external -I $IARA_DIR/include -I$IARA_DIR/external "

rm -rf *.o
rm -f a.out
clang++ $FLAGS -g -std=c++26 -c $IARA_DIR/runtime/virtual-fifo/*.cpp $IARA_DIR/runtime/virtual-fifo/*.cpp $IARA_DIR/runtime/common/*.cpp $IARA_DIR/external/enkiTS/*.cpp
clang++ $FLAGS -g -c -std=c++26 schedule.ll
clang $FLAGS -g -c  $IARA_DIR/applications/experiment-framework/*.c
clang++ -std=c++26 $FLAGS -g -c  $IARA_DIR/applications/experiment-framework/*.cpp
clang++ $FLAGS -g -lm *.o

