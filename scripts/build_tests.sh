#!/bin/bash

###############################################################################
# Build and Run Tests Script
#
# This script automates the three-step process for generating, building, and
# running regression tests using the experiment framework:
#
# 1. Generate CMakeLists.txt files from experiments.yaml
# 2. Configure CMake with generated test files
# 3. Run all tests via CTest
#
# Usage:
#   ./build_tests.sh [OPTIONS]
#
# Options:
#   -h, --help                    Show this help message
#   -j, --jobs N                  Number of parallel build jobs
#   -v, --verbose                 Enable verbose output
#   -e, --experiments FILTER      Filter applications (e.g., "05-cholesky")
#   -s, --sets FILTER             Filter experiment sets (e.g., "regression_test")
#
# Examples:
#   ./build_tests.sh                                # Build all regression tests
#   ./build_tests.sh -j 8                           # Build with 8 parallel jobs
#   ./build_tests.sh -e 05-cholesky                 # Build only cholesky tests
#   ./build_tests.sh -s regression_test             # Build only regression_test sets
#   ./build_tests.sh -e 05-cholesky -s regression_test  # Combine filters
###############################################################################

set -e

# Use IARA_DIR if defined, otherwise fall back to script directory
if [[ -z "$IARA_DIR" ]]; then
    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
else
    SCRIPT_DIR="$IARA_DIR"
fi

PARALLEL_JOBS=""
VERBOSE=0
EXPERIMENTS_FILTER=""
EXPERIMENT_SETS_FILTER=""

# Color codes
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Helper functions
log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

show_help() {
    sed -n '1,28p' "$0" | tail -n +3
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            show_help
            exit 0
            ;;
        -j|--jobs)
            PARALLEL_JOBS="$2"
            shift 2
            ;;
        -v|--verbose)
            VERBOSE=1
            shift
            ;;
        -e|--experiments)
            EXPERIMENTS_FILTER="$2"
            shift 2
            ;;
        -s|--sets)
            EXPERIMENT_SETS_FILTER="$2"
            shift 2
            ;;
        *)
            log_error "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done

# Main script
main() {
    cd "${SCRIPT_DIR}"

    log_info "Starting three-step test generation and execution..."

    # Step 1: Generate CMakeLists.txt files from experiments.yaml
    log_info "Step 1: Generating tests from experiments.yaml"

    # Set environment variables for filtering
    export IARA_DIR="${SCRIPT_DIR}"
    if [[ -n "${EXPERIMENTS_FILTER}" ]]; then
        export IARA_EXPERIMENTS_FILTER="${EXPERIMENTS_FILTER}"
    fi
    if [[ -n "${EXPERIMENT_SETS_FILTER}" ]]; then
        export IARA_EXPERIMENT_SETS_FILTER="${EXPERIMENT_SETS_FILTER}"
    fi

    python3 "${SCRIPT_DIR}/scripts/generate_experiments.py"

    if [[ $? -ne 0 ]]; then
        log_error "Failed to generate experiments"
        exit 1
    fi

    log_success "Test generation completed"

    # Step 2: Configure CMake with generated files
    log_info "Step 2: Configuring CMake with generated test files"

    # Build directory
    BUILD_DIR="${SCRIPT_DIR}/build_experiments"

    # CMake configuration
    CMAKE_ARGS="-DCMAKE_BUILD_TYPE=Release -DIARA_BUILD_TESTS=ON"

    if [[ -n "${EXPERIMENTS_FILTER}" ]]; then
        CMAKE_ARGS="${CMAKE_ARGS} -DIARA_EXPERIMENTS_FILTER=${EXPERIMENTS_FILTER}"
    fi
    if [[ -n "${EXPERIMENT_SETS_FILTER}" ]]; then
        CMAKE_ARGS="${CMAKE_ARGS} -DIARA_EXPERIMENT_SETS_FILTER=${EXPERIMENT_SETS_FILTER}"
    fi

    if [[ $VERBOSE -eq 1 ]]; then
        cmake -B "${BUILD_DIR}" ${CMAKE_ARGS} "${SCRIPT_DIR}"
    else
        cmake -B "${BUILD_DIR}" ${CMAKE_ARGS} "${SCRIPT_DIR}" > /dev/null 2>&1
    fi

    if [[ $? -ne 0 ]]; then
        log_error "CMake configuration failed"
        exit 1
    fi

    log_success "CMake configuration completed"

    # Step 3: Run all tests via CTest
    log_info "Step 3: Running tests via CTest"

    # Build command
    BUILD_CMD="cmake --build ${BUILD_DIR} --parallel"
    if [[ -n "${PARALLEL_JOBS}" ]]; then
        BUILD_CMD="${BUILD_CMD} ${PARALLEL_JOBS}"
    fi

    if [[ $VERBOSE -eq 1 ]]; then
        ${BUILD_CMD}
        ctest --build-dir "${BUILD_DIR}" --build-config Release -V
    else
        ${BUILD_CMD} > /dev/null 2>&1
        ctest --build-dir "${BUILD_DIR}" --build-config Release -V 2>&1 | grep -E "^(Test|[0-9]+/|[A-Z ])"
    fi

    if [[ $? -eq 0 ]]; then
        log_success "All tests completed successfully!"
        exit 0
    else
        log_error "Some tests failed"
        exit 1
    fi
}

# Run main
main
