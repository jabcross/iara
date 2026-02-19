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

@iara_runtime_data_node_401301_output_fifos = global [0 x ptr] undef, align 8
@iara_runtime_data_node_401301_input_fifos = global [1 x ptr] [ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 800)], align 8
@iara_runtime_data_node_401302_output_fifos = global [0 x ptr] undef, align 8
@iara_runtime_data_node_401302_input_fifos = global [1 x ptr] [ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 640)], align 8
@iara_runtime_data_node_13_output_fifos = global [2 x ptr] [ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 800), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 640)], align 8
@iara_runtime_data_node_13_input_fifos = global [2 x ptr] [ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 320), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 480)], align 8
@iara_runtime_data_node_12_output_fifos = global [1 x ptr] [ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 480)], align 8
@iara_runtime_data_node_12_input_fifos = global [1 x ptr] [ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 160)], align 8
@iara_runtime_data_node_301200_output_fifos = global [1 x ptr] [ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 160)], align 8
@iara_runtime_data_node_301200_input_fifos = global [0 x ptr] undef, align 8
@iara_runtime_data_node_11_output_fifos = global [1 x ptr] [ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 320)], align 8
@iara_runtime_data_node_11_input_fifos = global [1 x ptr] [ptr @iara_runtime_data__edge_infos], align 8
@iara_runtime_data_node_301100_output_fifos = global [1 x ptr] [ptr @iara_runtime_data__edge_infos], align 8
@iara_runtime_data_node_301100_input_fifos = global [0 x ptr] undef, align 8
@"deallocNode_2401301_13_c[0]" = constant [27 x i8] c"deallocNode_2401301_13_c[0]"
@"deallocNode_2401302_13_c[1]" = constant [27 x i8] c"deallocNode_2401302_13_c[1]"
@"edge_1213_12_b[0]->13_c[1]" = constant [26 x i8] c"edge_1213_12_b[0]->13_c[1]"
@"edge_1113_11_a[0]->13_c[0]" = constant [26 x i8] c"edge_1113_11_a[0]->13_c[0]"
@"allocEdge_301200_12_b[0]" = constant [24 x i8] c"allocEdge_301200_12_b[0]"
@"allocEdge_301100_11_a[0]" = constant [24 x i8] c"allocEdge_301100_11_a[0]"
@iara_runtime_data__edge_infos = global [6 x %VirtualFIFO_Edge] [%VirtualFIFO_Edge { %VirtualFIFO_Edge_StaticInfo { i64 2301100, i64 0, i64 -2, i64 8, i64 0, i64 0, i64 0, i64 8, i64 8, i64 -2, i64 -2, i64 1, i64 1 }, %VirtualFIFO_Edge_CodegenInfo { ptr @"allocEdge_301100_11_a[0]", { ptr, i64 } zeroinitializer, ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 104), ptr @iara_runtime_data__node_infos, ptr @iara_runtime_data__node_infos, ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 320) } }, %VirtualFIFO_Edge { %VirtualFIFO_Edge_StaticInfo { i64 2301200, i64 0, i64 -2, i64 8, i64 0, i64 0, i64 0, i64 8, i64 8, i64 -2, i64 -2, i64 1, i64 1 }, %VirtualFIFO_Edge_CodegenInfo { ptr @"allocEdge_301200_12_b[0]", { ptr, i64 } zeroinitializer, ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 312), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 208), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 208), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 480) } }, %VirtualFIFO_Edge { %VirtualFIFO_Edge_StaticInfo { i64 1113, i64 1, i64 8, i64 8, i64 0, i64 0, i64 0, i64 8, i64 8, i64 1, i64 1, i64 1, i64 1 }, %VirtualFIFO_Edge_CodegenInfo { ptr @"edge_1113_11_a[0]->13_c[0]", { ptr, i64 } zeroinitializer, ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 416), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 104), ptr @iara_runtime_data__node_infos, ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 800) } }, %VirtualFIFO_Edge { %VirtualFIFO_Edge_StaticInfo { i64 1213, i64 1, i64 8, i64 8, i64 1, i64 0, i64 0, i64 8, i64 8, i64 1, i64 1, i64 1, i64 1 }, %VirtualFIFO_Edge_CodegenInfo { ptr @"edge_1213_12_b[0]->13_c[1]", { ptr, i64 } zeroinitializer, ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 416), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 312), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 208), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 640) } }, %VirtualFIFO_Edge { %VirtualFIFO_Edge_StaticInfo { i64 2401302, i64 -1, i64 8, i64 -3, i64 1, i64 0, i64 0, i64 8, i64 8, i64 1, i64 1, i64 -3, i64 -3 }, %VirtualFIFO_Edge_CodegenInfo { ptr @"deallocNode_2401302_13_c[1]", { ptr, i64 } zeroinitializer, ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 520), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 416), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 208), ptr null } }, %VirtualFIFO_Edge { %VirtualFIFO_Edge_StaticInfo { i64 2401301, i64 -1, i64 8, i64 -3, i64 1, i64 0, i64 0, i64 8, i64 8, i64 1, i64 1, i64 -3, i64 -3 }, %VirtualFIFO_Edge_CodegenInfo { ptr @"deallocNode_2401301_13_c[0]", { ptr, i64 } zeroinitializer, ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 624), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 416), ptr @iara_runtime_data__node_infos, ptr null } }], align 8
@"deallocNode_401301_13_c[0]" = constant [26 x i8] c"deallocNode_401301_13_c[0]"
@"deallocNode_401302_13_c[1]" = constant [26 x i8] c"deallocNode_401302_13_c[1]"
@node_13_c = constant [9 x i8] c"node_13_c"
@node_12_b = constant [9 x i8] c"node_12_b"
@"allocNode_301200_12_b[0]" = constant [24 x i8] c"allocNode_301200_12_b[0]"
@node_11_a = constant [9 x i8] c"node_11_a"
@"allocNode_301100_11_a[0]" = constant [24 x i8] c"allocNode_301100_11_a[0]"
@iara_runtime_data__node_infos = global [7 x %VirtualFIFO_Node] [%VirtualFIFO_Node { %VirtualFIFO_Node_StaticInfo { i64 301100, i64 -2, i64 0, i64 -1, i64 2, i64 0 }, %VirtualFIFO_Node_CodegenInfo { ptr @"allocNode_301100_11_a[0]", ptr @iara_runtime_alloc, { ptr, i64 } { ptr @iara_runtime_data_node_301100_input_fifos, i64 0 }, { ptr, i64 } { ptr @iara_runtime_data_node_301100_output_fifos, i64 1 } }, %VirtualFIFO_Node_RuntimeInfo zeroinitializer }, %VirtualFIFO_Node { %VirtualFIFO_Node_StaticInfo { i64 11, i64 8, i64 1, i64 1, i64 1, i64 1 }, %VirtualFIFO_Node_CodegenInfo { ptr @node_11_a, ptr @iara_node_wrapper_11_a, { ptr, i64 } { ptr @iara_runtime_data_node_11_input_fifos, i64 1 }, { ptr, i64 } { ptr @iara_runtime_data_node_11_output_fifos, i64 1 } }, %VirtualFIFO_Node_RuntimeInfo zeroinitializer }, %VirtualFIFO_Node { %VirtualFIFO_Node_StaticInfo { i64 301200, i64 -2, i64 0, i64 -1, i64 2, i64 0 }, %VirtualFIFO_Node_CodegenInfo { ptr @"allocNode_301200_12_b[0]", ptr @iara_runtime_alloc, { ptr, i64 } { ptr @iara_runtime_data_node_301200_input_fifos, i64 0 }, { ptr, i64 } { ptr @iara_runtime_data_node_301200_output_fifos, i64 1 } }, %VirtualFIFO_Node_RuntimeInfo zeroinitializer }, %VirtualFIFO_Node { %VirtualFIFO_Node_StaticInfo { i64 12, i64 8, i64 1, i64 1, i64 1, i64 1 }, %VirtualFIFO_Node_CodegenInfo { ptr @node_12_b, ptr @iara_node_wrapper_12_b, { ptr, i64 } { ptr @iara_runtime_data_node_12_input_fifos, i64 1 }, { ptr, i64 } { ptr @iara_runtime_data_node_12_output_fifos, i64 1 } }, %VirtualFIFO_Node_RuntimeInfo zeroinitializer }, %VirtualFIFO_Node { %VirtualFIFO_Node_StaticInfo { i64 13, i64 16, i64 2, i64 3, i64 1, i64 1 }, %VirtualFIFO_Node_CodegenInfo { ptr @node_13_c, ptr @iara_node_wrapper_13_c, { ptr, i64 } { ptr @iara_runtime_data_node_13_input_fifos, i64 2 }, { ptr, i64 } { ptr @iara_runtime_data_node_13_output_fifos, i64 2 } }, %VirtualFIFO_Node_RuntimeInfo zeroinitializer }, %VirtualFIFO_Node { %VirtualFIFO_Node_StaticInfo { i64 401302, i64 -3, i64 1, i64 5, i64 -3, i64 0 }, %VirtualFIFO_Node_CodegenInfo { ptr @"deallocNode_401302_13_c[1]", ptr @iara_runtime_dealloc, { ptr, i64 } { ptr @iara_runtime_data_node_401302_input_fifos, i64 1 }, { ptr, i64 } { ptr @iara_runtime_data_node_401302_output_fifos, i64 0 } }, %VirtualFIFO_Node_RuntimeInfo zeroinitializer }, %VirtualFIFO_Node { %VirtualFIFO_Node_StaticInfo { i64 401301, i64 -3, i64 1, i64 5, i64 -3, i64 0 }, %VirtualFIFO_Node_CodegenInfo { ptr @"deallocNode_401301_13_c[0]", ptr @iara_runtime_dealloc, { ptr, i64 } { ptr @iara_runtime_data_node_401301_input_fifos, i64 1 }, { ptr, i64 } { ptr @iara_runtime_data_node_401301_output_fifos, i64 0 } }, %VirtualFIFO_Node_RuntimeInfo zeroinitializer }], align 8
@iara_runtime_nodes = constant { ptr, i64 } { ptr @iara_runtime_data__node_infos, i64 7 }, align 8
@iara_runtime_edges = constant { ptr, i64 } { ptr @iara_runtime_data__edge_infos, i64 6 }, align 8

declare !dbg !3 void @c(ptr, ptr)

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

define void @iara_node_wrapper_12_b(i64 %0, { ptr, i64 } %1) !dbg !15 {
  %3 = extractvalue { ptr, i64 } %1, 0, !dbg !16
  %4 = getelementptr %VirtualFIFO_Chunk, ptr %3, i32 0, i32 2, !dbg !17
  %5 = load ptr, ptr %4, align 8, !dbg !18
  call void @b(ptr %5), !dbg !19
  ret void, !dbg !20
}

define void @iara_node_wrapper_13_c(i64 %0, { ptr, i64 } %1) !dbg !21 {
  %3 = extractvalue { ptr, i64 } %1, 0, !dbg !22
  %4 = getelementptr %VirtualFIFO_Chunk, ptr %3, i32 0, i32 2, !dbg !23
  %5 = load ptr, ptr %4, align 8, !dbg !24
  %6 = getelementptr %VirtualFIFO_Chunk, ptr %3, i32 1, i32 2, !dbg !25
  %7 = load ptr, ptr %6, align 8, !dbg !26
  call void @c(ptr %5, ptr %7), !dbg !27
  ret void, !dbg !28
}

declare !dbg !29 void @iara_runtime_dealloc(i64, { ptr, i64 })

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2}

!0 = distinct !DICompileUnit(language: DW_LANG_C, file: !1, producer: "MLIR", isOptimized: true, runtimeVersion: 0, emissionKind: LineTablesOnly)
!1 = !DIFile(filename: "schedule.mlir", directory: "/scratch/pedro.ciambra/repos/iara/manual-builds/04-multithread")
!2 = !{i32 2, !"Debug Info Version", i32 3}
!3 = !DISubprogram(name: "c", linkageName: "c", scope: !1, file: !1, line: 774, type: !4, scopeLine: 774, spFlags: DISPFlagOptimized)
!4 = !DISubroutineType(cc: DW_CC_normal, types: !5)
!5 = !{}
!6 = !DISubprogram(name: "b", linkageName: "b", scope: !1, file: !1, line: 775, type: !4, scopeLine: 775, spFlags: DISPFlagOptimized)
!7 = !DISubprogram(name: "a", linkageName: "a", scope: !1, file: !1, line: 776, type: !4, scopeLine: 776, spFlags: DISPFlagOptimized)
!8 = !DISubprogram(name: "iara_runtime_alloc", linkageName: "iara_runtime_alloc", scope: !1, file: !1, line: 777, type: !4, scopeLine: 777, spFlags: DISPFlagOptimized)
!9 = distinct !DISubprogram(name: "iara_node_wrapper_11_a", linkageName: "iara_node_wrapper_11_a", scope: !1, file: !1, line: 778, type: !4, scopeLine: 778, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!10 = !DILocation(line: 779, column: 10, scope: !9)
!11 = !DILocation(line: 780, column: 10, scope: !9)
!12 = !DILocation(line: 781, column: 10, scope: !9)
!13 = !DILocation(line: 782, column: 5, scope: !9)
!14 = !DILocation(line: 783, column: 5, scope: !9)
!15 = distinct !DISubprogram(name: "iara_node_wrapper_12_b", linkageName: "iara_node_wrapper_12_b", scope: !1, file: !1, line: 785, type: !4, scopeLine: 785, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!16 = !DILocation(line: 786, column: 10, scope: !15)
!17 = !DILocation(line: 787, column: 10, scope: !15)
!18 = !DILocation(line: 788, column: 10, scope: !15)
!19 = !DILocation(line: 789, column: 5, scope: !15)
!20 = !DILocation(line: 790, column: 5, scope: !15)
!21 = distinct !DISubprogram(name: "iara_node_wrapper_13_c", linkageName: "iara_node_wrapper_13_c", scope: !1, file: !1, line: 792, type: !4, scopeLine: 792, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!22 = !DILocation(line: 793, column: 10, scope: !21)
!23 = !DILocation(line: 794, column: 10, scope: !21)
!24 = !DILocation(line: 795, column: 10, scope: !21)
!25 = !DILocation(line: 797, column: 10, scope: !21)
!26 = !DILocation(line: 798, column: 10, scope: !21)
!27 = !DILocation(line: 799, column: 5, scope: !21)
!28 = !DILocation(line: 800, column: 5, scope: !21)
!29 = !DISubprogram(name: "iara_runtime_dealloc", linkageName: "iara_runtime_dealloc", scope: !1, file: !1, line: 802, type: !4, scopeLine: 802, spFlags: DISPFlagOptimized)
