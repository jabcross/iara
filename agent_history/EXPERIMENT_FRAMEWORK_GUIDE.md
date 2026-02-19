# IaRa Unified Experiment Framework - User Guide

**For:** Users creating new applications and defining experiments
**Current Status:** Phase 1 & 2 Ready (Config & Build System)
**Last Updated:** 2026-01-14

---

## Table of Contents

1. [Overview](#overview)
2. [Quick Start](#quick-start)
3. [Creating a New Application](#creating-a-new-application)
4. [Defining Experiments](#defining-experiments)
5. [Generating Test Configuration](#generating-test-configuration)
6. [Building Experiments](#building-experiments)
7. [Understanding Results](#understanding-results)
8. [Common Tasks](#common-tasks)

---

## Overview

The IaRa Unified Experiment Framework automates:
- **Phase 1 (Configuration):** Define experiments once in YAML, generate test infrastructure
- **Phase 2 (Build):** Automatically build all test combinations with timing measurement
- **Phase 3 (Measurement):** Collect runtime measurements (coming soon)
- **Phase 4 (Visualization):** Generate plots and analysis (coming soon)
- **Phase 5 (CLI):** Complete command-line interface (coming soon)

### What You Get

When you define experiments in YAML, the framework automatically:
1. Generates all parameter combinations
2. Creates CMakeLists.txt with test definitions
3. Builds each test instance
4. Measures compilation time and binary size
5. Tracks errors and retries
6. Outputs results as JSON

---

## Quick Start

If you just want to see it work, use an existing application:

```bash
cd /scratch/pedro.ciambra/repos/iara

# View existing experiment definition
cat applications/05-cholesky/experiment/experiments.yaml

# Generate test configuration
python3 -m tools.experiment_framework generate \
    --app 05-cholesky \
    --set regression

# Check what was generated
ls -la applications/05-cholesky/experiment/
cat applications/05-cholesky/experiment/CMakeLists.txt | head -30

# Build all tests (optional, requires full IaRa environment)
python3 -m tools.experiment_framework build \
    --app 05-cholesky \
    --set regression

# Check results
ls -la applications/05-cholesky/experiment/results/
cat applications/05-cholesky/experiment/results/results_*.json | head -50
```

---

## Creating a New Application

### Step 1: Create the Application Directory Structure

```bash
cd /scratch/pedro.ciambra/repos/iara

# Create application directory
mkdir -p applications/99-myapp
mkdir -p applications/99-myapp/experiment
mkdir -p applications/99-myapp/code
mkdir -p applications/99-myapp/scripts
```

**Directory Layout:**
```
applications/99-myapp/
├── experiment/
│   ├── experiments.yaml          ← You create this (main file)
│   ├── CMakeLists.txt            ← Generated automatically
│   ├── .experiments.yaml.hash     ← Generated automatically
│   ├── results/                  ← Generated when you build
│   │   └── results_*.json
│   └── (optional code generation scripts)
├── code/
│   ├── source files (.c, .cpp, .mlir, etc.)
│   └── CMakeLists.txt (your app's build rules)
└── scripts/
    ├── codegen.sh (optional - if you generate code)
    └── other scripts
```

### Step 2: Create a Simple Application Entry Point

Create `applications/99-myapp/code/main.cpp`:

```cpp
#include <iostream>
#include <cmath>
#include <cstring>

// Command-line parameter handling
int main(int argc, char** argv) {
    int size = 1000;      // Default from --size parameter
    int iterations = 10;  // Default from --iterations parameter

    // Parse environment variables set by experiment framework
    if (const char* env_size = std::getenv("SIZE")) {
        size = std::atoi(env_size);
    }
    if (const char* env_iter = std::getenv("ITERATIONS")) {
        iterations = std::atoi(env_iter);
    }

    std::cout << "Running computation with size=" << size
              << " iterations=" << iterations << std::endl;

    // Dummy computation
    double result = 0.0;
    for (int i = 0; i < iterations; i++) {
        for (int j = 0; j < size; j++) {
            result += std::sqrt(j * 1.5);
        }
    }

    std::cout << "Result: " << result << std::endl;
    return 0;
}
```

### Step 3: Create the Experiment YAML File

Create `applications/99-myapp/experiment/experiments.yaml`:

```yaml
# Application metadata
application:
  name: "99-myapp"
  description: "Example application for framework demonstration"
  entry: "99-myapp"  # Name of the executable
  source_dir: "../code"
  build:
    type: "cmake"
    extra_compiler_args: ["-O2", "-march=native"]
    extra_linker_args: ["-lm"]  # Link math library

# Define input parameters that vary across tests
parameters:
  - name: "size"
    type: "int"
    description: "Problem size (number of elements)"
  - name: "iterations"
    type: "int"
    description: "Number of iterations"
  - name: "scheduler"
    type: "string"
    description: "Scheduler to use"

# Define computed parameters (derived from input parameters)
computed_parameters:
  - name: "total_work"
    type: "int"
    expression: "size * iterations"
    description: "Total computational work (size × iterations)"

# Define constraints (combinations that are invalid)
constraints:
  # Example: don't test very large sizes with many iterations
  - expression: "not (size > 5000 and iterations > 100)"
    description: "Skip huge problem sizes with many iterations"

# Define what we measure
measurements:
  - name: "wall_time"
    type: "float"
    description: "Wall clock time (seconds)"
    parser:
      type: "regex"
      pattern: 'Wall time: (\d+\.?\d*)'
      group: 1
    unit: "s"
    required: true
  - name: "peak_memory"
    type: "int"
    description: "Peak memory usage"
    parser:
      type: "regex"
      pattern: 'Peak memory: (\d+)'
      group: 1
    unit: "MB"
    required: false

# Define groups of related experiments
experiment_sets:
  - name: "quick_test"
    description: "Quick smoke test with small sizes"
    parameters:
      size: [100, 500]
      iterations: [1, 5]
      scheduler: ["sequential"]

  - name: "baseline"
    description: "Baseline performance measurements"
    parameters:
      size: [1000, 2000]
      iterations: [10, 20]
      scheduler: ["sequential", "omp-for"]

  - name: "scaling"
    description: "Study scaling with problem size"
    parameters:
      size: [1000, 2000, 5000, 10000]
      iterations: [10]
      scheduler: ["sequential", "omp-for"]

# Execution parameters
execution:
  timeout: 300  # seconds per test
  repetitions: 3  # number of runs per test
  env_vars:
    OMP_NUM_THREADS: "4"
    OMP_PROC_BIND: "close"

# Output configuration
output:
  results_dir: "./results"
  format: "json"
  keep_build_artifacts: true
```

---

## Defining Experiments

### YAML File Structure Explained

#### 1. **Application Section**
Describes your application:
```yaml
application:
  name: "99-myapp"                    # Unique identifier
  entry: "99-myapp"                   # Executable name after build
  source_dir: "../code"               # Where your code is
  build:
    type: "cmake"                     # Build system type
    extra_linker_args: ["-lm"]       # Libraries to link
```

#### 2. **Parameters Section**
Define what varies in your experiments:
```yaml
parameters:
  - name: "size"
    type: "int"
    description: "Problem size"

  - name: "scheduler"
    type: "string"
    description: "Which scheduler"
```

**Supported Types:** `int`, `float`, `string`, `bool`

#### 3. **Computed Parameters** (Optional)
Define derived parameters:
```yaml
computed_parameters:
  - name: "total_work"
    type: "int"
    expression: "size * iterations"   # Python expression using other params
    description: "Work = size × iterations"
```

**Use cases:**
- Block sizes derived from matrix size
- Derived metrics for analysis
- Work normalization

#### 4. **Constraints** (Optional)
Skip invalid combinations:
```yaml
constraints:
  # Boolean expression using any parameters
  - expression: "not (size > 5000 and iterations > 100)"
    description: "Skip huge combinations"
```

**Examples:**
```yaml
# Only test powers of 2
- expression: "size in [1024, 2048, 4096, 8192]"

# Don't combine specific values
- expression: "not (scheduler == 'sequential' and size > 10000)"

# Require computed parameter relationship
- expression: "total_work < 1000000"
```

#### 5. **Experiment Sets**
Group related parameter combinations:
```yaml
experiment_sets:
  - name: "quick_test"
    description: "Fast smoke tests"
    parameters:
      size: [100, 500]           # These values
      iterations: [1, 5]         # Cartesian product
      scheduler: ["sequential"]  # = 2 × 2 × 1 = 4 tests

  - name: "baseline"
    description: "Production benchmarks"
    parameters:
      size: [1000, 2000, 5000]
      iterations: [10]
      scheduler: ["sequential", "omp-for", "omp-task"]
      # = 3 × 1 × 3 = 9 tests
```

---

## Generating Test Configuration

### Step 1: Validate Your YAML

```bash
cd /scratch/pedro.ciambra/repos/iara

# Try generating with your new application
python3 -m tools.experiment_framework generate \
    --app 99-myapp \
    --set quick_test
```

**Expected output:**
```
✓ Configuration loaded successfully
✓ YAML hash computed: 2a3f5b7c...
✓ Parameter combinations generated: 4 instances
✓ CMakeLists.txt generated: applications/99-myapp/experiment/CMakeLists.txt
✓ Hash file stored: applications/99-myapp/experiment/.experiments.yaml.hash
```

### Step 2: Check Generated Files

```bash
# View what was generated
ls -la applications/99-myapp/experiment/

# Look at the generated CMakeLists.txt
head -50 applications/99-myapp/experiment/CMakeLists.txt
```

**Expected CMakeLists.txt structure:**
```cmake
# Auto-generated by tools.experiment_framework.generator
# Source: applications/99-myapp/experiment/experiments.yaml
# Experiment Set: quick_test
# Generated: 2026-01-14 12:34:56 UTC
# YAML Hash: 2a3f5b7c...

cmake_minimum_required(VERSION 3.20)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/IaRaApplications.cmake)

# Instance 1
# Instance: 99-myapp_sequential_iterations_1_size_100
iara_add_test_instance(
    NAME "99-myapp_sequential_iterations_1_size_100"
    EXPERIMENT_SET "quick_test"
    APPLICATION_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.."
    ENTRY "99-myapp"
    SCHEDULER "sequential"
    PARAMETERS "iterations=1" "size=100"
    BUILD_DIR "${CMAKE_BINARY_DIR}/99-myapp/quick_test/99-myapp_sequential_iterations_1_size_100"
    DEFINES "ITERATIONS=1" "SIZE=100" "SCHEDULER_SEQUENTIAL"
    LINKER_ARGS "-lm"
    TIMEOUT "300"
)

# Instance 2, 3, 4... (more instances)
```

### Step 3: Understand Generated Instance Names

Instance names follow a consistent pattern:
```
<app>_<scheduler>_<param1>_<value1>_<param2>_<value2>_...
```

For example:
- `99-myapp_sequential_iterations_1_size_100`
- `99-myapp_sequential_iterations_5_size_100`
- `99-myapp_sequential_iterations_1_size_500`
- `99-myapp_sequential_iterations_5_size_500`

**Key points:**
- Application name comes first
- Scheduler comes second
- Other parameters alphabetically
- Computed parameters are excluded from name
- Values are sanitized (spaces → hyphens, lowercase)

### Step 4: Regenerate When YAML Changes

When you update `experiments.yaml`:

```bash
# The framework detects changes via hash
python3 -m tools.experiment_framework generate \
    --app 99-myapp \
    --set quick_test

# Output tells you if file changed:
# ✓ YAML file unchanged (hash matches)
# OR
# ⚠ YAML file changed! Regenerating...
# ✓ CMakeLists.txt updated
```

---

## Building Experiments

### Step 1: Basic Build

```bash
cd /scratch/pedro.ciambra/repos/iara

# Build all instances from an experiment set
python3 -m tools.experiment_framework build \
    --app 99-myapp \
    --set quick_test
```

**What happens:**
1. Framework reads `experiments.yaml`
2. Extracts parameter combinations
3. Builds each test instance sequentially
4. Measures:
   - Compilation time (iara-opt, mlir-to-llvm, clang)
   - Binary size
   - Any errors
5. Writes results to `results/results_TIMESTAMP.json`

### Step 2: Monitor Build Progress

The build output shows:
```
Building experiment set: quick_test
Total instances: 4

[1/4] Building 99-myapp_sequential_iterations_1_size_100...
      Compilation time: 2.345s
      Binary size: 156,234 bytes
      ✓ Success

[2/4] Building 99-myapp_sequential_iterations_5_size_100...
      Compilation time: 2.401s
      Binary size: 156,234 bytes
      ✓ Success

[3/4] Building 99-myapp_sequential_iterations_1_size_500...
      Compilation time: 2.456s
      Binary size: 156,234 bytes
      ✓ Success

[4/4] Building 99-myapp_sequential_iterations_5_size_500...
      Compilation time: 2.512s
      Binary size: 156,234 bytes
      ✓ Success

Summary:
  Successful: 4/4 (100%)
  Failed: 0
  Total time: 9.714s
```

### Step 3: Handle Build Failures

If a build fails, the framework:
1. Logs the error
2. Retries up to 2 times (configurable)
3. Uses exponential backoff (1s, 2s, 4s)
4. Tracks all errors
5. Continues with next instance

Example of failure output:
```
[2/4] Building 99-myapp_omp-for_iterations_5_size_5000...
      ERROR (attempt 1): Compilation failed in iara-opt
      Retrying in 1 second...
      ERROR (attempt 2): Compilation failed in iara-opt
      Last error: "undefined reference to 'omp_parallel'"
      ✗ Failed after 2 attempts
```

### Step 4: Clean Rebuild

To force a clean rebuild:

```bash
# Remove build artifacts
rm -rf /tmp/experiment_builds/99-myapp/

# Then rebuild
python3 -m tools.experiment_framework build \
    --app 99-myapp \
    --set quick_test
```

---

## Phase 5: The `run` Command

The `run` command orchestrates the complete experiment pipeline, chaining all phases together in one convenient command.

### What It Does

The `run` command executes phases in order:
1. **Phase 1 - Generate:** Create CMakeLists.txt from experiments.yaml
2. **Phase 2 - Build:** Compile all instances and measure compilation
3. **Phase 3 - Execute:** Run instances and collect runtime measurements
4. **Phase 4 - Visualize:** Generate plots and Jupyter notebooks

### Basic Usage

```bash
cd /scratch/pedro.ciambra/repos/iara

# Run complete pipeline
python3 -m tools.experiment_framework run \
    --app 05-cholesky \
    --set regression
```

**Expected output:**
```
Phase 1: Generating CMakeLists.txt...
  ✓ Generated 42 instances
Phase 2: Building instances...
  ✓ Built 42/42 instances
Phase 3: Executing instances...
  ✓ Executed 42 instances
Phase 4: Generating visualizations...
  ✓ Generated 5 plot(s)
  ✓ Generated notebook: analysis_20260115T120000Z.ipynb
  ✓ Executed notebook

============================================================
PIPELINE COMPLETE
============================================================
Application: 05-cholesky
Experiment Set: regression
Results: applications/05-cholesky/experiment/results
```

### Skip Flags

Use skip flags to rerun only specific phases:

#### Skip Generate (Use Existing CMakeLists.txt)

```bash
# Skip generation if CMakeLists.txt hasn't changed
python3 -m tools.experiment_framework run \
    --app 05-cholesky \
    --set regression \
    --skip-generate
```

**Note:** When skipping generate, the framework validates that `experiments.yaml` hasn't changed by comparing hash files. If the YAML changed, you'll get an error and need to regenerate.

#### Skip Build (Use Existing Binaries)

```bash
# Skip build if binaries are already compiled
python3 -m tools.experiment_framework run \
    --app 05-cholesky \
    --set regression \
    --skip-build
```

**Use case:** You've already built the instances and just want to re-run measurements or regenerate visualizations.

#### Skip Execute (Use Existing Measurements)

```bash
# Skip execution if measurements are already collected
python3 -m tools.experiment_framework run \
    --app 05-cholesky \
    --set regression \
    --skip-execute
```

**Use case:** You want to regenerate plots with different settings without re-running experiments.

#### Combine Skip Flags

```bash
# Only regenerate visualizations
python3 -m tools.experiment_framework run \
    --app 05-cholesky \
    --set regression \
    --skip-generate \
    --skip-build \
    --skip-execute
```

### Exit Codes

The `run` command uses different exit codes to signal different outcomes:

- **0:** Complete success
- **1:** Partial failure (some instances failed but pipeline continued)
- **2:** Critical error (pipeline stopped)
- **3:** Hash mismatch (YAML changed, regeneration required)

**Example handling exit codes:**
```bash
python3 -m tools.experiment_framework run --app 05-cholesky --set regression

if [ $? -eq 0 ]; then
    echo "All experiments succeeded!"
elif [ $? -eq 1 ]; then
    echo "Some instances failed, check results"
elif [ $? -eq 3 ]; then
    echo "YAML changed, rerun without --skip-generate"
else
    echo "Critical error occurred"
fi
```

### Partial Failure Handling

The framework is resilient to partial failures:

- **Phase 2 (Build):** If some instances fail to build, the pipeline continues with successful ones
- **Phase 3 (Execute):** If some instances fail to execute, the pipeline continues
- **Phase 4 (Visualize):** Always runs if at least some results exist

**Example with partial failure:**
```
Phase 2: Building instances...
  ✓ Built 40/42 instances
  ⚠ Warning: 2 instance(s) failed to build
Phase 3: Executing instances...
  ✓ Executed 38 instances
  ⚠ Warning: 2 instance(s) had errors
Phase 4: Generating visualizations...
  ✓ Generated 5 plot(s)
```

### Workflow Examples

#### Typical Development Workflow

```bash
# First run: generate, build, execute, visualize
python3 -m tools.experiment_framework run \
    --app myapp \
    --set quick_test

# YAML unchanged, rebuild and rerun
python3 -m tools.experiment_framework run \
    --app myapp \
    --set quick_test \
    --skip-generate

# Just update visualizations
python3 -m tools.experiment_framework run \
    --app myapp \
    --set quick_test \
    --skip-generate --skip-build --skip-execute
```

#### Production Run

```bash
# Full pipeline for comprehensive experiment set
python3 -m tools.experiment_framework run \
    --app 05-cholesky \
    --set regression \
    --verbose

# Check exit code
if [ $? -eq 0 ]; then
    # Archive results
    tar -czf results_$(date +%Y%m%d).tar.gz \
        applications/05-cholesky/experiment/results/
fi
```

### Backward Compatibility

The old `scripts/experiment` wrapper is still available but deprecated:

```bash
# Old way (still works, shows deprecation warning)
scripts/experiment run --app 05-cholesky --set regression

# New way (preferred)
python3 -m tools.experiment_framework run --app 05-cholesky --set regression
```

---

## Understanding Results

### Step 1: Locate Results File

```bash
cd /scratch/pedro.ciambra/repos/iara

# Find the most recent results
ls -lart applications/99-myapp/experiment/results/ | tail -5

# View the results file
cat applications/99-myapp/experiment/results/results_2026-01-14T12-34-56Z.json
```

### Step 2: Results JSON Structure

```json
{
  "schema_version": "1.0.0",

  "experiment": {
    "application": "99-myapp",
    "experiment_set": "quick_test",
    "timestamp": "2026-01-14T12:34:56Z",
    "git_commit": "abc12345",
    "yaml_hash": "2a3f5b7c..."
  },

  "instances": [
    {
      "name": "99-myapp_sequential_iterations_1_size_100",
      "parameters": {
        "iterations": 1,
        "scheduler": "sequential",
        "size": 100
      },
      "compilation": {
        "iara_opt_time_s": 1.234,
        "iara_opt_memory_bytes": 102400,
        "mlir_llvm_time_s": 0.567,
        "mlir_llvm_memory_bytes": 51200,
        "clang_time_s": 0.890,
        "clang_memory_bytes": 204800,
        "total_time_s": 2.691
      },
      "binary": {
        "total_size_bytes": 156234,
        "text_section_bytes": 98765,
        "data_section_bytes": 45678,
        "bss_section_bytes": 11791
      }
    }
    // ... more instances
  ],

  "failed_instances": [
    {
      "name": "99-myapp_omp-for_iterations_5_size_5000",
      "attempts": 2,
      "errors": [
        {
          "attempt": 1,
          "phase": "build",
          "step": "iara-opt",
          "message": "undefined reference to 'omp_parallel'",
          "timestamp": "2026-01-14T12:34:58Z"
        }
      ]
    }
  ],

  "statistics": {
    "total_instances": 4,
    "successful_count": 3,
    "failed_count": 1,
    "success_rate": 0.75
  }
}
```

### Step 3: Parse Results Programmatically

```python
import json
from pathlib import Path

# Load results
results_file = Path("applications/99-myapp/experiment/results/results_*.json")
with open(results_file) as f:
    data = json.load(f)

# Access compilation times
for instance in data["instances"]:
    name = instance["name"]
    total_time = instance["compilation"]["total_time_s"]
    binary_size = instance["binary"]["total_size_bytes"]
    print(f"{name}: {total_time:.2f}s, {binary_size} bytes")

# Check success rate
success_rate = data["statistics"]["success_rate"]
print(f"Success rate: {success_rate*100:.1f}%")

# Find slowest build
slowest = max(data["instances"],
              key=lambda x: x["compilation"]["total_time_s"])
print(f"Slowest: {slowest['name']} ({slowest['compilation']['total_time_s']:.2f}s)")
```

---

## Common Tasks

### Task 1: Compare Two Experiment Sets

```bash
# Generate both sets
python3 -m tools.experiment_framework generate --app 99-myapp --set quick_test
python3 -m tools.experiment_framework generate --app 99-myapp --set baseline

# Build both
python3 -m tools.experiment_framework build --app 99-myapp --set quick_test
python3 -m tools.experiment_framework build --app 99-myapp --set baseline

# Compare results
python3 << 'EOF'
import json
from pathlib import Path

for set_name in ["quick_test", "baseline"]:
    results_path = Path(f"applications/99-myapp/experiment/results/results_*.json")
    # Find matching file...
    with open(results_path) as f:
        data = json.load(f)

    stats = data["statistics"]
    print(f"{set_name}:")
    print(f"  Total: {stats['total_instances']}")
    print(f"  Success: {stats['successful_count']}/{stats['total_instances']}")

    times = [i["compilation"]["total_time_s"]
             for i in data["instances"]]
    print(f"  Avg time: {sum(times)/len(times):.2f}s")
EOF
```

### Task 2: Find Slowest Tests

```bash
python3 << 'EOF'
import json
from pathlib import Path

results_file = Path("applications/99-myapp/experiment/results/results_*.json").glob("*")[0]
with open(results_file) as f:
    data = json.load(f)

# Sort by total compilation time
sorted_instances = sorted(
    data["instances"],
    key=lambda x: x["compilation"]["total_time_s"],
    reverse=True
)

print("Slowest 5 builds:")
for i, instance in enumerate(sorted_instances[:5], 1):
    print(f"{i}. {instance['name']}: {instance['compilation']['total_time_s']:.2f}s")
EOF
```

### Task 3: Analyze Parameter Impact

```bash
python3 << 'EOF'
import json
from pathlib import Path

results_file = Path("applications/99-myapp/experiment/results/results_*.json").glob("*")[0]
with open(results_file) as f:
    data = json.load(f)

# How does size affect compilation time?
by_size = {}
for instance in data["instances"]:
    size = instance["parameters"]["size"]
    time = instance["compilation"]["total_time_s"]
    if size not in by_size:
        by_size[size] = []
    by_size[size].append(time)

print("Compilation time by size:")
for size in sorted(by_size.keys()):
    times = by_size[size]
    avg = sum(times) / len(times)
    print(f"  Size {size}: {avg:.2f}s (avg of {len(times)} tests)")
EOF
```

### Task 4: Update and Regenerate

If you modify `experiments.yaml`:

```yaml
# OLD
- name: "quick_test"
  parameters:
    size: [100, 500]
    iterations: [1, 5]
    scheduler: ["sequential"]

# NEW - add more schedulers
- name: "quick_test"
  parameters:
    size: [100, 500]
    iterations: [1, 5]
    scheduler: ["sequential", "omp-for"]  # Added
```

```bash
# Generate again - framework detects change
python3 -m tools.experiment_framework generate \
    --app 99-myapp \
    --set quick_test

# Output:
# ⚠ YAML file changed! Regenerating...
# ✓ Parameter combinations generated: 8 instances (was 4)
# ✓ CMakeLists.txt updated
```

### Task 5: Troubleshoot Build Failures

```bash
# Check the error details
python3 << 'EOF'
import json
from pathlib import Path

results_file = Path("applications/99-myapp/experiment/results/results_*.json").glob("*")[0]
with open(results_file) as f:
    data = json.load(f)

if data["failed_instances"]:
    print("Failed builds:")
    for failed in data["failed_instances"]:
        print(f"\n{failed['name']}:")
        print(f"  Attempts: {failed['attempts']}")
        for error in failed["errors"]:
            print(f"  Attempt {error['attempt']}: {error['message']}")
else:
    print("All builds successful!")
EOF
```

---

## Architecture Overview

### How It Works

```
1. You Create YAML
   applications/99-myapp/experiment/experiments.yaml
   │
   ↓

2. Framework Generates
   python3 -m tools.experiment_framework generate --app 99-myapp --set quick_test
   │
   └─→ CMakeLists.txt with test instances
   └─→ Hash file for change detection
   │
   ↓

3. Framework Builds
   python3 -m tools.experiment_framework build --app 99-myapp --set quick_test
   │
   ├─→ CMake configures each instance
   ├─→ Measures compilation time (3 phases)
   ├─→ Measures binary size
   ├─→ Tracks errors and retries
   │
   ↓

4. Results Available
   applications/99-myapp/experiment/results/results_*.json
   │
   └─→ Complete build information
   └─→ Timing for each phase
   └─→ Error tracking
   └─→ Statistics
```

### What Phase 1 & 2 Provide

**Phase 1: Configuration**
- ✓ YAML file validation
- ✓ Parameter combination generation
- ✓ CMakeLists.txt generation
- ✓ Change detection via hashing

**Phase 2: Build System**
- ✓ Sequential instance building
- ✓ 3-phase compilation timing
- ✓ Binary size extraction
- ✓ Error tracking with retries
- ✓ JSON results output

### What's Coming

**Phase 3: Measurement Collection**
- Execute instances and measure runtime performance
- Collect output metrics

**Phase 4: Visualization**
- Generate plots and analysis
- Create Jupyter notebooks

**Phase 5: CLI Integration**
- Complete command-line interface
- Orchestration commands

---

## Tips & Tricks

### Tip 1: Small Test First
Always start with a small experiment set:
```yaml
- name: "smoke_test"
  parameters:
    size: [100]
    scheduler: ["sequential"]
```

### Tip 2: Check Constraints
Test constraints are working:
```bash
python3 << 'EOF'
from tools.experiment_framework import load_experiments_yaml, get_parameter_combinations
from pathlib import Path

config = load_experiments_yaml(Path("applications/99-myapp/experiment/experiments.yaml"))
combos = get_parameter_combinations(config, "quick_test")
print(f"Generated {len(combos)} combinations")

# See what was generated
for combo in combos[:3]:
    print(f"  {combo}")
EOF
```

### Tip 3: Understand Instance Naming
The instance name encodes all non-computed parameters:
```
99-myapp_sequential_iterations_1_size_100
 ^       ^                ^        ^  ^
 |       |                |        |  value
 |       |                |        parameter name
 |       |                value
 |       |
 |       parameter value (scheduler)
 application name
```

### Tip 4: Version Your Experiments
Use git to track YAML changes:
```bash
cd applications/99-myapp/experiment
git add experiments.yaml
git commit -m "Add new experiment set: scaling_study"

# Later, see what changed
git log --oneline experiments.yaml
git show HEAD:experiments.yaml  # View previous version
```

### Tip 5: Run Multiple Sets
```bash
for set in quick_test baseline scaling; do
    python3 -m tools.experiment_framework generate --app 99-myapp --set $set
    python3 -m tools.experiment_framework build --app 99-myapp --set $set
done
```

---

## Troubleshooting

### Problem: "Unknown CMake command iara_add_test_instance"
**Cause:** CMakeLists.txt generation issue
**Solution:** Ensure you're using the latest framework
```bash
cd /scratch/pedro.ciambra/repos/iara
git pull
python3 -m tools.experiment_framework generate --app 99-myapp --set quick_test
```

### Problem: "YAML validation error"
**Cause:** Invalid YAML syntax or missing required fields
**Solution:** Check the error message and fix the YAML
```bash
python3 << 'EOF'
from tools.experiment_framework import load_experiments_yaml
from pathlib import Path

try:
    config = load_experiments_yaml(
        Path("applications/99-myapp/experiment/experiments.yaml")
    )
    print("✓ YAML is valid")
except Exception as e:
    print(f"✗ Error: {e}")
EOF
```

### Problem: "No such file or directory"
**Cause:** Application directory structure is wrong
**Solution:** Create required directories
```bash
mkdir -p applications/99-myapp/experiment
mkdir -p applications/99-myapp/code
```

### Problem: Build fails on all instances
**Cause:** Application code has issues
**Solution:** Check the error in the JSON results
```bash
cat applications/99-myapp/experiment/results/results_*.json | python3 -m json.tool
```

---

## Next Steps

Once you're comfortable with Phase 1 & 2:

1. **Run multiple experiment sets** on your application
2. **Analyze the JSON results** to find performance patterns
3. **Wait for Phase 3** to collect actual runtime measurements
4. **Use Phase 4 visualizations** to explore results

For now, you can:
- ✓ Define experiments in YAML
- ✓ Generate test configurations
- ✓ Build all test combinations
- ✓ Get compilation timing and binary size
- ✓ Track errors and retries
- ✓ Export results as JSON

---

## Example: Complete Workflow

```bash
cd /scratch/pedro.ciambra/repos/iara

# 1. Create application structure
mkdir -p applications/my-example/{experiment,code}

# 2. Create experiments.yaml (see Section "Defining Experiments" above)
cat > applications/my-example/experiment/experiments.yaml << 'EOF'
application:
  name: "my-example"
  entry: "my-example"
  source_dir: "../code"
  build:
    type: "cmake"
    extra_linker_args: ["-lm"]

parameters:
  - name: "size"
    type: "int"
  - name: "scheduler"
    type: "string"

experiment_sets:
  - name: "test"
    parameters:
      size: [100, 500]
      scheduler: ["sequential"]
EOF

# 3. Generate test configuration
python3 -m tools.experiment_framework generate --app my-example --set test

# 4. View what was generated
cat applications/my-example/experiment/CMakeLists.txt

# 5. Build (requires code and full IaRa environment)
# python3 -m tools.experiment_framework build --app my-example --set test

# 6. Check results
# cat applications/my-example/experiment/results/results_*.json
```

---

**For questions or issues, see the troubleshooting section or check the framework documentation in `/home/pedro.ciambra/.claude/plans/agent_workspace/`**
