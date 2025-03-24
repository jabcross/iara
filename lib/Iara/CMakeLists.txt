add_mlir_dialect_library(MLIRIara
        IaraTypes.cpp
        IaraDialect.cpp
        IaraOps.cpp
        # IaraInterfaces.cpp
        Passes/FlattenPass.cpp
        Passes/LowerToTasksPass.cpp
        Passes/Schedule/BufferSizeCalculator.cpp
        Passes/Schedule/StaticDataCodegen.cpp
        ../Util/MlirUtil.cpp

        ADDITIONAL_HEADER_DIRS
        ${PROJECT_SOURCE_DIR}/include/Iara
        ${PROJECT_SOURCE_DIR}/include/Util

        DEPENDS
        MLIRIaraOpsIncGen
        MLIRIaraPassesIncGen
        # MLIRIaraInterfacesIncGen


        LINK_LIBS PUBLIC
        MLIRIR
        MLIRInferTypeOpInterface
        MLIRFuncDialect
        )