add_mlir_dialect_library(MLIRIara
        Dialect/IaraTypes.cpp
        Dialect/IaraDialect.cpp
        Dialect/IaraOps.cpp
        Dialect/Broadcast.cpp

        Passes/FlattenPass.cpp

        Passes/RingBuffer/Codegen/Codegen.cpp
        Passes/RingBuffer/SDF/SDF.cpp
        Passes/RingBuffer/RingBufferSchedulerPass.cpp

        Passes/Canonicalize/IaraCanonicalizePass.cpp


        Passes/VirtualFIFO/Codegen/Codegen.cpp
        Passes/VirtualFIFO/SDF/Analysis.cpp
        Passes/VirtualFIFO/SDF/BufferSizeCalculator.cpp
        Passes/VirtualFIFO/SDF/GenerateMemoryManagementNodes.cpp
        Passes/VirtualFIFO/SDF/SDF.cpp
        Passes/VirtualFIFO/BreakLoops.cpp
        Passes/VirtualFIFO/VirtualFIFOSchedulerPass.cpp

        ../../runtime/virtual-fifo/Common.cpp
        # ../../runtime/virtual-fifo/VirtualFIFO_Edge.cpp

        Util/Mlir.cpp

        ADDITIONAL_HEADER_DIRS
        ${PROJECT_SOURCE_DIR}/include/Iara

        DEPENDS
        MLIRIaraOpsIncGen
        MLIRIaraPassesIncGen
        # MLIRIaraInterfacesIncGen


        LINK_LIBS PUBLIC
        MLIRIR
        MLIRInferTypeOpInterface
        MLIRFuncDialect
        )
