
iara.actor @kernel_potrf {
              
 %1 = iara.in inout : tensor<1xf64>
  iara.out : tensor<1xf64>
  iara.dep
} { kernel }


iara.actor @kernel_trsm {
              
 %1 = iara.in  : tensor<1xf64>
 %2 = iara.in inout : tensor<1xf64>
  iara.out : tensor<1xf64>
  iara.dep
} { kernel }


iara.actor @kernel_gemm {
              
 %1 = iara.in  : tensor<1xf64>
 %2 = iara.in  : tensor<1xf64>
 %3 = iara.in inout : tensor<1xf64>
  iara.out : tensor<1xf64>
  iara.dep
} { kernel }


iara.actor @kernel_syrk {
              
 %1 = iara.in  : tensor<1xf64>
 %2 = iara.in inout : tensor<1xf64>
  iara.out : tensor<1xf64>
  iara.dep
} { kernel }


iara.actor @kernel_split {

  iara.out : tensor<1xf64>
  iara.out : tensor<1xf64>
  iara.out : tensor<1xf64>
  iara.out : tensor<1xf64>

} { kernel }


iara.actor @kernel_join {

    iara.in : tensor<1xf64>
    iara.in : tensor<1xf64>
    iara.in : tensor<1xf64>
    iara.in : tensor<1xf64>
  iara.dep
} { kernel }

iara.actor @main {
  %e_0_0_0, %e_0_1_0, %e_1_0_0, %e_1_1_0 = iara.node @kernel_split out : tensor<1xf64>, tensor<1xf64>, tensor<1xf64>, tensor<1xf64>
  %e_0_0_1 = iara.node @kernel_potrf inout %e_0_0_0 : tensor<1xf64> out : tensor<1xf64>
  %e_1_1_1 = iara.node @kernel_potrf inout %e_1_1_0 : tensor<1xf64> out : tensor<1xf64>
  %e_0_1_1 = iara.node @kernel_trsm in %e_0_0_1 : tensor<1xf64> inout %e_0_1_0 : tensor<1xf64> out : tensor<1xf64>
  %e_1_1_2 = iara.node @kernel_syrk in %e_0_1_1 : tensor<1xf64> inout %e_1_1_1 : tensor<1xf64> out : tensor<1xf64>
  iara.node @kernel_join in %e_0_0_1, %e_0_1_1, %e_1_0_0, %e_1_1_2 : tensor<1xf64>, tensor<1xf64>, tensor<1xf64>, tensor<1xf64>
  iara.dep
} { flat }
