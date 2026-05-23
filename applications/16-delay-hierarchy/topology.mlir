// 16-delay-hierarchy: delay edge crossing a hierarchical actor interface
//
// Tests that the flatten pass preserves delay attributes: an outer edge
// with a bare-integer delay feeds a sub-actor whose internal edge has
// no delay but connects directly to the internal node.  Without the
// flatten-delay transfer, the delay is lost, the internal node deadlocks
// waiting for initial tokens, and no output reaches the checker.
//
// Pattern derived from 11-forward-delay (flat, works) but with a
// sub-actor barrier that previously caused the delay to be dropped.

iara.actor @sub {
  %in = iara.in : tensor<3xi32>
  %n = iara.node @b in (%in:tensor<3xi32>) out (tensor<3xi32>)
  iara.out (%n : tensor<3xi32>)
}

iara.actor @run  {
  %a = iara.node @a out tensor<9xi32>
  %e = iara.edge %a : tensor<9xi32> -> tensor<3xi32> {delay = 3}
  %s = iara.node @sub in %e: tensor<3xi32> out tensor<3xi32>
  %o = iara.edge %s : tensor<3xi32> -> tensor<9xi32>
  iara.node @c in %o: tensor<9xi32>
}
