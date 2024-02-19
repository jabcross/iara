add_llvm_library(IaraPlugin
        # BUILDTREE_ONLY is only for testing purposes
        MODULE BUILDTREE_ONLY
        iara-plugin.cpp

        DEPENDS
        MLIRIara
        PLUGIN_TOOL
        mlir-opt

        LINK_LIBS
        MLIRIara
        )
target_include_directories(
        IaraPlugin
        PRIVATE
        "${IARA_BINARY_DIR}/include"
)
