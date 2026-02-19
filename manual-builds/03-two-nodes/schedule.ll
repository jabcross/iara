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

@iara_runtime_data_node_401201_output_fifos = global [0 x ptr] undef, align 8
@iara_runtime_data_node_401201_input_fifos = global [1 x ptr] [ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 320)], align 8
@iara_runtime_data_node_12_output_fifos = global [1 x ptr] [ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 320)], align 8
@iara_runtime_data_node_12_input_fifos = global [1 x ptr] [ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 160)], align 8
@iara_runtime_data_node_11_output_fifos = global [1 x ptr] [ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 160)], align 8
@iara_runtime_data_node_11_input_fifos = global [1 x ptr] [ptr @iara_runtime_data__edge_infos], align 8
@iara_runtime_data_node_301100_output_fifos = global [1 x ptr] [ptr @iara_runtime_data__edge_infos], align 8
@iara_runtime_data_node_301100_input_fifos = global [0 x ptr] undef, align 8
@"deallocNode_2401201_12_b[0]" = constant [27 x i8] c"deallocNode_2401201_12_b[0]"
@"edge_1112_11_a[0]->12_b[0]" = constant [26 x i8] c"edge_1112_11_a[0]->12_b[0]"
@"allocEdge_301100_11_a[0]" = constant [24 x i8] c"allocEdge_301100_11_a[0]"
@iara_runtime_data__edge_infos = global [3 x %VirtualFIFO_Edge] [%VirtualFIFO_Edge { %VirtualFIFO_Edge_StaticInfo { i64 2301100, i64 0, i64 -2, i64 4, i64 0, i64 0, i64 0, i64 4, i64 4, i64 -2, i64 -2, i64 1, i64 1 }, %VirtualFIFO_Edge_CodegenInfo { ptr @"allocEdge_301100_11_a[0]", { ptr, i64 } zeroinitializer, ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 104), ptr @iara_runtime_data__node_infos, ptr @iara_runtime_data__node_infos, ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 160) } }, %VirtualFIFO_Edge { %VirtualFIFO_Edge_StaticInfo { i64 1112, i64 1, i64 4, i64 4, i64 0, i64 0, i64 0, i64 4, i64 4, i64 1, i64 1, i64 1, i64 1 }, %VirtualFIFO_Edge_CodegenInfo { ptr @"edge_1112_11_a[0]->12_b[0]", { ptr, i64 } zeroinitializer, ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 208), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 104), ptr @iara_runtime_data__node_infos, ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__edge_infos, i64 320) } }, %VirtualFIFO_Edge { %VirtualFIFO_Edge_StaticInfo { i64 2401201, i64 -1, i64 4, i64 -3, i64 1, i64 0, i64 0, i64 4, i64 4, i64 1, i64 1, i64 -3, i64 -3 }, %VirtualFIFO_Edge_CodegenInfo { ptr @"deallocNode_2401201_12_b[0]", { ptr, i64 } zeroinitializer, ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 312), ptr getelementptr inbounds nuw (i8, ptr @iara_runtime_data__node_infos, i64 208), ptr @iara_runtime_data__node_infos, ptr null } }], align 8
@"deallocNode_401201_12_b[0]" = constant [26 x i8] c"deallocNode_401201_12_b[0]"
@node_12_b = constant [9 x i8] c"node_12_b"
@node_11_a = constant [9 x i8] c"node_11_a"
@"allocNode_301100_11_a[0]" = constant [24 x i8] c"allocNode_301100_11_a[0]"
@iara_runtime_data__node_infos = global [4 x %VirtualFIFO_Node] [%VirtualFIFO_Node { %VirtualFIFO_Node_StaticInfo { i64 301100, i64 -2, i64 0, i64 -1, i64 2, i64 0 }, %VirtualFIFO_Node_CodegenInfo { ptr @"allocNode_301100_11_a[0]", ptr @iara_runtime_alloc, { ptr, i64 } { ptr @iara_runtime_data_node_301100_input_fifos, i64 0 }, { ptr, i64 } { ptr @iara_runtime_data_node_301100_output_fifos, i64 1 } }, %VirtualFIFO_Node_RuntimeInfo zeroinitializer }, %VirtualFIFO_Node { %VirtualFIFO_Node_StaticInfo { i64 11, i64 4, i64 1, i64 1, i64 1, i64 1 }, %VirtualFIFO_Node_CodegenInfo { ptr @node_11_a, ptr @iara_node_wrapper_11_a, { ptr, i64 } { ptr @iara_runtime_data_node_11_input_fifos, i64 1 }, { ptr, i64 } { ptr @iara_runtime_data_node_11_output_fifos, i64 1 } }, %VirtualFIFO_Node_RuntimeInfo zeroinitializer }, %VirtualFIFO_Node { %VirtualFIFO_Node_StaticInfo { i64 12, i64 4, i64 1, i64 3, i64 1, i64 1 }, %VirtualFIFO_Node_CodegenInfo { ptr @node_12_b, ptr @iara_node_wrapper_12_b, { ptr, i64 } { ptr @iara_runtime_data_node_12_input_fifos, i64 1 }, { ptr, i64 } { ptr @iara_runtime_data_node_12_output_fifos, i64 1 } }, %VirtualFIFO_Node_RuntimeInfo zeroinitializer }, %VirtualFIFO_Node { %VirtualFIFO_Node_StaticInfo { i64 401201, i64 -3, i64 1, i64 5, i64 -3, i64 0 }, %VirtualFIFO_Node_CodegenInfo { ptr @"deallocNode_401201_12_b[0]", ptr @iara_runtime_dealloc, { ptr, i64 } { ptr @iara_runtime_data_node_401201_input_fifos, i64 1 }, { ptr, i64 } { ptr @iara_runtime_data_node_401201_output_fifos, i64 0 } }, %VirtualFIFO_Node_RuntimeInfo zeroinitializer }], align 8
@iara_runtime_nodes = constant { ptr, i64 } { ptr @iara_runtime_data__node_infos, i64 4 }, align 8
@iara_runtime_edges = constant { ptr, i64 } { ptr @iara_runtime_data__edge_infos, i64 3 }, align 8

declare !dbg !3 void @b(ptr)

declare !dbg !6 void @a(ptr)

declare !dbg !7 void @iara_runtime_alloc(i64, { ptr, i64 })

define void @iara_node_wrapper_11_a(i64 %0, { ptr, i64 } %1) !dbg !8 {
  %3 = extractvalue { ptr, i64 } %1, 0, !dbg !9
  %4 = getelementptr %VirtualFIFO_Chunk, ptr %3, i32 0, i32 2, !dbg !10
  %5 = load ptr, ptr %4, align 8, !dbg !11
  call void @a(ptr %5), !dbg !12
  ret void, !dbg !13
}

define void @iara_node_wrapper_12_b(i64 %0, { ptr, i64 } %1) !dbg !14 {
  %3 = extractvalue { ptr, i64 } %1, 0, !dbg !15
  %4 = getelementptr %VirtualFIFO_Chunk, ptr %3, i32 0, i32 2, !dbg !16
  %5 = load ptr, ptr %4, align 8, !dbg !17
  call void @b(ptr %5), !dbg !18
  ret void, !dbg !19
}

declare !dbg !20 void @iara_runtime_dealloc(i64, { ptr, i64 })

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2}

!0 = distinct !DICompileUnit(language: DW_LANG_C, file: !1, producer: "MLIR", isOptimized: true, runtimeVersion: 0, emissionKind: LineTablesOnly)
!1 = !DIFile(filename: "schedule.mlir", directory: "/scratch/pedro.ciambra/repos/iara/manual-builds/03-two-nodes")
!2 = !{i32 2, !"Debug Info Version", i32 3}
!3 = !DISubprogram(name: "b", linkageName: "b", scope: !1, file: !1, line: 421, type: !4, scopeLine: 421, spFlags: DISPFlagOptimized)
!4 = !DISubroutineType(cc: DW_CC_normal, types: !5)
!5 = !{}
!6 = !DISubprogram(name: "a", linkageName: "a", scope: !1, file: !1, line: 422, type: !4, scopeLine: 422, spFlags: DISPFlagOptimized)
!7 = !DISubprogram(name: "iara_runtime_alloc", linkageName: "iara_runtime_alloc", scope: !1, file: !1, line: 423, type: !4, scopeLine: 423, spFlags: DISPFlagOptimized)
!8 = distinct !DISubprogram(name: "iara_node_wrapper_11_a", linkageName: "iara_node_wrapper_11_a", scope: !1, file: !1, line: 424, type: !4, scopeLine: 424, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!9 = !DILocation(line: 425, column: 10, scope: !8)
!10 = !DILocation(line: 426, column: 10, scope: !8)
!11 = !DILocation(line: 427, column: 10, scope: !8)
!12 = !DILocation(line: 428, column: 5, scope: !8)
!13 = !DILocation(line: 429, column: 5, scope: !8)
!14 = distinct !DISubprogram(name: "iara_node_wrapper_12_b", linkageName: "iara_node_wrapper_12_b", scope: !1, file: !1, line: 431, type: !4, scopeLine: 431, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!15 = !DILocation(line: 432, column: 10, scope: !14)
!16 = !DILocation(line: 433, column: 10, scope: !14)
!17 = !DILocation(line: 434, column: 10, scope: !14)
!18 = !DILocation(line: 435, column: 5, scope: !14)
!19 = !DILocation(line: 436, column: 5, scope: !14)
!20 = !DISubprogram(name: "iara_runtime_dealloc", linkageName: "iara_runtime_dealloc", scope: !1, file: !1, line: 438, type: !4, scopeLine: 438, spFlags: DISPFlagOptimized)
