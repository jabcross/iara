// multithread

iara.actor @run  {
  %1 = iara.node @a out tensor<1xi64>
  %2 = iara.node @b out tensor<1xi64>
  iara.node @c in %1: tensor<1xi64>, %2: tensor<1xi64>
}

// CHECK: Ran in different threads