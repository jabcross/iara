add_mlir_dialect_library(MLIRIara
        IaraTypes.cpp
        IaraDialect.cpp
        IaraOps.cpp
        # IaraInterfaces.cpp
        Passes/FlattenPass.cpp
        # Passes/LowerToTasksPass.cpp
        Passes/FIFOScheduler/OoOSchedulerPass.cpp
        Codegen/Codegen.cpp
        # Passes/Schedule/StaticDataCodegen.cpp
        # Passes/Schedule/TotalFiringsCalculator.cpp
        Util/Mlir.cpp
        SDF/BufferSizeCalculator.cpp
        SDF/Canon.cpp
        SDF/GenerateMemoryManagementNodes.cpp
        SDF/SDF.cpp
        SDF/Analysis.cpp
        ../../runtime/ooo-scheduler/SDF_OoO_FIFO.cpp
        ../../runtime/ooo-scheduler/SDF_OoO_Node.cpp
        ../../runtime/ooo-scheduler/SDF_OoO_Scheduler.cpp

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
