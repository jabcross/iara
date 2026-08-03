// Regression: multi-rate iara.edge whose `out` size is genuinely
// param-derived (an arith chain from a block-arg, not a literal) —
// combines EdgeOp's out_static_sizes/out_dynamic_sizes (memref-style mixed
// static/dynamic size list) with block-arg param materialization. Mirrors
// 10-multi-rate's proven runtime pattern (a direct 2->3 expanding edge,
// no explicit broadcast node, IaRa's toroidal multi-rate read) but drives
// the expansion factor via %k instead of a fixed literal.
//
// @produce writes [10, 20] into a 2-element buffer. The edge presents
// `rep` (= k*2) elements to @consume via the toroidal wraparound read.
// main asserts the read-back values are k repetitions of [10, 20].
iara.actor @run(%k: i64) {
  %c2 = arith.constant 2 : i64
  %rep = arith.muli %k, %c2 : i64
  %rep_idx = arith.index_cast %rep : i64 to index
  %a = iara.node @produce out tensor<2xi32>
  %e = iara.edge %a : tensor<2xi32> -> tensor<?xi32> sizes [%rep_idx]
  iara.node @consume params %k : i64 in %e : tensor<?xi32>
} { default_params = [{k = 3}] }
