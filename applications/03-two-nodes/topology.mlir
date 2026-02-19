// Two nodes

iara.actor @run  {
  %1 = iara.node @a out tensor<1xi32>
  iara.node @b in %1 : tensor<1xi32>
}
