// A pure control-only (logic) dependency: node @a has a `none`-typed output,
// node @c a `none`-typed input, and there is no data edge between them at
// all. Exercises that a logic edge alone (no buffer, not a kernel arg) gates
// consumer firing.

iara.actor @run  {
  %0 = iara.node @a out none
  iara.node @c in %0 : none
} { }
