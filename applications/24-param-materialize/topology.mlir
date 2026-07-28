// Param-materialize regression. Block-arg param `n` (default 42) is materialized
// by --iara-param-materialize into an arith.constant and passed to @produce as a
// compile-time kernel argument. @produce writes n into its 1-element output;
// @check reads it back. main asserts the observed value is 42.
//
// Exercises end-to-end: block-arg param -> default_params -> arith.constant
// (Phase 1) -> SCCP (Phase 2, no-op here) -> node param -> scalar kernel arg.
iara.actor @run(%n: i64) {
  %0 = iara.node @produce params %n : i64 out i32
  iara.node @check in %0 : i32
} { default_params = [{n = 42}] }
