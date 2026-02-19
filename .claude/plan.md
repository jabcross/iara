# Experiment Framework Refactoring Plan v2

## Core Principles

1. **Tests are experiments**: Every test is a specific instance of an experiment
2. **CMake CTest integration**: Keep using CTest for debugger integration
3. **3-stage pipeline**: IaRa build (cached) → Application build (always fresh) → Run
4. **Declarative parameters**: Define experiments in data, not code
5. **Unified CMake**: Zero duplication between test and experiment builds

---

## Current Problems

### 1. CMake Duplication
- `test/CMakeLists.txt` (~700 lines) and `experiment/cholesky/CMakeLists.txt` (~470 lines) share 90% of code
- Bug fixes must be applied in both places
- Functions like `iara_runtime_sources()` duplicated

### 2. Test vs Experiment Split
- Tests are in `test/Iara/05-cholesky-2048_2/` with `parameters.sh`
- Experiments are in `experiment/cholesky/` with Python scripts
- Same application, different infrastructure
- **But tests are just single instances of experiments!**

### 3. Parameter Sprawl
- Application params in `parameters.sh` (NUM_BLOCKS, MATRIX_SIZE)
- Runtime params via env vars (IARA_RUNTIME_BACKEND)
- Scheduler params via env vars (IARA_OPT_SCHEDULER)
- Build params scattered across CMake files

### 4. Build Types
- Currently: Debug, Release, CompilerDebug, TestDebug, ExperimentSize, ExperimentPerf, ExperimentProf
- Too many options, confusing purpose

---

## Proposed Solution

### New Directory Structure

```
iara/
├── cmake/
│   └── IaRaApplications.cmake          # Shared build functions
│
├── applications/                        # Application sources (C/C++)
│   ├── cholesky/
│   │   ├── main.c
│   │   ├── cholesky.c
│   │   ├── codegen.sh                  # Generates topology.mlir, kernels
│   │   ├── generate_topology.py
│   │   └── generate_split_join.py
│   └── sift/
│       └── ...
│
├── experiments/                         # Declarative experiment definitions
│   ├── framework/                       # Python framework
│   │   ├── __init__.py
│   │   ├── config.py                   # Parameter definitions
│   │   ├── runner.py                   # Experiment runner
│   │   └── measurements.py             # Measurement parsing
│   │
│   ├── definitions/                     # Experiment parameter spaces
│   │   ├── cholesky.py                 # All cholesky experiments
│   │   └── sift.py
│   │
│   ├── instances/                       # Generated test instances
│   │   └── cholesky/
│   │       ├── 2048_2_vf-omp/
│   │       │   ├── CMakeLists.txt      # Generated from template
│   │       │   └── parameters.cmake    # Generated from definition
│   │       └── 10080_8_sequential/
│   │           └── ...
│   │
│   ├── results/                         # Experiment results
│   │   └── cholesky/
│   │       ├── results_20241127_123456.csv
│   │       └── build_metrics_20241127_123456.csv
│   │
│   └── run.py                          # Universal experiment runner
│
└── CMakeLists.txt
    # If IARA_BUILD_EXPERIMENTS=ON:
    #   add_subdirectory(experiments/instances)
    # Else:
    #   (build iara-opt only)
```

### Key Change: Tests ARE Experiments

Instead of separating "tests" and "experiments", **all tests are experiment instances**:

1. Experiment definitions live in `experiments/definitions/cholesky.py`
2. Python generates test instances in `experiments/instances/cholesky/2048_2_vf-omp/`
3. CMake builds each instance using shared functions
4. CTest can run any instance for debugging
5. Python can run all instances for measurement

---

## Parameter System

### Parameter Categories

#### 1. IaRa Parameters (Global)
Apply to iara-opt itself:
- `memory_allocator`: "system" | "custom"
- `parallel_hash`: true | false

#### 2. Runtime Parameters (Global)
Apply to runtime selection:
- `task_backend`: "omp" | "enkits"

#### 3. Application Parameters (Per-application)
Each application defines its own:

**Cholesky**:
- `matrix_size`: [2048, 4096, 8192, 10080]
- `num_blocks`: [1, 2, 4, 6, 8]
- `scheduler`: "sequential" | "omp-task" | "omp-for" | "enkits-task" | "vf-omp" | "vf-enkits"
- Computed: `block_size = matrix_size / num_blocks`

**SIFT** (example):
- `image_size`: [1024, 2048]
- `num_octaves`: [3, 4, 5]
- `scheduler`: ...

### YAML-Based Application Configuration

Each application has a YAML file defining parameter ranges and experiment sets:

```yaml
# applications/cholesky/experiments.yaml
application: cholesky
description: Cholesky decomposition experiments

# Define parameter ranges (used across all experiments)
parameters:
  matrix_size:
    type: int
    range: [2048, 4096, 8192, 10080, 18432, 32768]
    description: Size of the square matrix

  num_blocks:
    type: int
    range: [1, 2, 4, 6, 8]
    description: Number of blocks to divide matrix into

  scheduler:
    type: str
    options: [sequential, omp-task, omp-for, enkits-task, vf-omp, vf-enkits]
    description: Task scheduler type

# Computed parameters (Python expressions)
computed_parameters:
  block_size: "matrix_size // num_blocks"
  block_memory: "block_size * block_size * 8"

# Constraints (filter out invalid combinations)
constraints:
  - "num_blocks <= matrix_size"
  - "matrix_size % num_blocks == 0"

# Build configuration
build:
  runtime_flags:
    - "-DNUM_BLOCKS={num_blocks}"
    - "-DMATRIX_SIZE={matrix_size}"
    - "-DVERBOSE"
  linker_flags:
    - "-lopenblas"

# Measurements to extract
measurements:
  - name: init_time
    type: float
    unit: s
    pattern: "Initialization time:\\s+(\\d+\\.\\d+)"
    description: Matrix initialization time

  - name: convert_time
    type: float
    unit: s
    pattern: "Block conversion time:\\s+(\\d+\\.\\d+)"
    description: Time to convert to block format

  - name: compute_time
    type: float
    unit: s
    pattern: "Computation time:\\s+(\\d+\\.\\d+)"
    description: Pure computation time

  - name: wall_time
    type: float
    unit: s
    required: true
    components: [init_time, convert_time, compute_time]
    description: Total wall clock time

  - name: max_rss_kb
    type: int
    unit: KB
    source: time_command
    description: Maximum memory usage

# Named experiment sets
experiments:
  # Development test - generates 1 CTest instance
  dev:
    description: Single test for development/debugging
    generate_tests: true  # ← Mark this to generate CTest instances
    parameters:
      matrix_size: [2048]
      num_blocks: [2]
      scheduler: [vf-omp]

  # Baseline comparison - generates 6 CTest instances
  baseline:
    description: Compare schedulers at key sizes
    generate_tests: true
    parameters:
      matrix_size: [2048, 10080]
      num_blocks: [1, 8]
      scheduler: [sequential, omp-task, vf-omp]

  # Full scaling study - batch experiment (no CTest instances)
  scaling:
    description: Complete scaling study
    generate_tests: false  # Run via Python, not CTest
    parameters:
      matrix_size: all  # Use all values from range
      num_blocks: all
      scheduler: all
    execution:
      repetitions: 10
      warmup: 2
      slurm: auto

  # Scheduler comparison
  schedulers:
    description: Compare all schedulers at large size
    generate_tests: false
    parameters:
      matrix_size: [10080]
      num_blocks: [8]
      scheduler: all
    execution:
      repetitions: 10
      warmup: 2

# Visualization configuration
visualization:
  plots:
    - wall_time  # Composite - will show stacked
    - max_rss_kb
    - compile_time
    - binary_size

  # Grouping for plots
  group_by: [matrix_size, num_blocks]
  facet_by: scheduler

  # Scheduler display order and colors
  scheduler_order: [sequential, omp-task, vf-omp, vf-enkits]
  scheduler_colors:
    sequential: "#2ca02c"
    omp-task: "#1f77b4"
    vf-omp: "#ff7f0e"
    vf-enkits: "#d62728"

  # Generate relative plots (vs sequential baseline)
  relative_plots: [wall_time]

  # Output formats
  formats: [png, html]  # Both matplotlib and plotly
```

### Test Generation from YAML

The framework automatically generates CTest instances from experiments marked with `generate_tests: true`:

```python
# experiments/framework/generator.py

def generate_test_instances(app_config: dict, project_dir: Path):
    """Generate CMakeLists.txt for each test instance."""

    app_name = app_config['application']
    experiments_dir = project_dir / 'experiments' / 'instances' / app_name
    experiments_dir.mkdir(parents=True, exist_ok=True)

    for exp_name, exp_config in app_config['experiments'].items():
        # Skip experiments not marked for test generation
        if not exp_config.get('generate_tests', False):
            continue

        # Generate all parameter combinations
        instances = generate_parameter_combinations(
            exp_config['parameters'],
            app_config['parameters'],
            app_config.get('constraints', [])
        )

        for instance in instances:
            # Create instance directory with readable name
            instance_name = format_instance_name(instance)
            instance_dir = experiments_dir / instance_name
            instance_dir.mkdir(parents=True, exist_ok=True)

            # Generate CMakeLists.txt that calls iara_add_application()
            generate_cmake_for_instance(
                instance_dir,
                app_name,
                instance,
                app_config['build']
            )

def format_instance_name(params: dict) -> str:
    """Format instance name with parameter names and values.

    Example: matrix_size=2048_num_blocks=2_scheduler=vf-omp
    """
    parts = []
    for key in sorted(params.keys()):
        parts.append(f"{key}={params[key]}")
    return "_".join(parts)

def generate_cmake_for_instance(instance_dir: Path, app_name: str,
                                params: dict, build_config: dict):
    """Generate CMakeLists.txt for a single test instance."""

    cmake_content = f"""# Generated test instance: {instance_dir.name}
# Application: {app_name}
# Parameters: {params}

include(${{PROJECT_SOURCE_DIR}}/cmake/IaRaApplications.cmake)

# Call unified build function
iara_add_application(
    NAME {app_name}
    SCHEDULER {params['scheduler']}
    BUILD_TYPE test
    INSTANCE_DIR ${{CMAKE_CURRENT_SOURCE_DIR}}
"""

    # Add application-specific parameters
    for key, value in params.items():
        if key != 'scheduler':  # scheduler already handled
            cmake_content += f"    {key.upper()} {value}\n"

    cmake_content += ")\n"

    (instance_dir / "CMakeLists.txt").write_text(cmake_content)
```

### Workflow with YAML Configuration

```bash
# 1. Define application parameters and experiments in YAML
# applications/cholesky/experiments.yaml

# 2. Generate test instances from YAML
cd experiments
python run.py generate cholesky

# This creates:
# experiments/instances/cholesky/
#   matrix_size=2048_num_blocks=2_scheduler=vf-omp/CMakeLists.txt  # from 'dev'
#   matrix_size=2048_num_blocks=1_scheduler=sequential/CMakeLists.txt  # from 'baseline'
#   matrix_size=2048_num_blocks=1_scheduler=omp-task/CMakeLists.txt
#   ... (total 6 instances from 'baseline')

# 3. Build and run single test with CMake/CTest
cd ../build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make cholesky-vf-omp
ctest -R cholesky-vf-omp --verbose

# 4. Run batch experiment (no individual CTest instances)
cd ../experiments
python run.py run cholesky scaling --build-type Release

# This:
# - Builds all combinations (not as CTest instances)
# - Runs with repetitions/warmup
# - Collects results to CSV
# - Generates plots automatically
```

---

## Simplified Build Types

Consolidate to 3 clear build types:

```cmake
# CMakeLists.txt

# Release - For measurement (based on current ExperimentPerf)
set(CMAKE_CXX_FLAGS_RELEASE
    "-O3 -DNDEBUG -march=native -mtune=native -fno-rtti -fno-exceptions -flto"
)

# Debug - For debugging (maximum debug info)
set(CMAKE_CXX_FLAGS_DEBUG
    "-O0 -g -gdwarf-4 -fno-omit-frame-pointer"
)

# RelWithDebInfo - For profiling (performance + symbols)
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO
    "-O3 -DNDEBUG -march=native -mtune=native -g -gdwarf-4 -fno-omit-frame-pointer"
)
```

Remove: CompilerDebug, TestDebug, ExperimentSize, ExperimentPerf, ExperimentProf

---

## Unified CMake Build Functions

### cmake/IaRaApplications.cmake

```cmake
# Shared function to build any IaRa application
# Used by both individual tests and batch experiments
function(iara_add_application)
    # Parse arguments
    cmake_parse_arguments(APP
        ""  # Options
        "NAME;SCHEDULER;MATRIX_SIZE;NUM_BLOCKS;BUILD_DIR"  # Single-value
        "RUNTIME_FLAGS;LINKER_FLAGS"  # Multi-value
        ${ARGN}
    )

    # Auto-detect application sources
    set(app_src_dir "${PROJECT_SOURCE_DIR}/applications/${APP_NAME}")

    # Determine if baseline or IaRa scheduler
    set(is_baseline FALSE)
    if(APP_SCHEDULER MATCHES "^(sequential|omp-task|omp-for|enkits-task)$")
        set(is_baseline TRUE)
    endif()

    # Get runtime sources (if IaRa scheduler)
    if(NOT is_baseline)
        # Extract scheduler type and backend from scheduler name
        # vf-omp -> scheduler=virtual-fifo, backend=omp
        # vf-enkits -> scheduler=virtual-fifo, backend=enkits
        iara_parse_scheduler(${APP_SCHEDULER} sched_type backend)
        iara_runtime_sources(${sched_type} ${backend} runtime_srcs runtime_defs)
    endif()

    # Run codegen if exists
    if(EXISTS "${app_src_dir}/codegen.sh")
        iara_run_codegen(
            APP_DIR ${app_src_dir}
            BUILD_DIR ${APP_BUILD_DIR}
            PARAMS MATRIX_SIZE ${APP_MATRIX_SIZE} NUM_BLOCKS ${APP_NUM_BLOCKS}
        )
    endif()

    # Generate schedule (if IaRa scheduler)
    if(NOT is_baseline)
        iara_generate_schedule(
            TOPOLOGY ${APP_BUILD_DIR}/topology.mlir
            OUTPUT ${APP_BUILD_DIR}/schedule.o
            SCHEDULER ${sched_type}
        )
    endif()

    # Build executable
    # ALWAYS rebuild from scratch (compilation time is measured)
    # Add custom target to force clean before build
    set(target_name "${APP_NAME}-${APP_SCHEDULER}")

    add_executable(${target_name}
        ${app_sources}
        ${runtime_srcs}
        $<$<NOT:${is_baseline}>:${APP_BUILD_DIR}/schedule.o>
    )

    target_compile_options(${target_name} PRIVATE
        ${APP_RUNTIME_FLAGS}
        ${runtime_defs}
    )

    target_link_options(${target_name} PRIVATE
        ${APP_LINKER_FLAGS}
    )

    # Add to CTest
    add_test(NAME ${target_name} COMMAND ${target_name})
endfunction()
```

---

## Workflow

### Development: Run single test instance

```bash
# Generate test instance
cd experiments
python run.py generate cholesky_dev

# Build with CMake (cached iara-opt, fresh app build)
cd ../build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make cholesky-vf-omp

# Run with debugger
ctest -R cholesky-vf-omp --verbose
# or
gdb ./experiments/instances/cholesky/2048_2_vf-omp/a.out
```

### Measurement: Run experiment sweep

```bash
# Generate all instances for experiment
cd experiments
python run.py generate cholesky_scaling

# Build and run with measurements
python run.py run cholesky_scaling \
    --build-type Release \
    --repetitions 10 \
    --warmup 2 \
    --slurm auto

# Results saved to:
# experiments/results/cholesky/results_20241127_123456.csv
```

### Profiling: Run with perf

```bash
# Generate instance
python run.py generate cholesky_dev

# Build with profiling symbols
cd ../build
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
make cholesky-vf-omp

# Profile
perf record -g ./experiments/instances/cholesky/2048_2_vf-omp/a.out
perf report
```

---

## Implementation Plan

### Phase 1: CMake Unification

1. **Create `cmake/IaRaApplications.cmake`**
   - Extract `iara_runtime_sources()` from test/CMakeLists.txt
   - Extract `iara_collect_args()`
   - Create `iara_add_application()` function
   - Add helpers: `iara_parse_scheduler()`, `iara_run_codegen()`, `iara_generate_schedule()`

2. **Simplify build types**
   - Remove: CompilerDebug, TestDebug, ExperimentSize, ExperimentPerf, ExperimentProf
   - Keep: Debug, Release, RelWithDebInfo
   - Update flags to match new purposes

3. **Test with existing cholesky test**
   - Create `experiments/instances/cholesky/2048_2_vf-omp/`
   - Generate `CMakeLists.txt` that calls `iara_add_application()`
   - Verify builds and runs correctly

### Phase 2: Python Framework

1. **Create `experiments/framework/`**
   - `config.py`: Experiment, Parameter, Range classes
   - `runner.py`: Generate instances, run experiments, collect results
   - `measurements.py`: Parse output, extract metrics

2. **Create first experiment definition**
   - `experiments/definitions/cholesky.py`
   - Define parameter space
   - Define measurements

3. **Create instance generator**
   - `experiments/run.py generate <experiment>`
   - Generates `experiments/instances/<app>/<config>/CMakeLists.txt`
   - Each CMakeLists.txt calls `iara_add_application()` with specific params

4. **Create experiment runner**
   - `experiments/run.py run <experiment>`
   - Builds all instances (with timing)
   - Runs all instances (with repetitions)
   - Collects results to CSV

### Phase 3: Visualization Unification

**Goal**: Eliminate duplication between matplotlib and plotly, unified styling

1. **Create unified plotting backend**
   - Abstract `PlotRenderer` interface
   - `MatplotlibRenderer` and `PlotlyRenderer` implementations
   - Shared plot specification (data, grouping, styling)
   - Both renderers consume same specification

2. **Implement all plot types in both backends**
   - Simple bar plots (with bootstrap CI error bars)
   - Stacked bar plots (alpha + hatch for matplotlib, pattern approximation in plotly)
   - Grouped bar plots (facet by matrix_size, group by num_blocks)
   - Relative plots (vs baseline scheduler)
   - Line plots (for continuous parameters)

3. **Consistent styling across backends**
   - Unified color scheme (scheduler_colors dict)
   - Consistent fonts, sizes, layouts
   - Error bars (bootstrap CI) by default
   - Matching legends, labels, titles
   - Alpha + hatch shading for stacked bars (both backends)

4. **Reduce code duplication**
   - **Current**: Separate matplotlib and plotly functions (~1000 lines each)
   - **Target**: Shared specification + renderers (~300 lines common, ~200 per renderer)
   - Extract plot data preparation logic
   - Reuse grouping, aggregation, error bar computation

### Phase 4: Migration & Cleanup

1. **Convert existing tests to instances**
   - Move `test/Iara/05-cholesky-2048_2/` → generated instance
   - Delete old test directory
   - Update CMake to use experiments/instances/
   - **Preserve**: Logging with timestamps, result organization, all current outputs

2. **Remove old experiment infrastructure**
   - Delete `experiment/cholesky/run_experiment.py`
   - Delete old `experiment/experiment_framework.py`
   - Migrate to new unified visualization system
   - Verify all current plots still generated

3. **Documentation**
   - How to define new experiments
   - How to add new applications
   - How to use for development vs measurement
   - Migration guide from old to new

---

## Type-Safe, Composable Measurements & Visualization

### Measurement Type System

Every measurement has:
1. **Name**: Identifier (e.g., "wall_time", "init_time")
2. **Type**: int, float, str, bool
3. **Parser**: How to extract from output (regex, JSON path, etc.)
4. **Unit**: For display (seconds, bytes, %, etc.)
5. **Composition**: Optional - how it's composed from other measurements

#### Simple Measurements

```python
# experiments/framework/measurements.py

@dataclass
class Measurement:
    """A single measurement extracted from program output."""
    name: str
    type: type                          # int, float, str, bool
    parser: Callable[[str], Any]        # Extract value from output
    unit: str = ""                      # Display unit
    required: bool = False              # Fail if not found?
    description: str = ""

    def extract(self, stdout: str, stderr: str) -> Optional[Any]:
        """Extract and type-check value from output."""
        try:
            value = self.parser(stdout)
            if not isinstance(value, self.type):
                raise TypeError(f"Expected {self.type}, got {type(value)}")
            return value
        except Exception as e:
            if self.required:
                raise ValueError(f"Required measurement '{self.name}' not found: {e}")
            return None
```

#### Composite Measurements

```python
@dataclass
class CompositeMeasurement(Measurement):
    """A measurement composed of other measurements.

    When visualized, can show both total and breakdown.
    """
    components: List[str]               # Names of component measurements
    composition: Callable[[Dict], float] = sum  # How to combine components

    def compute(self, measurements: Dict[str, float]) -> float:
        """Compute composite value from components."""
        component_values = [measurements[c] for c in self.components if c in measurements]
        return self.composition(component_values)

    def is_complete(self, measurements: Dict[str, float]) -> bool:
        """Check if all components are available."""
        return all(c in measurements for c in self.components)
```

#### Example: Cholesky Measurements

```python
# experiments/definitions/cholesky.py

from framework.measurements import Measurement, CompositeMeasurement, regex_parser

# Simple measurements (extracted directly from output)
measurements = [
    # Core timing measurements
    Measurement(
        name="init_time",
        type=float,
        parser=regex_parser(r"Initialization time:\s+(\d+\.\d+)"),
        unit="s",
        description="Matrix initialization and allocation time",
    ),

    Measurement(
        name="convert_time",
        type=float,
        parser=regex_parser(r"Block conversion time:\s+(\d+\.\d+)"),
        unit="s",
        description="Time to convert matrix to block format",
    ),

    Measurement(
        name="compute_time",
        type=float,
        parser=regex_parser(r"Computation time:\s+(\d+\.\d+)"),
        unit="s",
        description="Pure Cholesky decomposition time",
    ),

    # Composite measurement: wall_time = init + convert + compute
    CompositeMeasurement(
        name="wall_time",
        type=float,
        components=["init_time", "convert_time", "compute_time"],
        composition=sum,
        unit="s",
        required=True,
        description="Total wall clock time (sum of all phases)",
    ),

    # System measurements (from /usr/bin/time)
    Measurement(
        name="max_rss_kb",
        type=int,
        parser=time_output_parser("max_rss"),
        unit="KB",
        description="Maximum resident set size",
    ),

    # Build measurements (collected separately)
    Measurement(
        name="compile_time",
        type=float,
        unit="s",
        description="Compilation time (build metric)",
    ),

    Measurement(
        name="binary_size",
        type=int,
        unit="bytes",
        description="Total binary size (build metric)",
    ),
]
```

### Visualization Framework

The visualization system automatically determines:
1. **What to plot**: Based on available measurements
2. **How to plot**: Single bars vs stacked bars based on composition
3. **Grouping**: Based on parameter dimensions

#### Automatic Plot Generation

```python
# experiments/framework/visualization.py

class VisualizationEngine:
    """Generates plots automatically from measurement definitions."""

    def __init__(self, measurements: List[Measurement], parameters: ParameterSpace):
        self.measurements = {m.name: m for m in measurements}
        self.parameters = parameters
        self.composites = {m.name: m for m in measurements if isinstance(m, CompositeMeasurement)}

    def plot_measurement(self,
                        measurement_name: str,
                        data: pd.DataFrame,
                        group_by: List[str],  # e.g., ["matrix_size", "num_blocks"]
                        facet_by: str = "scheduler") -> Figure:
        """
        Automatically plot a measurement.

        - If measurement is composite and all components available: stacked bar
        - Otherwise: simple bar/line plot
        - Automatically groups by specified parameters
        - Facets by scheduler (or other parameter)
        """
        measurement = self.measurements[measurement_name]

        # Check if this is a composite with available components
        if isinstance(measurement, CompositeMeasurement):
            available_components = [c for c in measurement.components
                                   if c in data.columns]

            if len(available_components) == len(measurement.components):
                # All components available - use stacked bar
                return self._plot_stacked_bar(
                    measurement_name=measurement_name,
                    components=measurement.components,
                    data=data,
                    group_by=group_by,
                    facet_by=facet_by
                )

        # Simple plot
        return self._plot_simple_bar(
            measurement_name=measurement_name,
            data=data,
            group_by=group_by,
            facet_by=facet_by
        )

    def _plot_stacked_bar(self, measurement_name, components, data, group_by, facet_by):
        """Create stacked bar plot showing component breakdown."""
        # Group data
        grouped = data.groupby(group_by + [facet_by])

        # Aggregate components
        means = {}
        for component in components:
            means[component] = grouped[component].mean()

        # Create stacked bars
        fig, ax = plt.subplots(figsize=(12, 6))

        # Plot each component as a stack
        bottom = None
        for i, component in enumerate(components):
            component_data = means[component]
            ax.bar(
                x=range(len(component_data)),
                height=component_data.values,
                bottom=bottom,
                label=self.measurements[component].description,
                alpha=0.7 - i*0.1,  # Darker for lower stack levels
                hatch='/' * (len(components) - i),  # More hatching for lower levels
            )

            if bottom is None:
                bottom = component_data.values
            else:
                bottom = bottom + component_data.values

        # Add total on top
        total = sum(means[c].values for c in components)
        for i, val in enumerate(total):
            ax.text(i, val, f"{val:.2f}{self.measurements[measurement_name].unit}",
                   ha='center', va='bottom', fontsize=8)

        # Styling
        measurement_obj = self.measurements[measurement_name]
        ax.set_ylabel(f"{measurement_obj.description} ({measurement_obj.unit})")
        ax.set_title(f"{measurement_obj.description} Breakdown")
        ax.legend()
        ax.grid(True, alpha=0.3)

        return fig

    def generate_all_plots(self, data: pd.DataFrame) -> Dict[str, Figure]:
        """Generate all relevant plots automatically."""
        plots = {}

        # Plot each top-level measurement
        for name, measurement in self.measurements.items():
            if name not in data.columns:
                continue

            # Skip if this is a component of another measurement
            # (will be plotted as part of the composite)
            if any(name in comp.components
                  for comp in self.composites.values()):
                continue

            plots[name] = self.plot_measurement(
                measurement_name=name,
                data=data,
                group_by=["matrix_size", "num_blocks"],  # Could be inferred
                facet_by="scheduler"
            )

        return plots
```

#### Visualization Configuration

Users can configure visualization in the experiment definition:

```python
# experiments/definitions/cholesky.py

cholesky = Experiment(
    # ... parameters, measurements ...

    visualization=VisualizationConfig(
        # Which measurements to plot
        plots=[
            "wall_time",      # Composite - will show stacked
            "max_rss_kb",     # Simple - will show simple bars
            "compile_time",   # Build metric
            "binary_size",    # Build metric
        ],

        # Grouping and faceting
        group_by=["matrix_size", "num_blocks"],
        facet_by="scheduler",

        # Scheduler order and colors
        scheduler_order=["sequential", "omp-task", "vf-omp", "vf-enkits"],
        scheduler_colors={
            "sequential": "#2ca02c",
            "omp-task": "#1f77b4",
            "vf-omp": "#ff7f0e",
            "vf-enkits": "#d62728",
        },

        # Additional plot types
        relative_plots=["wall_time"],  # Also create relative (vs sequential) plots

        # Output formats
        formats=["png", "html"],  # Generate both matplotlib and plotly
    )
)
```

### Automatic Plot Decision Logic

```python
def decide_plot_type(measurement: Measurement, data: pd.DataFrame) -> str:
    """
    Automatically decide how to plot a measurement.

    Returns: "stacked_bar" | "simple_bar" | "line" | "scatter"
    """
    # If composite with all components available -> stacked bar
    if isinstance(measurement, CompositeMeasurement):
        if all(c in data.columns for c in measurement.components):
            return "stacked_bar"

    # If continuous metric over continuous parameter -> line plot
    if measurement.type == float and has_continuous_param(data):
        return "line"

    # Default: simple bar
    return "simple_bar"

def has_continuous_param(data: pd.DataFrame) -> bool:
    """Check if data has continuous parameter (e.g., matrix_size)."""
    # Check if numeric parameter with >5 unique values
    for col in data.columns:
        if data[col].dtype in [int, float]:
            if data[col].nunique() > 5:
                return True
    return False
```

### Example Workflow

```python
# User defines experiment with composite measurements
cholesky = Experiment(
    measurements=[
        Measurement("init_time", ...),
        Measurement("convert_time", ...),
        Measurement("compute_time", ...),
        CompositeMeasurement(
            name="wall_time",
            components=["init_time", "convert_time", "compute_time"],
            ...
        ),
    ],
    ...
)

# Run experiment
runner = ExperimentRunner(cholesky)
results = runner.run()

# Visualize - automatically creates stacked plot for wall_time
viz = VisualizationEngine(cholesky.measurements, cholesky.parameters)
plots = viz.generate_all_plots(results)

# wall_time plot will show:
# - Stacked bars with init_time (bottom), convert_time (middle), compute_time (top)
# - Different shading/hatching for each component
# - Total time labeled on top
# - Grouped by matrix_size and num_blocks
# - Faceted by scheduler
```

### Benefits

1. **Type Safety**: Measurements are type-checked at extraction time
2. **Automatic Visualization**: No manual plotting code per experiment
3. **Composability**: Define complex measurements from simple ones
4. **Flexibility**: Can plot components individually or together
5. **Consistency**: All experiments use same visualization logic
6. **Declarative**: Specify what to measure, framework handles how to plot

---

## Benefits

### For Development
- Single test instance, fast iteration
- Full debugger support (gdb, lldb)
- Same infrastructure as experiments
- No switching between "test" and "experiment" modes

### For Measurement
- Batch generation and execution
- SLURM integration
- Automatic result collection
- Build time measurement (always fresh builds)

### For Maintenance
- Zero CMake duplication
- Single source of truth
- Tests and experiments use same code path
- Easy to extend with new applications

---

## Design Decisions (Resolved)

1. **Force rebuild of applications**: ✓ Option A - Custom target that cleans before build
   - Ensures accurate compilation time measurement
   - Preserves CMake's dependency tracking for iara-opt

2. **Instance naming**: ✓ Readable with parameter names
   - Format: `matrix_size=2048_num_blocks=2_scheduler=vf-omp`
   - Easy to identify, grep-able, self-documenting

3. **IaRa parameter handling**: ✓ Global for now
   - Keep IaRa params (memory_allocator, parallel_hash) global
   - Can extend to per-experiment later if needed

4. **CTest integration**: ✓ Declarative per-instance
   - Mark instances as tests in experiment definition YAML
   - Dev instances = tests, batch experiment instances = not tests
   - Example: `is_test: true` in instance metadata

5. **Backward compatibility**: ✓ No backward compat, preserve functionality
   - Clean break from old test/ structure
   - **Preserve**: Logging, timestamping, result organization
   - Regenerate needed tests from experiment definitions

6. **Measurement validation**: ✓ Validate at all stages
   - Definition time: Check component references valid
   - Extraction time: Type-check, handle missing gracefully
   - Runtime: Continue experiment even if some runs fail (experiments are long)
   - Logging: Log all errors, save partial results

7. **Visualization**: ✓ Automatic + configuration with full feature support
   - **Support all current plot types**:
     - Separate plots per matrix_size (faceting)
     - Grouped bars per num_blocks
     - Stacked bars for composite measurements
   - **Both matplotlib AND plotly**
   - **Unified rendering**: Extract common plotting logic to avoid duplication
   - **Styling**: Alpha + hatch shading for stacked (matplotlib), match in plotly
   - **Error bars**: By default (bootstrap CI)
   - **Colors**: Consistent scheduler colors across both backends

---

## Comparison: From Scratch vs. Extending Spinner

### What is Spinner?

[Spinner](https://github.com/LSC-Unicamp/spinner) is an open-source HPC benchmarking tool from LSC-Unicamp that:
- Runs parameterized sweeps via YAML configuration
- Executes commands with template variables (`{{param}}`)
- Captures output with regex patterns
- Automatically retries failed runs
- Stores results in dataframes (pickle)
- Supports basic plotting (x_axis, y_axis, group_by)

### Spinner's Architecture

```yaml
metadata:
  runs: 10
  timeout: 300
  retry: 1

applications:
  my_app:
    command: ./a.out --size {{matrix_size}} --blocks {{num_blocks}}
    capture:
      - pattern: "Time: (\\d+\\.\\d+)"
    plot:
      x_axis: matrix_size
      y_axis: captured_value

benchmarks:
  sweep1:
    matrix_size: [2048, 4096]
    num_blocks: [2, 4]
```

### Feature Comparison

| Feature | IaRa Needs | Spinner Has | Gap Analysis |
|---------|-----------|-------------|--------------|
| **YAML Configuration** | ✓ Per-application | ✓ Global config | Need app-specific YAML |
| **Parameter Sweeps** | ✓ With constraints | ✓ Basic Cartesian product | Need constraint validation |
| **Computed Parameters** | ✓ (block_size = matrix_size / num_blocks) | ✗ | Must add |
| **Measurements** | ✓ Type-safe, composite | ✓ Basic regex capture | Need type system, composition |
| **Build Integration** | ✓ CMake, compile-time measurement | ✗ | Must add completely |
| **CTest Integration** | ✓ Generate test instances | ✗ | Must add |
| **Visualization** | ✓ Stacked bars, faceting, matplotlib+plotly | ✓ Basic plots | Need major extension |
| **SLURM Support** | ✓ | ? (unclear) | May need extension |
| **Result Storage** | ✓ Timestamped CSVs | ✓ Pickle files | Need CSV export |
| **Error Handling** | ✓ Continue on failure, partial results | ✓ Retry logic | Good foundation |

### Option 1: Build From Scratch

**Pros:**
- Complete control over architecture
- Tailored exactly to IaRa workflow
- No dependencies on external project maintenance
- Clean integration with CMake

**Cons:**
- Must implement: YAML parsing, parameter sweeps, retry logic, result storage
- ~2-3 weeks extra development time
- Reinventing working solutions

**Estimated Effort:**
- **CMake Unification**: 3-4 days (same either way)
- **Python Framework**: 2 weeks
  - YAML parsing & validation: 2 days
  - Parameter system: 2 days
  - Runner & execution: 3 days
  - Measurements & parsing: 2 days
  - Result storage: 1 day
- **Visualization**: 1 week
- **Total**: ~4 weeks

### Option 2: Fork/Extend Spinner

**Pros:**
- YAML parsing, parameter sweeps, retry logic already done
- Result storage, logging infrastructure exists
- Active project (latest release v1.2.0, 97 commits)
- MIT licensed (permissive)
- Python package, easy to install

**Cons:**
- Need to understand their codebase (schema.py is 14KB)
- Architecture may not fit IaRa workflow
- **CRITICAL GAPS**:
  - No build system integration (CMake)
  - No CTest generation
  - No composite measurements
  - Visualization too basic (no stacking, faceting, error bars)
  - No per-application configuration (global only)

**Estimated Effort:**
- **Learning Spinner**: 1-2 days
- **CMake Integration**: 4-5 days (harder - not designed for this)
- **Extend Schema**: 2 days
  - Add computed parameters
  - Add constraints
  - Add composite measurements
  - Add per-app config
- **Build System Hooks**: 3 days
  - CMake generation
  - Compile-time measurement
  - CTest integration
- **Visualization Rewrite**: 1 week (their system too basic)
- **Total**: ~3.5 weeks

### REVISED Recommendation: Extend Spinner as LSC Contribution

**Context**: You are part of LSC, Spinner is your lab's tool. Contributing these features benefits the entire lab and HPC community.

**New Strategy: Extend Spinner with IaRa Features**

This approach provides:

1. **Lab-Wide Benefits**:
   - Other LSC projects can use build system integration
   - Composite measurements benefit all benchmarking work
   - Advanced visualization helps everyone
   - Shared maintenance across lab members

2. **Upstream Contribution Opportunities**:
   - Type-safe measurement system (general feature)
   - Computed parameters (widely useful)
   - Constraint validation (common need)
   - Build system hooks (CMake, Make, etc.)
   - Advanced visualization (matplotlib + plotly)
   - Per-application configuration

3. **IaRa-Specific Extensions**:
   - Keep as plugins/extensions if too specific
   - Or propose as optional features
   - CTest generation might be IaRa-specific

### Proposed Extension Architecture

**Core Spinner Enhancements** (upstream contributions):

```yaml
# Enhanced Spinner schema - backward compatible

# 1. Per-application configuration (NEW)
applications:
  cholesky:
    # Existing fields
    command: ./a.out --size {{matrix_size}}
    capture: [...]

    # NEW: Type-safe measurements
    measurements:
      - name: wall_time
        type: float
        unit: s
        pattern: "Time: (\\d+\\.\\d+)"
        components: [init_time, compute_time]  # NEW: Composite

      - name: init_time
        type: float
        unit: s
        pattern: "Init: (\\d+\\.\\d+)"

    # NEW: Build integration (optional)
    build:
      system: cmake  # or make, script
      target: cholesky-app
      measure_time: true

    # NEW: Advanced visualization
    visualization:
      stacked: [wall_time]  # Use stacked bars for composites
      facet_by: scheduler
      error_bars: true

# 2. Enhanced parameter system (NEW)
parameters:
  matrix_size:
    type: int
    range: [2048, 4096, 8192]
    description: Matrix dimension

  num_blocks:
    type: int
    range: [1, 2, 4, 8]

  # NEW: Computed parameters
  block_size:
    type: int
    computed: "matrix_size // num_blocks"

  # NEW: Constraints
  constraints:
    - "num_blocks <= matrix_size"
    - "matrix_size % num_blocks == 0"

# 3. Named experiment sets (NEW - like our YAML proposal)
experiments:
  dev:
    description: Development test
    parameters:
      matrix_size: [2048]
      num_blocks: [2]

  scaling:
    description: Full sweep
    parameters:
      matrix_size: all
      num_blocks: all
    execution:
      repetitions: 10
```

**IaRa-Specific Plugin** (separate package or optional feature):

```python
# spinner_iara/ - Plugin for Spinner
from spinner import ApplicationAdapter

class IaRaAdapter(ApplicationAdapter):
    """Spinner adapter for IaRa applications with CMake."""

    def build(self, config):
        """Generate CMakeLists, build with CMake, measure time."""
        # Generate CMakeLists.txt from config
        # Run cmake/make with timing
        # Return build metrics

    def generate_ctest_instances(self, config):
        """Generate CTest test instances for debugging."""
        # IaRa-specific: Create test/Iara/ equivalents
```

### Implementation Plan for Spinner Extension

**Phase 1: Core Enhancements (Upstream to Spinner)**

1. **Extend schema.py** (2-3 days):
   - Add type-safe measurement definitions
   - Add computed parameters
   - Add constraints
   - Add per-application config option
   - Maintain backward compatibility

2. **Type-safe measurement system** (2 days):
   - `Measurement` class with type checking
   - `CompositeMeasurement` for aggregation
   - Integrate with existing capture system

3. **Advanced visualization** (1 week):
   - Add plotly backend (keep existing matplotlib)
   - Stacked bar plots for composites
   - Faceting support
   - Error bars (bootstrap CI)
   - Consistent styling

4. **Build system support** (3 days):
   - Generic `BuildSystem` interface
   - `CMakeBuildSystem` implementation
   - `MakefileBuildSystem` implementation
   - Build time measurement

**Phase 2: IaRa-Specific Extensions** (can be separate repo)

1. **CMake generation** (2 days):
   - Generate CMakeLists.txt from Spinner config
   - Call `iara_add_application()` from unified CMake functions

2. **CTest integration** (1 day):
   - Generate test instances for debugging
   - Mark with `generate_tests: true` flag

3. **IaRa adapter** (1 day):
   - Integrate with IaRa build workflow
   - Handle schedule generation, runtime linking

**Total Effort**: ~3.5 weeks (same as before, but now it benefits the lab!)

### Contribution Strategy

1. **Discuss with Spinner maintainers**:
   - Present type-safe measurements use case
   - Show need for build integration
   - Demonstrate visualization improvements
   - Get feedback on architecture

2. **Incremental PRs**:
   - PR 1: Type-safe measurements + composites
   - PR 2: Computed parameters + constraints
   - PR 3: Build system integration
   - PR 4: Advanced visualization (plotly + stacking)
   - PR 5: Per-application config

3. **IaRa-specific parts**:
   - Keep as `spinner-iara` plugin package
   - Or add as optional feature with `pip install spinner[iara]`

### Benefits of This Approach

**For LSC/Spinner**:
- Type-safe measurements help all HPC benchmarking
- Build integration useful for compiled benchmarks
- Better visualization for publications
- More powerful than current Spinner

**For IaRa**:
- Leverage existing, tested infrastructure
- Lab support for maintenance
- Potential for other labs to use IaRa
- Collaboration with Spinner developers

**For HPC Community**:
- Open-source contribution
- Published tool with these features
- Can cite Spinner paper + your extensions

### Revised Conclusion

**Extend Spinner** with these features as **LSC contributions**:
- Benefits entire lab, not just IaRa
- Strengthens Spinner as HPC tool
- Shared maintenance burden
- Publication opportunities
- Keep IaRa-specific parts as plugin

This is **the better choice** given your LSC membership!

---

## Implementation Progress

### Phase 1: CMake Unification ✅ COMPLETED

**Status**: All tasks completed successfully

**What was done**:

1. **Created [cmake/IaRaApplications.cmake](cmake/IaRaApplications.cmake)** - Unified build functions (~700 lines)
   - `iara_setup_environment()` - One-time environment setup
   - `iara_collect_args()` - Utility for collecting args from env
   - `iara_runtime_sources()` - Determines runtime sources based on scheduler
   - `iara_map_scheduler()` - Maps scheduler names to IaRa components
   - `iara_is_baseline_scheduler()` - Checks if scheduler needs IaRa runtime
   - `iara_add_application()` - Unified application build function

2. **Refactored [test/CMakeLists.txt](test/CMakeLists.txt)** - Reduced from ~700 to ~60 lines
   - Removed all duplicated functions
   - Now includes shared functions from `cmake/IaRaApplications.cmake`
   - `iara_add_runtime_test()` is now a thin wrapper around `iara_add_application()`
   - Preserves all existing functionality (CTest integration, build targets, etc.)

3. **Refactored [experiment/cholesky/CMakeLists.txt](experiment/cholesky/CMakeLists.txt)** - Reduced from ~470 to ~80 lines
   - Removed all duplicated functions
   - Now uses shared `iara_add_application()` function
   - Maintains all experiment functionality

4. **Simplified build types in [CMakeLists.txt](CMakeLists.txt)**:
   - **Before**: 7 build types (Debug, Release, CompilerDebug, TestDebug, ExperimentSize, ExperimentPerf, ExperimentProf)
   - **After**: 3 build types:
     - `Debug` - Maximum debug info, `-O0 -g -gdwarf`
     - `Release` - Performance optimized, `-O3 -DNDEBUG -march=native -flto`
     - `RelWithDebInfo` - Profiling, `-O3 -g -gdwarf -DNDEBUG -march=native -flto`

**Impact**:
- Eliminated ~90% code duplication between test and experiment builds
- Single source of truth for build logic
- Bug fixes now apply automatically to both tests and experiments
- Clearer build type semantics
- Foundation for future YAML-based configuration

**Files Modified**:
- [cmake/IaRaApplications.cmake](cmake/IaRaApplications.cmake) - NEW FILE
- [test/CMakeLists.txt](test/CMakeLists.txt) - ~700 → ~60 lines
- [experiment/cholesky/CMakeLists.txt](experiment/cholesky/CMakeLists.txt) - ~470 → ~80 lines
- [CMakeLists.txt](CMakeLists.txt) - Simplified build types section

### Next Steps

1. **Fork and setup Spinner** ✅
   - Spinner cloned to `/scratch/pedro.ciambra/repos/spinner-fork`
   - Examined schema structure (pydantic-based)

2. **Extend Spinner with IaRa features** (Ready to begin)
   - Add type-safe measurements and composites
   - Add computed parameters and constraints
   - Add build system integration
   - Add YAML per-application configuration
   - Unify visualization (matplotlib + plotly)
