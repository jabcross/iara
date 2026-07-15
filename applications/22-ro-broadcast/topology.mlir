// All-read-only broadcast: producer @a fans out to three read-only consumers
// @b. Every consumer borrows the same buffer (no copy). Target behavior with
// the FirstKeepsBuffer ownership strategy: 0 copies, 3 reuses — all three @b
// read @a's original buffer, freed once after all three finish (join -> dealloc).
//
// Baseline (CopyAllButOne, today): 1 reuse (aliased out[0]) + 2 copies.

iara.actor @run  {
  %1 = iara.node @a out i32
  iara.node @b in %1 : i32
  iara.node @b in %1 : i32
  iara.node @b in %1 : i32
} { flat }
