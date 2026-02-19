# Implementation Plan: New Test Generation Architecture

## Summary

Implement a three-step test generation and execution system to replace the current CMake-based auto-discovery system:

1. **generate_experiments.py** - Python script that processes all `experiments.yaml` files and generates one `CMakeLists.txt` per application containing all test instances
2. **CMake Reconfiguration** - Run CMake with the generated files, creating build directories with proper naming and test targets
3. **CTest Execution** - Run all tests via CTest with proper dependencies (run-* depends on build-*)

## Key Design Decisions

### Instance Naming Convention

```
<XX-experiment-name>_<scheduler>_<param-name-1>_<param-value-1>_<param-name-2>_<param-value-2>_...
```

Example: `05-cholesky_vf-omp_matrix-size_2048_num-blocks_4`

### Test Naming Convention

Test names follow the same pattern as instance names, with a prefix:

```
build-<XX-experiment-name>_<scheduler>_<param-name-1>_<param-value-1>_<param-name-2>_<param-value-2>_...

run-<XX-experiment-name>_<scheduler>_<param-name-1>_<param-value-1>_<param-name-2>_<param-value-2>_...
```

Examples:
- `build-05-cholesky_vf-omp_matrix-size_2048_num-blocks_4`
- `run-05-cholesky_vf-omp_matrix-size_2048_num-blocks_4`

**Rules:**
- All experiments have numbered names and the number should always be present, both as identifiers and in file names. An application name that is not preceded by a number is an error.
- Extract the application name from the experiments.yaml (e.g. "05-cholesky")
- Scheduler parameter comes immediately after the application name - **ONLY the scheduler value is shown, not the name**
- Remaining parameters in the order they appear in the YAML
- Parameters separated by underscores
- Both parameter names AND values are included for all parameters **except** the scheduler (which shows only the value)

### Build Directory Structure

```
iara/build_experiments/
├── <experiment-name>/
│   ├── <experiment-set-1>/
│   │   ├── <instance-name-1>/
│   │   │   ├── CMakeCache.txt
│   │   │   ├── CMakeFiles/
│   │   │   ├── Makefile
│   │   │   └── (compiled binaries and objects)
│   │   ├── <instance-name-2>/
│   │   └── ...
│   ├── <experiment-set-2>/
│   │   └── ...
├── <experiment-name-2>/
│   └── ...
└── ...
```

Each instance gets a directory at `build_experiments/<experiment-name>/<experiment-set>/<instance-name>/` where:
- `<experiment-name>` is the application name (e.g., "05-cholesky")
- `<experiment-set>` is the experiment set name from the YAML (e.g., "regression_test", "smoke", "full")
- `<instance-name>` is the full instance name following the naming convention

### Generated CMakeLists.txt Per Application

**File location:** `applications/<XX-name>/generated/CMakeLists.txt` (auto-generated)

- `<experiment_set>` is the name of the set as defined by the yaml

**Contains:**
- One `iara_add_test_instance()` call per instance
- All parameter and configuration information for that instance
- Uses CMake macro to delegate to existing `iara_add_application()` function


**Scope:**
- Includes ALL test instances for ALL `generate_tests: true` experiment sets in that application
- Not limited to regression_test set

### Test Target Structure

For each instance, two CTest targets:

1. **build-<instance-name>** - Compilation target
   - Custom target that depends on executable generation
   - Calls `iara_add_application()` to perform full compilation pipeline
   - Success = executable created
   - Failure = any compilation step fails

2. **run-<instance-name>** - Execution target
   - CTest test that executes the compiled binary
   - Depends on `build-<instance-name>`
   - Success = executable returns 0
   - Failure = executable returns non-zero or times out

### Integration with Existing Framework

**Reuse:**
- `iara_add_application()` function from `cmake/IaRaApplications.cmake` (already handles full compilation pipeline)
- `iara_map_scheduler()` for scheduler mapping
- `iara_runtime_sources()` for runtime library selection
- Parameter/constraint validation logic from experiment-framework

**Preserve:**
- All scheduler-specific compilation flags and defines
- Build configuration from experiments.yaml (extra_linker_args, defines)
- Runtime source selection based on scheduler type
- Parameter name-to-environment-variable mapping

## Implementation Details

### Phase 1: Create generate_experiments.py

**Location:** `scripts/generate_experiments.py`

**Responsibilities:**
1. Discover all `applications/*/experiment/experiments.yaml` files
2. For each YAML file:
   - Parse configuration
   - For each `experiment_set` with `generate_tests: true`:
     - Extract parameter definitions
     - Generate all valid parameter combinations (respecting constraints)
     - Compute derived parameters if present
     - Generate CMakeLists.txt code for each instance
3. Write `applications/<XX-name>/experiment/CMakeLists.txt`

**Algorithm:**
```
for each applications/*/experiment/experiments.yaml:
    config = parse_yaml()
    app_name = config.application.entry  # e.g., "05-cholesky"

    for each experiment_set where generate_tests == true:
        param_names = list of parameter names in definition order
        param_choices = list of parameter value lists

        combinations = cartesian_product(*param_choices)
        combinations = filter_by_constraints(combinations)

        for each combo in combinations:
            instance_name = generate_instance_name(app_name, combo)
            cmake_code += generate_cmake_call(instance_name, combo, config)

    write(applications/{app_name}/experiment/CMakeLists.txt, cmake_code)
```

**Key Functions:**
- `find_yaml_files()` → List of experiment YAML paths
- `parse_experiments_yaml(path)` → Configuration dict
- `generate_instance_name(app_name, params)` → String name
- `apply_constraints(combinations, constraints, computed_params)` → Filtered combinations
- `generate_cmake_code(instances, config)` → CMakeLists.txt string
- `write_cmake_file(path, content)` → Write to disk

### Phase 2: Create CMake Test Framework Function

**Location:** Create new function in `cmake/IaRaApplications.cmake`

**Function Name:** `iara_add_test_instance()`

**Purpose:** Wrap `iara_add_application()` to create proper test targets

**Interface:**
```cmake
iara_add_test_instance(
    NAME "05-cholesky_vf-omp_matrix-size_2048_num-blocks_2"
    EXPERIMENT_SET "regression_test"
    APPLICATION_DIR "applications/cholesky"
    ENTRY "05-cholesky"
    SCHEDULER "vf-omp"
    PARAMETERS "matrix_size=2048" "num_blocks=2"
    BUILD_DIR "build_experiments/05-cholesky/regression_test/05-cholesky_vf-omp_matrix-size_2048_num-blocks_2"
    DEFINES "VERBOSE;MATRIX_SIZE=2048;NUM_BLOCKS=2"
    LINKER_ARGS "-lopenblas"
)
```

**Implementation:**
1. Call `iara_add_application()` with extracted parameters
2. Create `build-<name>` custom target for the executable
3. Create `run-<name>` CTest test for execution
4. Set up test dependency chain (run-* depends on build-*)

### Phase 3: Update Root CMakeLists.txt

**Separate Build Paths:**
- Add `add_subdirectory(src)` for building iara-opt (compiler toolchain) - always included
- Add conditional `add_subdirectory(experiment)` for building tests - only when `IARA_BUILD_TESTS=ON`
- Ensure iara-opt is built independently of test generation

**Filtering Experiments:**
- Add CMake cache variable `IARA_EXPERIMENTS_FILTER` (default: empty = all)
- Add CMake cache variable `IARA_EXPERIMENT_SETS_FILTER` (default: empty = all)
- Pass these to `generate_experiments.py` script
- Script should filter by application name pattern and experiment set name pattern before generating CMakeLists.txt files

**Changes:**
- When `IARA_BUILD_TESTS=ON`, call `generate_experiments.py` with filter variables
- Iterate through applications and include their generated `experiment/CMakeLists.txt` files
- Keep existing test auto-discovery for backward compatibility (disabled by default)

### Phase 4: Update build_tests.sh

**Orchestration:**
```bash
#!/bin/bash

# Step 1: Generate CMakeLists.txt files from experiments.yaml
# Can be filtered via environment variables:
#   IARA_EXPERIMENTS_FILTER - filter applications (e.g., "05-cholesky")
#   IARA_EXPERIMENT_SETS_FILTER - filter experiment sets (e.g., "regression_test")
python3 scripts/generate_experiments.py

# Step 2: Configure CMake with generated files
cmake -B build_experiments \
    -DCMAKE_BUILD_TYPE=Release \
    -DIARA_BUILD_TESTS=ON \
    .

# Step 3: Run all tests
ctest --build-dir build_experiments --build-config Release -V
```

**Filtering Examples:**
```bash
# Build only cholesky tests
export IARA_EXPERIMENTS_FILTER="05-cholesky"
./build_tests.sh

# Build only regression_test experiment sets (for all applications)
export IARA_EXPERIMENT_SETS_FILTER="regression_test"
./build_tests.sh

# Build only cholesky regression tests
export IARA_EXPERIMENTS_FILTER="05-cholesky"
export IARA_EXPERIMENT_SETS_FILTER="regression_test"
./build_tests.sh
```

## Critical Implementation Details

### Parameter Extraction from YAML

For example with cholesky experiments.yaml:
```yaml
parameters:
  - name: matrix_size
    type: int
  - name: num_blocks
    type: int
  - name: scheduler
    type: str
    choices: [sequential, vf-omp]
```

Instance for `matrix_size=2048, num_blocks=4, scheduler=vf-omp`:
- Name: `05-cholesky_vf-omp_matrix-size_2048_num-blocks_4`
- CMake defines: `MATRIX_SIZE=2048 NUM_BLOCKS=4`
- Environment vars: `MATRIX_SIZE=2048 NUM_BLOCKS=4`

### Computed Parameters

If the YAML specifies:
```yaml
computed_parameters:
  - name: block_size
    expression: "matrix_size // num_blocks"
```

Then in the CMakeLists.txt:
```cmake
iara_add_test_instance(
    ...
    COMPUTED_PARAMS "block_size=512"
    ...
)
```

### Test Instance Information Preservation

The generated CMakeLists.txt must encode all the information that was previously scattered:

From experiments.yaml:
- `application.entry` → Used as ENTRY
- `application.sources` → Used to find source files
- `application.build.extra_linker_args` → Passed to LINKER_ARGS
- `application.build.defines` → Passed to DEFINES
- Parameter values → Converted to environment variables and defines

From experiment_set:
- Only instances from sets with `generate_tests: true`
- All parameter combinations that pass constraints

### CMake Macro for Auto-Generated Files

The generated CMakeLists.txt should NOT duplicate the compilation logic. Instead:

```cmake
# Auto-generated by generate_experiments.py
include(${CMAKE_SOURCE_DIR}/cmake/IaRaApplications.cmake)

iara_add_test_instance(
    NAME "05-cholesky_vf-omp_matrix-size_2048_num-blocks_2"
    EXPERIMENT_SET "regression_test"
    APPLICATION_DIR "applications/cholesky"
    ENTRY "05-cholesky"
    SCHEDULER "vf-omp"
    PARAMETERS "matrix_size=2048" "num_blocks=2"
    BUILD_DIR "build_experiments/05-cholesky/regression_test/05-cholesky_vf-omp_matrix-size_2048_num-blocks_2"
    DEFINES "VERBOSE;MATRIX_SIZE=2048;NUM_BLOCKS=2"
    LINKER_ARGS "-lopenblas"
)

# More instances...
```

The `iara_add_test_instance()` function delegates all compilation work to `iara_add_application()`.

## Data Flow Summary

```
applications/*/experiment/experiments.yaml
           ↓
[generate_experiments.py]
  - Parse YAML
  - Generate combinations
  - Apply constraints
  - Generate CMake code
           ↓
applications/*/experiment/CMakeLists.txt (auto-generated)
           ↓
[cmake -B build_experiments ...]
  - Include generated CMakeLists.txt files
  - Call iara_add_test_instance() for each instance
  - Call iara_add_application() for each (via iara_add_test_instance)
           ↓
build_experiments/<experiment-name>/<experiment-set>/<instance-name>/
  - CMake configuration files
  - Makefiles
  - Compiled objects and binaries
           ↓
CTest test targets:
  - build-<instance-name>
  - run-<instance-name> (depends on build-*)
           ↓
[ctest --build-dir build_experiments ...]
  - Execute tests in dependency order
  - Report pass/fail
```

## Testing Strategy

1. **Test generate_experiments.py in isolation:**
   - Parse a simple YAML file
   - Verify instance name generation
   - Verify parameter extraction
   - Verify constraint validation
   - Verify CMakeLists.txt generation

2. **Test CMake integration:**
   - Configure with generated files
   - Verify build directories are created
   - Verify targets are defined
   - Verify dependencies are set

3. **Test end-to-end:**
   - Run build_tests.sh for regression_test set
   - Verify all tests build successfully
   - Verify all tests execute and report correct results
   - Verify failures are properly detected

## Files to Create/Modify

### Create:
- `scripts/generate_experiments.py` - Main generation script
- `IMPLEMENTATION_PLAN.md` - This file (for documentation)

### Modify:
- `cmake/IaRaApplications.cmake` - Add `iara_add_test_instance()` function
- `CMakeLists.txt` (root) - Add logic to include generated CMakeLists.txt files when IARA_BUILD_TESTS=ON
- `scripts/build_tests.sh` - Update to execute all three steps

### Auto-Generated:
- `applications/*/experiment/CMakeLists.txt` - One per application (generated by generate_experiments.py)

## Success Criteria

1. ✅ `generate_experiments.py` successfully parses all experiments.yaml files
2. ✅ Instance names follow the specified convention
3. ✅ CMakeLists.txt files are generated correctly
4. ✅ CMake configuration creates all build directories
5. ✅ All build-* and run-* targets exist and are properly named
6. ✅ run-* targets depend on build-* targets
7. ✅ All compilation steps work (topology generation, iara-opt, lowering, compilation)
8. ✅ All tests can execute via `ctest`
9. ✅ Test results are accurate (pass/fail reporting is correct)
10. ✅ `build_tests.sh` orchestrates all three steps correctly
11. ✅ Script works from any directory with IARA_DIR environment variable

## Next Steps

After approval of this plan:
1. Implement generate_experiments.py
2. Add iara_add_test_instance() to cmake/IaRaApplications.cmake
3. Update CMakeLists.txt to integrate generated files
4. Update build_tests.sh
5. Test building blocks one by one as per user request
