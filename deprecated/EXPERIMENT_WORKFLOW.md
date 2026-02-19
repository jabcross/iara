# Cholesky Experiment Workflow

This document describes the complete workflow for running Cholesky decomposition experiments using the IaRa experiment framework.

## Overview

The experiment framework provides end-to-end automation for:
1. **Configuration Management**: Define experiments, parameters, measurements in YAML
2. **Test Generation**: Auto-generate test instances from parameter combinations
3. **Build & Execution**: Compile and run tests with proper configuration
4. **Measurement Collection**: Type-safe extraction of results using regex parsers
5. **Visualization**: Generate plots for performance analysis

## Architecture

```
experiments.yaml
    ↓
generate_test_instances.py  → test/Iara/05-cholesky-*/parameters.sh
    ↓
generate_cmakelists.py      → test/Iara/05-cholesky-*/CMakeLists.txt
    ↓
run_experiments.py          → CMake build + test execution
    ↓
experiment/cholesky/results/results_*.csv
    ↓
visualize_results.py        → plots/
```

## Quick Start

### Step 1: Configure Experiments

Edit `applications/cholesky/experiments.yaml` to define:
- **Parameters**: matrix_size, num_blocks, scheduler
- **Measurements**: compute_time, wall_time, memory, etc.
- **Experiment Sets**: dev, baseline, iara, scaling, full
- **Visualization**: plots to generate

### Step 2: Generate Test Instances

```bash
cd /scratch/pedro.ciambra/repos/iara
python3 applications/cholesky/generate_test_instances.py
python3 applications/cholesky/generate_cmakelists.py
```

This creates:
- `test/Iara/05-cholesky-<params>/parameters.sh` - Runtime parameters
- `test/Iara/05-cholesky-<params>/CMakeLists.txt` - Build configuration

### Step 3: Run Experiments

```bash
python3 applications/cholesky/run_experiments.py
```

This:
1. Configures CMake with `-DIARA_BUILD_TESTS=ON`
2. Builds all test instances
3. Executes each test with warmup and repetitions
4. Collects measurements using Spinner's regex parsers
5. Saves results to `experiment/cholesky/results/results_YYYYMMDD_HHMMSS.csv`

### Step 4: Visualize Results

```bash
# All plots, matplotlib backend
python3 applications/cholesky/visualize_results.py experiment/cholesky/results/results_*.csv

# Specific plots, plotly interactive
python3 applications/cholesky/visualize_results.py experiment/cholesky/results/results_*.csv \
  --backend plotly --plots runtime scaling memory

# Filter to specific matrix size
python3 applications/cholesky/visualize_results.py experiment/cholesky/results/results_*.csv \
  --matrix-size 2048
```

## Configuration Files

### experiments.yaml Structure

```yaml
application:
  name: cholesky
  build:
    defines: [VERBOSE, SCHEDULER_IARA]
    extra_linker_args: [-lopenblas]

parameters:
  - name: matrix_size
    type: int
  - name: num_blocks
    type: int
  - name: scheduler
    type: str
    choices: [sequential, omp-for, vf-omp, ...]

computed_parameters:
  - name: block_size
    expression: "matrix_size // num_blocks"

constraints:
  - expression: "matrix_size % num_blocks == 0"
  - expression: "block_size >= 32"

measurements:
  - name: compute_time
    type: float
    parser:
      type: regex
      pattern: 'Computation time:\s+(\d+\.?\d*)'
    unit: s

  - name: overhead_time
    type: float
    composite:
      components: [init_time, convert_time]
      operation: sum
    unit: s

experiment_sets:
  - name: dev
    parameters:
      matrix_size: [2048]
      num_blocks: [2, 4]
      scheduler: [sequential, vf-omp]
    generate_tests: true

execution:
  repetitions: 5
  warmup: 1
  timeout: 300
  environment:
    OMP_NUM_THREADS: "8"
```

### Runtime vs Build Configuration

**Runtime Parameters** (`parameters.sh`):
```bash
export MATRIX_SIZE=2048
export NUM_BLOCKS=2
export SCHEDULER=sequential
export BLOCK_SIZE=1024
```

**Build Configuration** (CMakeLists.txt):
```cmake
set(EXTRA_KERNEL_ARGS "-DVERBOSE -DSCHEDULER_IARA" CACHE STRING "...")
set(EXTRA_LINKER_ARGS "-lopenblas" CACHE STRING "...")
```

## Generated Files

### Test Instance Structure

```
test/Iara/05-cholesky-2048_2_sequential/
├── parameters.sh         # Runtime parameters
├── CMakeLists.txt        # Build configuration from YAML
└── build-sequential/     # CMake build directory
    ├── a.out             # Compiled executable
    └── schedule.o        # IaRa schedule (if applicable)
```

### Result CSV Format

```csv
test_name,scheduler,matrix_size,num_blocks,run_num,measurement,value,unit
05-cholesky-2048_2_sequential,sequential,2048,2,1,compute_time,0.045,s
05-cholesky-2048_2_sequential,sequential,2048,2,1,wall_time,0.050,s
05-cholesky-2048_2_sequential,sequential,2048,2,1,max_rss_mb,1024,MB
```

## Key Components

### generate_test_instances.py

Generates test directories with parameter combinations:
- Reads experiment sets from YAML
- Validates parameter combinations against constraints
- Computes derived parameters (e.g., block_size)
- Creates parameters.sh for each instance

### generate_cmakelists.py

Generates CMakeLists.txt for each test instance:
- Reads build configuration from YAML
- Sets EXTRA_KERNEL_ARGS (defines)
- Sets EXTRA_LINKER_ARGS (libraries)
- Uses CMake cache variables for propagation

### run_experiments.py

Executes all test instances:
1. **Build Phase**:
   - Configures CMake once for all tests
   - Builds all instances in parallel
   - Uses `iara-opt` for IaRa schedule generation

2. **Execution Phase**:
   - Finds compiled executables
   - Parses parameters.sh for each test
   - Runs warmup iterations (not recorded)
   - Runs repetitions with measurement collection
   - Extracts measurements using Spinner's regex parsers

3. **Output Phase**:
   - Aggregates results into CSV
   - One row per measurement per run
   - Saves to `experiment/cholesky/results/results_YYYYMMDD_HHMMSS.csv`

### visualize_results.py

Generates analysis plots from CSV results:
- **Runtime Comparison**: Bar chart of compute_time by scheduler
- **Scaling Analysis**: Line plot of compute_time vs num_blocks
- **Time Breakdown**: Stacked bar chart of init_time + convert_time + compute_time
- **Memory Usage**: Bar chart of max_rss_mb by scheduler
- **Summary Report**: Text file with statistics

## Workflow Example: Dev Set

### 1. Generate Test Instances

```bash
$ python3 applications/cholesky/generate_test_instances.py

Generating test instances to: test/Iara

Processing experiment set: dev
  Found 4 valid combinations
  Created: test/Iara/05-cholesky-2048_2_sequential
  Created: test/Iara/05-cholesky-2048_2_vf-omp
  Created: test/Iara/05-cholesky-2048_4_sequential
  Created: test/Iara/05-cholesky-2048_4_vf-omp

Total directories created: 4
```

### 2. Generate CMakeLists.txt

```bash
$ python3 applications/cholesky/generate_cmakelists.py

Created the following CMakeLists.txt files:
  test/Iara/05-cholesky-2048_2_sequential/CMakeLists.txt
  test/Iara/05-cholesky-2048_2_vf-omp/CMakeLists.txt
  test/Iara/05-cholesky-2048_4_sequential/CMakeLists.txt
  test/Iara/05-cholesky-2048_4_vf-omp/CMakeLists.txt
```

### 3. Run Experiments

```bash
$ python3 applications/cholesky/run_experiments.py

Found 4 test instances

Running 05-cholesky-2048_2_sequential...
  Run 1/5 completed
  Run 2/5 completed
  ...

Running 05-cholesky-2048_2_vf-omp...
  ...

Results saved to: experiment/cholesky/results/results_20251128_183042.csv
Total measurements: 80  (4 tests × 5 runs × 4 measurements)
```

### 4. Visualize Results

```bash
$ python3 applications/cholesky/visualize_results.py experiment/cholesky/results/results_20251128_183042.csv

Loaded 80 result rows
Saved: experiment/cholesky/results/plots/runtime_comparison.png
Saved: experiment/cholesky/results/plots/scaling_analysis.png
Saved: experiment/cholesky/results/plots/time_breakdown.png
Saved: experiment/cholesky/results/plots/summary_report.txt

Visualizations saved to: experiment/cholesky/results/plots/
```

## Extending the Framework

### Add New Measurements

1. Update `experiments.yaml`:
```yaml
measurements:
  - name: my_new_metric
    type: float
    parser:
      type: regex
      pattern: 'My Metric:\s+(\d+\.?\d*)'
    unit: ms
```

2. Ensure the program outputs matching text:
```
My Metric: 123.45
```

3. Re-run experiments - new measurement automatically collected and visualized

### Add New Experiment Set

1. Update `experiments.yaml`:
```yaml
experiment_sets:
  - name: my_experiments
    description: "My custom experiments"
    parameters:
      matrix_size: [2048, 4096]
      num_blocks: [2, 4, 8]
      scheduler: [sequential, vf-omp]
    generate_tests: true
```

2. Re-generate test instances and run

### Add New Plot Type

Edit `visualize_results.py` and add a method:
```python
def plot_custom_analysis(self):
    # Your plotting code here
    pass
```

Then call from `main()`:
```python
if 'custom' in plots:
    viz.plot_custom_analysis()
```

## Performance Tips

- **Fast iteration**: Use `dev` set with small matrix sizes
- **Parallel build**: CMake automatically parallelizes test compilation
- **Reuse builds**: Running experiments twice skips rebuild (uses cached config)
- **Filter results**: Use `--matrix-size` flag to focus visualization on specific sizes

## Troubleshooting

### Build Failures

Check `build_experiments/CMakeFiles/` for detailed error messages:
```bash
less build_experiments/test/CMakeFiles/run-05-cholesky-*/build.make
```

### Missing Measurements

Verify program outputs expected text:
```bash
build_experiments/test/Iara/05-cholesky-2048_2_sequential/build-sequential/a.out 2>&1 | grep "Computation time"
```

### Memory Issues

Reduce `matrix_size` or `repetitions` in `experiments.yaml`

### Visualization Errors

Install required packages:
```bash
pip install matplotlib seaborn pandas numpy
pip install plotly  # for interactive plots
```

## Integration with IaRa Development

The experiment framework integrates with the main IaRa build:
- Uses `iara-opt` from `build_experiments/bin/iara-opt`
- Compiles with IaRa runtime and schedules
- Builds test instances as part of `IARA_BUILD_TESTS=ON`

To build IaRa-specific schedulers, set in `experiments.yaml`:
```yaml
parameters:
  - name: scheduler
    choices:
      - vf-omp        # Virtual FIFO + OpenMP
      - vf-enkits     # Virtual FIFO + EnkiTS
```

These will use the full IaRa pipeline (topology → schedule → compilation).

## References

- [IaRa Documentation](../../README.md)
- [Spinner Fork Config Loader](../../../../spinner-fork/)
- [CMake Configuration](../../cmake/IaRaApplications.cmake)
