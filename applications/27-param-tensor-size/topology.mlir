// F3 regression: param-driven NODE-OUTPUT tensor size ("different matrix sizes").
// %n (default 4) sizes @produce's dynamic output buffer via the new `sizes`
// clause. --iara-param-materialize resolves `out tensor<?xi32> sizes %n` to a
// static `tensor<4xi32>` (and the edge + consumer input follow). %n is index for
// the size operand; index_cast gives the i64 the kernels take to know the count.
iara.actor @run(%n: index) {
  %n_i64 = arith.index_cast %n : index to i64
  %buf = iara.node @produce params %n_i64 : i64 out tensor<?xi32> sizes %n
  iara.node @consume params %n_i64 : i64 in %buf : tensor<?xi32>
} { default_params = [{n = 4}] }
