!SiftKpt = tensor<556xi8>
iara.actor @blur2x { // subgraph
    %gauss_coefs1 = iara.in : tensor<126xf32>
    %imgOri = iara.in : tensor<512000xf32>
    %col_sizes1 = iara.in : tensor<6xi32>
    %iterPrev = iara.in : tensor<512000xf32>
    %iter_nb1 = iara.in : tensor<1xi32>
    %col_sizes2 = iara.in : tensor<6xi32>
    %gauss_coefs2 = iara.in : tensor<126xf32>
    %iter_nb2 = iara.in : tensor<1xi32>
    %imgBlurred = iara.in : tensor<2048000xf32>
    %imgGT = iara.node @row_filter_transpose2x_1 in %gauss_coefs1, %imgOri, %col_sizes1, %iterPrev, %iter_nb1 : tensor<126xf32>, tensor<512000xf32>, tensor<6xi32>, tensor<512000xf32>, tensor<1xi32> out : tensor<512000xf32>
    %imgGT = iara.node @row_filter_transpose2x_2 in %col_sizes2, %e2, %gauss_coefs2, %iter_nb2 : tensor<6xi32>, tensor<512000xf32>, tensor<126xf32>, tensor<1xi32> out : tensor<512000xf32>
    %img_out = iara.node @BarrierTranspose2x_1 in %e1 : tensor<2048000xf32> out : tensor<2048000xf32>
    %img_out = iara.node @BarrierTranspose2x_2 in %e3 : tensor<2048000xf32> out : tensor<2048000xf32>
    iara.out ( %gauss_coefs1 : tensor<126xf32> ) : tensor<126xf32>
    iara.out ( %imgOri : tensor<512000xf32> ) : tensor<512000xf32>
    iara.out ( %col_sizes1 : tensor<6xi32> ) : tensor<6xi32>
    iara.out ( %iterPrev : tensor<512000xf32> ) : tensor<512000xf32>
    iara.out ( %iter_nb1 : tensor<1xi32> ) : tensor<1xi32>
    iara.out ( %col_sizes2 : tensor<6xi32> ) : tensor<6xi32>
    iara.out ( %gauss_coefs2 : tensor<126xf32> ) : tensor<126xf32>
    iara.out ( %iter_nb2 : tensor<1xi32> ) : tensor<1xi32>
    iara.out ( %imgBlurred : tensor<2048000xf32> ) : tensor<2048000xf32>
    iara.dep
} // end subgraph blur2x
iara.actor @blur { // subgraph
    %gauss_coefs1 = iara.in : tensor<126xf32>
    %imgOri = iara.in : tensor<128000xf32>
    %col_sizes1 = iara.in : tensor<6xi32>
    %iterPrev = iara.in : tensor<128000xf32>
    %iter_nb1 = iara.in : tensor<1xi32>
    %iter_nb2 = iara.in : tensor<1xi32>
    %gauss_coefs2 = iara.in : tensor<126xf32>
    %col_sizes2 = iara.in : tensor<6xi32>
    %imgBlurred = iara.in : tensor<512000xf32>
    %imgGT = iara.node @row_filter_transpose_1 in %gauss_coefs1, %imgOri, %col_sizes1, %iterPrev, %iter_nb1 : tensor<126xf32>, tensor<128000xf32>, tensor<6xi32>, tensor<128000xf32>, tensor<1xi32> out : tensor<128000xf32>
    %imgGT = iara.node @row_filter_transpose_2 in %gauss_coefs2, %e2, %col_sizes2, %iter_nb2 : tensor<126xf32>, tensor<128000xf32>, tensor<6xi32>, tensor<1xi32> out : tensor<128000xf32>
    %img_out = iara.node @BarrierTranspose_1 in %e1 : tensor<512000xf32> out : tensor<512000xf32>
    %img_out = iara.node @BarrierTranspose_2 in %e3 : tensor<512000xf32> out : tensor<512000xf32>
    iara.out ( %gauss_coefs1 : tensor<126xf32> ) : tensor<126xf32>
    iara.out ( %imgOri : tensor<128000xf32> ) : tensor<128000xf32>
    iara.out ( %col_sizes1 : tensor<6xi32> ) : tensor<6xi32>
    iara.out ( %iterPrev : tensor<128000xf32> ) : tensor<128000xf32>
    iara.out ( %iter_nb1 : tensor<1xi32> ) : tensor<1xi32>
    iara.out ( %iter_nb2 : tensor<1xi32> ) : tensor<1xi32>
    iara.out ( %gauss_coefs2 : tensor<126xf32> ) : tensor<126xf32>
    iara.out ( %col_sizes2 : tensor<6xi32> ) : tensor<6xi32>
    iara.out ( %imgBlurred : tensor<512000xf32> ) : tensor<512000xf32>
    iara.dep
} // end subgraph blur
iara.actor @Htop_sift { // subgraph
    %image = iara.in : tensor<128000xi8>
    %keypoints = iara.in : tensor<1400x!SiftKpt>
    %nbKeypoints = iara.in : tensor<1xi32>
    %columns_sizes, %gaussian_coefs = iara.node @compute_gaussian_coefs out : tensor<6xi32>, tensor<126xf32>
    %keypoints_out = iara.node @extract_descriptor in %e6, %e14, %e16 : tensor<351x!SiftKpt>, tensor<8191500xf32>, tensor<8191500xf32> out : tensor<351x!SiftKpt>
    %keypoints = iara.node @detect_keypoints in %e13, %e15, %e65, %e7, %e8, %e9, %e10, %e11, %e12, %e72, %e73 : tensor<8191500xf32>, tensor<8191500xf32>, tensor<13652500xf32>, tensor<1xi32>, tensor<1xi32>, tensor<1xi32>, tensor<1xi32>, tensor<1xi32>, tensor<1xi32>, tensor<1xi32>, tensor<1xi32> out : tensor<351x!SiftKpt>
    %start_octave, %stop_octave, %start_layer, %stop_layer, %start_line, %stop_line, %start_col, %stop_col = iara.node @ITERATOR_detect_keypoints out : tensor<4xi32>, tensor<4xi32>, tensor<4xi32>, tensor<4xi32>, tensor<4xi32>, tensor<4xi32>, tensor<4xi32>, tensor<4xi32>
    %forDetection, %forExtraction = iara.node @BdRot_BROADCAST in %e30 : tensor<8191500xf32> out : tensor<32766000xf32>, tensor<32766000xf32>
    %forDetection, %forExtraction = iara.node @BdGrd_BROADCAST in %e29 : tensor<8191500xf32> out : tensor<32766000xf32>, tensor<32766000xf32>
    %dogPyr = iara.node @build_dog_pyr in %e32, %e17, %e18, %e19, %e20, %e21, %e22, %e68, %e69 : tensor<16383000xf32>, tensor<1xi32>, tensor<1xi32>, tensor<1xi32>, tensor<1xi32>, tensor<1xi32>, tensor<1xi32>, tensor<1xi32>, tensor<1xi32> out : tensor<3413125xf32>
    %start_octave, %stop_octave, %start_layer, %stop_layer, %start_line, %stop_line, %start_col, %stop_col = iara.node @ITERATOR_build_dog_pyr out : tensor<4xi32>, tensor<4xi32>, tensor<4xi32>, tensor<4xi32>, tensor<4xi32>, tensor<4xi32>, tensor<4xi32>, tensor<4xi32>
    %grdPyr, %rotPyr = iara.node @build_grd_rot_pyr in %e31, %e23, %e24, %e25, %e26, %e27, %e28, %e70, %e71 : tensor<16383000xf32>, tensor<1xi32>, tensor<1xi32>, tensor<1xi32>, tensor<1xi32>, tensor<1xi32>, tensor<1xi32>, tensor<1xi32>, tensor<1xi32> out : tensor<2047875xf32>, tensor<2047875xf32>
    %start_octave, %stop_octave, %start_layer, %stop_layer, %start_line, %stop_line, %start_col, %stop_col = iara.node @ITERATOR_build_grd_rot_pyr out : tensor<4xi32>, tensor<4xi32>, tensor<4xi32>, tensor<4xi32>, tensor<4xi32>, tensor<4xi32>, tensor<4xi32>, tensor<4xi32>
    %float_img = iara.node @to_float in %image : tensor<128000xi8> out : tensor<128000xf32>
    %forDogPyr, %forGrdRotPyr = iara.node @BdGpyr_BROADCAST in %e33 : tensor<16383000xf32> out : tensor<65532000xf32>, tensor<65532000xf32>
    %gpyr = iara.node @MERGE_gpyr in %e49, %e48, %e52, %e54 : tensor<3072000xf32>, tensor<12288000xf32>, tensor<768000xf32>, tensor<768000xf32> out : tensor<16383000xf32>
    %forUpsample, %forDirectGaussian, %forDownSample = iara.node @BdFloatImg_BROADCAST in %e34 : tensor<512000xf32> out : tensor<512000xf32>, tensor<3072000xf32>, tensor<512000xf32>
    %img2x = iara.node @upsample2x in %e75, %e76 : tensor<128800xf32>, tensor<1xi32> out : tensor<512000xf32>
    %imgDown2x = iara.node @downsample2xN in %e38, %e1, %e56 : tensor<128000xf32>, tensor<32000xf32>, tensor<1xi32> out : tensor<32000xf32>
    %rt2x1, %rt2x2, %rt1, %rt2, %seq1, %seqN = iara.node @BdCoefs_BROADCAST in %e36 : tensor<126xf32> out : tensor<3024xf32>, tensor<3024xf32>, tensor<3024xf32>, tensor<3024xf32>, tensor<756xf32>, tensor<3024xf32>
    %rt2x1, %rt2x2, %rt1, %rt2, %seq1, %seqN = iara.node @BdSizes_BROADCAST in %e35 : tensor<6xi32> out : tensor<144xi32>, tensor<144xi32>, tensor<144xi32>, tensor<144xi32>, tensor<36xi32>, tensor<144xi32>
    %forBlurUp2x = iara.node @BdBlurUp2x_BROADCAST in %e37 : tensor<2048000xf32> out : tensor<12288000xf32>
    %forRec, %forBlur1 = iara.node @BdBlurDown2x1_BROADCAST in %e40 : tensor<128000xf32> out : tensor<512000xf32>, tensor<768000xf32>
    %imgBlurred = iara.node @seq_blur1 in %e41, %e2, %e44, %e43, %e46 : tensor<128000xf32>, tensor<128000xf32>, tensor<126xf32>, tensor<6xi32>, tensor<1xi32> out : tensor<128000xf32>
    %img = iara.node @downsample2x1 in %e39 : tensor<128000xf32> out : tensor<32000xf32>
    %imgBlurred = iara.node @seq_blurN in %e42, %e3, %e63, %e62, %e61, %e57 : tensor<32000xf32>, tensor<32000xf32>, tensor<126xf32>, tensor<6xi32>, tensor<1xi32>, tensor<1xi32> out : tensor<32000xf32>
    %forBlurN, %forRec = iara.node @BdBlurDown2xN_BROADCAST in %e50 : tensor<32000xf32> out : tensor<192000xf32>, tensor<32000xf32>
    %iter = iara.node @counterGpyrLayer out : tensor<6xi32>
    %for2x, %forDirectGaussian, %forBlur1, %for2x_2, %forDirectGaussian_2, %forBdOctave = iara.node @BdCounterGpyr_BROADCAST in %e45 : tensor<1xi32> out : tensor<4xi32>, tensor<4xi32>, tensor<1xi32>, tensor<4xi32>, tensor<4xi32>, tensor<1xi32>
    %iter = iara.node @counterOctaveDownN out : tensor<4xi32>
    %forRec, %forMerge = iara.node @BdGT_BROADCAST in %e77 : tensor<512000xf32> out : tensor<512000xf32>, tensor<512000xf32>
    %forRec, %forMerge = iara.node @BdGT_2x_BROADCAST in %e78 : tensor<2048000xf32> out : tensor<2048000xf32>, tensor<2048000xf32>
    %forRec, %forMerge = iara.node @BdBlurred1_BROADCAST in %e51 : tensor<128000xf32> out : tensor<128000xf32>, tensor<128000xf32>
    %forRec, %forMerge = iara.node @BdBlurredN_BROADCAST in %e53 : tensor<32000xf32> out : tensor<32000xf32>, tensor<32000xf32>
    %forDown, %forBlur = iara.node @BdOctaveDown_BROADCAST in %e55 : tensor<1xi32> out : tensor<1xi32>, tensor<6xi32>
    %keypoints_out, %nbKeypoints_out = iara.node @MERGE_keypoints in %e58 : tensor<1404x!SiftKpt> out : tensor<1400x!SiftKpt>, tensor<1xi32>
    %forBlurN = iara.node @BdCounterGpyrXOctave_BROADCAST in %e67 : tensor<6xi32> out : tensor<24xi32>
    %forDetection = iara.node @BdDoG_BROADCAST in %e64 : tensor<13652500xf32> out : tensor<54610000xf32>
    %iters_out = iara.node @BarrierCounterGpyr in %e66 : tensor<6xi32> out : tensor<6xi32>
    %out = iara.node @SPLIT_upsample2x in %e74 : tensor<512000xf32> out : tensor<515200xf32>
    %iter = iara.node @counterPLevels out : tensor<4xi32>
    %gauss_coefs1, %imgOri, %col_sizes1, %iterPrev, %iter_nb1, %col_sizes2, %gauss_coefs2, %iter_nb2, %imgBlurred = iara.node @blur2x in %e83, %e91, %e81, %e5, %e87, %e82, %e84, %e88 : tensor<126xf32>, tensor<512000xf32>, tensor<6xi32>, tensor<512000xf32>, tensor<1xi32>, tensor<6xi32>, tensor<126xf32>, tensor<1xi32>, tensor<2048000xf32> out : tensor<126xf32>, tensor<512000xf32>, tensor<6xi32>, tensor<512000xf32>, tensor<1xi32>, tensor<6xi32>, tensor<126xf32>, tensor<1xi32>, tensor<2048000xf32>
    %gauss_coefs1, %imgOri, %col_sizes1, %iterPrev, %iter_nb1, %iter_nb2, %gauss_coefs2, %col_sizes2, %imgBlurred = iara.node @blur in %e85, %e92, %e79, %e4, %e89, %e90, %e86, %e80 : tensor<126xf32>, tensor<128000xf32>, tensor<6xi32>, tensor<128000xf32>, tensor<1xi32>, tensor<1xi32>, tensor<126xf32>, tensor<6xi32>, tensor<512000xf32> out : tensor<126xf32>, tensor<128000xf32>, tensor<6xi32>, tensor<128000xf32>, tensor<1xi32>, tensor<1xi32>, tensor<126xf32>, tensor<6xi32>, tensor<512000xf32>
    iara.out ( %image : tensor<128000xi8> ) : tensor<128000xi8>
    iara.out ( %keypoints : tensor<1400x!SiftKpt> ) : tensor<1400x!SiftKpt>
    iara.out ( %nbKeypoints : tensor<1xi32> ) : tensor<1xi32>
    iara.dep
} // end subgraph Htop_sift
