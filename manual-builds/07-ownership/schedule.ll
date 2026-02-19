; ModuleID = 'LLVMDialectModule'
source_filename = "LLVMDialectModule"

%VirtualFIFO_Edge = type { %VirtualFIFO_Edge_StaticInfo, %VirtualFIFO_Edge_CodegenInfo }
%VirtualFIFO_Edge_StaticInfo = type { i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64 }
%VirtualFIFO_Edge_CodegenInfo = type { ptr, { ptr, i64 }, ptr, ptr, ptr, ptr }
%VirtualFIFO_Node = type { %VirtualFIFO_Node_StaticInfo, %VirtualFIFO_Node_CodegenInfo, %VirtualFIFO_Node_RuntimeInfo }
%VirtualFIFO_Node_StaticInfo = type { i64, i64, i64, i64, i64, i64 }
%VirtualFIFO_Node_CodegenInfo = type { ptr, ptr, { ptr, i64 }, { ptr, i64 } }
%VirtualFIFO_Node_RuntimeInfo = type { ptr }
%VirtualFIFO_Chunk = type { ptr, i64, ptr, i64 }

@iara_runtime_data_node_401801_output_fifos = global [0 x ptr] undef, align 8
@iara_runtime_data_node_401801_input_fifos = global [1 x ptr] [ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 2240)], align 8
@iara_runtime_data_node_18_output_fifos = global [1 x ptr] [ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 2240)], align 8
@iara_runtime_data_node_18_input_fifos = global [1 x ptr] [ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 2080)], align 8
@iara_runtime_data_node_401701_output_fifos = global [0 x ptr] undef, align 8
@iara_runtime_data_node_401701_input_fifos = global [1 x ptr] [ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 1920)], align 8
@iara_runtime_data_node_17_output_fifos = global [1 x ptr] [ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 1920)], align 8
@iara_runtime_data_node_17_input_fifos = global [1 x ptr] [ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 1760)], align 8
@iara_runtime_data_node_16_output_fifos = global [1 x ptr] [ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 2080)], align 8
@iara_runtime_data_node_16_input_fifos = global [1 x ptr] [ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 1600)], align 8
@iara_runtime_data_node_15_output_fifos = global [1 x ptr] [ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 1760)], align 8
@iara_runtime_data_node_15_input_fifos = global [1 x ptr] [ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 1440)], align 8
@iara_runtime_data_node_401401_output_fifos = global [0 x ptr] undef, align 8
@iara_runtime_data_node_401401_input_fifos = global [1 x ptr] [ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 1280)], align 8
@iara_runtime_data_node_14_output_fifos = global [1 x ptr] [ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 1280)], align 8
@iara_runtime_data_node_14_input_fifos = global [1 x ptr] [ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 1120)], align 8
@iara_runtime_data_node_401301_output_fifos = global [0 x ptr] undef, align 8
@iara_runtime_data_node_401301_input_fifos = global [1 x ptr] [ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 960)], align 8
@iara_runtime_data_node_13_output_fifos = global [1 x ptr] [ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 960)], align 8
@iara_runtime_data_node_13_input_fifos = global [1 x ptr] [ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 800)], align 8
@iara_runtime_data_node_12_output_fifos = global [4 x ptr] [ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 1600), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 1440), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 800), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 1120)], align 8
@iara_runtime_data_node_12_input_fifos = global [4 x ptr] [ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 160), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 320), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 480), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 640)], align 8
@iara_runtime_data_node_301203_output_fifos = global [1 x ptr] [ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 640)], align 8
@iara_runtime_data_node_301203_input_fifos = global [0 x ptr] undef, align 8
@iara_runtime_data_node_301202_output_fifos = global [1 x ptr] [ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 480)], align 8
@iara_runtime_data_node_301202_input_fifos = global [0 x ptr] undef, align 8
@iara_runtime_data_node_301201_output_fifos = global [1 x ptr] [ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 320)], align 8
@iara_runtime_data_node_301201_input_fifos = global [0 x ptr] undef, align 8
@iara_runtime_data_node_11_output_fifos = global [1 x ptr] [ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 160)], align 8
@iara_runtime_data_node_11_input_fifos = global [1 x ptr] [ptr @iara_runtime_data__edge_infos], align 8
@iara_runtime_data_node_301100_output_fifos = global [1 x ptr] [ptr @iara_runtime_data__edge_infos], align 8
@iara_runtime_data_node_301100_input_fifos = global [0 x ptr] undef, align 8
@"deallocNode_2401801_18_b[0]" = constant [27 x i8] c"deallocNode_2401801_18_b[0]"
@"edge_1618_16_c[0]->18_b[0]" = constant [26 x i8] c"edge_1618_16_c[0]->18_b[0]"
@"deallocNode_2401701_17_b[0]" = constant [27 x i8] c"deallocNode_2401701_17_b[0]"
@"edge_1517_15_c[0]->17_b[0]" = constant [26 x i8] c"edge_1517_15_c[0]->17_b[0]"
@"edge_1216_12_iara_broadcast_tensor<1xi32>_1io_1_1_1[0]->16_c[0]" = constant [63 x i8] c"edge_1216_12_iara_broadcast_tensor<1xi32>_1io_1_1_1[0]->16_c[0]"
@"edge_1215_12_iara_broadcast_tensor<1xi32>_1io_1_1_1[1]->15_c[0]" = constant [63 x i8] c"edge_1215_12_iara_broadcast_tensor<1xi32>_1io_1_1_1[1]->15_c[0]"
@"deallocNode_2401401_14_b[0]" = constant [27 x i8] c"deallocNode_2401401_14_b[0]"
@"edge_1214_12_iara_broadcast_tensor<1xi32>_1io_1_1_1[3]->14_b[0]" = constant [63 x i8] c"edge_1214_12_iara_broadcast_tensor<1xi32>_1io_1_1_1[3]->14_b[0]"
@"deallocNode_2401301_13_b[0]" = constant [27 x i8] c"deallocNode_2401301_13_b[0]"
@"edge_1213_12_iara_broadcast_tensor<1xi32>_1io_1_1_1[2]->13_b[0]" = constant [63 x i8] c"edge_1213_12_iara_broadcast_tensor<1xi32>_1io_1_1_1[2]->13_b[0]"
@"allocEdge_301203_12_iara_broadcast_tensor<1xi32>_1io_1_1_1[3]" = constant [61 x i8] c"allocEdge_301203_12_iara_broadcast_tensor<1xi32>_1io_1_1_1[3]"
@"allocEdge_301202_12_iara_broadcast_tensor<1xi32>_1io_1_1_1[2]" = constant [61 x i8] c"allocEdge_301202_12_iara_broadcast_tensor<1xi32>_1io_1_1_1[2]"
@"allocEdge_301201_12_iara_broadcast_tensor<1xi32>_1io_1_1_1[1]" = constant [61 x i8] c"allocEdge_301201_12_iara_broadcast_tensor<1xi32>_1io_1_1_1[1]"
@"edge_1112_11_a[0]->12_iara_broadcast_tensor<1xi32>_1io_1_1_1[0]" = constant [63 x i8] c"edge_1112_11_a[0]->12_iara_broadcast_tensor<1xi32>_1io_1_1_1[0]"
@"allocEdge_301100_11_a[0]" = constant [24 x i8] c"allocEdge_301100_11_a[0]"
@iara_runtime_data__edge_infos = global [15 x %VirtualFIFO_Edge] [%VirtualFIFO_Edge { %VirtualFIFO_Edge_StaticInfo { i64 2301100, i64 0, i64 -2, i64 4, i64 0, i64 0, i64 0, i64 4, i64 4, i64 -2, i64 -2, i64 1, i64 1 }, %VirtualFIFO_Edge_CodegenInfo { ptr @"allocEdge_301100_11_a[0]", { ptr, i64 } zeroinitializer, ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 104), ptr @iara_runtime_data__node_infos, ptr @iara_runtime_data__node_infos, ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 160) } }, %VirtualFIFO_Edge { %VirtualFIFO_Edge_StaticInfo { i64 1112, i64 1, i64 4, i64 4, i64 0, i64 0, i64 0, i64 4, i64 4, i64 1, i64 1, i64 1, i64 1 }, %VirtualFIFO_Edge_CodegenInfo { ptr @"edge_1112_11_a[0]->12_iara_broadcast_tensor<1xi32>_1io_1_1_1[0]", { ptr, i64 } zeroinitializer, ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 520), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 104), ptr @iara_runtime_data__node_infos, ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 1600) } }, %VirtualFIFO_Edge { %VirtualFIFO_Edge_StaticInfo { i64 2301201, i64 0, i64 -2, i64 4, i64 1, i64 0, i64 0, i64 4, i64 4, i64 -2, i64 -2, i64 1, i64 1 }, %VirtualFIFO_Edge_CodegenInfo { ptr @"allocEdge_301201_12_iara_broadcast_tensor<1xi32>_1io_1_1_1[1]", { ptr, i64 } zeroinitializer, ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 520), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 208), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 208), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 1440) } }, %VirtualFIFO_Edge { %VirtualFIFO_Edge_StaticInfo { i64 2301202, i64 0, i64 -2, i64 4, i64 2, i64 0, i64 0, i64 4, i64 4, i64 -2, i64 -2, i64 1, i64 1 }, %VirtualFIFO_Edge_CodegenInfo { ptr @"allocEdge_301202_12_iara_broadcast_tensor<1xi32>_1io_1_1_1[2]", { ptr, i64 } zeroinitializer, ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 520), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 312), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 312), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 800) } }, %VirtualFIFO_Edge { %VirtualFIFO_Edge_StaticInfo { i64 2301203, i64 0, i64 -2, i64 4, i64 3, i64 0, i64 0, i64 4, i64 4, i64 -2, i64 -2, i64 1, i64 1 }, %VirtualFIFO_Edge_CodegenInfo { ptr @"allocEdge_301203_12_iara_broadcast_tensor<1xi32>_1io_1_1_1[3]", { ptr, i64 } zeroinitializer, ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 520), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 416), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 416), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 1120) } }, %VirtualFIFO_Edge { %VirtualFIFO_Edge_StaticInfo { i64 1213, i64 1, i64 4, i64 4, i64 0, i64 0, i64 0, i64 4, i64 4, i64 1, i64 1, i64 1, i64 1 }, %VirtualFIFO_Edge_CodegenInfo { ptr @"edge_1213_12_iara_broadcast_tensor<1xi32>_1io_1_1_1[2]->13_b[0]", { ptr, i64 } zeroinitializer, ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 624), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 520), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 312), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 960) } }, %VirtualFIFO_Edge { %VirtualFIFO_Edge_StaticInfo { i64 2401301, i64 -1, i64 4, i64 -3, i64 1, i64 0, i64 0, i64 4, i64 4, i64 1, i64 1, i64 -3, i64 -3 }, %VirtualFIFO_Edge_CodegenInfo { ptr @"deallocNode_2401301_13_b[0]", { ptr, i64 } zeroinitializer, ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 728), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 624), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 312), ptr null } }, %VirtualFIFO_Edge { %VirtualFIFO_Edge_StaticInfo { i64 1214, i64 1, i64 4, i64 4, i64 0, i64 0, i64 0, i64 4, i64 4, i64 1, i64 1, i64 1, i64 1 }, %VirtualFIFO_Edge_CodegenInfo { ptr @"edge_1214_12_iara_broadcast_tensor<1xi32>_1io_1_1_1[3]->14_b[0]", { ptr, i64 } zeroinitializer, ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 832), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 520), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 416), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 1280) } }, %VirtualFIFO_Edge { %VirtualFIFO_Edge_StaticInfo { i64 2401401, i64 -1, i64 4, i64 -3, i64 1, i64 0, i64 0, i64 4, i64 4, i64 1, i64 1, i64 -3, i64 -3 }, %VirtualFIFO_Edge_CodegenInfo { ptr @"deallocNode_2401401_14_b[0]", { ptr, i64 } zeroinitializer, ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 936), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 832), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 416), ptr null } }, %VirtualFIFO_Edge { %VirtualFIFO_Edge_StaticInfo { i64 1215, i64 1, i64 4, i64 4, i64 0, i64 0, i64 0, i64 4, i64 4, i64 1, i64 1, i64 1, i64 1 }, %VirtualFIFO_Edge_CodegenInfo { ptr @"edge_1215_12_iara_broadcast_tensor<1xi32>_1io_1_1_1[1]->15_c[0]", { ptr, i64 } zeroinitializer, ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 1040), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 520), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 208), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 1760) } }, %VirtualFIFO_Edge { %VirtualFIFO_Edge_StaticInfo { i64 1216, i64 2, i64 4, i64 4, i64 0, i64 0, i64 0, i64 4, i64 4, i64 1, i64 1, i64 1, i64 1 }, %VirtualFIFO_Edge_CodegenInfo { ptr @"edge_1216_12_iara_broadcast_tensor<1xi32>_1io_1_1_1[0]->16_c[0]", { ptr, i64 } zeroinitializer, ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 1144), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 520), ptr @iara_runtime_data__node_infos, ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 2080) } }, %VirtualFIFO_Edge { %VirtualFIFO_Edge_StaticInfo { i64 1517, i64 2, i64 4, i64 4, i64 0, i64 0, i64 0, i64 4, i64 4, i64 1, i64 1, i64 1, i64 1 }, %VirtualFIFO_Edge_CodegenInfo { ptr @"edge_1517_15_c[0]->17_b[0]", { ptr, i64 } zeroinitializer, ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 1248), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 1040), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 208), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 1920) } }, %VirtualFIFO_Edge { %VirtualFIFO_Edge_StaticInfo { i64 2401701, i64 -1, i64 4, i64 -3, i64 1, i64 0, i64 0, i64 4, i64 4, i64 1, i64 1, i64 -3, i64 -3 }, %VirtualFIFO_Edge_CodegenInfo { ptr @"deallocNode_2401701_17_b[0]", { ptr, i64 } zeroinitializer, ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 1352), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 1248), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 208), ptr null } }, %VirtualFIFO_Edge { %VirtualFIFO_Edge_StaticInfo { i64 1618, i64 3, i64 4, i64 4, i64 0, i64 0, i64 0, i64 4, i64 4, i64 1, i64 1, i64 1, i64 1 }, %VirtualFIFO_Edge_CodegenInfo { ptr @"edge_1618_16_c[0]->18_b[0]", { ptr, i64 } zeroinitializer, ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 1456), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 1144), ptr @iara_runtime_data__node_infos, ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 2240) } }, %VirtualFIFO_Edge { %VirtualFIFO_Edge_StaticInfo { i64 2401801, i64 -1, i64 4, i64 -3, i64 1, i64 0, i64 0, i64 4, i64 4, i64 1, i64 1, i64 -3, i64 -3 }, %VirtualFIFO_Edge_CodegenInfo { ptr @"deallocNode_2401801_18_b[0]", { ptr, i64 } zeroinitializer, ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 1560), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 1456), ptr @iara_runtime_data__node_infos, ptr null } }], align 8
@"deallocNode_401801_18_b[0]" = constant [26 x i8] c"deallocNode_401801_18_b[0]"
@node_18_b = constant [9 x i8] c"node_18_b"
@"deallocNode_401701_17_b[0]" = constant [26 x i8] c"deallocNode_401701_17_b[0]"
@node_17_b = constant [9 x i8] c"node_17_b"
@node_16_c = constant [9 x i8] c"node_16_c"
@node_15_c = constant [9 x i8] c"node_15_c"
@"deallocNode_401401_14_b[0]" = constant [26 x i8] c"deallocNode_401401_14_b[0]"
@node_14_b = constant [9 x i8] c"node_14_b"
@"deallocNode_401301_13_b[0]" = constant [26 x i8] c"deallocNode_401301_13_b[0]"
@node_13_b = constant [9 x i8] c"node_13_b"
@"node_12_iara_broadcast_tensor<1xi32>_1io_1_1_1" = constant [46 x i8] c"node_12_iara_broadcast_tensor<1xi32>_1io_1_1_1"
@"allocNode_301203_12_iara_broadcast_tensor<1xi32>_1io_1_1_1[3]" = constant [61 x i8] c"allocNode_301203_12_iara_broadcast_tensor<1xi32>_1io_1_1_1[3]"
@"allocNode_301202_12_iara_broadcast_tensor<1xi32>_1io_1_1_1[2]" = constant [61 x i8] c"allocNode_301202_12_iara_broadcast_tensor<1xi32>_1io_1_1_1[2]"
@"allocNode_301201_12_iara_broadcast_tensor<1xi32>_1io_1_1_1[1]" = constant [61 x i8] c"allocNode_301201_12_iara_broadcast_tensor<1xi32>_1io_1_1_1[1]"
@node_11_a = constant [9 x i8] c"node_11_a"
@"allocNode_301100_11_a[0]" = constant [24 x i8] c"allocNode_301100_11_a[0]"
@iara_runtime_data__node_infos = global [16 x %VirtualFIFO_Node] [%VirtualFIFO_Node { %VirtualFIFO_Node_StaticInfo { i64 301100, i64 -2, i64 0, i64 -1, i64 4, i64 0 }, %VirtualFIFO_Node_CodegenInfo { ptr @"allocNode_301100_11_a[0]", ptr @iara_runtime_alloc, { ptr, i64 } { ptr @iara_runtime_data_node_301100_input_fifos, i64 0 }, { ptr, i64 } { ptr @iara_runtime_data_node_301100_output_fifos, i64 1 } }, %VirtualFIFO_Node_RuntimeInfo zeroinitializer }, %VirtualFIFO_Node { %VirtualFIFO_Node_StaticInfo { i64 11, i64 4, i64 1, i64 1, i64 1, i64 1 }, %VirtualFIFO_Node_CodegenInfo { ptr @node_11_a, ptr @iara_node_wrapper_11_a, { ptr, i64 } { ptr @iara_runtime_data_node_11_input_fifos, i64 1 }, { ptr, i64 } { ptr @iara_runtime_data_node_11_output_fifos, i64 1 } }, %VirtualFIFO_Node_RuntimeInfo zeroinitializer }, %VirtualFIFO_Node { %VirtualFIFO_Node_StaticInfo { i64 301201, i64 -2, i64 0, i64 1, i64 3, i64 0 }, %VirtualFIFO_Node_CodegenInfo { ptr @"allocNode_301201_12_iara_broadcast_tensor<1xi32>_1io_1_1_1[1]", ptr @iara_runtime_alloc, { ptr, i64 } { ptr @iara_runtime_data_node_301201_input_fifos, i64 0 }, { ptr, i64 } { ptr @iara_runtime_data_node_301201_output_fifos, i64 1 } }, %VirtualFIFO_Node_RuntimeInfo zeroinitializer }, %VirtualFIFO_Node { %VirtualFIFO_Node_StaticInfo { i64 301202, i64 -2, i64 0, i64 1, i64 2, i64 0 }, %VirtualFIFO_Node_CodegenInfo { ptr @"allocNode_301202_12_iara_broadcast_tensor<1xi32>_1io_1_1_1[2]", ptr @iara_runtime_alloc, { ptr, i64 } { ptr @iara_runtime_data_node_301202_input_fifos, i64 0 }, { ptr, i64 } { ptr @iara_runtime_data_node_301202_output_fifos, i64 1 } }, %VirtualFIFO_Node_RuntimeInfo zeroinitializer }, %VirtualFIFO_Node { %VirtualFIFO_Node_StaticInfo { i64 301203, i64 -2, i64 0, i64 1, i64 2, i64 0 }, %VirtualFIFO_Node_CodegenInfo { ptr @"allocNode_301203_12_iara_broadcast_tensor<1xi32>_1io_1_1_1[3]", ptr @iara_runtime_alloc, { ptr, i64 } { ptr @iara_runtime_data_node_301203_input_fifos, i64 0 }, { ptr, i64 } { ptr @iara_runtime_data_node_301203_output_fifos, i64 1 } }, %VirtualFIFO_Node_RuntimeInfo zeroinitializer }, %VirtualFIFO_Node { %VirtualFIFO_Node_StaticInfo { i64 12, i64 16, i64 4, i64 3, i64 1, i64 1 }, %VirtualFIFO_Node_CodegenInfo { ptr @"node_12_iara_broadcast_tensor<1xi32>_1io_1_1_1", ptr @"iara_node_wrapper_12_iara_broadcast_tensor<1xi32>_1io_1_1_1", { ptr, i64 } { ptr @iara_runtime_data_node_12_input_fifos, i64 4 }, { ptr, i64 } { ptr @iara_runtime_data_node_12_output_fifos, i64 4 } }, %VirtualFIFO_Node_RuntimeInfo zeroinitializer }, %VirtualFIFO_Node { %VirtualFIFO_Node_StaticInfo { i64 13, i64 4, i64 1, i64 5, i64 1, i64 1 }, %VirtualFIFO_Node_CodegenInfo { ptr @node_13_b, ptr @iara_node_wrapper_13_b, { ptr, i64 } { ptr @iara_runtime_data_node_13_input_fifos, i64 1 }, { ptr, i64 } { ptr @iara_runtime_data_node_13_output_fifos, i64 1 } }, %VirtualFIFO_Node_RuntimeInfo zeroinitializer }, %VirtualFIFO_Node { %VirtualFIFO_Node_StaticInfo { i64 401301, i64 -3, i64 1, i64 7, i64 -3, i64 0 }, %VirtualFIFO_Node_CodegenInfo { ptr @"deallocNode_401301_13_b[0]", ptr @iara_runtime_dealloc, { ptr, i64 } { ptr @iara_runtime_data_node_401301_input_fifos, i64 1 }, { ptr, i64 } { ptr @iara_runtime_data_node_401301_output_fifos, i64 0 } }, %VirtualFIFO_Node_RuntimeInfo zeroinitializer }, %VirtualFIFO_Node { %VirtualFIFO_Node_StaticInfo { i64 14, i64 4, i64 1, i64 5, i64 1, i64 1 }, %VirtualFIFO_Node_CodegenInfo { ptr @node_14_b, ptr @iara_node_wrapper_14_b, { ptr, i64 } { ptr @iara_runtime_data_node_14_input_fifos, i64 1 }, { ptr, i64 } { ptr @iara_runtime_data_node_14_output_fifos, i64 1 } }, %VirtualFIFO_Node_RuntimeInfo zeroinitializer }, %VirtualFIFO_Node { %VirtualFIFO_Node_StaticInfo { i64 401401, i64 -3, i64 1, i64 7, i64 -3, i64 0 }, %VirtualFIFO_Node_CodegenInfo { ptr @"deallocNode_401401_14_b[0]", ptr @iara_runtime_dealloc, { ptr, i64 } { ptr @iara_runtime_data_node_401401_input_fifos, i64 1 }, { ptr, i64 } { ptr @iara_runtime_data_node_401401_output_fifos, i64 0 } }, %VirtualFIFO_Node_RuntimeInfo zeroinitializer }, %VirtualFIFO_Node { %VirtualFIFO_Node_StaticInfo { i64 15, i64 4, i64 1, i64 5, i64 1, i64 1 }, %VirtualFIFO_Node_CodegenInfo { ptr @node_15_c, ptr @iara_node_wrapper_15_c, { ptr, i64 } { ptr @iara_runtime_data_node_15_input_fifos, i64 1 }, { ptr, i64 } { ptr @iara_runtime_data_node_15_output_fifos, i64 1 } }, %VirtualFIFO_Node_RuntimeInfo zeroinitializer }, %VirtualFIFO_Node { %VirtualFIFO_Node_StaticInfo { i64 16, i64 4, i64 1, i64 5, i64 1, i64 1 }, %VirtualFIFO_Node_CodegenInfo { ptr @node_16_c, ptr @iara_node_wrapper_16_c, { ptr, i64 } { ptr @iara_runtime_data_node_16_input_fifos, i64 1 }, { ptr, i64 } { ptr @iara_runtime_data_node_16_output_fifos, i64 1 } }, %VirtualFIFO_Node_RuntimeInfo zeroinitializer }, %VirtualFIFO_Node { %VirtualFIFO_Node_StaticInfo { i64 17, i64 4, i64 1, i64 7, i64 1, i64 1 }, %VirtualFIFO_Node_CodegenInfo { ptr @node_17_b, ptr @iara_node_wrapper_17_b, { ptr, i64 } { ptr @iara_runtime_data_node_17_input_fifos, i64 1 }, { ptr, i64 } { ptr @iara_runtime_data_node_17_output_fifos, i64 1 } }, %VirtualFIFO_Node_RuntimeInfo zeroinitializer }, %VirtualFIFO_Node { %VirtualFIFO_Node_StaticInfo { i64 401701, i64 -3, i64 1, i64 9, i64 -3, i64 0 }, %VirtualFIFO_Node_CodegenInfo { ptr @"deallocNode_401701_17_b[0]", ptr @iara_runtime_dealloc, { ptr, i64 } { ptr @iara_runtime_data_node_401701_input_fifos, i64 1 }, { ptr, i64 } { ptr @iara_runtime_data_node_401701_output_fifos, i64 0 } }, %VirtualFIFO_Node_RuntimeInfo zeroinitializer }, %VirtualFIFO_Node { %VirtualFIFO_Node_StaticInfo { i64 18, i64 4, i64 1, i64 7, i64 1, i64 1 }, %VirtualFIFO_Node_CodegenInfo { ptr @node_18_b, ptr @iara_node_wrapper_18_b, { ptr, i64 } { ptr @iara_runtime_data_node_18_input_fifos, i64 1 }, { ptr, i64 } { ptr @iara_runtime_data_node_18_output_fifos, i64 1 } }, %VirtualFIFO_Node_RuntimeInfo zeroinitializer }, %VirtualFIFO_Node { %VirtualFIFO_Node_StaticInfo { i64 401801, i64 -3, i64 1, i64 9, i64 -3, i64 0 }, %VirtualFIFO_Node_CodegenInfo { ptr @"deallocNode_401801_18_b[0]", ptr @iara_runtime_dealloc, { ptr, i64 } { ptr @iara_runtime_data_node_401801_input_fifos, i64 1 }, { ptr, i64 } { ptr @iara_runtime_data_node_401801_output_fifos, i64 0 } }, %VirtualFIFO_Node_RuntimeInfo zeroinitializer }], align 8
@iara_runtime_nodes = constant { ptr, i64 } { ptr @iara_runtime_data__node_infos, i64 16 }, align 8
@iara_runtime_edges = constant { ptr, i64 } { ptr @iara_runtime_data__edge_infos, i64 15 }, align 8

declare !dbg !3 void @c(ptr)

declare !dbg !6 void @b(ptr)

declare !dbg !7 void @a(ptr)

declare !dbg !8 void @iara_runtime_alloc(i64, { ptr, i64 })

define void @iara_node_wrapper_11_a(i64 %0, { ptr, i64 } %1) !dbg !9 {
  %3 = extractvalue { ptr, i64 } %1, 0, !dbg !10
  %4 = getelementptr %VirtualFIFO_Chunk, ptr %3, i32 0, i32 2, !dbg !11
  %5 = load ptr, ptr %4, align 8, !dbg !12
  call void @a(ptr %5), !dbg !13
  ret void, !dbg !14
}

define void @"iara_node_wrapper_12_iara_broadcast_tensor<1xi32>_1io_1_1_1"(i64 %0, { ptr, i64 } %1) !dbg !15 {
  %3 = extractvalue { ptr, i64 } %1, 0, !dbg !16
  %4 = getelementptr %VirtualFIFO_Chunk, ptr %3, i32 0, i32 2, !dbg !17
  %5 = load ptr, ptr %4, align 8, !dbg !18
  %6 = getelementptr %VirtualFIFO_Chunk, ptr %3, i32 1, i32 2, !dbg !19
  %7 = load ptr, ptr %6, align 8, !dbg !20
  %8 = getelementptr %VirtualFIFO_Chunk, ptr %3, i32 2, i32 2, !dbg !21
  %9 = load ptr, ptr %8, align 8, !dbg !22
  %10 = getelementptr %VirtualFIFO_Chunk, ptr %3, i32 3, i32 2, !dbg !23
  %11 = load ptr, ptr %10, align 8, !dbg !24
  call void @"iara_broadcast_tensor<1xi32>_1io_1_1_1"(ptr %5, ptr %7, ptr %9, ptr %11), !dbg !25
  ret void, !dbg !26
}

define void @iara_node_wrapper_13_b(i64 %0, { ptr, i64 } %1) !dbg !27 {
  %3 = extractvalue { ptr, i64 } %1, 0, !dbg !28
  %4 = getelementptr %VirtualFIFO_Chunk, ptr %3, i32 0, i32 2, !dbg !29
  %5 = load ptr, ptr %4, align 8, !dbg !30
  call void @b(ptr %5), !dbg !31
  ret void, !dbg !32
}

declare !dbg !33 void @iara_runtime_dealloc(i64, { ptr, i64 })

define void @iara_node_wrapper_14_b(i64 %0, { ptr, i64 } %1) !dbg !34 {
  %3 = extractvalue { ptr, i64 } %1, 0, !dbg !35
  %4 = getelementptr %VirtualFIFO_Chunk, ptr %3, i32 0, i32 2, !dbg !36
  %5 = load ptr, ptr %4, align 8, !dbg !37
  call void @b(ptr %5), !dbg !38
  ret void, !dbg !39
}

define void @iara_node_wrapper_15_c(i64 %0, { ptr, i64 } %1) !dbg !40 {
  %3 = extractvalue { ptr, i64 } %1, 0, !dbg !41
  %4 = getelementptr %VirtualFIFO_Chunk, ptr %3, i32 0, i32 2, !dbg !42
  %5 = load ptr, ptr %4, align 8, !dbg !43
  call void @c(ptr %5), !dbg !44
  ret void, !dbg !45
}

define void @iara_node_wrapper_16_c(i64 %0, { ptr, i64 } %1) !dbg !46 {
  %3 = extractvalue { ptr, i64 } %1, 0, !dbg !47
  %4 = getelementptr %VirtualFIFO_Chunk, ptr %3, i32 0, i32 2, !dbg !48
  %5 = load ptr, ptr %4, align 8, !dbg !49
  call void @c(ptr %5), !dbg !50
  ret void, !dbg !51
}

define void @iara_node_wrapper_17_b(i64 %0, { ptr, i64 } %1) !dbg !52 {
  %3 = extractvalue { ptr, i64 } %1, 0, !dbg !53
  %4 = getelementptr %VirtualFIFO_Chunk, ptr %3, i32 0, i32 2, !dbg !54
  %5 = load ptr, ptr %4, align 8, !dbg !55
  call void @b(ptr %5), !dbg !56
  ret void, !dbg !57
}

define void @iara_node_wrapper_18_b(i64 %0, { ptr, i64 } %1) !dbg !58 {
  %3 = extractvalue { ptr, i64 } %1, 0, !dbg !59
  %4 = getelementptr %VirtualFIFO_Chunk, ptr %3, i32 0, i32 2, !dbg !60
  %5 = load ptr, ptr %4, align 8, !dbg !61
  call void @b(ptr %5), !dbg !62
  ret void, !dbg !63
}

define void @"iara_broadcast_tensor<1xi32>_1io_1_1_1"(ptr %0, ptr %1, ptr %2, ptr %3) !dbg !64 {
  call void @llvm.memcpy.p0.p0.i64(ptr %1, ptr %0, i64 4, i1 true), !dbg !65
  %5 = getelementptr i8, ptr %1, i32 4, !dbg !66
  call void @llvm.memcpy.p0.p0.i64(ptr %5, ptr %0, i64 4, i1 true), !dbg !67
  call void @llvm.memcpy.p0.p0.i64(ptr %2, ptr %0, i64 4, i1 true), !dbg !68
  %6 = getelementptr i8, ptr %2, i32 4, !dbg !69
  call void @llvm.memcpy.p0.p0.i64(ptr %6, ptr %0, i64 4, i1 true), !dbg !70
  call void @llvm.memcpy.p0.p0.i64(ptr %3, ptr %0, i64 4, i1 true), !dbg !71
  %7 = getelementptr i8, ptr %3, i32 4, !dbg !72
  call void @llvm.memcpy.p0.p0.i64(ptr %7, ptr %0, i64 4, i1 true), !dbg !73
  ret void, !dbg !74
}

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: readwrite)
declare void @llvm.memcpy.p0.p0.i64(ptr noalias writeonly captures(none), ptr noalias readonly captures(none), i64, i1 immarg) #0

attributes #0 = { nocallback nofree nounwind willreturn memory(argmem: readwrite) }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2}

!0 = distinct !DICompileUnit(language: DW_LANG_C, file: !1, producer: "MLIR", isOptimized: true, runtimeVersion: 0, emissionKind: LineTablesOnly)
!1 = !DIFile(filename: "schedule.mlir", directory: "/scratch/pedro.ciambra/repos/iara/manual-builds/07-ownership")
!2 = !{i32 2, !"Debug Info Version", i32 3}
!3 = !DISubprogram(name: "c", linkageName: "c", scope: !1, file: !1, line: 1834, type: !4, scopeLine: 1834, spFlags: DISPFlagOptimized)
!4 = !DISubroutineType(cc: DW_CC_normal, types: !5)
!5 = !{}
!6 = !DISubprogram(name: "b", linkageName: "b", scope: !1, file: !1, line: 1835, type: !4, scopeLine: 1835, spFlags: DISPFlagOptimized)
!7 = !DISubprogram(name: "a", linkageName: "a", scope: !1, file: !1, line: 1836, type: !4, scopeLine: 1836, spFlags: DISPFlagOptimized)
!8 = !DISubprogram(name: "iara_runtime_alloc", linkageName: "iara_runtime_alloc", scope: !1, file: !1, line: 1837, type: !4, scopeLine: 1837, spFlags: DISPFlagOptimized)
!9 = distinct !DISubprogram(name: "iara_node_wrapper_11_a", linkageName: "iara_node_wrapper_11_a", scope: !1, file: !1, line: 1838, type: !4, scopeLine: 1838, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!10 = !DILocation(line: 1839, column: 10, scope: !9)
!11 = !DILocation(line: 1840, column: 10, scope: !9)
!12 = !DILocation(line: 1841, column: 10, scope: !9)
!13 = !DILocation(line: 1842, column: 5, scope: !9)
!14 = !DILocation(line: 1843, column: 5, scope: !9)
!15 = distinct !DISubprogram(name: "iara_node_wrapper_12_iara_broadcast_tensor<1xi32>_1io_1_1_1", linkageName: "iara_node_wrapper_12_iara_broadcast_tensor<1xi32>_1io_1_1_1", scope: !1, file: !1, line: 1845, type: !4, scopeLine: 1845, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!16 = !DILocation(line: 1846, column: 10, scope: !15)
!17 = !DILocation(line: 1847, column: 10, scope: !15)
!18 = !DILocation(line: 1848, column: 10, scope: !15)
!19 = !DILocation(line: 1850, column: 10, scope: !15)
!20 = !DILocation(line: 1851, column: 10, scope: !15)
!21 = !DILocation(line: 1853, column: 10, scope: !15)
!22 = !DILocation(line: 1854, column: 10, scope: !15)
!23 = !DILocation(line: 1856, column: 11, scope: !15)
!24 = !DILocation(line: 1857, column: 11, scope: !15)
!25 = !DILocation(line: 1858, column: 5, scope: !15)
!26 = !DILocation(line: 1859, column: 5, scope: !15)
!27 = distinct !DISubprogram(name: "iara_node_wrapper_13_b", linkageName: "iara_node_wrapper_13_b", scope: !1, file: !1, line: 1861, type: !4, scopeLine: 1861, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!28 = !DILocation(line: 1862, column: 10, scope: !27)
!29 = !DILocation(line: 1863, column: 10, scope: !27)
!30 = !DILocation(line: 1864, column: 10, scope: !27)
!31 = !DILocation(line: 1865, column: 5, scope: !27)
!32 = !DILocation(line: 1866, column: 5, scope: !27)
!33 = !DISubprogram(name: "iara_runtime_dealloc", linkageName: "iara_runtime_dealloc", scope: !1, file: !1, line: 1868, type: !4, scopeLine: 1868, spFlags: DISPFlagOptimized)
!34 = distinct !DISubprogram(name: "iara_node_wrapper_14_b", linkageName: "iara_node_wrapper_14_b", scope: !1, file: !1, line: 1869, type: !4, scopeLine: 1869, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!35 = !DILocation(line: 1870, column: 10, scope: !34)
!36 = !DILocation(line: 1871, column: 10, scope: !34)
!37 = !DILocation(line: 1872, column: 10, scope: !34)
!38 = !DILocation(line: 1873, column: 5, scope: !34)
!39 = !DILocation(line: 1874, column: 5, scope: !34)
!40 = distinct !DISubprogram(name: "iara_node_wrapper_15_c", linkageName: "iara_node_wrapper_15_c", scope: !1, file: !1, line: 1876, type: !4, scopeLine: 1876, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!41 = !DILocation(line: 1877, column: 10, scope: !40)
!42 = !DILocation(line: 1878, column: 10, scope: !40)
!43 = !DILocation(line: 1879, column: 10, scope: !40)
!44 = !DILocation(line: 1880, column: 5, scope: !40)
!45 = !DILocation(line: 1881, column: 5, scope: !40)
!46 = distinct !DISubprogram(name: "iara_node_wrapper_16_c", linkageName: "iara_node_wrapper_16_c", scope: !1, file: !1, line: 1883, type: !4, scopeLine: 1883, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!47 = !DILocation(line: 1884, column: 10, scope: !46)
!48 = !DILocation(line: 1885, column: 10, scope: !46)
!49 = !DILocation(line: 1886, column: 10, scope: !46)
!50 = !DILocation(line: 1887, column: 5, scope: !46)
!51 = !DILocation(line: 1888, column: 5, scope: !46)
!52 = distinct !DISubprogram(name: "iara_node_wrapper_17_b", linkageName: "iara_node_wrapper_17_b", scope: !1, file: !1, line: 1890, type: !4, scopeLine: 1890, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!53 = !DILocation(line: 1891, column: 10, scope: !52)
!54 = !DILocation(line: 1892, column: 10, scope: !52)
!55 = !DILocation(line: 1893, column: 10, scope: !52)
!56 = !DILocation(line: 1894, column: 5, scope: !52)
!57 = !DILocation(line: 1895, column: 5, scope: !52)
!58 = distinct !DISubprogram(name: "iara_node_wrapper_18_b", linkageName: "iara_node_wrapper_18_b", scope: !1, file: !1, line: 1897, type: !4, scopeLine: 1897, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!59 = !DILocation(line: 1898, column: 10, scope: !58)
!60 = !DILocation(line: 1899, column: 10, scope: !58)
!61 = !DILocation(line: 1900, column: 10, scope: !58)
!62 = !DILocation(line: 1901, column: 5, scope: !58)
!63 = !DILocation(line: 1902, column: 5, scope: !58)
!64 = distinct !DISubprogram(name: "iara_broadcast_tensor<1xi32>_1io_1_1_1", linkageName: "iara_broadcast_tensor<1xi32>_1io_1_1_1", scope: !1, file: !1, line: 1922, type: !4, scopeLine: 1922, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!65 = !DILocation(line: 1924, column: 5, scope: !64)
!66 = !DILocation(line: 1925, column: 10, scope: !64)
!67 = !DILocation(line: 1926, column: 5, scope: !64)
!68 = !DILocation(line: 1927, column: 5, scope: !64)
!69 = !DILocation(line: 1928, column: 10, scope: !64)
!70 = !DILocation(line: 1929, column: 5, scope: !64)
!71 = !DILocation(line: 1930, column: 5, scope: !64)
!72 = !DILocation(line: 1931, column: 10, scope: !64)
!73 = !DILocation(line: 1932, column: 5, scope: !64)
!74 = !DILocation(line: 1933, column: 5, scope: !64)
