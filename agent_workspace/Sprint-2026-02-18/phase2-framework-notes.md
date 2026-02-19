# Phase 2 Notes: Testing Framework

## Architecture Overview

The framework is a 4-phase pipeline invoked via `python3 -m tools.experiment_framework`:

1. **generate** → CMakeLists.txt from experiments.yaml
2. **build** → cmake configure + build with GNU time measurement
3. **execute** → run binaries N times, parse measurements from stdout/stderr
4. **visualize** → Vega-Lite JSON + Jupyter notebook

`run` command orchestrates all 4 phases sequentially.

## Module Summary

### config.py
- Loads/validates experiments.yaml (required keys: application, parameters, measurements, experiment_sets)
- SHA256 hash tracking for change detection (.experiments.yaml.hash)
- `get_parameter_combinations()`: Cartesian product of parameter values, filtered by constraints, extended by computed_parameters. Uses `eval()` for constraint/computed expressions.

### generator.py
- Generates CMakeLists.txt with `iara_add_test_instance()` calls (one per parameter combination)
- Each instance is guarded by `if(IARA_EXPERIMENT_INSTANCE STREQUAL "name")` — only one builds per cmake invocation
- Degridder-specific: `_add_degridder_defines()` with hardcoded size→parameter mapping (small/medium/large)
- Degridder topology.mlir generation from template with placeholder substitution
- Instance naming convention: `<app>_<scheduler>_<param1>_<value1>_...`

### builder.py
- Per-instance: clean → cmake configure → cmake build → parse GNU time files → extract binary size
- Timing from `iara_opt_time.txt`, `mlir_llvm_time.txt`, `clang_time.txt` (wall clock extracted)
- Results written to `results_<timestamp>.json` with schema_version 1.0.0
- Retry logic with exponential backoff (max_retries=2)
- Binary section analysis via `size -A`

### collector.py
- Executes binaries via `/usr/bin/time -v <executable>`, captures GNU time output
- Measurement parsing: regex, JSON, line-number based parsers (declared in YAML)
- Unit conversion (ms→s, KB→bytes, etc.)
- Custom converter functions via eval() of lambda expressions
- Statistics: mean, std, min, max, count per measurement across runs

### visualizer.py
- Translates Plotly-style declarative plot specs from YAML → Vega-Lite v5 JSON
- Supported types: grouped_bars, stacked_bar, grouped_stacked_bars, line_plot
- Auto unit conversion (selects μs/ms/s or B/KB/MB/GB based on magnitude)
- HTML generation with vega-embed CDN for standalone rendering

### notebook.py
- Generates nbformat v4 Jupyter notebooks programmatically
- Cells: metadata, failure report, data loading, statistics (pandas), visualizations (altair)
- Execution via nbconvert (with .env.cached spack environment)

## Key Design Decisions
- **One cmake invocation per instance** (via IARA_EXPERIMENT_INSTANCE filter) — ensures clean builds
- **Declarative YAML** defines parameter space, constraints, computed params, measurements, and visualization
- **JSON results schema** (v1.0.0) accumulates through phases: build adds compilation data, execute adds execution data
- **Degridder parameters are hardcoded** in generator.py (size→NUM_VISIBILITIES/GRID_SIZE mapping)

## Questions for User
1. The `_add_degridder_defines()` in generator.py has hardcoded size mappings (small→3924480 visibilities, etc.). Are these derived from the SKA dataset sizes, and should they always match the topology template substitutions?
2. The framework uses `eval()` for constraints, computed parameters, and custom converter lambdas. Is this a conscious trade-off for flexibility, or would you prefer a safer alternative?
3. The `onetime.sh` script support in the `run` command — is this used for degridder's data download/setup? I see `applications/13-degridder/onetime.sh` in the git status.
