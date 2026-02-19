module {
  llvm.mlir.global external @iara_runtime_data_node_401201_output_fifos() {addr_space = 0 : i32, alignment = 8 : i64} : !llvm.array<0 x ptr> {
    %0 = llvm.mlir.undef : !llvm.array<0 x ptr>
    llvm.return %0 : !llvm.array<0 x ptr>
  }
  llvm.mlir.global external @iara_runtime_data_node_401201_input_fifos() {addr_space = 0 : i32, alignment = 8 : i64} : !llvm.array<1 x ptr> {
    %0 = llvm.mlir.undef : !llvm.array<1 x ptr>
    %1 = llvm.mlir.addressof @iara_runtime_data__edge_infos : !llvm.ptr
    %2 = llvm.getelementptr %1[2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<"VirtualFIFO_Edge", (struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Edge_CodegenInfo", (ptr, struct<(ptr, i64)>, ptr, ptr, ptr, ptr)>)>
    %3 = llvm.insertvalue %2, %0[0] : !llvm.array<1 x ptr> 
    llvm.return %3 : !llvm.array<1 x ptr>
  }
  llvm.mlir.global external @iara_runtime_data_node_12_output_fifos() {addr_space = 0 : i32, alignment = 8 : i64} : !llvm.array<1 x ptr> {
    %0 = llvm.mlir.undef : !llvm.array<1 x ptr>
    %1 = llvm.mlir.addressof @iara_runtime_data__edge_infos : !llvm.ptr
    %2 = llvm.getelementptr %1[2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<"VirtualFIFO_Edge", (struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Edge_CodegenInfo", (ptr, struct<(ptr, i64)>, ptr, ptr, ptr, ptr)>)>
    %3 = llvm.insertvalue %2, %0[0] : !llvm.array<1 x ptr> 
    llvm.return %3 : !llvm.array<1 x ptr>
  }
  llvm.mlir.global external @iara_runtime_data_node_12_input_fifos() {addr_space = 0 : i32, alignment = 8 : i64} : !llvm.array<1 x ptr> {
    %0 = llvm.mlir.undef : !llvm.array<1 x ptr>
    %1 = llvm.mlir.addressof @iara_runtime_data__edge_infos : !llvm.ptr
    %2 = llvm.getelementptr %1[1] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<"VirtualFIFO_Edge", (struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Edge_CodegenInfo", (ptr, struct<(ptr, i64)>, ptr, ptr, ptr, ptr)>)>
    %3 = llvm.insertvalue %2, %0[0] : !llvm.array<1 x ptr> 
    llvm.return %3 : !llvm.array<1 x ptr>
  }
  llvm.mlir.global external @iara_runtime_data_node_11_output_fifos() {addr_space = 0 : i32, alignment = 8 : i64} : !llvm.array<1 x ptr> {
    %0 = llvm.mlir.undef : !llvm.array<1 x ptr>
    %1 = llvm.mlir.addressof @iara_runtime_data__edge_infos : !llvm.ptr
    %2 = llvm.getelementptr %1[1] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<"VirtualFIFO_Edge", (struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Edge_CodegenInfo", (ptr, struct<(ptr, i64)>, ptr, ptr, ptr, ptr)>)>
    %3 = llvm.insertvalue %2, %0[0] : !llvm.array<1 x ptr> 
    llvm.return %3 : !llvm.array<1 x ptr>
  }
  llvm.mlir.global external @iara_runtime_data_node_11_input_fifos() {addr_space = 0 : i32, alignment = 8 : i64} : !llvm.array<1 x ptr> {
    %0 = llvm.mlir.undef : !llvm.array<1 x ptr>
    %1 = llvm.mlir.addressof @iara_runtime_data__edge_infos : !llvm.ptr
    %2 = llvm.getelementptr %1[0] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<"VirtualFIFO_Edge", (struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Edge_CodegenInfo", (ptr, struct<(ptr, i64)>, ptr, ptr, ptr, ptr)>)>
    %3 = llvm.insertvalue %2, %0[0] : !llvm.array<1 x ptr> 
    llvm.return %3 : !llvm.array<1 x ptr>
  }
  llvm.mlir.global external @iara_runtime_data_node_301100_output_fifos() {addr_space = 0 : i32, alignment = 8 : i64} : !llvm.array<1 x ptr> {
    %0 = llvm.mlir.undef : !llvm.array<1 x ptr>
    %1 = llvm.mlir.addressof @iara_runtime_data__edge_infos : !llvm.ptr
    %2 = llvm.getelementptr %1[0] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<"VirtualFIFO_Edge", (struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Edge_CodegenInfo", (ptr, struct<(ptr, i64)>, ptr, ptr, ptr, ptr)>)>
    %3 = llvm.insertvalue %2, %0[0] : !llvm.array<1 x ptr> 
    llvm.return %3 : !llvm.array<1 x ptr>
  }
  llvm.mlir.global external @iara_runtime_data_node_301100_input_fifos() {addr_space = 0 : i32, alignment = 8 : i64} : !llvm.array<0 x ptr> {
    %0 = llvm.mlir.undef : !llvm.array<0 x ptr>
    llvm.return %0 : !llvm.array<0 x ptr>
  }
  llvm.mlir.global external constant @"deallocNode_2401201_12_b[0]"("deallocNode_2401201_12_b[0]") {addr_space = 0 : i32}
  llvm.mlir.global external constant @"edge_1112_11_a[0]->12_b[0]"("edge_1112_11_a[0]->12_b[0]") {addr_space = 0 : i32}
  llvm.mlir.global external constant @"allocEdge_301100_11_a[0]"("allocEdge_301100_11_a[0]") {addr_space = 0 : i32}
  llvm.mlir.global external @iara_runtime_data__edge_infos() {addr_space = 0 : i32, alignment = 8 : i64} : !llvm.array<3 x struct<"VirtualFIFO_Edge", (struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Edge_CodegenInfo", (ptr, struct<(ptr, i64)>, ptr, ptr, ptr, ptr)>)>> {
    %0 = llvm.mlir.undef : !llvm.array<3 x struct<"VirtualFIFO_Edge", (struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Edge_CodegenInfo", (ptr, struct<(ptr, i64)>, ptr, ptr, ptr, ptr)>)>>
    %c2301100_i64 = arith.constant 2301100 : i64
    %c0_i64 = arith.constant 0 : i64
    %c-2_i64 = arith.constant -2 : i64
    %c4_i64 = arith.constant 4 : i64
    %c0_i64_0 = arith.constant 0 : i64
    %c0_i64_1 = arith.constant 0 : i64
    %c0_i64_2 = arith.constant 0 : i64
    %c4_i64_3 = arith.constant 4 : i64
    %c4_i64_4 = arith.constant 4 : i64
    %c-2_i64_5 = arith.constant -2 : i64
    %c-2_i64_6 = arith.constant -2 : i64
    %c1_i64 = arith.constant 1 : i64
    %c1_i64_7 = arith.constant 1 : i64
    %1 = llvm.mlir.undef : !llvm.struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)>
    %2 = llvm.insertvalue %c2301100_i64, %1[0] : !llvm.struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)> 
    %3 = llvm.insertvalue %c0_i64, %2[1] : !llvm.struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)> 
    %4 = llvm.insertvalue %c-2_i64, %3[2] : !llvm.struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)> 
    %5 = llvm.insertvalue %c4_i64, %4[3] : !llvm.struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)> 
    %6 = llvm.insertvalue %c0_i64_0, %5[4] : !llvm.struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)> 
    %7 = llvm.insertvalue %c0_i64_1, %6[5] : !llvm.struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)> 
    %8 = llvm.insertvalue %c0_i64_2, %7[6] : !llvm.struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)> 
    %9 = llvm.insertvalue %c4_i64_3, %8[7] : !llvm.struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)> 
    %10 = llvm.insertvalue %c4_i64_4, %9[8] : !llvm.struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)> 
    %11 = llvm.insertvalue %c-2_i64_5, %10[9] : !llvm.struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)> 
    %12 = llvm.insertvalue %c-2_i64_6, %11[10] : !llvm.struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)> 
    %13 = llvm.insertvalue %c1_i64, %12[11] : !llvm.struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)> 
    %14 = llvm.insertvalue %c1_i64_7, %13[12] : !llvm.struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)> 
    %15 = llvm.mlir.addressof @iara_runtime_data__node_infos : !llvm.ptr
    %16 = llvm.getelementptr %15[0] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<"VirtualFIFO_Node", (struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)>, struct<"VirtualFIFO_Node_RuntimeInfo", (ptr)>)>
    %17 = llvm.mlir.addressof @iara_runtime_data__node_infos : !llvm.ptr
    %18 = llvm.getelementptr %17[1] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<"VirtualFIFO_Node", (struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)>, struct<"VirtualFIFO_Node_RuntimeInfo", (ptr)>)>
    %19 = llvm.mlir.addressof @iara_runtime_data__node_infos : !llvm.ptr
    %20 = llvm.getelementptr %19[0] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<"VirtualFIFO_Node", (struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)>, struct<"VirtualFIFO_Node_RuntimeInfo", (ptr)>)>
    %21 = llvm.mlir.addressof @iara_runtime_data__edge_infos : !llvm.ptr
    %22 = llvm.getelementptr %21[1] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<"VirtualFIFO_Edge", (struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Edge_CodegenInfo", (ptr, struct<(ptr, i64)>, ptr, ptr, ptr, ptr)>)>
    %23 = llvm.mlir.addressof @"allocEdge_301100_11_a[0]" : !llvm.ptr
    %24 = llvm.getelementptr %23[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<24 x i8>
    %25 = llvm.mlir.undef : !llvm.struct<(ptr, i64)>
    %26 = llvm.mlir.zero : !llvm.ptr
    %27 = llvm.insertvalue %26, %25[0] : !llvm.struct<(ptr, i64)> 
    %28 = llvm.mlir.constant(0 : i64) : i64
    %29 = llvm.insertvalue %28, %27[1] : !llvm.struct<(ptr, i64)> 
    %30 = llvm.mlir.undef : !llvm.struct<"VirtualFIFO_Edge_CodegenInfo", (ptr, struct<(ptr, i64)>, ptr, ptr, ptr, ptr)>
    %31 = llvm.insertvalue %24, %30[0] : !llvm.struct<"VirtualFIFO_Edge_CodegenInfo", (ptr, struct<(ptr, i64)>, ptr, ptr, ptr, ptr)> 
    %32 = llvm.insertvalue %29, %31[1] : !llvm.struct<"VirtualFIFO_Edge_CodegenInfo", (ptr, struct<(ptr, i64)>, ptr, ptr, ptr, ptr)> 
    %33 = llvm.insertvalue %18, %32[2] : !llvm.struct<"VirtualFIFO_Edge_CodegenInfo", (ptr, struct<(ptr, i64)>, ptr, ptr, ptr, ptr)> 
    %34 = llvm.insertvalue %16, %33[3] : !llvm.struct<"VirtualFIFO_Edge_CodegenInfo", (ptr, struct<(ptr, i64)>, ptr, ptr, ptr, ptr)> 
    %35 = llvm.insertvalue %20, %34[4] : !llvm.struct<"VirtualFIFO_Edge_CodegenInfo", (ptr, struct<(ptr, i64)>, ptr, ptr, ptr, ptr)> 
    %36 = llvm.insertvalue %22, %35[5] : !llvm.struct<"VirtualFIFO_Edge_CodegenInfo", (ptr, struct<(ptr, i64)>, ptr, ptr, ptr, ptr)> 
    %37 = llvm.mlir.undef : !llvm.struct<"VirtualFIFO_Edge", (struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Edge_CodegenInfo", (ptr, struct<(ptr, i64)>, ptr, ptr, ptr, ptr)>)>
    %38 = llvm.insertvalue %14, %37[0] : !llvm.struct<"VirtualFIFO_Edge", (struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Edge_CodegenInfo", (ptr, struct<(ptr, i64)>, ptr, ptr, ptr, ptr)>)> 
    %39 = llvm.insertvalue %36, %38[1] : !llvm.struct<"VirtualFIFO_Edge", (struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Edge_CodegenInfo", (ptr, struct<(ptr, i64)>, ptr, ptr, ptr, ptr)>)> 
    %40 = llvm.insertvalue %39, %0[0] : !llvm.array<3 x struct<"VirtualFIFO_Edge", (struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Edge_CodegenInfo", (ptr, struct<(ptr, i64)>, ptr, ptr, ptr, ptr)>)>> 
    %c1112_i64 = arith.constant 1112 : i64
    %c1_i64_8 = arith.constant 1 : i64
    %c4_i64_9 = arith.constant 4 : i64
    %c4_i64_10 = arith.constant 4 : i64
    %c0_i64_11 = arith.constant 0 : i64
    %c0_i64_12 = arith.constant 0 : i64
    %c0_i64_13 = arith.constant 0 : i64
    %c4_i64_14 = arith.constant 4 : i64
    %c4_i64_15 = arith.constant 4 : i64
    %c1_i64_16 = arith.constant 1 : i64
    %c1_i64_17 = arith.constant 1 : i64
    %c1_i64_18 = arith.constant 1 : i64
    %c1_i64_19 = arith.constant 1 : i64
    %41 = llvm.mlir.undef : !llvm.struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)>
    %42 = llvm.insertvalue %c1112_i64, %41[0] : !llvm.struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)> 
    %43 = llvm.insertvalue %c1_i64_8, %42[1] : !llvm.struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)> 
    %44 = llvm.insertvalue %c4_i64_9, %43[2] : !llvm.struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)> 
    %45 = llvm.insertvalue %c4_i64_10, %44[3] : !llvm.struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)> 
    %46 = llvm.insertvalue %c0_i64_11, %45[4] : !llvm.struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)> 
    %47 = llvm.insertvalue %c0_i64_12, %46[5] : !llvm.struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)> 
    %48 = llvm.insertvalue %c0_i64_13, %47[6] : !llvm.struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)> 
    %49 = llvm.insertvalue %c4_i64_14, %48[7] : !llvm.struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)> 
    %50 = llvm.insertvalue %c4_i64_15, %49[8] : !llvm.struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)> 
    %51 = llvm.insertvalue %c1_i64_16, %50[9] : !llvm.struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)> 
    %52 = llvm.insertvalue %c1_i64_17, %51[10] : !llvm.struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)> 
    %53 = llvm.insertvalue %c1_i64_18, %52[11] : !llvm.struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)> 
    %54 = llvm.insertvalue %c1_i64_19, %53[12] : !llvm.struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)> 
    %55 = llvm.mlir.addressof @iara_runtime_data__node_infos : !llvm.ptr
    %56 = llvm.getelementptr %55[1] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<"VirtualFIFO_Node", (struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)>, struct<"VirtualFIFO_Node_RuntimeInfo", (ptr)>)>
    %57 = llvm.mlir.addressof @iara_runtime_data__node_infos : !llvm.ptr
    %58 = llvm.getelementptr %57[2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<"VirtualFIFO_Node", (struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)>, struct<"VirtualFIFO_Node_RuntimeInfo", (ptr)>)>
    %59 = llvm.mlir.addressof @iara_runtime_data__node_infos : !llvm.ptr
    %60 = llvm.getelementptr %59[0] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<"VirtualFIFO_Node", (struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)>, struct<"VirtualFIFO_Node_RuntimeInfo", (ptr)>)>
    %61 = llvm.mlir.addressof @iara_runtime_data__edge_infos : !llvm.ptr
    %62 = llvm.getelementptr %61[2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<"VirtualFIFO_Edge", (struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Edge_CodegenInfo", (ptr, struct<(ptr, i64)>, ptr, ptr, ptr, ptr)>)>
    %63 = llvm.mlir.addressof @"edge_1112_11_a[0]->12_b[0]" : !llvm.ptr
    %64 = llvm.getelementptr %63[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<26 x i8>
    %65 = llvm.mlir.undef : !llvm.struct<(ptr, i64)>
    %66 = llvm.mlir.zero : !llvm.ptr
    %67 = llvm.insertvalue %66, %65[0] : !llvm.struct<(ptr, i64)> 
    %68 = llvm.mlir.constant(0 : i64) : i64
    %69 = llvm.insertvalue %68, %67[1] : !llvm.struct<(ptr, i64)> 
    %70 = llvm.mlir.undef : !llvm.struct<"VirtualFIFO_Edge_CodegenInfo", (ptr, struct<(ptr, i64)>, ptr, ptr, ptr, ptr)>
    %71 = llvm.insertvalue %64, %70[0] : !llvm.struct<"VirtualFIFO_Edge_CodegenInfo", (ptr, struct<(ptr, i64)>, ptr, ptr, ptr, ptr)> 
    %72 = llvm.insertvalue %69, %71[1] : !llvm.struct<"VirtualFIFO_Edge_CodegenInfo", (ptr, struct<(ptr, i64)>, ptr, ptr, ptr, ptr)> 
    %73 = llvm.insertvalue %58, %72[2] : !llvm.struct<"VirtualFIFO_Edge_CodegenInfo", (ptr, struct<(ptr, i64)>, ptr, ptr, ptr, ptr)> 
    %74 = llvm.insertvalue %56, %73[3] : !llvm.struct<"VirtualFIFO_Edge_CodegenInfo", (ptr, struct<(ptr, i64)>, ptr, ptr, ptr, ptr)> 
    %75 = llvm.insertvalue %60, %74[4] : !llvm.struct<"VirtualFIFO_Edge_CodegenInfo", (ptr, struct<(ptr, i64)>, ptr, ptr, ptr, ptr)> 
    %76 = llvm.insertvalue %62, %75[5] : !llvm.struct<"VirtualFIFO_Edge_CodegenInfo", (ptr, struct<(ptr, i64)>, ptr, ptr, ptr, ptr)> 
    %77 = llvm.mlir.undef : !llvm.struct<"VirtualFIFO_Edge", (struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Edge_CodegenInfo", (ptr, struct<(ptr, i64)>, ptr, ptr, ptr, ptr)>)>
    %78 = llvm.insertvalue %54, %77[0] : !llvm.struct<"VirtualFIFO_Edge", (struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Edge_CodegenInfo", (ptr, struct<(ptr, i64)>, ptr, ptr, ptr, ptr)>)> 
    %79 = llvm.insertvalue %76, %78[1] : !llvm.struct<"VirtualFIFO_Edge", (struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Edge_CodegenInfo", (ptr, struct<(ptr, i64)>, ptr, ptr, ptr, ptr)>)> 
    %80 = llvm.insertvalue %79, %40[1] : !llvm.array<3 x struct<"VirtualFIFO_Edge", (struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Edge_CodegenInfo", (ptr, struct<(ptr, i64)>, ptr, ptr, ptr, ptr)>)>> 
    %c2401201_i64 = arith.constant 2401201 : i64
    %c-1_i64 = arith.constant -1 : i64
    %c4_i64_20 = arith.constant 4 : i64
    %c-3_i64 = arith.constant -3 : i64
    %c1_i64_21 = arith.constant 1 : i64
    %c0_i64_22 = arith.constant 0 : i64
    %c0_i64_23 = arith.constant 0 : i64
    %c4_i64_24 = arith.constant 4 : i64
    %c4_i64_25 = arith.constant 4 : i64
    %c1_i64_26 = arith.constant 1 : i64
    %c1_i64_27 = arith.constant 1 : i64
    %c-3_i64_28 = arith.constant -3 : i64
    %c-3_i64_29 = arith.constant -3 : i64
    %81 = llvm.mlir.undef : !llvm.struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)>
    %82 = llvm.insertvalue %c2401201_i64, %81[0] : !llvm.struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)> 
    %83 = llvm.insertvalue %c-1_i64, %82[1] : !llvm.struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)> 
    %84 = llvm.insertvalue %c4_i64_20, %83[2] : !llvm.struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)> 
    %85 = llvm.insertvalue %c-3_i64, %84[3] : !llvm.struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)> 
    %86 = llvm.insertvalue %c1_i64_21, %85[4] : !llvm.struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)> 
    %87 = llvm.insertvalue %c0_i64_22, %86[5] : !llvm.struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)> 
    %88 = llvm.insertvalue %c0_i64_23, %87[6] : !llvm.struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)> 
    %89 = llvm.insertvalue %c4_i64_24, %88[7] : !llvm.struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)> 
    %90 = llvm.insertvalue %c4_i64_25, %89[8] : !llvm.struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)> 
    %91 = llvm.insertvalue %c1_i64_26, %90[9] : !llvm.struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)> 
    %92 = llvm.insertvalue %c1_i64_27, %91[10] : !llvm.struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)> 
    %93 = llvm.insertvalue %c-3_i64_28, %92[11] : !llvm.struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)> 
    %94 = llvm.insertvalue %c-3_i64_29, %93[12] : !llvm.struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)> 
    %95 = llvm.mlir.addressof @iara_runtime_data__node_infos : !llvm.ptr
    %96 = llvm.getelementptr %95[2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<"VirtualFIFO_Node", (struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)>, struct<"VirtualFIFO_Node_RuntimeInfo", (ptr)>)>
    %97 = llvm.mlir.addressof @iara_runtime_data__node_infos : !llvm.ptr
    %98 = llvm.getelementptr %97[3] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<"VirtualFIFO_Node", (struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)>, struct<"VirtualFIFO_Node_RuntimeInfo", (ptr)>)>
    %99 = llvm.mlir.addressof @iara_runtime_data__node_infos : !llvm.ptr
    %100 = llvm.getelementptr %99[0] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<"VirtualFIFO_Node", (struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)>, struct<"VirtualFIFO_Node_RuntimeInfo", (ptr)>)>
    %101 = llvm.mlir.zero : !llvm.ptr
    %102 = llvm.mlir.addressof @"deallocNode_2401201_12_b[0]" : !llvm.ptr
    %103 = llvm.getelementptr %102[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<27 x i8>
    %104 = llvm.mlir.undef : !llvm.struct<(ptr, i64)>
    %105 = llvm.mlir.zero : !llvm.ptr
    %106 = llvm.insertvalue %105, %104[0] : !llvm.struct<(ptr, i64)> 
    %107 = llvm.mlir.constant(0 : i64) : i64
    %108 = llvm.insertvalue %107, %106[1] : !llvm.struct<(ptr, i64)> 
    %109 = llvm.mlir.undef : !llvm.struct<"VirtualFIFO_Edge_CodegenInfo", (ptr, struct<(ptr, i64)>, ptr, ptr, ptr, ptr)>
    %110 = llvm.insertvalue %103, %109[0] : !llvm.struct<"VirtualFIFO_Edge_CodegenInfo", (ptr, struct<(ptr, i64)>, ptr, ptr, ptr, ptr)> 
    %111 = llvm.insertvalue %108, %110[1] : !llvm.struct<"VirtualFIFO_Edge_CodegenInfo", (ptr, struct<(ptr, i64)>, ptr, ptr, ptr, ptr)> 
    %112 = llvm.insertvalue %98, %111[2] : !llvm.struct<"VirtualFIFO_Edge_CodegenInfo", (ptr, struct<(ptr, i64)>, ptr, ptr, ptr, ptr)> 
    %113 = llvm.insertvalue %96, %112[3] : !llvm.struct<"VirtualFIFO_Edge_CodegenInfo", (ptr, struct<(ptr, i64)>, ptr, ptr, ptr, ptr)> 
    %114 = llvm.insertvalue %100, %113[4] : !llvm.struct<"VirtualFIFO_Edge_CodegenInfo", (ptr, struct<(ptr, i64)>, ptr, ptr, ptr, ptr)> 
    %115 = llvm.insertvalue %101, %114[5] : !llvm.struct<"VirtualFIFO_Edge_CodegenInfo", (ptr, struct<(ptr, i64)>, ptr, ptr, ptr, ptr)> 
    %116 = llvm.mlir.undef : !llvm.struct<"VirtualFIFO_Edge", (struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Edge_CodegenInfo", (ptr, struct<(ptr, i64)>, ptr, ptr, ptr, ptr)>)>
    %117 = llvm.insertvalue %94, %116[0] : !llvm.struct<"VirtualFIFO_Edge", (struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Edge_CodegenInfo", (ptr, struct<(ptr, i64)>, ptr, ptr, ptr, ptr)>)> 
    %118 = llvm.insertvalue %115, %117[1] : !llvm.struct<"VirtualFIFO_Edge", (struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Edge_CodegenInfo", (ptr, struct<(ptr, i64)>, ptr, ptr, ptr, ptr)>)> 
    %119 = llvm.insertvalue %118, %80[2] : !llvm.array<3 x struct<"VirtualFIFO_Edge", (struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Edge_CodegenInfo", (ptr, struct<(ptr, i64)>, ptr, ptr, ptr, ptr)>)>> 
    llvm.return %119 : !llvm.array<3 x struct<"VirtualFIFO_Edge", (struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Edge_CodegenInfo", (ptr, struct<(ptr, i64)>, ptr, ptr, ptr, ptr)>)>>
  }
  llvm.mlir.global external constant @"deallocNode_401201_12_b[0]"("deallocNode_401201_12_b[0]") {addr_space = 0 : i32}
  llvm.mlir.global external constant @node_12_b("node_12_b") {addr_space = 0 : i32}
  llvm.mlir.global external constant @node_11_a("node_11_a") {addr_space = 0 : i32}
  llvm.mlir.global external constant @"allocNode_301100_11_a[0]"("allocNode_301100_11_a[0]") {addr_space = 0 : i32}
  llvm.mlir.global external @iara_runtime_data__node_infos() {addr_space = 0 : i32, alignment = 8 : i64} : !llvm.array<4 x struct<"VirtualFIFO_Node", (struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)>, struct<"VirtualFIFO_Node_RuntimeInfo", (ptr)>)>> {
    %0 = llvm.mlir.undef : !llvm.array<4 x struct<"VirtualFIFO_Node", (struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)>, struct<"VirtualFIFO_Node_RuntimeInfo", (ptr)>)>>
    %c301100_i64 = arith.constant 301100 : i64
    %c-2_i64 = arith.constant -2 : i64
    %c0_i64 = arith.constant 0 : i64
    %c-1_i64 = arith.constant -1 : i64
    %c2_i64 = arith.constant 2 : i64
    %c0_i64_0 = arith.constant 0 : i64
    %1 = llvm.mlir.undef : !llvm.struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)>
    %2 = llvm.insertvalue %c301100_i64, %1[0] : !llvm.struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)> 
    %3 = llvm.insertvalue %c-2_i64, %2[1] : !llvm.struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)> 
    %4 = llvm.insertvalue %c0_i64, %3[2] : !llvm.struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)> 
    %5 = llvm.insertvalue %c-1_i64, %4[3] : !llvm.struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)> 
    %6 = llvm.insertvalue %c2_i64, %5[4] : !llvm.struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)> 
    %7 = llvm.insertvalue %c0_i64_0, %6[5] : !llvm.struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)> 
    %8 = llvm.mlir.undef : !llvm.struct<(ptr, i64)>
    %9 = llvm.mlir.zero : !llvm.ptr
    %10 = llvm.insertvalue %9, %8[0] : !llvm.struct<(ptr, i64)> 
    %11 = llvm.mlir.constant(0 : i64) : i64
    %12 = llvm.mlir.undef : !llvm.struct<(ptr, i64)>
    %13 = llvm.mlir.addressof @iara_runtime_data_node_301100_input_fifos : !llvm.ptr
    %14 = llvm.getelementptr %13[0] : (!llvm.ptr) -> !llvm.ptr, !llvm.ptr
    %15 = llvm.insertvalue %14, %12[0] : !llvm.struct<(ptr, i64)> 
    %16 = llvm.mlir.constant(0 : i64) : i64
    %17 = llvm.insertvalue %16, %15[1] : !llvm.struct<(ptr, i64)> 
    %18 = llvm.mlir.undef : !llvm.struct<(ptr, i64)>
    %19 = llvm.mlir.zero : !llvm.ptr
    %20 = llvm.insertvalue %19, %18[0] : !llvm.struct<(ptr, i64)> 
    %21 = llvm.mlir.constant(0 : i64) : i64
    %22 = llvm.mlir.undef : !llvm.struct<(ptr, i64)>
    %23 = llvm.mlir.addressof @iara_runtime_data_node_301100_output_fifos : !llvm.ptr
    %24 = llvm.getelementptr %23[0] : (!llvm.ptr) -> !llvm.ptr, !llvm.ptr
    %25 = llvm.insertvalue %24, %22[0] : !llvm.struct<(ptr, i64)> 
    %26 = llvm.mlir.constant(1 : i64) : i64
    %27 = llvm.insertvalue %26, %25[1] : !llvm.struct<(ptr, i64)> 
    %28 = llvm.mlir.addressof @"allocNode_301100_11_a[0]" : !llvm.ptr
    %29 = llvm.getelementptr %28[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<24 x i8>
    %30 = llvm.mlir.addressof @iara_runtime_alloc : !llvm.ptr
    %31 = llvm.mlir.undef : !llvm.struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)>
    %32 = llvm.insertvalue %29, %31[0] : !llvm.struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)> 
    %33 = llvm.insertvalue %30, %32[1] : !llvm.struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)> 
    %34 = llvm.insertvalue %17, %33[2] : !llvm.struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)> 
    %35 = llvm.insertvalue %27, %34[3] : !llvm.struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)> 
    %36 = llvm.mlir.zero : !llvm.ptr
    %37 = llvm.mlir.undef : !llvm.struct<"VirtualFIFO_Node_RuntimeInfo", (ptr)>
    %38 = llvm.insertvalue %36, %37[0] : !llvm.struct<"VirtualFIFO_Node_RuntimeInfo", (ptr)> 
    %39 = llvm.mlir.undef : !llvm.struct<"VirtualFIFO_Node", (struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)>, struct<"VirtualFIFO_Node_RuntimeInfo", (ptr)>)>
    %40 = llvm.insertvalue %7, %39[0] : !llvm.struct<"VirtualFIFO_Node", (struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)>, struct<"VirtualFIFO_Node_RuntimeInfo", (ptr)>)> 
    %41 = llvm.insertvalue %35, %40[1] : !llvm.struct<"VirtualFIFO_Node", (struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)>, struct<"VirtualFIFO_Node_RuntimeInfo", (ptr)>)> 
    %42 = llvm.insertvalue %38, %41[2] : !llvm.struct<"VirtualFIFO_Node", (struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)>, struct<"VirtualFIFO_Node_RuntimeInfo", (ptr)>)> 
    %43 = llvm.insertvalue %42, %0[0] : !llvm.array<4 x struct<"VirtualFIFO_Node", (struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)>, struct<"VirtualFIFO_Node_RuntimeInfo", (ptr)>)>> 
    %c11_i64 = arith.constant 11 : i64
    %c4_i64 = arith.constant 4 : i64
    %c1_i64 = arith.constant 1 : i64
    %c1_i64_1 = arith.constant 1 : i64
    %c1_i64_2 = arith.constant 1 : i64
    %c1_i64_3 = arith.constant 1 : i64
    %44 = llvm.mlir.undef : !llvm.struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)>
    %45 = llvm.insertvalue %c11_i64, %44[0] : !llvm.struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)> 
    %46 = llvm.insertvalue %c4_i64, %45[1] : !llvm.struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)> 
    %47 = llvm.insertvalue %c1_i64, %46[2] : !llvm.struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)> 
    %48 = llvm.insertvalue %c1_i64_1, %47[3] : !llvm.struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)> 
    %49 = llvm.insertvalue %c1_i64_2, %48[4] : !llvm.struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)> 
    %50 = llvm.insertvalue %c1_i64_3, %49[5] : !llvm.struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)> 
    %51 = llvm.mlir.undef : !llvm.struct<(ptr, i64)>
    %52 = llvm.mlir.zero : !llvm.ptr
    %53 = llvm.insertvalue %52, %51[0] : !llvm.struct<(ptr, i64)> 
    %54 = llvm.mlir.constant(0 : i64) : i64
    %55 = llvm.mlir.undef : !llvm.struct<(ptr, i64)>
    %56 = llvm.mlir.addressof @iara_runtime_data_node_11_input_fifos : !llvm.ptr
    %57 = llvm.getelementptr %56[0] : (!llvm.ptr) -> !llvm.ptr, !llvm.ptr
    %58 = llvm.insertvalue %57, %55[0] : !llvm.struct<(ptr, i64)> 
    %59 = llvm.mlir.constant(1 : i64) : i64
    %60 = llvm.insertvalue %59, %58[1] : !llvm.struct<(ptr, i64)> 
    %61 = llvm.mlir.undef : !llvm.struct<(ptr, i64)>
    %62 = llvm.mlir.zero : !llvm.ptr
    %63 = llvm.insertvalue %62, %61[0] : !llvm.struct<(ptr, i64)> 
    %64 = llvm.mlir.constant(0 : i64) : i64
    %65 = llvm.mlir.undef : !llvm.struct<(ptr, i64)>
    %66 = llvm.mlir.addressof @iara_runtime_data_node_11_output_fifos : !llvm.ptr
    %67 = llvm.getelementptr %66[0] : (!llvm.ptr) -> !llvm.ptr, !llvm.ptr
    %68 = llvm.insertvalue %67, %65[0] : !llvm.struct<(ptr, i64)> 
    %69 = llvm.mlir.constant(1 : i64) : i64
    %70 = llvm.insertvalue %69, %68[1] : !llvm.struct<(ptr, i64)> 
    %71 = llvm.mlir.addressof @node_11_a : !llvm.ptr
    %72 = llvm.getelementptr %71[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<9 x i8>
    %73 = llvm.mlir.addressof @iara_node_wrapper_11_a : !llvm.ptr
    %74 = llvm.mlir.undef : !llvm.struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)>
    %75 = llvm.insertvalue %72, %74[0] : !llvm.struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)> 
    %76 = llvm.insertvalue %73, %75[1] : !llvm.struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)> 
    %77 = llvm.insertvalue %60, %76[2] : !llvm.struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)> 
    %78 = llvm.insertvalue %70, %77[3] : !llvm.struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)> 
    %79 = llvm.mlir.zero : !llvm.ptr
    %80 = llvm.mlir.undef : !llvm.struct<"VirtualFIFO_Node_RuntimeInfo", (ptr)>
    %81 = llvm.insertvalue %79, %80[0] : !llvm.struct<"VirtualFIFO_Node_RuntimeInfo", (ptr)> 
    %82 = llvm.mlir.undef : !llvm.struct<"VirtualFIFO_Node", (struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)>, struct<"VirtualFIFO_Node_RuntimeInfo", (ptr)>)>
    %83 = llvm.insertvalue %50, %82[0] : !llvm.struct<"VirtualFIFO_Node", (struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)>, struct<"VirtualFIFO_Node_RuntimeInfo", (ptr)>)> 
    %84 = llvm.insertvalue %78, %83[1] : !llvm.struct<"VirtualFIFO_Node", (struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)>, struct<"VirtualFIFO_Node_RuntimeInfo", (ptr)>)> 
    %85 = llvm.insertvalue %81, %84[2] : !llvm.struct<"VirtualFIFO_Node", (struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)>, struct<"VirtualFIFO_Node_RuntimeInfo", (ptr)>)> 
    %86 = llvm.insertvalue %85, %43[1] : !llvm.array<4 x struct<"VirtualFIFO_Node", (struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)>, struct<"VirtualFIFO_Node_RuntimeInfo", (ptr)>)>> 
    %c12_i64 = arith.constant 12 : i64
    %c4_i64_4 = arith.constant 4 : i64
    %c1_i64_5 = arith.constant 1 : i64
    %c3_i64 = arith.constant 3 : i64
    %c1_i64_6 = arith.constant 1 : i64
    %c1_i64_7 = arith.constant 1 : i64
    %87 = llvm.mlir.undef : !llvm.struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)>
    %88 = llvm.insertvalue %c12_i64, %87[0] : !llvm.struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)> 
    %89 = llvm.insertvalue %c4_i64_4, %88[1] : !llvm.struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)> 
    %90 = llvm.insertvalue %c1_i64_5, %89[2] : !llvm.struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)> 
    %91 = llvm.insertvalue %c3_i64, %90[3] : !llvm.struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)> 
    %92 = llvm.insertvalue %c1_i64_6, %91[4] : !llvm.struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)> 
    %93 = llvm.insertvalue %c1_i64_7, %92[5] : !llvm.struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)> 
    %94 = llvm.mlir.undef : !llvm.struct<(ptr, i64)>
    %95 = llvm.mlir.zero : !llvm.ptr
    %96 = llvm.insertvalue %95, %94[0] : !llvm.struct<(ptr, i64)> 
    %97 = llvm.mlir.constant(0 : i64) : i64
    %98 = llvm.mlir.undef : !llvm.struct<(ptr, i64)>
    %99 = llvm.mlir.addressof @iara_runtime_data_node_12_input_fifos : !llvm.ptr
    %100 = llvm.getelementptr %99[0] : (!llvm.ptr) -> !llvm.ptr, !llvm.ptr
    %101 = llvm.insertvalue %100, %98[0] : !llvm.struct<(ptr, i64)> 
    %102 = llvm.mlir.constant(1 : i64) : i64
    %103 = llvm.insertvalue %102, %101[1] : !llvm.struct<(ptr, i64)> 
    %104 = llvm.mlir.undef : !llvm.struct<(ptr, i64)>
    %105 = llvm.mlir.zero : !llvm.ptr
    %106 = llvm.insertvalue %105, %104[0] : !llvm.struct<(ptr, i64)> 
    %107 = llvm.mlir.constant(0 : i64) : i64
    %108 = llvm.mlir.undef : !llvm.struct<(ptr, i64)>
    %109 = llvm.mlir.addressof @iara_runtime_data_node_12_output_fifos : !llvm.ptr
    %110 = llvm.getelementptr %109[0] : (!llvm.ptr) -> !llvm.ptr, !llvm.ptr
    %111 = llvm.insertvalue %110, %108[0] : !llvm.struct<(ptr, i64)> 
    %112 = llvm.mlir.constant(1 : i64) : i64
    %113 = llvm.insertvalue %112, %111[1] : !llvm.struct<(ptr, i64)> 
    %114 = llvm.mlir.addressof @node_12_b : !llvm.ptr
    %115 = llvm.getelementptr %114[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<9 x i8>
    %116 = llvm.mlir.addressof @iara_node_wrapper_12_b : !llvm.ptr
    %117 = llvm.mlir.undef : !llvm.struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)>
    %118 = llvm.insertvalue %115, %117[0] : !llvm.struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)> 
    %119 = llvm.insertvalue %116, %118[1] : !llvm.struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)> 
    %120 = llvm.insertvalue %103, %119[2] : !llvm.struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)> 
    %121 = llvm.insertvalue %113, %120[3] : !llvm.struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)> 
    %122 = llvm.mlir.zero : !llvm.ptr
    %123 = llvm.mlir.undef : !llvm.struct<"VirtualFIFO_Node_RuntimeInfo", (ptr)>
    %124 = llvm.insertvalue %122, %123[0] : !llvm.struct<"VirtualFIFO_Node_RuntimeInfo", (ptr)> 
    %125 = llvm.mlir.undef : !llvm.struct<"VirtualFIFO_Node", (struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)>, struct<"VirtualFIFO_Node_RuntimeInfo", (ptr)>)>
    %126 = llvm.insertvalue %93, %125[0] : !llvm.struct<"VirtualFIFO_Node", (struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)>, struct<"VirtualFIFO_Node_RuntimeInfo", (ptr)>)> 
    %127 = llvm.insertvalue %121, %126[1] : !llvm.struct<"VirtualFIFO_Node", (struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)>, struct<"VirtualFIFO_Node_RuntimeInfo", (ptr)>)> 
    %128 = llvm.insertvalue %124, %127[2] : !llvm.struct<"VirtualFIFO_Node", (struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)>, struct<"VirtualFIFO_Node_RuntimeInfo", (ptr)>)> 
    %129 = llvm.insertvalue %128, %86[2] : !llvm.array<4 x struct<"VirtualFIFO_Node", (struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)>, struct<"VirtualFIFO_Node_RuntimeInfo", (ptr)>)>> 
    %c401201_i64 = arith.constant 401201 : i64
    %c-3_i64 = arith.constant -3 : i64
    %c1_i64_8 = arith.constant 1 : i64
    %c5_i64 = arith.constant 5 : i64
    %c-3_i64_9 = arith.constant -3 : i64
    %c0_i64_10 = arith.constant 0 : i64
    %130 = llvm.mlir.undef : !llvm.struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)>
    %131 = llvm.insertvalue %c401201_i64, %130[0] : !llvm.struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)> 
    %132 = llvm.insertvalue %c-3_i64, %131[1] : !llvm.struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)> 
    %133 = llvm.insertvalue %c1_i64_8, %132[2] : !llvm.struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)> 
    %134 = llvm.insertvalue %c5_i64, %133[3] : !llvm.struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)> 
    %135 = llvm.insertvalue %c-3_i64_9, %134[4] : !llvm.struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)> 
    %136 = llvm.insertvalue %c0_i64_10, %135[5] : !llvm.struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)> 
    %137 = llvm.mlir.undef : !llvm.struct<(ptr, i64)>
    %138 = llvm.mlir.zero : !llvm.ptr
    %139 = llvm.insertvalue %138, %137[0] : !llvm.struct<(ptr, i64)> 
    %140 = llvm.mlir.constant(0 : i64) : i64
    %141 = llvm.mlir.undef : !llvm.struct<(ptr, i64)>
    %142 = llvm.mlir.addressof @iara_runtime_data_node_401201_input_fifos : !llvm.ptr
    %143 = llvm.getelementptr %142[0] : (!llvm.ptr) -> !llvm.ptr, !llvm.ptr
    %144 = llvm.insertvalue %143, %141[0] : !llvm.struct<(ptr, i64)> 
    %145 = llvm.mlir.constant(1 : i64) : i64
    %146 = llvm.insertvalue %145, %144[1] : !llvm.struct<(ptr, i64)> 
    %147 = llvm.mlir.undef : !llvm.struct<(ptr, i64)>
    %148 = llvm.mlir.zero : !llvm.ptr
    %149 = llvm.insertvalue %148, %147[0] : !llvm.struct<(ptr, i64)> 
    %150 = llvm.mlir.constant(0 : i64) : i64
    %151 = llvm.mlir.undef : !llvm.struct<(ptr, i64)>
    %152 = llvm.mlir.addressof @iara_runtime_data_node_401201_output_fifos : !llvm.ptr
    %153 = llvm.getelementptr %152[0] : (!llvm.ptr) -> !llvm.ptr, !llvm.ptr
    %154 = llvm.insertvalue %153, %151[0] : !llvm.struct<(ptr, i64)> 
    %155 = llvm.mlir.constant(0 : i64) : i64
    %156 = llvm.insertvalue %155, %154[1] : !llvm.struct<(ptr, i64)> 
    %157 = llvm.mlir.addressof @"deallocNode_401201_12_b[0]" : !llvm.ptr
    %158 = llvm.getelementptr %157[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<26 x i8>
    %159 = llvm.mlir.addressof @iara_runtime_dealloc : !llvm.ptr
    %160 = llvm.mlir.undef : !llvm.struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)>
    %161 = llvm.insertvalue %158, %160[0] : !llvm.struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)> 
    %162 = llvm.insertvalue %159, %161[1] : !llvm.struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)> 
    %163 = llvm.insertvalue %146, %162[2] : !llvm.struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)> 
    %164 = llvm.insertvalue %156, %163[3] : !llvm.struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)> 
    %165 = llvm.mlir.zero : !llvm.ptr
    %166 = llvm.mlir.undef : !llvm.struct<"VirtualFIFO_Node_RuntimeInfo", (ptr)>
    %167 = llvm.insertvalue %165, %166[0] : !llvm.struct<"VirtualFIFO_Node_RuntimeInfo", (ptr)> 
    %168 = llvm.mlir.undef : !llvm.struct<"VirtualFIFO_Node", (struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)>, struct<"VirtualFIFO_Node_RuntimeInfo", (ptr)>)>
    %169 = llvm.insertvalue %136, %168[0] : !llvm.struct<"VirtualFIFO_Node", (struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)>, struct<"VirtualFIFO_Node_RuntimeInfo", (ptr)>)> 
    %170 = llvm.insertvalue %164, %169[1] : !llvm.struct<"VirtualFIFO_Node", (struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)>, struct<"VirtualFIFO_Node_RuntimeInfo", (ptr)>)> 
    %171 = llvm.insertvalue %167, %170[2] : !llvm.struct<"VirtualFIFO_Node", (struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)>, struct<"VirtualFIFO_Node_RuntimeInfo", (ptr)>)> 
    %172 = llvm.insertvalue %171, %129[3] : !llvm.array<4 x struct<"VirtualFIFO_Node", (struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)>, struct<"VirtualFIFO_Node_RuntimeInfo", (ptr)>)>> 
    llvm.return %172 : !llvm.array<4 x struct<"VirtualFIFO_Node", (struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)>, struct<"VirtualFIFO_Node_RuntimeInfo", (ptr)>)>>
  }
  llvm.func @b(!llvm.ptr) attributes {llvm.emit_c_interface, sym_visibility = "private"}
  llvm.func @a(!llvm.ptr) attributes {llvm.emit_c_interface, sym_visibility = "private"}
  llvm.func @iara_runtime_alloc(i64, !llvm.struct<(ptr, i64)>) attributes {llvm.emit_c_interface, sym_visibility = "private"}
  llvm.func @iara_node_wrapper_11_a(%arg0: i64, %arg1: !llvm.struct<(ptr, i64)>) attributes {llvm.emit_c_interface} {
    %0 = llvm.extractvalue %arg1[0] : !llvm.struct<(ptr, i64)> 
    %1 = llvm.getelementptr %0[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<"VirtualFIFO_Chunk", (ptr, i64, ptr, i64)>
    %2 = llvm.load %1 : !llvm.ptr -> !llvm.ptr
    llvm.call @a(%2) : (!llvm.ptr) -> ()
    llvm.return
  }
  llvm.func @iara_node_wrapper_12_b(%arg0: i64, %arg1: !llvm.struct<(ptr, i64)>) attributes {llvm.emit_c_interface} {
    %0 = llvm.extractvalue %arg1[0] : !llvm.struct<(ptr, i64)> 
    %1 = llvm.getelementptr %0[0, 2] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<"VirtualFIFO_Chunk", (ptr, i64, ptr, i64)>
    %2 = llvm.load %1 : !llvm.ptr -> !llvm.ptr
    llvm.call @b(%2) : (!llvm.ptr) -> ()
    llvm.return
  }
  llvm.func @iara_runtime_dealloc(i64, !llvm.struct<(ptr, i64)>) attributes {llvm.emit_c_interface, sym_visibility = "private"}
  llvm.mlir.global external constant @iara_runtime_nodes() {addr_space = 0 : i32, alignment = 8 : i64} : !llvm.struct<(ptr, i64)> {
    %0 = llvm.mlir.undef : !llvm.struct<(ptr, i64)>
    %1 = llvm.mlir.addressof @iara_runtime_data__node_infos : !llvm.ptr
    %2 = llvm.getelementptr %1[0] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<"VirtualFIFO_Node", (struct<"VirtualFIFO_Node_StaticInfo", (i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Node_CodegenInfo", (ptr, ptr, struct<(ptr, i64)>, struct<(ptr, i64)>)>, struct<"VirtualFIFO_Node_RuntimeInfo", (ptr)>)>
    %3 = llvm.insertvalue %2, %0[0] : !llvm.struct<(ptr, i64)> 
    %4 = llvm.mlir.constant(4 : i64) : i64
    %5 = llvm.insertvalue %4, %3[1] : !llvm.struct<(ptr, i64)> 
    llvm.return %5 : !llvm.struct<(ptr, i64)>
  }
  llvm.mlir.global external constant @iara_runtime_edges() {addr_space = 0 : i32, alignment = 8 : i64} : !llvm.struct<(ptr, i64)> {
    %0 = llvm.mlir.undef : !llvm.struct<(ptr, i64)>
    %1 = llvm.mlir.addressof @iara_runtime_data__edge_infos : !llvm.ptr
    %2 = llvm.getelementptr %1[0] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<"VirtualFIFO_Edge", (struct<"VirtualFIFO_Edge_StaticInfo", (i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64)>, struct<"VirtualFIFO_Edge_CodegenInfo", (ptr, struct<(ptr, i64)>, ptr, ptr, ptr, ptr)>)>
    %3 = llvm.insertvalue %2, %0[0] : !llvm.struct<(ptr, i64)> 
    %4 = llvm.mlir.constant(3 : i64) : i64
    %5 = llvm.insertvalue %4, %3[1] : !llvm.struct<(ptr, i64)> 
    llvm.return %5 : !llvm.struct<(ptr, i64)>
  }
}

