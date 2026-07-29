// F4 regression: parameters passed across a sub-graph hierarchy boundary.
// @inner is a sub-graph actor with a block-arg param %p. @run instantiates it
// TWICE with different param values (a=7, b=11). --flatten clones @inner per
// call site, substituting the callee's %p with the caller node's param operand
// (FlattenPass.cpp:181-190), so each @emit sees its own value. Both instances
// feed one @check (a single connected graph, like 04-multithread) which sees
// both. Covers cross-hierarchy params + per-call-site specialization.
iara.actor @inner(%p: i64) {
  %0 = iara.node @emit params %p : i64 out i32
  iara.out(%0 : i32)
}

iara.actor @run(%a: i64, %b: i64) {
  %x = iara.node @inner params %a : i64 out i32
  %y = iara.node @inner params %b : i64 out i32
  iara.node @check in %x : i32, %y : i32
} { default_params = [{a = 7}, {b = 11}] }
