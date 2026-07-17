// All-read-only broadcast with a MULTI-RATE output (replication). Producer @a
// fans out through an explicit iara_broadcast to two read-only consumers:
//   - @b reads the buffer 1:1 (fires once per buffer, mult=1)
//   - @c reads a 3x-replicated view 2 ints at a time, firing 3x (mult=3); each
//     firing aliases @a's ONE physical buffer via the toroidal wrap
//     (offset mod L, L = 8 bytes), so all three reads see [10,20] zero-copy.
//
// FirstKeepsBuffer target: 0 copies, 4 reuses (1 from @b + 3 from @c). The join
// owns the buffer and frees it once after 1 (@b) + 3 (@c) = 4 logic tokens.
// Baseline (CopyAllButOne): the broadcast physically replicates [10,20] x3 into
// a 6-int buffer, and @b's read is a separate copy.
//
// This exercises the convert path (explicit @iara_broadcast, multi-rate reader),
// the case SIFT's L -> K*L broadcasts hit -- unlike 22-ro-broadcast (all 1:1).

iara.actor @run {
  %a = iara.node @a out tensor<2xi32>
  %b0, %b1 = iara.node @iara_broadcast in ( %a : tensor<2xi32> )
                                       out ( tensor<2xi32>, tensor<6xi32> )
  iara.node @b in %b0 : tensor<2xi32>
  %e = iara.edge %b1 : tensor<6xi32> -> tensor<2xi32>
  iara.node @c in %e : tensor<2xi32>
} { flat }
