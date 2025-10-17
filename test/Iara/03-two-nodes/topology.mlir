iara.actor @run {
  %in = iara.in inout : tensor<16777216xf64>
  %e_0_0_0, %e_0_1_0, %e_1_0_0, %e_1_1_0 = iara.node @kernel_split out ( tensor<16777216xf64>, tensor<16777216xf64>, tensor<16777216xf64>, tensor<16777216xf64> ) 
  %e_0_0_1 = iara.node @kernel_potrf inout ( %e_0_0_0:tensor<16777216xf64> )
 out ( tensor<16777216xf64> )
  %e_0_1_1 = iara.node @kernel_trsm in ( %e_0_0_1:tensor<16777216xf64> )
 inout ( %e_0_1_0:tensor<16777216xf64> )
 out ( tensor<16777216xf64> )
  %e_1_1_1 = iara.node @kernel_syrk in ( %e_0_1_1:tensor<16777216xf64> )
 inout ( %e_1_1_0:tensor<16777216xf64> )
 out ( tensor<16777216xf64> )
  %e_1_1_2 = iara.node @kernel_potrf inout ( %e_1_1_1:tensor<16777216xf64> )
 out ( tensor<16777216xf64> )
  iara.node @kernel_join in (%e_0_0_1 : tensor<16777216xf64>, %e_0_1_1 : tensor<16777216xf64>, %e_1_0_0 : tensor<16777216xf64>, %e_1_1_2 : tensor<16777216xf64>)}
