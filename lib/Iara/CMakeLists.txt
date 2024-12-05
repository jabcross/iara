add_subdirectory(Passes/Schedule)

add_mlir_dialect_library(MLIRIara
        IaraTypes.cpp
        IaraDialect.cpp
        IaraOps.cpp
        # IaraInterfaces.cpp
        Passes/Schedule/TaskScheduler.cpp
        Passes/Schedule/OpenMPScheduler.cpp
        Passes/FlattenPass.cpp
        Passes/CollapseEdgesPass.cpp
        Passes/LowerToOpenMPPass.cpp
        Passes/LowerToTasksPass.cpp
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