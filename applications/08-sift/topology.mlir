



module {
iara.actor @blur2x { // subgraph
    %gauss_coefs1_i_i = iara.in : tensor<126xf32>
    %imgOri_i_i = iara.in : tensor<512000xf32>
    %col_sizes1_i_i = iara.in : tensor<6xi32>
    %iterPrev_i_i = iara.in : tensor<512000xf32>
    %iter_nb1_i_i = iara.in : tensor<1xi32>
    %col_sizes2_i_i = iara.in : tensor<6xi32>
    %gauss_coefs2_i_i = iara.in : tensor<126xf32>
    %iter_nb2_i_i = iara.in : tensor<1xi32>
   %e1_s = iara.node @row_filter_transpose2x_1 in ( %gauss_coefs1_d:tensor<126xf32>, %imgOri_d:tensor<512000xf32>, %col_sizes1_d:tensor<6xi32>, %iterPrev_d:tensor<512000xf32>, %iter_nb1_d:tensor<1xi32> ) out ( tensor<512000xf32> )
   %e3_s = iara.node @row_filter_transpose2x_2 in ( %col_sizes2_d:tensor<6xi32>, %e2_d:tensor<512000xf32>, %gauss_coefs2_d:tensor<126xf32>, %iter_nb2_d:tensor<1xi32> ) out ( tensor<512000xf32> )
   %e2_s = iara.node @BarrierTranspose2x_1 in ( %e1_d:tensor<2048000xf32> ) out ( tensor<2048000xf32> )
   %imgBlurred_s = iara.node @BarrierTranspose2x_2 in ( %e3_d:tensor<2048000xf32> ) out ( tensor<2048000xf32> )
   %gauss_coefs1_d = iara.edge %gauss_coefs1_i_i : tensor<126xf32> -> tensor<126xf32>
   %imgOri_d = iara.edge %imgOri_i_i : tensor<512000xf32> -> tensor<512000xf32>
   %col_sizes1_d = iara.edge %col_sizes1_i_i : tensor<6xi32> -> tensor<6xi32>
   %iterPrev_d = iara.edge %iterPrev_i_i : tensor<512000xf32> -> tensor<512000xf32>
   %iter_nb1_d = iara.edge %iter_nb1_i_i : tensor<1xi32> -> tensor<1xi32>
   %e1_d = iara.edge %e1_s : tensor<512000xf32> -> tensor<2048000xf32>
   %col_sizes2_d = iara.edge %col_sizes2_i_i : tensor<6xi32> -> tensor<6xi32>
   %e2_d = iara.edge %e2_s : tensor<2048000xf32> -> tensor<512000xf32>
   %gauss_coefs2_d = iara.edge %gauss_coefs2_i_i : tensor<126xf32> -> tensor<126xf32>
   %iter_nb2_d = iara.edge %iter_nb2_i_i : tensor<1xi32> -> tensor<1xi32>
   %e3_d = iara.edge %e3_s : tensor<512000xf32> -> tensor<2048000xf32>
   %imgBlurred_o_i = iara.edge %imgBlurred_s : tensor<2048000xf32> -> tensor<2048000xf32>
    iara.out ( %imgBlurred_o_i : tensor<2048000xf32> )
} // end subgraph blur2x
iara.actor @blur { // subgraph
    %gauss_coefs1_i_i = iara.in : tensor<126xf32>
    %imgOri_i_i = iara.in : tensor<128000xf32>
    %col_sizes1_i_i = iara.in : tensor<6xi32>
    %iterPrev_i_i = iara.in : tensor<128000xf32>
    %iter_nb1_i_i = iara.in : tensor<1xi32>
    %iter_nb2_i_i = iara.in : tensor<1xi32>
    %gauss_coefs2_i_i = iara.in : tensor<126xf32>
    %col_sizes2_i_i = iara.in : tensor<6xi32>
   %e1_s = iara.node @row_filter_transpose_1 in ( %gauss_coefs1_d:tensor<126xf32>, %imgOri_d:tensor<128000xf32>, %col_sizes1_d:tensor<6xi32>, %iterPrev_d:tensor<128000xf32>, %iter_nb1_d:tensor<1xi32> ) out ( tensor<128000xf32> )
   %e3_s = iara.node @row_filter_transpose_2 in ( %gauss_coefs2_d:tensor<126xf32>, %e2_d:tensor<128000xf32>, %col_sizes2_d:tensor<6xi32>, %iter_nb2_d:tensor<1xi32> ) out ( tensor<128000xf32> )
   %e2_s = iara.node @BarrierTranspose_1 in ( %e1_d:tensor<512000xf32> ) out ( tensor<512000xf32> )
   %imgBlurred_s = iara.node @BarrierTranspose_2 in ( %e3_d:tensor<512000xf32> ) out ( tensor<512000xf32> )
   %gauss_coefs1_d = iara.edge %gauss_coefs1_i_i : tensor<126xf32> -> tensor<126xf32>
   %imgOri_d = iara.edge %imgOri_i_i : tensor<128000xf32> -> tensor<128000xf32>
   %col_sizes1_d = iara.edge %col_sizes1_i_i : tensor<6xi32> -> tensor<6xi32>
   %iterPrev_d = iara.edge %iterPrev_i_i : tensor<128000xf32> -> tensor<128000xf32>
   %iter_nb1_d = iara.edge %iter_nb1_i_i : tensor<1xi32> -> tensor<1xi32>
   %e1_d = iara.edge %e1_s : tensor<128000xf32> -> tensor<512000xf32>
   %gauss_coefs2_d = iara.edge %gauss_coefs2_i_i : tensor<126xf32> -> tensor<126xf32>
   %e2_d = iara.edge %e2_s : tensor<512000xf32> -> tensor<128000xf32>
   %col_sizes2_d = iara.edge %col_sizes2_i_i : tensor<6xi32> -> tensor<6xi32>
   %iter_nb2_d = iara.edge %iter_nb2_i_i : tensor<1xi32> -> tensor<1xi32>
   %e3_d = iara.edge %e3_s : tensor<128000xf32> -> tensor<512000xf32>
   %imgBlurred_o_i = iara.edge %imgBlurred_s : tensor<512000xf32> -> tensor<512000xf32>
    iara.out ( %imgBlurred_o_i : tensor<512000xf32> )
} // end subgraph blur
iara.actor @Htop_sift { // subgraph
    %image_i_i = iara.in : tensor<128000xi8>
   %e35_s, %e36_s = iara.node @compute_gaussian_coefs out ( tensor<6xi32>, tensor<126xf32> ) // No inputs - uses hardcoded SIFT parameters
   %e58_s = iara.node @extract_descriptor in ( %e6_d:tensor<351x!llvm.struct<(i32, i32, f32, f32, f32, f32, f32, f32, f32, f32, f32, array<128 x f32>)>>, %e14_d:tensor<8191500xf32>, %e16_d:tensor<8191500xf32> ) out ( tensor<351x!llvm.struct<(i32, i32, f32, f32, f32, f32, f32, f32, f32, f32, f32, array<128 x f32>)>> )
   %e6_s = iara.node @detect_keypoints in ( %e13_d:tensor<8191500xf32>, %e15_d:tensor<8191500xf32>, %e65_d:tensor<13652500xf32>, %e7_d:tensor<1xi32>, %e8_d:tensor<1xi32>, %e9_d:tensor<1xi32>, %e10_d:tensor<1xi32>, %e11_d:tensor<1xi32>, %e12_d:tensor<1xi32>, %e72_d:tensor<1xi32>, %e73_d:tensor<1xi32> ) out ( tensor<351x!llvm.struct<(i32, i32, f32, f32, f32, f32, f32, f32, f32, f32, f32, array<128 x f32>)>> )
   %e7_s, %e8_s, %e9_s, %e10_s, %e11_s, %e12_s, %e72_s, %e73_s = iara.node @ITERATOR_detect_keypoints out ( tensor<4xi32>, tensor<4xi32>, tensor<4xi32>, tensor<4xi32>, tensor<4xi32>, tensor<4xi32>, tensor<4xi32>, tensor<4xi32> )
   %e15_s, %e16_s = iara.node @iara_broadcast in ( %e30_d:tensor<8191500xf32> ) out ( tensor<32766000xf32>, tensor<32766000xf32> )
   %e13_s, %e14_s = iara.node @iara_broadcast in ( %e29_d:tensor<8191500xf32> ) out ( tensor<32766000xf32>, tensor<32766000xf32> )
   %e64_s = iara.node @build_dog_pyr in ( %e32_d:tensor<16383000xf32>, %e17_d:tensor<1xi32>, %e18_d:tensor<1xi32>, %e19_d:tensor<1xi32>, %e20_d:tensor<1xi32>, %e21_d:tensor<1xi32>, %e22_d:tensor<1xi32>, %e68_d:tensor<1xi32>, %e69_d:tensor<1xi32> ) out ( tensor<3413125xf32> )
   %e17_s, %e18_s, %e19_s, %e20_s, %e21_s, %e22_s, %e68_s, %e69_s = iara.node @ITERATOR_build_dog_pyr out ( tensor<4xi32>, tensor<4xi32>, tensor<4xi32>, tensor<4xi32>, tensor<4xi32>, tensor<4xi32>, tensor<4xi32>, tensor<4xi32> )
   %e29_s, %e30_s = iara.node @build_grd_rot_pyr in ( %e31_d:tensor<16383000xf32>, %e23_d:tensor<1xi32>, %e24_d:tensor<1xi32>, %e25_d:tensor<1xi32>, %e26_d:tensor<1xi32>, %e27_d:tensor<1xi32>, %e28_d:tensor<1xi32>, %e70_d:tensor<1xi32>, %e71_d:tensor<1xi32> ) out ( tensor<2047875xf32>, tensor<2047875xf32> )
   %e23_s, %e24_s, %e25_s, %e26_s, %e27_s, %e28_s, %e70_s, %e71_s = iara.node @ITERATOR_build_grd_rot_pyr out ( tensor<4xi32>, tensor<4xi32>, tensor<4xi32>, tensor<4xi32>, tensor<4xi32>, tensor<4xi32>, tensor<4xi32>, tensor<4xi32> )
   %e34_s = iara.node @to_float in ( %image_d:tensor<128000xi8> ) out ( tensor<128000xf32> )
   %e32_s, %e31_s = iara.node @iara_broadcast in ( %e33_d:tensor<16383000xf32> ) out ( tensor<65532000xf32>, tensor<65532000xf32> )
   %e33_s = iara.node @MERGE_gpyr in ( %e49_d:tensor<3072000xf32>, %e48_d:tensor<12288000xf32>, %e52_d:tensor<768000xf32>, %e54_d:tensor<768000xf32> ) out ( tensor<16383000xf32> )
   %e74_s, %e92_s, %e39_s = iara.node @iara_broadcast in ( %e34_d:tensor<512000xf32> ) out ( tensor<512000xf32>, tensor<3072000xf32>, tensor<512000xf32> )
   %e37_s = iara.node @upsample2x in ( %e75_d:tensor<128800xf32>, %e76_d:tensor<1xi32> ) out ( tensor<512000xf32> )
   %e50_s = iara.node @downsample2xN in ( %e38_d:tensor<128000xf32>, %e1_d:tensor<32000xf32>, %e56_d:tensor<1xi32> ) out ( tensor<32000xf32> )
   %e83_s, %e84_s, %e85_s, %e86_s, %e44_s, %e63_s = iara.node @iara_broadcast in ( %e36_d:tensor<126xf32> ) out ( tensor<3024xf32>, tensor<3024xf32>, tensor<3024xf32>, tensor<3024xf32>, tensor<756xf32>, tensor<3024xf32> )
   %e81_s, %e82_s, %e79_s, %e80_s, %e43_s, %e62_s = iara.node @iara_broadcast in ( %e35_d:tensor<6xi32> ) out ( tensor<144xi32>, tensor<144xi32>, tensor<144xi32>, tensor<144xi32>, tensor<36xi32>, tensor<144xi32> )
   %e91_s = iara.node @iara_broadcast in ( %e37_d:tensor<2048000xf32> ) out ( tensor<12288000xf32> )
   %e38_s, %e41_s = iara.node  @iara_broadcast in ( %e40_d:tensor<128000xf32> ) out ( tensor<512000xf32>, tensor<768000xf32> )
   %e51_s = iara.node @seq_blur1 in ( %e41_d:tensor<128000xf32>, %e2_d:tensor<128000xf32>, %e44_d:tensor<126xf32>, %e43_d:tensor<6xi32>, %e46_d:tensor<1xi32> ) out ( tensor<128000xf32> )
   %e40_s = iara.node @downsample2x1 in ( %e39_d:tensor<128000xf32> ) out ( tensor<32000xf32> )
   %e53_s = iara.node @seq_blurN in ( %e42_d:tensor<32000xf32>, %e3_d:tensor<32000xf32>, %e63_d:tensor<126xf32>, %e62_d:tensor<6xi32>, %e61_d:tensor<1xi32>, %e57_d:tensor<1xi32> ) out ( tensor<32000xf32> )
   %e42_s, %e1_s = iara.node @iara_broadcast in ( %e50_d:tensor<32000xf32> ) out ( tensor<192000xf32>, tensor<32000xf32> )
   %e45_s = iara.node @counterGpyrLayer out ( tensor<6xi32> )
   %e87_s, %e89_s, %e46_s, %e88_s, %e90_s, %e66_s = iara.node @iara_broadcast in ( %e45_d:tensor<1xi32> ) out ( tensor<4xi32>, tensor<4xi32>, tensor<1xi32>, tensor<4xi32>, tensor<4xi32>, tensor<1xi32> )
   %e55_s = iara.node @counterOctaveDownN out ( tensor<4xi32> )
   %e4_s, %e49_s = iara.node @iara_broadcast in ( %e77_d:tensor<512000xf32> ) out ( tensor<512000xf32>, tensor<512000xf32> )
   %e5_s, %e48_s = iara.node @iara_broadcast in ( %e78_d:tensor<2048000xf32> ) out ( tensor<2048000xf32>, tensor<2048000xf32> )
   %e2_s, %e52_s = iara.node @iara_broadcast in ( %e51_d:tensor<128000xf32> ) out ( tensor<128000xf32>, tensor<128000xf32> )
   %e3_s, %e54_s = iara.node @iara_broadcast in ( %e53_d:tensor<32000xf32> ) out ( tensor<32000xf32>, tensor<32000xf32> )
   %e56_s, %e57_s = iara.node @iara_broadcast in ( %e55_d:tensor<1xi32> ) out ( tensor<1xi32>, tensor<6xi32> )
   %keypoints_s, %nbKeypoints_s = iara.node @MERGE_keypoints in ( %e58_d:tensor<1404x!llvm.struct<(i32, i32, f32, f32, f32, f32, f32, f32, f32, f32, f32, array<128 x f32>)>> ) out ( tensor<1400x!llvm.struct<(i32, i32, f32, f32, f32, f32, f32, f32, f32, f32, f32, array<128 x f32>)>>, tensor<1xi32> )
   %e61_s = iara.node @iara_broadcast in ( %e67_d:tensor<6xi32> ) out ( tensor<24xi32> )
   %e65_s = iara.node @iara_broadcast in ( %e64_d:tensor<13652500xf32> ) out ( tensor<54610000xf32> )
   %e67_s = iara.node @BarrierCounterGpyr in ( %e66_d:tensor<6xi32> ) out ( tensor<6xi32> )
   %e75_s = iara.node @SPLIT_upsample2x in ( %e74_d:tensor<512000xf32> ) out ( tensor<515200xf32> )
   %e76_s = iara.node @counterPLevels out ( tensor<4xi32> )
   %e78_s = iara.node @blur2x in ( %e83_d:tensor<126xf32>, %e91_d:tensor<512000xf32>, %e81_d:tensor<6xi32>, %e5_d:tensor<512000xf32>, %e87_d:tensor<1xi32>, %e82_d:tensor<6xi32>, %e84_d:tensor<126xf32>, %e88_d:tensor<1xi32> ) out ( tensor<2048000xf32> )
   %e77_s = iara.node @blur in ( %e85_d:tensor<126xf32>, %e92_d:tensor<128000xf32>, %e79_d:tensor<6xi32>, %e4_d:tensor<128000xf32>, %e89_d:tensor<1xi32>, %e90_d:tensor<1xi32>, %e86_d:tensor<126xf32>, %e80_d:tensor<6xi32> ) out ( tensor<512000xf32> )
   %e35_d = iara.edge %e35_s : tensor<6xi32> -> tensor<6xi32>
   %e36_d = iara.edge %e36_s : tensor<126xf32> -> tensor<126xf32>
   %e6_d = iara.edge %e6_s : tensor<351x!llvm.struct<(i32, i32, f32, f32, f32, f32, f32, f32, f32, f32, f32, array<128 x f32>)>> -> tensor<351x!llvm.struct<(i32, i32, f32, f32, f32, f32, f32, f32, f32, f32, f32, array<128 x f32>)>>
   %e14_d = iara.edge %e14_s : tensor<32766000xf32> -> tensor<8191500xf32>
   %e16_d = iara.edge %e16_s : tensor<32766000xf32> -> tensor<8191500xf32>
   %e58_d = iara.edge %e58_s : tensor<351x!llvm.struct<(i32, i32, f32, f32, f32, f32, f32, f32, f32, f32, f32, array<128 x f32>)>> -> tensor<1404x!llvm.struct<(i32, i32, f32, f32, f32, f32, f32, f32, f32, f32, f32, array<128 x f32>)>>
   %e13_d = iara.edge %e13_s : tensor<32766000xf32> -> tensor<8191500xf32>
   %e15_d = iara.edge %e15_s : tensor<32766000xf32> -> tensor<8191500xf32>
   %e65_d = iara.edge %e65_s : tensor<54610000xf32> -> tensor<13652500xf32>
   %e7_d = iara.edge %e7_s : tensor<4xi32> -> tensor<1xi32>
   %e8_d = iara.edge %e8_s : tensor<4xi32> -> tensor<1xi32>
   %e9_d = iara.edge %e9_s : tensor<4xi32> -> tensor<1xi32>
   %e10_d = iara.edge %e10_s : tensor<4xi32> -> tensor<1xi32>
   %e11_d = iara.edge %e11_s : tensor<4xi32> -> tensor<1xi32>
   %e12_d = iara.edge %e12_s : tensor<4xi32> -> tensor<1xi32>
   %e72_d = iara.edge %e72_s : tensor<4xi32> -> tensor<1xi32>
   %e73_d = iara.edge %e73_s : tensor<4xi32> -> tensor<1xi32>
   %e30_d = iara.edge %e30_s : tensor<2047875xf32> -> tensor<8191500xf32>
   %e29_d = iara.edge %e29_s : tensor<2047875xf32> -> tensor<8191500xf32>
   %e32_d = iara.edge %e32_s : tensor<65532000xf32> -> tensor<16383000xf32>
   %e17_d = iara.edge %e17_s : tensor<4xi32> -> tensor<1xi32>
   %e18_d = iara.edge %e18_s : tensor<4xi32> -> tensor<1xi32>
   %e19_d = iara.edge %e19_s : tensor<4xi32> -> tensor<1xi32>
   %e20_d = iara.edge %e20_s : tensor<4xi32> -> tensor<1xi32>
   %e21_d = iara.edge %e21_s : tensor<4xi32> -> tensor<1xi32>
   %e22_d = iara.edge %e22_s : tensor<4xi32> -> tensor<1xi32>
   %e68_d = iara.edge %e68_s : tensor<4xi32> -> tensor<1xi32>
   %e69_d = iara.edge %e69_s : tensor<4xi32> -> tensor<1xi32>
   %e64_d = iara.edge %e64_s : tensor<3413125xf32> -> tensor<13652500xf32>
   %e31_d = iara.edge %e31_s : tensor<65532000xf32> -> tensor<16383000xf32>
   %e23_d = iara.edge %e23_s : tensor<4xi32> -> tensor<1xi32>
   %e24_d = iara.edge %e24_s : tensor<4xi32> -> tensor<1xi32>
   %e25_d = iara.edge %e25_s : tensor<4xi32> -> tensor<1xi32>
   %e26_d = iara.edge %e26_s : tensor<4xi32> -> tensor<1xi32>
   %e27_d = iara.edge %e27_s : tensor<4xi32> -> tensor<1xi32>
   %e28_d = iara.edge %e28_s : tensor<4xi32> -> tensor<1xi32>
   %e70_d = iara.edge %e70_s : tensor<4xi32> -> tensor<1xi32>
   %e71_d = iara.edge %e71_s : tensor<4xi32> -> tensor<1xi32>
   %image_d = iara.edge %image_i_i : tensor<128000xi8> -> tensor<128000xi8>
   %e34_d = iara.edge %e34_s : tensor<128000xf32> -> tensor<512000xf32>
   %e33_d = iara.edge %e33_s : tensor<16383000xf32> -> tensor<16383000xf32>
   %e49_d = iara.edge %e49_s : tensor<512000xf32> -> tensor<3072000xf32>
   %e48_d = iara.edge %e48_s : tensor<2048000xf32> -> tensor<12288000xf32>
   %e52_d = iara.edge %e52_s : tensor<128000xf32> -> tensor<768000xf32>
   %e54_d = iara.edge %e54_s : tensor<32000xf32> -> tensor<768000xf32>
   %e74_d = iara.edge %e74_s : tensor<512000xf32> -> tensor<512000xf32>
   %e92_d = iara.edge %e92_s : tensor<3072000xf32> -> tensor<128000xf32>
   %e39_d = iara.edge %e39_s : tensor<512000xf32> -> tensor<128000xf32>
   %e75_d = iara.edge %e75_s : tensor<515200xf32> -> tensor<128800xf32>
   %e76_d = iara.edge %e76_s : tensor<4xi32> -> tensor<1xi32>
   %e37_d = iara.edge %e37_s : tensor<512000xf32> -> tensor<2048000xf32>
   %e38_d = iara.edge %e38_s : tensor<512000xf32> -> tensor<128000xf32>
   %e1_d = iara.edge %e1_s : tensor<32000xf32> -> tensor<32000xf32> { delay = 32000 }
   %e56_d = iara.edge %e56_s : tensor<1xi32> -> tensor<1xi32>
   %e50_d = iara.edge %e50_s : tensor<32000xf32> -> tensor<32000xf32>
   %e83_d = iara.edge %e83_s : tensor<3024xf32> -> tensor<126xf32>
   %e84_d = iara.edge %e84_s : tensor<3024xf32> -> tensor<126xf32>
   %e85_d = iara.edge %e85_s : tensor<3024xf32> -> tensor<126xf32>
   %e86_d = iara.edge %e86_s : tensor<3024xf32> -> tensor<126xf32>
   %e44_d = iara.edge %e44_s : tensor<756xf32> -> tensor<126xf32>
   %e63_d = iara.edge %e63_s : tensor<3024xf32> -> tensor<126xf32>
   %e81_d = iara.edge %e81_s : tensor<144xi32> -> tensor<6xi32>
   %e82_d = iara.edge %e82_s : tensor<144xi32> -> tensor<6xi32>
   %e79_d = iara.edge %e79_s : tensor<144xi32> -> tensor<6xi32>
   %e80_d = iara.edge %e80_s : tensor<144xi32> -> tensor<6xi32>
   %e43_d = iara.edge %e43_s : tensor<36xi32> -> tensor<6xi32>
   %e62_d = iara.edge %e62_s : tensor<144xi32> -> tensor<6xi32>
   %e91_d = iara.edge %e91_s : tensor<12288000xf32> -> tensor<512000xf32>
   %e40_d = iara.edge %e40_s : tensor<32000xf32> -> tensor<128000xf32>
   %e41_d = iara.edge %e41_s : tensor<768000xf32> -> tensor<128000xf32>
   %e2_d = iara.edge %e2_s : tensor<128000xf32> -> tensor<128000xf32> { delay = 128000 }
   %e46_d = iara.edge %e46_s : tensor<1xi32> -> tensor<1xi32>
   %e51_d = iara.edge %e51_s : tensor<128000xf32> -> tensor<128000xf32>
   %e42_d = iara.edge %e42_s : tensor<192000xf32> -> tensor<32000xf32>
   %e3_d = iara.edge %e3_s : tensor<32000xf32> -> tensor<32000xf32> { delay = 32000 }
   %e61_d = iara.edge %e61_s : tensor<24xi32> -> tensor<1xi32>
   %e57_d = iara.edge %e57_s : tensor<6xi32> -> tensor<1xi32>
   %e53_d = iara.edge %e53_s : tensor<32000xf32> -> tensor<32000xf32>
   %e45_d = iara.edge %e45_s : tensor<6xi32> -> tensor<1xi32>
   %e87_d = iara.edge %e87_s : tensor<4xi32> -> tensor<1xi32>
   %e89_d = iara.edge %e89_s : tensor<4xi32> -> tensor<1xi32>
   %e88_d = iara.edge %e88_s : tensor<4xi32> -> tensor<1xi32>
   %e90_d = iara.edge %e90_s : tensor<4xi32> -> tensor<1xi32>
   %e66_d = iara.edge %e66_s : tensor<1xi32> -> tensor<6xi32>
   %e55_d = iara.edge %e55_s : tensor<4xi32> -> tensor<1xi32>
   %e77_d = iara.edge %e77_s : tensor<512000xf32> -> tensor<512000xf32>
   %e4_d = iara.edge %e4_s : tensor<512000xf32> -> tensor<128000xf32> { delay = 512000 }
   %e78_d = iara.edge %e78_s : tensor<2048000xf32> -> tensor<2048000xf32>
   %e5_d = iara.edge %e5_s : tensor<2048000xf32> -> tensor<512000xf32> { delay = 2048000 }
   %keypoints_o_i = iara.edge %keypoints_s : tensor<1400x!llvm.struct<(i32, i32, f32, f32, f32, f32, f32, f32, f32, f32, f32, array<128 x f32>)>> -> tensor<1400x!llvm.struct<(i32, i32, f32, f32, f32, f32, f32, f32, f32, f32, f32, array<128 x f32>)>>
   %nbKeypoints_o_i = iara.edge %nbKeypoints_s : tensor<1xi32> -> tensor<1xi32>
   %e67_d = iara.edge %e67_s : tensor<6xi32> -> tensor<6xi32>
    iara.out ( %keypoints_o_i : tensor<1400x!llvm.struct<(i32, i32, f32, f32, f32, f32, f32, f32, f32, f32, f32, array<128 x f32>)>> )
    iara.out ( %nbKeypoints_o_i : tensor<1xi32> )
} // end subgraph Htop_sift
iara.actor @run { // subgraph
   %e2_s, %e6_s, %e11_s = iara.node @iara_broadcast in ( %e4_d:tensor<512xi8> ) out ( tensor<512xi8>, tensor<512xi8>, tensor<512xi8> )
   %e1_s = iara.node @read_pgm in ( %e2_d:tensor<512xi8> ) out ( tensor<512000xi8> )
   %e3_s, %e5_s = iara.node @iara_broadcast in ( %e1_d:tensor<512000xi8> ) out ( tensor<512000xi8>, tensor<512000xi8> )
   iara.node @draw_keypoints_to_ppm_file in ( %e9_d:tensor<1400x!llvm.struct<(i32, i32, f32, f32, f32, f32, f32, f32, f32, f32, f32, array<128 x f32>)>>, %e10_d:tensor<1xi32>, %e5_d:tensor<512000xi8>, %e6_d:tensor<512xi8> )
   %e7_s, %e8_s = iara.node @Htop_sift in ( %e3_d:tensor<128000xi8> ) out ( tensor<1400x!llvm.struct<(i32, i32, f32, f32, f32, f32, f32, f32, f32, f32, f32, array<128 x f32>)>>, tensor<1xi32> )
   %e4_s = iara.node @filename1 out ( tensor<512xi8> )
   %e9_s, %e13_s = iara.node @iara_broadcast in ( %e7_d:tensor<1400x!llvm.struct<(i32, i32, f32, f32, f32, f32, f32, f32, f32, f32, f32, array<128 x f32>)>> ) out ( tensor<1400x!llvm.struct<(i32, i32, f32, f32, f32, f32, f32, f32, f32, f32, f32, array<128 x f32>)>>, tensor<1400x!llvm.struct<(i32, i32, f32, f32, f32, f32, f32, f32, f32, f32, f32, array<128 x f32>)>> )
   %e10_s, %e12_s = iara.node @iara_broadcast in ( %e8_d:tensor<1xi32> ) out ( tensor<1xi32>, tensor<1xi32> )
   iara.node @export_keypoints_to_key_file in ( %e11_d:tensor<512xi8>, %e13_d:tensor<1400x!llvm.struct<(i32, i32, f32, f32, f32, f32, f32, f32, f32, f32, f32, array<128 x f32>)>>, %e12_d:tensor<1xi32> )
   %e4_d = iara.edge %e4_s : tensor<512xi8> -> tensor<512xi8>
   %e2_d = iara.edge %e2_s : tensor<512xi8> -> tensor<512xi8>
   %e6_d = iara.edge %e6_s : tensor<512xi8> -> tensor<512xi8>
   %e11_d = iara.edge %e11_s : tensor<512xi8> -> tensor<512xi8>
   %e1_d = iara.edge %e1_s : tensor<512000xi8> -> tensor<512000xi8>
   %e3_d = iara.edge %e3_s : tensor<512000xi8> -> tensor<128000xi8>
   %e5_d = iara.edge %e5_s : tensor<512000xi8> -> tensor<512000xi8>
   %e9_d = iara.edge %e9_s : tensor<1400x!llvm.struct<(i32, i32, f32, f32, f32, f32, f32, f32, f32, f32, f32, array<128 x f32>)>> -> tensor<1400x!llvm.struct<(i32, i32, f32, f32, f32, f32, f32, f32, f32, f32, f32, array<128 x f32>)>>
   %e10_d = iara.edge %e10_s : tensor<1xi32> -> tensor<1xi32>
   %e7_d = iara.edge %e7_s : tensor<1400x!llvm.struct<(i32, i32, f32, f32, f32, f32, f32, f32, f32, f32, f32, array<128 x f32>)>> -> tensor<1400x!llvm.struct<(i32, i32, f32, f32, f32, f32, f32, f32, f32, f32, f32, array<128 x f32>)>>
   %e8_d = iara.edge %e8_s : tensor<1xi32> -> tensor<1xi32>
   %e13_d = iara.edge %e13_s : tensor<1400x!llvm.struct<(i32, i32, f32, f32, f32, f32, f32, f32, f32, f32, f32, array<128 x f32>)>> -> tensor<1400x!llvm.struct<(i32, i32, f32, f32, f32, f32, f32, f32, f32, f32, f32, array<128 x f32>)>>
   %e12_d = iara.edge %e12_s : tensor<1xi32> -> tensor<1xi32>
} // end subgraph run
}
