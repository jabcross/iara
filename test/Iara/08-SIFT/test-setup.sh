python $IARA_DIR/scripts/dif-to-iara.py $IARA_DIR/test/Iara/08-SIFT/topology.dif \
  | sed 's:module {:!SiftKpt = !llvm.struct<"struct.SiftKeypoint", (i32, i32, f32, f32, f32, f32, f32, f32, f32, f32, f32, array<128 x f32>)>\nmodule {:' >$IARA_DIR/build/test/Iara/08-SIFT/build/topology.mlir

export TOPOLOGY_FILE=$IARA_DIR/build/test/Iara/08-SIFT/build/topology.mlir
