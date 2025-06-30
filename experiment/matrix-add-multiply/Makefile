# Makefile for Matrix Add-Multiply Experiment
# Converted from build.sh

# Required environment variables (must be set externally)
ifndef MATRIX_ORDER
$(error MATRIX_ORDER must be set)
endif

ifndef INSTANCE_DIR
$(error INSTANCE_DIR must be set)
endif

ifndef IARA_DIR
$(error IARA_DIR must be set)
endif

ifndef SCHEDULER_MODE
$(error SCHEDULER_MODE must be set)
endif

ifndef LLVM_DIR
$(error LLVM_DIR must be set)
endif

# Derived variables
EXP_DIR := $(dir $(realpath $(lastword $(MAKEFILE_LIST))))
MATRIXNUMELEMENTS := $(shell echo $$(($(MATRIX_ORDER) * $(MATRIX_ORDER))))
BUILD_DIR := $(INSTANCE_DIR)/build
PATH_TO_EXP_SOURCES := $(EXP_DIR)
OMP_INCLUDE := /usr/lib/clang/19/include

# Flags and includes
IARA_FLAGS ?= --flatten --$(SCHEDULER_MODE)='main-actor=run'
# COMPILER_FLAGS := -g -stdlib=libc++ -fopenmp -DMATRIXORDER=$(MATRIX_ORDER)
COMPILER_FLAGS := -stdlib=libc++ -g -fopenmp -DMATRIXORDER=$(MATRIX_ORDER) -DIARA_USE_MUTEX_HASHMAP


LINKER_FLAGS := -lomp -lpthread -lc++ -lc++abi
INCLUDES := -I$(BUILD_DIR) -I$(IARA_DIR)/include -I$(IARA_DIR)/external \
            -I$(LLVM_DIR)/mlir/include -I$(LLVM_DIR)/llvm/include -I$(OMP_INCLUDE)

# Scheduler sources
SCHEDULER_SOURCES := $(wildcard $(IARA_DIR)/runtime/$(SCHEDULER_MODE)/*.c*)

# Source files
C_SOURCES := $(wildcard $(PATH_TO_EXP_SOURCES)/*.c)
CPP_SOURCES := $(wildcard $(PATH_TO_EXP_SOURCES)/*.cpp)

# Object files
C_OBJECTS := $(patsubst $(PATH_TO_EXP_SOURCES)/%.c,$(BUILD_DIR)/%.o,$(C_SOURCES))
CPP_OBJECTS := $(patsubst $(PATH_TO_EXP_SOURCES)/%.cpp,$(BUILD_DIR)/%.o,$(CPP_SOURCES))
SCHEDULER_OBJECTS := $(patsubst $(IARA_DIR)/runtime/$(SCHEDULER_MODE)/%.cpp,$(BUILD_DIR)/%.o,$(filter %.cpp,$(SCHEDULER_SOURCES))) \
                     $(patsubst $(IARA_DIR)/runtime/$(SCHEDULER_MODE)/%.c,$(BUILD_DIR)/%.o,$(filter %.c,$(SCHEDULER_SOURCES)))

ALL_OBJECTS := $(BUILD_DIR)/schedule.o $(C_OBJECTS) $(CPP_OBJECTS) $(SCHEDULER_OBJECTS)


# Default target
.PHONY: all
all: $(BUILD_DIR)/a.out

# Create build directory
$(BUILD_DIR):
	@echo "Creating build directory $(BUILD_DIR)"
	mkdir -p $(BUILD_DIR)

# Generate topology.mlir from template
$(BUILD_DIR)/topology.mlir: $(EXP_DIR)/topology.mlir.template | $(BUILD_DIR)
	@echo "Generating topology.mlir (Matrix size: $(MATRIXNUMELEMENTS))..."
	sed -e "s/MATRIXNUMELEMENTS/$(MATRIXNUMELEMENTS)/g" $< > $@

# Build Iara compiler
.PHONY: build-iara
build-iara:
	@echo "Building Iara compiler..."
	$(IARA_DIR)/scripts/build-iara.sh

# Generate schedule.mlir using iara-opt
$(BUILD_DIR)/schedule.mlir: $(BUILD_DIR)/topology.mlir build-iara
	@echo "Running iara-opt..."
	cd $(BUILD_DIR) && \
	time -f 'iara-opt took %E and returned code %x' \
	iara-opt $(IARA_FLAGS) topology.mlir > schedule.mlir 2>/dev/null

# Convert MLIR to LLVM IR
$(BUILD_DIR)/schedule.ll: $(BUILD_DIR)/schedule.mlir
	@echo "Converting MLIR to LLVM IR..."
	cd $(BUILD_DIR) && \
	bash -x $(IARA_DIR)/scripts/mlir-to-llvmir.sh schedule.mlir

# Compile schedule to object file
$(BUILD_DIR)/schedule.o: $(BUILD_DIR)/schedule.ll
	@echo "Compiling schedule..."
	cd $(BUILD_DIR) && \
	time -f 'compiling schedule took %E and returned code %x' \
	ccache clang++ --std=c++17 $(COMPILER_FLAGS) $(INCLUDES) -xir -c schedule.ll -o schedule.o

# Compile C kernel files
$(BUILD_DIR)/%.o: $(PATH_TO_EXP_SOURCES)/%.c | $(BUILD_DIR)
	@echo "Compiling C kernel: $<"
	cd $(BUILD_DIR) && \
	time -f 'compiling c kernels took %E and returned code %x' \
	ccache clang++ $(COMPILER_FLAGS) $(INCLUDES) $(EXTRA_KERNEL_ARGS) -xc -c $< -o $@

# Compile C++ kernel files
$(BUILD_DIR)/%.o: $(PATH_TO_EXP_SOURCES)/%.cpp | $(BUILD_DIR)
	@echo "Compiling C++ kernel: $<"
	cd $(BUILD_DIR) && \
	time -f 'compiling cpp kernels took %E and returned code %x' \
	ccache clang -v $(INCLUDES) -xc++ -std=c++20 $(COMPILER_FLAGS) -c $< -o $@

# Compile scheduler sources
$(BUILD_DIR)/%.o: $(IARA_DIR)/runtime/$(SCHEDULER_MODE)/%.cpp | $(BUILD_DIR)
	@echo "Compiling runtime: $<"
	cd $(BUILD_DIR) && \
	time -f 'compiling runtime took %E and returned code %x' \
	ccache clang++ -ftime-trace --std=c++17 $(COMPILER_FLAGS) $(INCLUDES) -xc++ -std=c++20 -c $< -o $@

# Compile runtime
$(BUILD_DIR)/%.o: $(IARA_DIR)/runtime/$(SCHEDULER_MODE)/%.c | $(BUILD_DIR)
	@echo "Compiling runtime: $<"
	cd $(BUILD_DIR) && \
	time -f 'compiling runtime took %E and returned code %x' \
	ccache clang++ --std=c++17 $(COMPILER_FLAGS) $(INCLUDES) -xc -c $< -o $@

# Link final executable
$(BUILD_DIR)/a.out: $(ALL_OBJECTS)
	@echo "Linking executable..."
	cd $(BUILD_DIR) && \
	time -f 'linking took %E and returned code %x' \
	ccache clang++ -v --std=c++17 -fuse-ld=mold $(LINKER_FLAGS) $(INCLUDES) $(EXTRA_LINKER_ARGS) $(notdir $(ALL_OBJECTS)) ${EXP_DIR}/matrices.o -o a.out

# Clean build artifacts
.PHONY: clean
clean:
	@echo "Cleaning build directory..."
	rm $(BUILD_DIR)/*

# Help target
.PHONY: help
help:
	@echo "Matrix Add-Multiply Experiment Makefile"
	@echo ""
	@echo "Required environment variables:"
	@echo "  MATRIX_ORDER      - Size of the matrix (e.g., 64)"
	@echo "  EXPERIMENT_SUFFIX - Suffix for the experiment instance"
	@echo "  IARA_DIR          - Path to Iara directory"
	@echo "  SCHEDULER_MODE    - Scheduler mode (e.g., ring-buffer, virtual-fifo)"
	@echo "  LLVM_DIR          - Path to LLVM directory"
	@echo ""
	@echo "Optional environment variables:"
	@echo "  IARA_FLAGS        - Additional flags for iara-opt"
	@echo "  EXTRA_KERNEL_ARGS - Additional compiler flags for kernels"
	@echo "  EXTRA_LINKER_ARGS - Additional linker flags"
	@echo ""
	@echo "Targets:"
	@echo "  all               - Build the complete executable (default)"
	@echo "  build-iara        - Build only the Iara compiler"
	@echo "  clean             - Remove all build artifacts"
	@echo "  help              - Show this help message"

# Include extra arguments if available
-include $(PATH_TO_EXP_SOURCES)/extra_args.mk