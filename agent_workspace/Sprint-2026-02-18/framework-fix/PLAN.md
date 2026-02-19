# Plan: Fix Experiment Framework

## Goal

Remove application-specific logic from the framework, standardize parameter naming, consolidate CLI, add regression command.

## Design Decision: ALL_CAPS Parameter Names

**Canonical parameter names are ALL_CAPS**, matching the C define / env var / CMake variable name. This eliminates the `export_as` indirection and the `parameter_metadata` section entirely.

A `label` field in each parameter definition provides the human-readable name for display and instance naming.

Example:
```yaml
parameters:
  - name: NUM_KERNEL_SUPPORT
    type: int
    label: "Kernel Support"
    description: "Support size for convolution kernels"
```

This means the framework passes `NUM_KERNEL_SUPPORT=8` everywhere:
- As `-DNUM_KERNEL_SUPPORT=8` compiler flag
- As `NUM_KERNEL_SUPPORT=8` env var for codegen.sh
- As CMake variable
- No mapping, no `export_as`, no `parameter_metadata`

Instance names use the label (lowercased, sanitized): `13-degridder_vf-omp_kernel-support_8_...`

The `scheduler` parameter is special-cased (it's always lowercase and maps to framework behavior, not a C define).

---

## Changes

### 1. YAML Schema Changes

**Remove `parameter_metadata` section entirely** from all three YAML files.

**Parameter definitions gain a `label` field** (optional, defaults to titlecase of name).

**All parameter `name` fields become ALL_CAPS** (except `scheduler` which stays lowercase).

#### 1a. `applications/13-degridder/experiment/experiments.yaml`

Parameters become:
```yaml
parameters:
  - name: SIZE
    type: str
    label: "Dataset Size"
  - name: NUM_CORES
    type: int
    label: "CPU Cores"
  - name: NUM_CHUNK
    type: int
    label: "Chunks"
  - name: NUM_KERNEL_SUPPORT
    type: int
    label: "Kernel Support"
  - name: scheduler
    type: str
    label: "Scheduler"
```

Computed parameters (NEW — replaces hardcoded `_add_degridder_defines`):
```yaml
computed_parameters:
  - name: GRID_SIZE
    type: int
    expression: "{'small': 2560, 'medium': 3840, 'large': 5120}[SIZE]"
  - name: NUM_VISIBILITIES
    type: int
    expression: "{'small': 3924480, 'medium': 5886720, 'large': 7848960}[SIZE]"
  - name: NUM_SCENARIO
    type: int
    expression: "{'small': 1, 'medium': 2, 'large': 3}[SIZE]"
  - name: NUMBER_SAMPLE_IN_KERNEL
    type: int
    expression: "(NUM_KERNEL_SUPPORT + 1) * 16 * (NUM_KERNEL_SUPPORT + 1) * 16"
  - name: TOTAL_KERNELS_SAMPLES
    type: int
    expression: "17 * NUMBER_SAMPLE_IN_KERNEL"
  - name: NUM_VISIB_D_N_CHUNK
    type: int
    expression: "NUM_VISIBILITIES // NUM_CHUNK"
```

Experiment sets reference ALL_CAPS names:
```yaml
experiment_sets:
  - name: regression
    parameters:
      SIZE: [small]
      NUM_CORES: [1]
      NUM_CHUNK: [1]
      NUM_KERNEL_SUPPORT: [8]
      scheduler: [vf-omp]
```

Remove `parameter_metadata` section.

#### 1b. `applications/05-cholesky/experiment/experiments.yaml`

Parameters become:
```yaml
parameters:
  - name: MATRIX_SIZE
    type: int
    label: "Matrix Size"
  - name: NUM_BLOCKS
    type: int
    label: "Number of Blocks"
  - name: scheduler
    type: str
    label: "Scheduler"

computed_parameters:
  - name: BLOCK_SIZE
    type: int
    expression: "MATRIX_SIZE // NUM_BLOCKS"
    label: "Block Size"
```

Experiment sets reference ALL_CAPS:
```yaml
experiment_sets:
  - name: regression
    parameters:
      MATRIX_SIZE: [2048, 4096]
      NUM_BLOCKS: [1, 2, 4]
      scheduler: [sequential, omp-for, omp-task, enkits-task, vf-sequential, vf-omp, vf-enkits]
```

Constraints reference ALL_CAPS:
```yaml
constraints:
  - expression: "MATRIX_SIZE % NUM_BLOCKS == 0"
  - expression: "NUM_BLOCKS >= 1"
  - expression: "MATRIX_SIZE > 0"
  - expression: "BLOCK_SIZE >= 32"
```

Remove `parameter_metadata` section.

#### 1c. `applications/08-sift/experiment/experiments.yaml`

Parameters become:
```yaml
parameters:
  - name: IMAGE_WIDTH
    type: int
    label: "Image Width"
  - name: IMAGE_HEIGHT
    type: int
    label: "Image Height"
  - name: scheduler
    type: str
    label: "Scheduler"
```

Remove `parameter_metadata` section.

### 2. Generator Changes

**File: `tools/experiment_framework/generator.py`**

- **Delete** `_add_degridder_defines()` function
- **Delete** `generate_topology_mlir_if_needed()` function
- **Delete** the `if 'size' in params` call site
- **Simplify** `generate_cmake_instance()`:
  - Remove the `parameter_metadata` / `export_as` lookup
  - The define name IS the parameter name (already ALL_CAPS)
  - `scheduler` is excluded from DEFINES (handled separately as SCHEDULER_IARA etc.)
  - Computed parameters are included in DEFINES (they're in the params dict)
- **Update** `generate_instance_name()`:
  - Use `label` field from parameter definitions for instance naming (lowercased)
  - Fall back to lowercase parameter name if no label

### 3. Config Changes

**File: `tools/experiment_framework/config.py`**

- **Remove** references to `parameter_metadata` from schema validation
  - Remove from `optional_keys` dict
- No changes to `get_parameter_combinations()` — it already works with any key names

### 4. CLI Consolidation

**File: `tools/experiment_framework/cli.py`**

- Add `repetitions` and `timeout` optional parameters to `run_execute()`
- Replace inline `build` command body with call to `run_build()`
- Replace inline `execute` command body with call to `run_execute()`
- Replace inline `visualize` command body with call to `run_visualize()`
- Each command section becomes ~8 lines (path setup + helper call)

### 5. Codegen Script Updates

**File: `applications/05-cholesky/codegen.sh`**

Change env var reads from `$NUM_BLOCKS` / `$MATRIX_SIZE` (no change needed — already ALL_CAPS).

**File: `applications/13-degridder/codegen.sh`**

Change `$SIZE` → already ALL_CAPS. Change `$NUM_KERNEL_SUPPORT` → already ALL_CAPS. Change `$NUM_CHUNK` → already ALL_CAPS. No changes needed — codegen.sh already reads ALL_CAPS env vars.

**File: `applications/13-degridder/generate_topology.py`** — verify it reads `sys.argv`, not env vars. If so, codegen.sh passes the values.

### 6. Add `regression` CLI Command

**File: `tools/experiment_framework/cli.py`**

Add a new `regression` subcommand:

```
python3 -m tools.experiment_framework regression
```

Behavior:
1. Scan `applications/` for directories matching numbered pattern (03-*, 04-*, etc.)
2. For each that has `experiment/experiments.yaml`:
   - Find experiment sets with `generate_tests: true`
   - Run generate → build for each
3. Run all generated tests via `ctest` in the build directory
4. Report summary (passed/failed/skipped)

This covers unit tests (03-12) and any experiment set marked as regression-friendly.

---

## Files Modified

| File | Summary |
|------|---------|
| `applications/13-degridder/experiment/experiments.yaml` | ALL_CAPS params, add computed_parameters for derived constants, remove parameter_metadata |
| `applications/05-cholesky/experiment/experiments.yaml` | ALL_CAPS params, remove parameter_metadata |
| `applications/08-sift/experiment/experiments.yaml` | ALL_CAPS params, remove parameter_metadata |
| `tools/experiment_framework/generator.py` | Remove degridder-specific functions, simplify DEFINES generation |
| `tools/experiment_framework/config.py` | Remove parameter_metadata from schema validation |
| `tools/experiment_framework/cli.py` | Consolidate commands, add `regression` command |

## Files NOT Modified

| File | Reason |
|------|--------|
| `cmake/IaRaApplications.cmake` | Already handles everything correctly |
| `applications/05-cholesky/codegen.sh` | Already reads ALL_CAPS env vars |
| `applications/13-degridder/codegen.sh` | Already reads ALL_CAPS env vars |
| `tools/experiment_framework/builder.py` | No changes needed |
| `tools/experiment_framework/collector.py` | No changes needed |
| `tools/experiment_framework/visualizer.py` | No changes needed (field names in Vega-Lite specs will use ALL_CAPS but that's cosmetic) |

## Verification

1. Generate degridder CMakeLists.txt for `debug-sequential` set:
   - Verify DEFINES: `GRID_SIZE=2560`, `NUM_VISIBILITIES=3924480`, etc.
   - No duplicates, no degridder-specific code in generator.py
2. Generate cholesky CMakeLists.txt for `regression` set:
   - Verify DEFINES: `MATRIX_SIZE=2048`, `NUM_BLOCKS=1`, `BLOCK_SIZE=2048`
   - Constraints filter correctly (BLOCK_SIZE >= 32)
3. Generate SIFT CMakeLists.txt for `regression` set:
   - Verify DEFINES: `IMAGE_WIDTH=800`, `IMAGE_HEIGHT=640`
4. Run `regression` command — should discover and test all unit applications
5. Verify `run` vs individual commands produce identical results
