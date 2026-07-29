// F2 regression: computed parameter via arith + SCCP constant folding.
// Mirrors degridder's `(nkp+1)*OVERSAMPLING` shape: area = (rows+1)*cols.
// With rows=5, cols=7  =>  area = (5+1)*7 = 42. --iara-param-materialize
// replaces the block-arg params with arith.constant, then nested --sccp folds
// the addi/muli chain to a single constant that reaches @produce as a scalar arg.
iara.actor @run(%rows: i64, %cols: i64) {
  %c1 = arith.constant 1 : i64
  %r1 = arith.addi %rows, %c1 : i64
  %area = arith.muli %r1, %cols : i64
  %0 = iara.node @produce params %area : i64 out i32
  iara.node @check in %0 : i32
} { default_params = [{rows = 5}, {cols = 7}] }
