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

@iara_runtime_data_node_401401_output_fifos = global [0 x ptr] undef, align 8
@iara_runtime_data_node_401401_input_fifos = global [1 x ptr] [ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 960)], align 8
@iara_runtime_data_node_14_output_fifos = global [1 x ptr] [ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 960)], align 8
@iara_runtime_data_node_14_input_fifos = global [1 x ptr] [ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 800)], align 8
@iara_runtime_data_node_401301_output_fifos = global [0 x ptr] undef, align 8
@iara_runtime_data_node_401301_input_fifos = global [1 x ptr] [ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 640)], align 8
@iara_runtime_data_node_13_output_fifos = global [1 x ptr] [ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 640)], align 8
@iara_runtime_data_node_13_input_fifos = global [1 x ptr] [ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 480)], align 8
@iara_runtime_data_node_12_output_fifos = global [2 x ptr] [ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 800), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 480)], align 8
@iara_runtime_data_node_12_input_fifos = global [2 x ptr] [ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 160), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 320)], align 8
@iara_runtime_data_node_301201_output_fifos = global [1 x ptr] [ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 320)], align 8
@iara_runtime_data_node_301201_input_fifos = global [0 x ptr] undef, align 8
@iara_runtime_data_node_11_output_fifos = global [1 x ptr] [ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 160)], align 8
@iara_runtime_data_node_11_input_fifos = global [1 x ptr] [ptr @iara_runtime_data__edge_infos], align 8
@iara_runtime_data_node_301100_output_fifos = global [1 x ptr] [ptr @iara_runtime_data__edge_infos], align 8
@iara_runtime_data_node_301100_input_fifos = global [0 x ptr] undef, align 8
@"deallocNode_2401401_14_c[0]" = constant [27 x i8] c"deallocNode_2401401_14_c[0]"
@"edge_1214_12_iara_broadcast_tensor<1xi32>_1io_1[0]->14_c[0]" = constant [59 x i8] c"edge_1214_12_iara_broadcast_tensor<1xi32>_1io_1[0]->14_c[0]"
@"deallocNode_2401301_13_b[0]" = constant [27 x i8] c"deallocNode_2401301_13_b[0]"
@"edge_1213_12_iara_broadcast_tensor<1xi32>_1io_1[1]->13_b[0]" = constant [59 x i8] c"edge_1213_12_iara_broadcast_tensor<1xi32>_1io_1[1]->13_b[0]"
@"allocEdge_301201_12_iara_broadcast_tensor<1xi32>_1io_1[1]" = constant [57 x i8] c"allocEdge_301201_12_iara_broadcast_tensor<1xi32>_1io_1[1]"
@"edge_1112_11_a[0]->12_iara_broadcast_tensor<1xi32>_1io_1[0]" = constant [59 x i8] c"edge_1112_11_a[0]->12_iara_broadcast_tensor<1xi32>_1io_1[0]"
@"allocEdge_301100_11_a[0]" = constant [24 x i8] c"allocEdge_301100_11_a[0]"
@iara_runtime_data__edge_infos = global [7 x %VirtualFIFO_Edge] [%VirtualFIFO_Edge { %VirtualFIFO_Edge_StaticInfo { i64 2301100, i64 0, i64 -2, i64 4, i64 0, i64 0, i64 0, i64 4, i64 4, i64 -2, i64 -2, i64 1, i64 1 }, %VirtualFIFO_Edge_CodegenInfo { ptr @"allocEdge_301100_11_a[0]", { ptr, i64 } zeroinitializer, ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 104), ptr @iara_runtime_data__node_infos, ptr @iara_runtime_data__node_infos, ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 160) } }, %VirtualFIFO_Edge { %VirtualFIFO_Edge_StaticInfo { i64 1112, i64 1, i64 4, i64 4, i64 0, i64 0, i64 0, i64 4, i64 4, i64 1, i64 1, i64 1, i64 1 }, %VirtualFIFO_Edge_CodegenInfo { ptr @"edge_1112_11_a[0]->12_iara_broadcast_tensor<1xi32>_1io_1[0]", { ptr, i64 } zeroinitializer, ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 312), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 104), ptr @iara_runtime_data__node_infos, ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 800) } }, %VirtualFIFO_Edge { %VirtualFIFO_Edge_StaticInfo { i64 2301201, i64 0, i64 -2, i64 4, i64 1, i64 0, i64 0, i64 4, i64 4, i64 -2, i64 -2, i64 1, i64 1 }, %VirtualFIFO_Edge_CodegenInfo { ptr @"allocEdge_301201_12_iara_broadcast_tensor<1xi32>_1io_1[1]", { ptr, i64 } zeroinitializer, ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 312), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 208), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 208), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 480) } }, %VirtualFIFO_Edge { %VirtualFIFO_Edge_StaticInfo { i64 1213, i64 1, i64 4, i64 4, i64 0, i64 0, i64 0, i64 4, i64 4, i64 1, i64 1, i64 1, i64 1 }, %VirtualFIFO_Edge_CodegenInfo { ptr @"edge_1213_12_iara_broadcast_tensor<1xi32>_1io_1[1]->13_b[0]", { ptr, i64 } zeroinitializer, ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 416), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 312), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 208), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 640) } }, %VirtualFIFO_Edge { %VirtualFIFO_Edge_StaticInfo { i64 2401301, i64 -1, i64 4, i64 -3, i64 1, i64 0, i64 0, i64 4, i64 4, i64 1, i64 1, i64 -3, i64 -3 }, %VirtualFIFO_Edge_CodegenInfo { ptr @"deallocNode_2401301_13_b[0]", { ptr, i64 } zeroinitializer, ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 520), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 416), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 208), ptr null } }, %VirtualFIFO_Edge { %VirtualFIFO_Edge_StaticInfo { i64 1214, i64 2, i64 4, i64 4, i64 0, i64 0, i64 0, i64 4, i64 4, i64 1, i64 1, i64 1, i64 1 }, %VirtualFIFO_Edge_CodegenInfo { ptr @"edge_1214_12_iara_broadcast_tensor<1xi32>_1io_1[0]->14_c[0]", { ptr, i64 } zeroinitializer, ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 624), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 312), ptr @iara_runtime_data__node_infos, ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 960) } }, %VirtualFIFO_Edge { %VirtualFIFO_Edge_StaticInfo { i64 2401401, i64 -1, i64 4, i64 -3, i64 1, i64 0, i64 0, i64 4, i64 4, i64 1, i64 1, i64 -3, i64 -3 }, %VirtualFIFO_Edge_CodegenInfo { ptr @"deallocNode_2401401_14_c[0]", { ptr, i64 } zeroinitializer, ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 728), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 624), ptr @iara_runtime_data__node_infos, ptr null } }], align 8
@"deallocNode_401401_14_c[0]" = constant [26 x i8] c"deallocNode_401401_14_c[0]"
@node_14_c = constant [9 x i8] c"node_14_c"
@"deallocNode_401301_13_b[0]" = constant [26 x i8] c"deallocNode_401301_13_b[0]"
@node_13_b = constant [9 x i8] c"node_13_b"
@"node_12_iara_broadcast_tensor<1xi32>_1io_1" = constant [42 x i8] c"node_12_iara_broadcast_tensor<1xi32>_1io_1"
@"allocNode_301201_12_iara_broadcast_tensor<1xi32>_1io_1[1]" = constant [57 x i8] c"allocNode_301201_12_iara_broadcast_tensor<1xi32>_1io_1[1]"
@node_11_a = constant [9 x i8] c"node_11_a"
@"allocNode_301100_11_a[0]" = constant [24 x i8] c"allocNode_301100_11_a[0]"
@iara_runtime_data__node_infos = global [8 x %VirtualFIFO_Node] [%VirtualFIFO_Node { %VirtualFIFO_Node_StaticInfo { i64 301100, i64 -2, i64 0, i64 -1, i64 3, i64 0 }, %VirtualFIFO_Node_CodegenInfo { ptr @"allocNode_301100_11_a[0]", ptr @iara_runtime_alloc, { ptr, i64 } { ptr @iara_runtime_data_node_301100_input_fifos, i64 0 }, { ptr, i64 } { ptr @iara_runtime_data_node_301100_output_fifos, i64 1 } }, %VirtualFIFO_Node_RuntimeInfo zeroinitializer }, %VirtualFIFO_Node { %VirtualFIFO_Node_StaticInfo { i64 11, i64 4, i64 1, i64 1, i64 1, i64 1 }, %VirtualFIFO_Node_CodegenInfo { ptr @node_11_a, ptr @iara_node_wrapper_11_a, { ptr, i64 } { ptr @iara_runtime_data_node_11_input_fifos, i64 1 }, { ptr, i64 } { ptr @iara_runtime_data_node_11_output_fifos, i64 1 } }, %VirtualFIFO_Node_RuntimeInfo zeroinitializer }, %VirtualFIFO_Node { %VirtualFIFO_Node_StaticInfo { i64 301201, i64 -2, i64 0, i64 1, i64 2, i64 0 }, %VirtualFIFO_Node_CodegenInfo { ptr @"allocNode_301201_12_iara_broadcast_tensor<1xi32>_1io_1[1]", ptr @iara_runtime_alloc, { ptr, i64 } { ptr @iara_runtime_data_node_301201_input_fifos, i64 0 }, { ptr, i64 } { ptr @iara_runtime_data_node_301201_output_fifos, i64 1 } }, %VirtualFIFO_Node_RuntimeInfo zeroinitializer }, %VirtualFIFO_Node { %VirtualFIFO_Node_StaticInfo { i64 12, i64 8, i64 2, i64 3, i64 1, i64 1 }, %VirtualFIFO_Node_CodegenInfo { ptr @"node_12_iara_broadcast_tensor<1xi32>_1io_1", ptr @"iara_node_wrapper_12_iara_broadcast_tensor<1xi32>_1io_1", { ptr, i64 } { ptr @iara_runtime_data_node_12_input_fifos, i64 2 }, { ptr, i64 } { ptr @iara_runtime_data_node_12_output_fifos, i64 2 } }, %VirtualFIFO_Node_RuntimeInfo zeroinitializer }, %VirtualFIFO_Node { %VirtualFIFO_Node_StaticInfo { i64 13, i64 4, i64 1, i64 5, i64 1, i64 1 }, %VirtualFIFO_Node_CodegenInfo { ptr @node_13_b, ptr @iara_node_wrapper_13_b, { ptr, i64 } { ptr @iara_runtime_data_node_13_input_fifos, i64 1 }, { ptr, i64 } { ptr @iara_runtime_data_node_13_output_fifos, i64 1 } }, %VirtualFIFO_Node_RuntimeInfo zeroinitializer }, %VirtualFIFO_Node { %VirtualFIFO_Node_StaticInfo { i64 401301, i64 -3, i64 1, i64 7, i64 -3, i64 0 }, %VirtualFIFO_Node_CodegenInfo { ptr @"deallocNode_401301_13_b[0]", ptr @iara_runtime_dealloc, { ptr, i64 } { ptr @iara_runtime_data_node_401301_input_fifos, i64 1 }, { ptr, i64 } { ptr @iara_runtime_data_node_401301_output_fifos, i64 0 } }, %VirtualFIFO_Node_RuntimeInfo zeroinitializer }, %VirtualFIFO_Node { %VirtualFIFO_Node_StaticInfo { i64 14, i64 4, i64 1, i64 5, i64 1, i64 1 }, %VirtualFIFO_Node_CodegenInfo { ptr @node_14_c, ptr @iara_node_wrapper_14_c, { ptr, i64 } { ptr @iara_runtime_data_node_14_input_fifos, i64 1 }, { ptr, i64 } { ptr @iara_runtime_data_node_14_output_fifos, i64 1 } }, %VirtualFIFO_Node_RuntimeInfo zeroinitializer }, %VirtualFIFO_Node { %VirtualFIFO_Node_StaticInfo { i64 401401, i64 -3, i64 1, i64 7, i64 -3, i64 0 }, %VirtualFIFO_Node_CodegenInfo { ptr @"deallocNode_401401_14_c[0]", ptr @iara_runtime_dealloc, { ptr, i64 } { ptr @iara_runtime_data_node_401401_input_fifos, i64 1 }, { ptr, i64 } { ptr @iara_runtime_data_node_401401_output_fifos, i64 0 } }, %VirtualFIFO_Node_RuntimeInfo zeroinitializer }], align 8
@iara_runtime_nodes = constant { ptr, i64 } { ptr @iara_runtime_data__node_infos, i64 8 }, align 8
@iara_runtime_edges = constant { ptr, i64 } { ptr @iara_runtime_data__edge_infos, i64 7 }, align 8

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

define void @"iara_node_wrapper_12_iara_broadcast_tensor<1xi32>_1io_1"(i64 %0, { ptr, i64 } %1) !dbg !15 {
  %3 = extractvalue { ptr, i64 } %1, 0, !dbg !16
  %4 = getelementptr %VirtualFIFO_Chunk, ptr %3, i32 0, i32 2, !dbg !17
  %5 = load ptr, ptr %4, align 8, !dbg !18
  %6 = getelementptr %VirtualFIFO_Chunk, ptr %3, i32 1, i32 2, !dbg !19
  %7 = load ptr, ptr %6, align 8, !dbg !20
  call void @"iara_broadcast_tensor<1xi32>_1io_1"(ptr %5, ptr %7), !dbg !21
  ret void, !dbg !22
}

define void @iara_node_wrapper_13_b(i64 %0, { ptr, i64 } %1) !dbg !23 {
  %3 = extractvalue { ptr, i64 } %1, 0, !dbg !24
  %4 = getelementptr %VirtualFIFO_Chunk, ptr %3, i32 0, i32 2, !dbg !25
  %5 = load ptr, ptr %4, align 8, !dbg !26
  call void @b(ptr %5), !dbg !27
  ret void, !dbg !28
}

declare !dbg !29 void @iara_runtime_dealloc(i64, { ptr, i64 })

define void @iara_node_wrapper_14_c(i64 %0, { ptr, i64 } %1) !dbg !30 {
  %3 = extractvalue { ptr, i64 } %1, 0, !dbg !31
  %4 = getelementptr %VirtualFIFO_Chunk, ptr %3, i32 0, i32 2, !dbg !32
  %5 = load ptr, ptr %4, align 8, !dbg !33
  call void @c(ptr %5), !dbg !34
  ret void, !dbg !35
}

define void @"iara_broadcast_tensor<1xi32>_1io_1"(ptr %0, ptr %1) !dbg !36 {
  call void @llvm.memcpy.p0.p0.i64(ptr %1, ptr %0, i64 4, i1 true), !dbg !37
  %3 = getelementptr i8, ptr %1, i32 4, !dbg !38
  call void @llvm.memcpy.p0.p0.i64(ptr %3, ptr %0, i64 4, i1 true), !dbg !39
  ret void, !dbg !40
}

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: readwrite)
declare void @llvm.memcpy.p0.p0.i64(ptr noalias writeonly captures(none), ptr noalias readonly captures(none), i64, i1 immarg) #0

attributes #0 = { nocallback nofree nounwind willreturn memory(argmem: readwrite) }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2}

!0 = distinct !DICompileUnit(language: DW_LANG_C, file: !1, producer: "MLIR", isOptimized: true, runtimeVersion: 0, emissionKind: LineTablesOnly)
!1 = !DIFile(filename: "schedule.mlir", directory: "/scratch/pedro.ciambra/repos/iara/manual-builds/06-broadcast")
!2 = !{i32 2, !"Debug Info Version", i32 3}
!3 = !DISubprogram(name: "c", linkageName: "c", scope: !1, file: !1, line: 892, type: !4, scopeLine: 892, spFlags: DISPFlagOptimized)
!4 = !DISubroutineType(cc: DW_CC_normal, types: !5)
!5 = !{}
!6 = !DISubprogram(name: "b", linkageName: "b", scope: !1, file: !1, line: 893, type: !4, scopeLine: 893, spFlags: DISPFlagOptimized)
!7 = !DISubprogram(name: "a", linkageName: "a", scope: !1, file: !1, line: 894, type: !4, scopeLine: 894, spFlags: DISPFlagOptimized)
!8 = !DISubprogram(name: "iara_runtime_alloc", linkageName: "iara_runtime_alloc", scope: !1, file: !1, line: 895, type: !4, scopeLine: 895, spFlags: DISPFlagOptimized)
!9 = distinct !DISubprogram(name: "iara_node_wrapper_11_a", linkageName: "iara_node_wrapper_11_a", scope: !1, file: !1, line: 896, type: !4, scopeLine: 896, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!10 = !DILocation(line: 897, column: 10, scope: !9)
!11 = !DILocation(line: 898, column: 10, scope: !9)
!12 = !DILocation(line: 899, column: 10, scope: !9)
!13 = !DILocation(line: 900, column: 5, scope: !9)
!14 = !DILocation(line: 901, column: 5, scope: !9)
!15 = distinct !DISubprogram(name: "iara_node_wrapper_12_iara_broadcast_tensor<1xi32>_1io_1", linkageName: "iara_node_wrapper_12_iara_broadcast_tensor<1xi32>_1io_1", scope: !1, file: !1, line: 903, type: !4, scopeLine: 903, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!16 = !DILocation(line: 904, column: 10, scope: !15)
!17 = !DILocation(line: 905, column: 10, scope: !15)
!18 = !DILocation(line: 906, column: 10, scope: !15)
!19 = !DILocation(line: 908, column: 10, scope: !15)
!20 = !DILocation(line: 909, column: 10, scope: !15)
!21 = !DILocation(line: 910, column: 5, scope: !15)
!22 = !DILocation(line: 911, column: 5, scope: !15)
!23 = distinct !DISubprogram(name: "iara_node_wrapper_13_b", linkageName: "iara_node_wrapper_13_b", scope: !1, file: !1, line: 913, type: !4, scopeLine: 913, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!24 = !DILocation(line: 914, column: 10, scope: !23)
!25 = !DILocation(line: 915, column: 10, scope: !23)
!26 = !DILocation(line: 916, column: 10, scope: !23)
!27 = !DILocation(line: 917, column: 5, scope: !23)
!28 = !DILocation(line: 918, column: 5, scope: !23)
!29 = !DISubprogram(name: "iara_runtime_dealloc", linkageName: "iara_runtime_dealloc", scope: !1, file: !1, line: 920, type: !4, scopeLine: 920, spFlags: DISPFlagOptimized)
!30 = distinct !DISubprogram(name: "iara_node_wrapper_14_c", linkageName: "iara_node_wrapper_14_c", scope: !1, file: !1, line: 921, type: !4, scopeLine: 921, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!31 = !DILocation(line: 922, column: 10, scope: !30)
!32 = !DILocation(line: 923, column: 10, scope: !30)
!33 = !DILocation(line: 924, column: 10, scope: !30)
!34 = !DILocation(line: 925, column: 5, scope: !30)
!35 = !DILocation(line: 926, column: 5, scope: !30)
!36 = distinct !DISubprogram(name: "iara_broadcast_tensor<1xi32>_1io_1", linkageName: "iara_broadcast_tensor<1xi32>_1io_1", scope: !1, file: !1, line: 946, type: !4, scopeLine: 946, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!37 = !DILocation(line: 948, column: 5, scope: !36)
!38 = !DILocation(line: 949, column: 10, scope: !36)
!39 = !DILocation(line: 950, column: 5, scope: !36)
!40 = !DILocation(line: 951, column: 5, scope: !36)
