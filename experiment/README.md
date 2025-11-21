# Experiment Framework

A reusable, data-driven framework for running build-and-benchmark experiments with automatic visualization generation.

## Overview

This framework provides:
- **Application-agnostic experiment orchestration** via the adapter pattern
- **Parallel SLURM execution** - jobs submitted immediately after builds complete
- **Automatic visualization generation** after experiments finish
- **Comprehensive metrics collection** (compile time, runtime, memory usage)
- **Incremental result saving** with timestamped outputs
- **Detailed logging** for reproducibility
- **Progress bars** - real-time progress tracking during builds and runs (optional, requires `tqdm`)

## Directory Structure

```
experiment/
├── experiment_framework.py        # Core reusable library
├── visualize_experiment.py        # Visualization generator
├── README.md                      # This file
└── <experiment_name>/             # Per-experiment directories
    ├── run_experiment_refactored.py  # Experiment-specific runner
    ├── instances/                 # Build artifacts (one per config)
    ├── results/                   # Runtime results CSVs
    ├── build_metrics/             # Build metrics CSVs
    ├── logs/                      # Execution logs
    ├── images/                    # Generated visualizations
    └── notebooks/                 # Timestamped notebook backups
```

## Dependencies

### Required
- Python 3.7+
- pandas
- matplotlib
- numpy

### Optional
- `tqdm` - For progress bars during experiments (highly recommended)
  ```bash
  pip install tqdm
  ```
  If not installed, the framework will work without progress bars.

## Quick Start

### Running an Experiment

```bash
cd experiment/cholesky
./run_experiment_refactored.py \
  --matrix-sizes 2048 4096 \
  --num-blocks 1 2 4 \
  --schedulers sequential omp-task virtual-fifo \
  --repetitions 10 \
  --warmup 2
```

### Key Features

#### 1. Parallel SLURM Execution

When using SLURM (auto-detected on `sorgan` or via `--slurm yes`):
- **Build phase**: Each configuration is built sequentially
- **Immediate submission**: As soon as a binary is built, all its SLURM jobs are submitted
- **Parallel execution**: Multiple configurations run on compute nodes while builds continue
- **Collection phase**: After all builds complete, results are collected from all jobs

**Timeline example:**
```
Build config1 → Build config2 → Build config3 → Collect all results
    ↓             ↓             ↓
  Run config1   Run config2   Run config3   (all running in parallel)
```

#### 2. Automatic Visualization

After experiments complete, visualizations are automatically generated:
- Compilation time comparison
- Runtime performance with bootstrap 95% CI error bars
- Memory usage (max RSS) with bootstrap 95% CI error bars
- Images saved to `experiment/<name>/images/`
- Notebook backup saved to `experiment/<name>/notebooks/visualize_results_<timestamp>.ipynb`

To generate visualizations manually:
```bash
python experiment/visualize_experiment.py experiment/cholesky
```

#### 3. Incremental Result Saving

Results are saved incrementally after each configuration completes, so data is preserved even if the experiment is interrupted.

## Creating a New Experiment

### 1. Implement an ApplicationAdapter

```python
from experiment_framework import ApplicationAdapter, ExperimentConfig
from pathlib import Path
from typing import List, Dict, Any, Optional

class MyAppAdapter(ApplicationAdapter):
    def __init__(self, sources_dir: Path):
        self.sources_dir = sources_dir

    def get_build_env(self, config: ExperimentConfig) -> Dict[str, str]:
        """Return environment variables for building."""
        return {
            "MY_APP_PARAM": str(config.params["my_param"]),
            "PATH_TO_TEST_SOURCES": str(self.sources_dir),
        }

    def get_cmake_args(self, config: ExperimentConfig) -> List[str]:
        """Return CMake arguments."""
        return [
            f"-DCMAKE_BUILD_TYPE=Release",
            f"-DMY_PARAM={config.params['my_param']}",
        ]

    def get_build_target(self, config: ExperimentConfig) -> str:
        """Return the make target name."""
        return f"my-app-{config.params['variant']}"

    def get_executable_path(self, config: ExperimentConfig, build_dir: Path) -> Path:
        """Return path to the built executable."""
        return build_dir / "bin" / "my_app"

    def parse_program_output(self, stdout: str, stderr: str) -> Optional[Dict[str, Any]]:
        """Parse application output and extract metrics."""
        # Parse stdout/stderr to extract metrics like wall_time, throughput, etc.
        for line in stdout.split('\n'):
            if line.startswith("Time:"):
                time = float(line.split(":")[-1].strip())
                return {"wall_time": time}
        return None  # Invalid output

    def get_run_command(self, executable: Path) -> List[str]:
        """Return the command to execute."""
        return [str(executable), "--flag", "value"]
```

### 2. Create Configuration Factory

```python
def create_my_app_config(size: int, variant: str) -> ExperimentConfig:
    """Factory for creating configurations."""
    name = f"{size}_{variant}"
    params = {
        "size": size,
        "variant": variant,
        # Add any other parameters
    }
    return ExperimentConfig(name=name, params=params)
```

### 3. Create Experiment Runner Script

```python
#!/usr/bin/env python3
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent.parent))

from experiment_framework import ExperimentRunner
from my_app_adapter import MyAppAdapter, create_my_app_config

def main():
    # Generate configurations
    configs = [
        create_my_app_config(size, variant)
        for size in [100, 200, 400]
        for variant in ["fast", "balanced", "small"]
    ]

    # Set up runner
    adapter = MyAppAdapter(sources_dir=Path("path/to/sources"))
    runner = ExperimentRunner(
        project_dir=Path("/path/to/project"),
        experiment_name="my_app",
        app_adapter=adapter,
        num_repetitions=10,
        warmup_runs=2
    )

    # Run experiment
    runner.run_experiment(configs)

    # Generate visualizations
    runner.run_visualization(scheduler_order=["variant1", "variant2"])

if __name__ == "__main__":
    main()
```

## Output Files

All outputs are timestamped with format `YYYYMMDD_HHMMSS`:

- **`results/results_<timestamp>.csv`**: Runtime measurements
  - Columns: config parameters, wall_time, real_time, max_rss_kb, run_number, timestamp
- **`build_metrics/build_metrics_<timestamp>.csv`**: Build metrics
  - Columns: config_name, compile_time (if measured)
- **`logs/experiment_<timestamp>.log`**: Detailed execution log
- **`metadata.json`**: Experiment metadata (configurations, git commit, etc.)
- **`images/*.png`**: Generated visualizations
- **`notebooks/visualize_results_<timestamp>.ipynb`**: Notebook backup

## Configuration Options

### ExperimentRunner Parameters

```python
ExperimentRunner(
    project_dir: Path,              # Root directory (e.g., IARA_DIR)
    experiment_name: str,           # Experiment name (e.g., "cholesky")
    app_adapter: ApplicationAdapter, # Your adapter implementation
    num_repetitions: int = 1,       # Measurement runs per config
    warmup_runs: int = 0,           # Warmup runs before measurements
    use_slurm: Optional[bool] = None, # None = auto-detect, True/False = force
    measure_compile_time: bool = True, # Whether to measure compile time
    slurm_config: Optional[Dict] = None # SLURM parameters override
)
```

### SLURM Configuration

Pass a dict to customize SLURM job parameters:

```python
slurm_config = {
    "nodes": 1,
    "ntasks": 1,
    "cpus_per_task": 48,
    "partition": "cpu",
    "nodelist": "compute[1-4]",
    "time_limit": "00:10:00"
}

runner = ExperimentRunner(..., slurm_config=slurm_config)
```

## CLI Arguments (Example: Cholesky)

```bash
--matrix-sizes SIZE [SIZE ...]     # Matrix sizes to test
--num-blocks NUM [NUM ...]          # Number of blocks to test
--schedulers SCHED [SCHED ...]      # Schedulers to test
--repetitions N                     # Measurement repetitions per config
--warmup N                          # Warmup runs per config
--skip-build                        # Skip build phase (binaries exist)
--skip-run                          # Skip run phase (only build)
--slurm {auto,yes,no}               # SLURM execution mode
--measure-compile-time              # Measure compilation time
```

## Extending the Framework

### Adding New Metrics

1. Collect metrics in your `parse_program_output()` method
2. Return them in the dict: `{"wall_time": 1.23, "my_metric": 456}`
3. They'll automatically appear in results CSV

### Custom Visualizations

1. Subclass `ExperimentVisualizer` in `visualize_experiment.py`
2. Add new plot methods
3. Call them in `generate_all()`

### Experiment-Specific Notebook Configuration

To customize image ordering and titles in generated notebooks, create a `notebook_config.py` file in your experiment directory:

```python
# experiment/<your_experiment>/notebook_config.py
from typing import List, Tuple

def get_image_info(image_files: List[str]) -> List[Tuple[str, str]]:
    """Return ordered list of (filename, title) pairs."""
    image_info = []

    # Add your experiment-specific images with custom titles
    if "my_custom_plot.png" in image_files:
        image_info.append(("my_custom_plot.png", "My Custom Plot Title"))

    # Add standard images
    standard = [
        ("runtime_performance.png", "Runtime Performance"),
        ("memory_usage.png", "Memory Usage"),
    ]
    for filename, title in standard:
        if filename in image_files:
            image_info.append((filename, title))

    return image_info
```

If no `notebook_config.py` exists, images will be displayed in alphabetical order with auto-generated titles.

### Supporting New Build Systems

The framework assumes CMake + make, but you can:
1. Override `build_configuration()` in `ExperimentRunner`
2. Implement custom build logic
3. Return build metrics dict

## Best Practices

1. **Use descriptive config names**: Include all varying parameters in the name
2. **Start with small experiments**: Test with 1-2 configs first
3. **Check logs**: Review `logs/experiment_*.log` for detailed execution info
4. **Version control metadata.json**: It captures git commit for reproducibility
5. **Keep notebooks updated**: After modifying visualizations, update the notebook backup

## Troubleshooting

**Build failures:**
- Check `logs/experiment_*.log` for CMake/make errors
- Verify environment variables are set correctly
- Ensure source paths exist

**SLURM jobs stuck:**
- Check SLURM queue: `squeue -u $USER`
- Review SLURM output files in `instances/<config>/slurm_jobs/`
- Increase time limit in `slurm_config`

**Visualization errors:**
- Ensure matplotlib and pandas are installed
- Check that results CSV files exist and are valid
- Review visualization log output for specific errors

**Missing results:**
- Check if `parse_program_output()` is correctly extracting metrics
- Verify program produces expected output format
- Look for errors in execution logs
