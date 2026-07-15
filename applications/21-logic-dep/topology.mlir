// Logic (control-only) dependencies with NO data edges. Two producers @a1/@a2
// each have a `none`-typed output; consumer @c has two `none`-typed inputs and
// no data input at all. @c must fire only after BOTH tokens arrive — i.e. its
// firing threshold must equal 2 logic tokens.
//
// This is the N-ary join shape (N=2). It discriminates the data-triggered logic
// gating: if the threshold drops logic tokens (the old bug), @c has threshold 0,
// so it is both source-fired by the scheduler and fires on the first token —
// running before @a2 (and possibly before @a1), which the self-check catches.

iara.actor @run  {
  %0 = iara.node @a1 out none
  %1 = iara.node @a2 out none
  iara.node @c in ( %0 : none, %1 : none )
} { }
