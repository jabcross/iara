# Cholesky Decomposition Experiments

This directory contains tools for running robust performance experiments on the Cholesky decomposition implementation with different schedulers.

## Quick Start

### 1. Run a Small Test
```bash
# Source the environment first
source $IARA_DIR/.env

# Run a small subset of tests
python3 run_experiment.py --subset small
```

### 2. Run Full Experiment
```bash
python3 run_experiment.py \
    --matrix-sizes 2048 4096 8192 \
    --num-blocks 1 2 4 8 \
    --schedulers sequential omp-task virtual-fifo \
    --repetitions 10 \
    --warmup 2 \
    --output results.csv
```

### 3. Analyze Results
```bash
python3 analyze_results.py \
    --input results.csv \
    --output-stats statistics.csv \
    --output-comparison comparison.csv \
    --show-outliers
```

## Features

### `run_experiment.py`

**Robust experiment execution with:**
- ✅ Automated building of all configurations
- ✅ Build verification (smoke tests)
- ✅ Multiple repetitions with warmup runs
- ✅ Incremental result saving (crash-resilient)
- ✅ Detailed logging
- ✅ Error handling and retry logic
- ✅ Metadata tracking (timestamp, git commit, etc.)

**Command-line options:**
```
--matrix-sizes     List of matrix sizes (default: 2048 4096 ... 16384)
--num-blocks       List of block counts (default: 1 2 4 8 16 32)
--schedulers       List of schedulers (default: sequential omp-task virtual-fifo)
--repetitions      Number of runs per config (default: 10)
--warmup           Warmup runs before measurement (default: 2)
--output           Output CSV file (default: results.csv)
--skip-build       Skip build phase if binaries exist
--subset           Run predefined subset: small, medium, large
```

**Example subsets:**
- `--subset small`: Quick test (2 matrix sizes, 2 block configs, 5 reps)
- `--subset medium`: Moderate test (3 sizes, 4 block configs, 10 reps)
- `--subset large`: Full test (8 sizes, 6 block configs, 10 reps)

### `analyze_results.py`

**Statistical analysis with:**
- ✅ Mean, std, min, max, median per configuration
- ✅ 95% confidence intervals
- ✅ Coefficient of variation (measurement stability)
- ✅ Outlier detection (IQR method)
- ✅ Scheduler comparison and speedup calculation
- ✅ Summary statistics

**Outputs:**
- `statistics.csv`: Detailed stats per configuration
- `comparison.csv`: Scheduler speedups relative to baseline
- Console summary with key insights

## Output Files

### `results.csv`
Raw measurements with columns:
- `matrix_size`, `num_blocks`, `block_size`, `block_memory`
- `scheduler`: sequential, omp-task, or virtual-fifo
- `wall_time`: Measured execution time (seconds)
- `real_time`: Total elapsed time including overhead
- `run_number`: Which repetition (0-indexed, negative for warmup)
### `results.csv`
Raw measurements with columns:
- `matrix_size`, `num_blocks`, `block_size`, `block_memory`
- `scheduler`: sequential, omp-task, or virtual-fifo
- `wall_time`: Measured execution time (seconds)
- `real_time`: Total elapsed time including overhead
- `max_rss_kb`: Maximum resident set size in KB (memory usage)
- `run_number`: Which repetition (0-indexed, negative for warmup)
- `timestamp`: ISO format timestamp

### `build_metrics.csv`
Compilation and binary size metrics (measured once per configuration):
- `config_name`: Configuration identifier (e.g., 256_1_sequential)
- `compile_time`: Total compilation time in seconds
- `text_size`: Size of .text section (code) in bytes
- `data_size`: Size of .data section (initialized data) in bytes
- `bss_size`: Size of .bss section (uninitialized data) in bytes
- `total_size`: Total binary file size in bytes

### `statistics.csv`
Aggregated statistics:
- `count`: Number of valid measurements
- `mean`, `std`, `min`, `max`, `median`
- `ci_lower`, `ci_upper`: 95% confidence interval
- `cv_percent`: Coefficient of variation (%)

### `comparison.csv`
Scheduler performance comparison:
- Mean wall time per scheduler
- Speedup columns (e.g., `virtual-fifo_speedup`)

### `experiment.log`
Detailed execution log with:
- Build commands and results
- Run-by-run timings
- Error messages
- Timestamps

### `metadata.json`
Experiment metadata:
- Configuration parameters
- Git commit hash
- Timestamp
- Number of configurations

## Directory Structure

```
cholesky/
├── run_experiment.py       # Main experiment runner
├── analyze_results.py      # Statistical analysis
├── README.md              # This file
├── results.csv            # Raw measurements (generated)
├── statistics.csv         # Aggregated stats (generated)
├── comparison.csv         # Scheduler comparison (generated)
├── experiment.log         # Execution log (generated)
├── metadata.json          # Experiment metadata (generated)
├── build.ipynb           # Legacy: manual build notebook
├── plot.ipynb            # Legacy: manual plotting notebook
├── measurement.csv        # Legacy: old results
└── instances/            # Build artifacts (one dir per config)
    ├── 2048_1_sequential/
    │   └── build/
    │       └── a.out
    ├── 2048_1_omp-task/
    └── ...
```

## Best Practices

### For Quick Testing
```bash
# Test a single configuration
python3 run_experiment.py \
    --matrix-sizes 1024 \
    --num-blocks 4 \
    --schedulers virtual-fifo \
    --repetitions 3
```

### For Production Runs
```bash
# Full experiment with good statistics
python3 run_experiment.py \
    --subset large \
    --repetitions 20 \
    --warmup 3 \
    --output production_results.csv
```

### Incremental Runs
If you need to add more data to existing results, just append to the same file:
```bash
# First run
python3 run_experiment.py --matrix-sizes 2048 --output results.csv

# Add more data (appends to results.csv)
python3 run_experiment.py --matrix-sizes 4096 --output results.csv
```

### Resuming After Failures
The script saves results incrementally. If it crashes:
1. Check `experiment.log` to see what failed
2. Run again with `--skip-build` if builds completed
3. Results are appended, so no data is lost

## Visualization

You can still use the Jupyter notebooks for visualization:
1. Run experiments with `run_experiment.py`
2. Load `results.csv` or `statistics.csv` in `plot.ipynb`
3. Create custom plots as needed

Or use the analysis script to generate comparison tables, then visualize with your preferred tool.

## Troubleshooting

### "IARA_DIR environment variable not set"
Solution: `source $IARA_DIR/.env` before running

### "Build failed for ..."
Check `experiment.log` for detailed build errors. Common issues:
- Environment not sourced
- Missing dependencies
- Compiler errors

### High coefficient of variation (>10%)
Indicates measurement instability. Try:
- More warmup runs: `--warmup 5`
- More repetitions: `--repetitions 20`
- Running on idle system (close other programs)

### All runs timeout
Increase timeout in script (line ~79):
```python
timeout=300  # Change to 600 for 10 minutes
```
