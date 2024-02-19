add_mlir_dialect_library(MLIRIara
        IaraTypes.cpp
        IaraDialect.cpp
        IaraOps.cpp
        IaraPasses.cpp

        ADDITIONAL_HEADER_DIRS
        ${PROJECT_SOURCE_DIR}/include/Iara

        DEPENDS
        MLIRIaraOpsIncGen
        MLIRIaraPassesIncGen

        LINK_LIBS PUBLIC
        MLIRIR
        MLIRInferTypeOpInterface
        MLIRFuncDialect
        )
