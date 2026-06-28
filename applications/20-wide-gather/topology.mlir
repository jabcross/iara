// Wide gather: three producers feed one sink with three input ports. Combined
// with the per-set build flag IARA_SEMAPHORE_INLINE_ARGS=2 (see
// experiment/experiments.yaml), the sink's arg count (3) exceeds the ring
// slot's inline capacity (K=2), forcing the ArgStore heap-allocation path and
// its cleanup in release().

iara.actor @run  {
  %0 = iara.node @a out tensor<1xi32>
  %1 = iara.node @b out tensor<1xi32>
  %2 = iara.node @c out tensor<1xi32>
  iara.node @sink in %0 : tensor<1xi32>, %1 : tensor<1xi32>, %2 : tensor<1xi32>
} { }
