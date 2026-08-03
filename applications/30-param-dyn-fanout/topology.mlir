// F5 regression: a dynamic-typed value fanned out through the copy-path
// broadcast, where one consumer is hierarchical. Canonicalize must insert the
// fan-out broadcast with a PLACEHOLDER output (it can't size a dynamic type
// yet, and borrow would rebuild the hierarchical @sink node pre-flatten,
// breaking its signature match) and always keep one edge per output/input.
// Flatten inlines @sink; --iara-param-materialize then resolves the
// placeholder to the real static type and renames/codegens the broadcast.
//
// @emit (sub-graph) produces a param-sized tensor [1..n]. @run fans it out to
// @sink (a hierarchical sub-graph that checks the first element) and @check (a
// leaf that verifies the whole buffer). main asserts both saw the data.
iara.actor @emit(%n: i64) {
  %n_idx = arith.index_cast %n : i64 to index
  %a = iara.node @produce out tensor<?xi32> sizes [%n_idx]
  iara.out(%a : tensor<?xi32>) <%n_idx>
}

iara.actor @sink(%n: i64) {
  %n_idx = arith.index_cast %n : i64 to index
  %in = iara.in <%n_idx> : tensor<?xi32>
  %h = iara.node @check_head in %in : tensor<?xi32> out i32
  iara.out(%h : i32)
}

iara.actor @run(%n: i64) {
  %n_idx = arith.index_cast %n : i64 to index
  %x = iara.node @emit params %n : i64 out tensor<?xi32> sizes [%n_idx]
  %t = iara.node @sink
    params %n : i64
    in %x : tensor<?xi32>
    out i32
  iara.node @check in %x : tensor<?xi32>
  iara.node @record in %t : i32
} { default_params = [{n = 4}] }
