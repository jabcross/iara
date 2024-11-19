add_subdirectory(Passes/Schedule)

add_mlir_dialect_library(MLIRIara
        IaraTypes.cpp
        IaraDialect.cpp
        IaraOps.cpp
        IaraPasses.cpp
        # IaraInterfaces.cpp
        Passes/Schedule/TaskScheduler.cpp
        Passes/Schedule/OpenMPScheduler.cpp
        Passes/Flatten.cpp
        Passes/CollapseEdges.cpp
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