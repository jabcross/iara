# IaRa Experiment Framework

The experiment framework provides a complete system for generating, building, running, and analyzing experiments across all IaRa applications.

## Architecture

The framework consists of three main components:

### 1. Test Generation (generate_experiments.py)
- Reads experiment specifications from `experiments.yaml` in each application
- Generates CMakeLists.txt files for test configuration and building
- Supports multiple experiment sets (e.g., "regression", "baseline", "scaling")
- Declaratively specifies parameters, constraints, measurements, and visualizations

### 2. Test Execution (build_tests.sh)
- Three-step orchestration:
  1. Generate tests from YAML specifications
  2. Configure CMake with generated files
  3. Run all tests via CTest
- All build and runtime executables are placed in `build_experiments/<app>/<experiment_set>/`

### 3. Measurement Collection and Analysis
The measurement pipeline consists of three Python scripts:

#### collect_binary_data.py
- Extracts ELF section sizes from compiled binaries using `size -A`
- Generates CSV: `<app>_<experiment_set>_binary_data.csv`
- Outputs: test_name, scheduler, measurement, section, value, unit

#### collect_runtime_measurements.py
- Runs executables and extracts timing measurements via regex parsing
- Captures: init_time, convert_time, compute_time from program output
- Generates CSV: `<app>_<experiment_set>_runtime_measurements.csv`
- Outputs: test_name, scheduler, matrix_size, num_blocks, measurement, value, unit

#### merge_measurements.py
- Combines all measurement CSVs into a single results file
- Adds binary compilation time estimates
- Generates CSV: `results_YYYYMMDD_HHMMSS.csv` in results directory
- Outputs: test_name, scheduler, matrix_size, num_blocks, measurement, section, value, unit

#### run_experiment_analysis.py
- Master orchestration script that runs the complete workflow
- Flexible to work with any application and experiment set
- Supports skipping individual stages

## Usage

### Building and Running Experiments

```bash
# Build all regression tests
./scripts/build_tests.sh

# Build only 05-cholesky regression tests
./scripts/build_tests.sh -e 05-cholesky

# Build with custom parallelism
./scripts/build_tests.sh -j 16
```

This produces executables in: `build_experiments/<app>/<experiment_set>/<test_name>/a.out`

### Collecting Measurements

```bash
# Run complete workflow for 05-cholesky regression
python3 applications/experiment-framework/run_experiment_analysis.py \
    --app-name 05-cholesky \
    --experiment-set regression

# Specify custom directories
python3 applications/experiment-framework/run_experiment_analysis.py \
    --build-dir /path/to/build_experiments \
    --results-dir /path/to/results \
    --app-name 05-cholesky

# Skip specific stages
python3 applications/experiment-framework/run_experiment_analysis.py \
    --skip-binary \
    --skip-runtime
```

### Individual Scripts

Each script can be run independently:

```bash
# Collect binary data only
python3 applications/experiment-framework/collect_binary_data.py

# Collect runtime measurements only
python3 applications/experiment-framework/collect_runtime_measurements.py

# Merge pre-existing measurements
python3 applications/experiment-framework/merge_measurements.py
```

## Directory Structure

```
applications/
├── 05-cholesky/
│   ├── experiment/
│   │   ├── experiments.yaml          # Experiment specifications
│   │   ├── CMakeLists.txt            # Generated test configuration
│   │   └── results/                  # Results directory
│   │       └── results_YYYYMMDD_HHMMSS.csv
│   └── src/
├── 06-broadcast/
│   ├── experiment/
│   │   ├── experiments.yaml
│   │   ├── CMakeLists.txt
│   │   └── results/
│   └── src/
└── experiment-framework/
    ├── README.md                      # This file
    ├── generate_experiments.py
    ├── collect_binary_data.py
    ├── collect_runtime_measurements.py
    ├── merge_measurements.py
    └── run_experiment_analysis.py

build_experiments/
├── 05-cholesky/
│   ├── regression/
│   │   ├── 05-cholesky_regression_sequential_matrix_size_2048_num_blocks_2/
│   │   │   ├── CMakeFiles/
│   │   │   ├── a.out                 # Compiled executable
│   │   │   └── ...
│   │   ├── 05-cholesky_regression_vf-omp_matrix_size_2048_num_blocks_2/
│   │   │   ├── a.out
│   │   │   └── ...
│   │   └── ...
│   ├── 05-cholesky_regression_binary_data.csv
│   └── 05-cholesky_regression_runtime_measurements.csv
└── ...
```

## Experiment Specification (experiments.yaml)

Each application specifies its experiments in `applications/<app>/experiment/experiments.yaml`:

```yaml
application:
  name: 05-cholesky
  description: "Application description"

parameters:
  # Parameter definitions
  - name: matrix_size
    type: int
    description: "..."

experiment_sets:
  - name: regression
    description: "Quick regression tests"
    parameters:
      matrix_size: [2048]
      num_blocks: [2, 4]
      scheduler: [sequential, vf-omp]
    generate_tests: true

measurements:
  # Measurement definitions
  - name: compute_time
    type: float
    parser:
      type: regex
      pattern: 'Wall time:\s+(\d+\.?\d*)'
      group: 1
    unit: s
    required: true

visualization:
  plots:
    # Plot specifications (declarative format)
    runtime_performance:
      title: "..."
      type: grouped_stacked_bars
      ...
```

## Data Format

All measurement CSVs follow a consistent schema:

```
test_name,scheduler,matrix_size,num_blocks,measurement,section,value,unit
05-cholesky_regression_sequential_matrix_size_2048_num_blocks_2,sequential,2048,2,init_time,,1.234,s
05-cholesky_regression_sequential_matrix_size_2048_num_blocks_2,sequential,2048,2,convert_time,,0.567,s
05-cholesky_regression_vf-omp_matrix_size_2048_num_blocks_2,vf-omp,2048,2,.text,,65536,bytes
...
```

### Column Definitions
- **test_name**: Full test identifier from CMakeLists.txt
- **scheduler**: Scheduling strategy (sequential, vf-omp, vf-enkits, etc.)
- **matrix_size**: Matrix size parameter (if applicable)
- **num_blocks**: Number of blocks parameter (if applicable)
- **measurement**: Measurement type (e.g., "init_time", "section_size", "binary_compilation_time")
- **section**: ELF section name for binary data (null for runtime measurements)
- **value**: Numeric value of the measurement
- **unit**: Unit of measurement (s, bytes, MB, etc.)

## Generic Design

The framework is designed to work with any application that follows the standard structure:

1. Define experiments in `applications/<app>/experiment/experiments.yaml`
2. Place application source in `applications/<app>/src/`
3. Generate tests: `python3 scripts/generate_experiments.py`
4. Build tests: `./scripts/build_tests.sh`
5. Analyze results: `python3 applications/experiment-framework/run_experiment_analysis.py`

Scripts automatically discover the application structure and generate appropriate test configurations.

## Integration with CMake

The test framework integrates with CMake through:

1. `cmake/IaRaApplications.cmake`: Defines macros for test configuration
2. `cmake/IaRaVirtualFIFO.cmake`: Handles Virtual FIFO compilation
3. Auto-generated `applications/<app>/experiment/CMakeLists.txt`

All test targets use phony target dependencies to ensure complete rebuilds for accurate measurement collection.

## Measurement Accuracy

### Build Time
Currently estimated based on test type. Future: measure from build logs.

### Runtime Measurements
- Extracted via regex from program output
- Supports custom parsers in YAML specification
- Multiple measurements per test run

### Binary Data
- Section sizes extracted via `size -A` command
- Includes all ELF sections (.text, .data, .rodata, etc.)
- Measurement type: "section_size"

## Extensibility

To add a new application:

1. Create `applications/<new-app>/experiment/experiments.yaml`
2. Define parameters, measurements, and experiment sets
3. Run framework tools with `--app-name <new-app>`

All scripts automatically adapt to the new application structure.
