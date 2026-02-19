for i in $(ls $IARA_DIR/applications); do
  mkdir -p $IARA_DIR/manual-builds/$i
  cd $IARA_DIR/manual-builds/$i

  rm -rf build.sh

  if [[ -f $IARA_DIR/applications/$i/codegen.sh ]]; then
    echo "sh -x \$IARA_DIR/applications/$i/codegen.sh" >> build.sh
  fi

  if [[ -f $IARA_DIR/applications/$i/topology.mlir ]]; then
    echo "iara-opt --flatten --iara-canonicalize --virtual-fifo=main-actor=run \$IARA_DIR/applications/$i/topology.mlir > schedule.mlir" >> build.sh
    echo "\$IARA_DIR/scripts/mlir-to-llvmir.sh schedule.mlir" >> build.sh
  fi

  echo "FLAGS=\"-fopenmp -lomp -stdlib=libstdc++ -DSCHEDULER_IARA -I\$IARA_DIR/external -I \$IARA_DIR/include -I\$IARA_DIR/external \"" >> build.sh

  if [[ -d $IARA_DIR/applications/$i/include ]]; then 
    echo "FLAGS=\$FLAGS -I\$IARA_DIR/applications/$i/include" >> build.sh
  fi  

  cat >> build.sh <<END

rm -rf *.o
rm -f a.out
clang++ \$FLAGS -g -std=c++26 -c \$IARA_DIR/runtime/virtual-fifo/*.cpp \$IARA_DIR/runtime/virtual-fifo/*.cpp \$IARA_DIR/runtime/common/*.cpp \$IARA_DIR/external/enkiTS/*.cpp
clang++ \$FLAGS -g -c -std=c++26 schedule.ll
clang \$FLAGS -g -c  \$IARA_DIR/applications/$i/*.c
clang++ -std=c++26 \$FLAGS -g -c  \$IARA_DIR/applications/$i/*.cpp
clang++ \$FLAGS -g -lm *.o

END

done