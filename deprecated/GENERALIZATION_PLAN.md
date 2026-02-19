# Experiment Framework Generalization Plan

## Overview
This plan outlines the strategy to generalize the experiment framework shared between Cholesky, SIFT, and the upcoming Degridder applications. The goal is to eliminate code duplication while maintaining the flexibility for app-specific behavior through declarative YAML configuration.

## Current State Analysis

### Code Duplication
- **Cholesky**: 507 lines in run_experiments.py
- **SIFT**: 505 lines in run_experiments.py (copy-paste with bugs)
- **Shared Code**: ~400+ lines (80% identical)
- **App-Specific Code**: ~100 lines (20%)

### Visualization Code (Cholesky Only)
- **visualization.py**: 531 lines - Plotly visualization engine
- **visualize_notebook.py**: 267 lines - Jupyter notebook generator
- **visualize_results.py**: 544 lines - CLI visualization tool
- **regenerate_visualization.py**: 116 lines - Regenerate from CSV
- **Status**: SIFT has no visualization; Degridder uses Jupyter notebooks

### Configuration
- **Cholesky experiments.yaml**: 306 lines - Comprehensive
- **SIFT experiments.yaml**: 128 lines - Minimal (copy template)
- **Degridder**: No YAML config, uses Jupyter + shell scripts

### Known Issues
1. **SIFT Runner Bug**: References undefined variables (matrix_size, num_blocks, block_size) instead of image_size
2. **Hardcoded App Names**: Test filter uses 'cholesky'/'sift' strings throughout
3. **Parameter Mapping**: No declarative way to specify parameter→environment variable mapping
4. **Test Naming**: Test directory patterns hardcoded in create_test_directory()
5. **CMakeLists.txt Comments**: Hardcoded references to "Matrix Size" and "Cholesky"

## Phase 1: Consolidate Shared Code

### 1.1 Create Base Experiment Runner
**File**: `applications/experiment-framework/base_runner.py`

**Extract from cholesky/run_experiments.py:**
```python
# Purely generic, no app-specific references:
- load_yaml_config(yaml_path)
- get_computed_params(config)
- validate_combination(params, constraints)
- compute_derived_params(params, computed_params)
- generate_combinations(exp_set, constraints, computed_params)
- get_measurement_unit(config, measurement_name)
- setup_cmake_build(app_name, build_dir)
- find_executable(test_dir, scheduler)
- run_executable(exe_path, params, timeout, env_vars)
- parse_csv_results(csv_path)
- write_csv_results(results, csv_path)
- main_cli_parser()  # Shared argument parsing
```

**Strategy**:
- Keep 100% of logic intact (no refactoring)
- Move to framework module
- Import and use in both cholesky/sift runners

### 1.2 Create App-Specific Runner Base Class
**File**: `applications/experiment-framework/app_runner.py`

```python
class AppExperimentRunner:
    def __init__(self, yaml_config_path, app_name, app_filter):
        self.yaml_config = load_yaml_config(yaml_config_path)
        self.app_name = app_name
        self.app_filter = app_filter

    def create_test_directory(self, params):
        """App must implement: construct test dir name from params"""
        raise NotImplementedError

    def get_parameter_exports(self, params):
        """App must implement: map params to shell environment variables"""
        raise NotImplementedError

    def run(self, experiment_set_name):
        """Framework provides orchestration"""
        # Use base_runner functions
        # Call app-specific methods when needed
```

**Usage**:
```python
# cholesky/experiment/run_experiments.py
from experiment_framework.app_runner import AppExperimentRunner

class CholekskyRunner(AppExperimentRunner):
    def create_test_directory(self, params):
        return f"05-cholesky-{params['matrix_size']}_{params['num_blocks']}_{params['scheduler']}"

    def get_parameter_exports(self, params):
        return {
            'MATRIX_SIZE': params['matrix_size'],
            'NUM_BLOCKS': params['num_blocks'],
            'SCHEDULER': params['scheduler'],
            'BLOCK_SIZE': params['block_size'],
        }

if __name__ == '__main__':
    runner = CholekskyRunner('experiments.yaml', 'cholesky', 'cholesky')
    runner.run(args.experiment_set)
```

### 1.3 Fix SIFT Runner Bugs
**File**: `applications/sift/experiment/run_experiments.py`

**Bugs to Fix**:
1. Line 92-107: Wrong variable references in parameters.sh writing
2. Line 314: Test filter should use 'sift' not 'cholesky'
3. Line 352: Wrong column names in test_name pattern

**Simple Fix**: Replace entire content with AppExperimentRunner subclass:
```python
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).parent.parent.parent / 'experiment-framework'))
from app_runner import AppExperimentRunner

class SiftRunner(AppExperimentRunner):
    def create_test_directory(self, params):
        return f"08-sift-{params['image_size']}_{params['scheduler']}"

    def get_parameter_exports(self, params):
        return {
            'IMAGE_SIZE': params['image_size'],
            'SCHEDULER': params['scheduler'],
        }

if __name__ == '__main__':
    runner = SiftRunner('experiments.yaml', 'sift', 'sift')
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('-e', '--experiment-set', required=False, help='Experiment set')
    parser.add_argument('--list', action='store_true', help='List sets')
    args = parser.parse_args()
    runner.run(args.experiment_set if args.experiment_set else 'dev')
```

---

## Phase 2: Generalize Visualization Framework

### 2.1 Create Generic Visualization Engine
**File**: `applications/experiment-framework/visualizer.py`

**Strategy**: Keep Cholesky's Plotly logic, make parameter names generic via config

```python
class ExperimentVisualizer:
    def __init__(self, csv_path, yaml_config, param_mapping=None):
        """
        Generic visualizer that uses YAML to understand what to plot.

        param_mapping: Dict mapping config names to CSV column names
        Example: {'matrix_size': 'matrix_size', 'num_blocks': 'num_blocks'}
        """
        self.csv = pd.read_csv(csv_path)
        self.config = yaml_config
        self.param_map = param_mapping or {}
        self.plots = self.config.get('visualization', {}).get('plots', {})

    def generate_all(self):
        """Generate all plots specified in yaml['visualization']['plots']"""
        for plot_name, plot_config in self.plots.items():
            plot_type = plot_config.get('type', 'grouped_bars')
            if plot_type == 'grouped_bars':
                self._plot_grouped_bars(plot_name, plot_config)
            elif plot_type == 'stacked_bar':
                self._plot_stacked_bar(plot_name, plot_config)
            # etc.

    def _plot_grouped_bars(self, name, config):
        # Generic implementation that works for any parameter names
        # by reading metric/x_axis/etc from config
```

### 2.2 Update experiments.yaml Schema with Parameter Metadata
**Add to each experiments.yaml**:

```yaml
# Metadata for visualization and environment export
parameter_metadata:
  matrix_size:
    export_as: MATRIX_SIZE  # Environment variable name
    type: int
    label: "Matrix Size"
    csv_column: "matrix_size"

  num_blocks:
    export_as: NUM_BLOCKS
    type: int
    label: "Number of Blocks"
    csv_column: "num_blocks"

  scheduler:
    export_as: SCHEDULER
    type: str
    label: "Scheduler"
    csv_column: "scheduler"

# Test instance naming template
test_naming:
  prefix: "05-cholesky"
  template: "{prefix}-{matrix_size}_{num_blocks}_{scheduler}"

# Visualization parameter mapping (for generic visualizer)
visualization:
  parameter_mapping:
    primary_param: matrix_size    # One subplot per value
    group_param: num_blocks       # Group bars by this
    separate_plots_by: null       # Or separate by this param

  plots:
    runtime_performance:
      title: "Cholesky Runtime Performance"
      type: grouped_stacked_bars
      y_axis:
        metric: wall_time
        stack_composite: true
      x_axis:
        matrix_size: separate_plots
        num_blocks: group_by
```

### 2.3 Create Notebook Generator Base Class
**File**: `applications/experiment-framework/notebook_generator.py`

```python
def create_analysis_notebook(csv_path, yaml_config, param_mapping=None):
    """Generate Jupyter notebook from CSV results"""
    visualizer = ExperimentVisualizer(csv_path, yaml_config, param_mapping)
    cells = []

    # Title cell
    cells.append(markdown_cell(f"# {yaml_config['application']['name']} Experiment Analysis"))

    # Load data cell
    cells.append(code_cell(f"""
import pandas as pd
import matplotlib.pyplot as plt
from pathlib import Path

csv_file = '{csv_path}'
results = pd.read_csv(csv_file)
print(f'Loaded {{len(results)}} measurements')
"""))

    # Generate visualization cells for each plot
    for plot_name, plot_config in visualizer.plots.items():
        # Create cell that calls visualizer
        cells.append(code_cell(f"""
visualizer.generate_plot('{plot_name}')
"""))

    # Statistics cell
    cells.append(code_cell(generate_stats_code(yaml_config)))

    return write_notebook(cells, f"analysis_{timestamp}.ipynb")
```

### 2.4 Create Visualization CLI Tool
**File**: `applications/experiment-framework/visualize.py`

```python
#!/usr/bin/env python3
"""Generic visualization tool for experiment results"""

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('csv_file', help='Results CSV')
    parser.add_argument('--yaml', help='experiments.yaml config')
    parser.add_argument('--format', choices=['html', 'png', 'interactive'], default='html')
    args = parser.parse_args()

    config = load_yaml_config(args.yaml)
    visualizer = ExperimentVisualizer(args.csv_file, config)
    visualizer.generate_all()
    visualizer.save(format=args.format)
```

---

## Phase 3: Update YAML Schema

### 3.1 Extend experiments.yaml Template
Add to all experiments.yaml files:

```yaml
# Standard schema all experiments must follow
application:
  name: "app_name"
  description: "..."
  entry: "XX-app"
  sources:
    app_dir: "applications/app_name/src"
    test_dir: "test/Iara/XX-app"

# Mandatory section: How to map parameters
parameter_metadata:
  param_name:
    export_as: ENV_VAR_NAME      # What environment variable
    type: [int|str|float]
    label: "Display name"
    csv_column: "csv_column_name" # How it appears in CSV

# Mandatory section: Test directory naming
test_naming:
  prefix: "XX-app"
  template: "{prefix}-{param1}_{param2}_{scheduler}"

# App parameters, constraints, measurements (existing, unchanged)
parameters: [...]
constraints: [...]
measurements: [...]
experiment_sets: [...]

# Execution (optional, only if needed)
execution:
  repetitions: 5
  warmup: 1
  timeout: 300
  environment:
    VAR: value

# Visualization: Unified schema for all apps
visualization:
  enabled: true
  defaults:
    backend: plotly

  # Parameter mapping for generic visualizer
  parameter_mapping:
    x_axis_param: param_name     # What parameter for X-axis subplots
    group_param: param_name      # What parameter for grouped bars
    color_param: scheduler       # What parameter for colors

  # Declarative plot specifications
  plots:
    plot_name:
      title: "Title"
      type: [grouped_bars|stacked_bar|line_plot|scatter]
      y_axis:
        metric: measurement_name
        stack_composite: [true|false]
      x_axis:
        param_name: [separate_plots|group_by|ignore]
      colors: [by_scheduler|by_measurement|custom]
      error_bars: [std_dev|std_err|none]

# Output (existing, unchanged)
output:
  results_dir: "..."
  csv_file: "results_{timestamp}.csv"
```

---

## Phase 4: Implement Degridder Integration

### 4.1 Create Degridder Directory Structure
```
applications/degridder/
├── src/
│   ├── *.c                      # Copy from experiment/Code/src/
│   ├── *.h                      # Copy from experiment/Code/include/
│   └── kernels.cpp
├── experiment/
│   ├── experiments.yaml          # NEW: Declare parameters, sets, measurements
│   ├── run_experiments.py        # NEW: AppExperimentRunner subclass
│   ├── generate_test_instances_degridder.py
│   ├── generate_cmakelists_degridder.py
│   ├── results/                  # Results storage
│   └── profiling/                # Profiling data
├── codegen.sh                    # If topology generation needed
└── perf.sh                       # Performance testing script
```

### 4.2 Create Degridder experiments.yaml
```yaml
application:
  name: degridder
  description: "Radio Astronomy Degridding using IaRa dataflow"
  entry: "XX-degridder"  # TBD: exact entry code
  sources:
    app_dir: "applications/degridder/src"

parameters:
  # From original: templates use parameters like cell_size, grid_size
  cell_size:
    type: int
    description: "Cell size in pixels"

  grid_size:
    type: int
    description: "Grid dimension"

  scheduler:
    type: str
    choices: [sequential, vf-omp, vf-enkits]

measurements:
  wall_time:
    type: float
    parser:
      type: regex
      pattern: 'Wall time:\s+(\d+\.?\d*)'
    unit: s
  # Add others based on degridder output

experiment_sets:
  dev:
    description: "Quick test"
    parameters:
      cell_size: [256]
      grid_size: [512]
      scheduler: [sequential, vf-omp]

visualization:
  enabled: true
  parameter_mapping:
    x_axis_param: cell_size
    group_param: grid_size
  plots:
    runtime:
      title: "Degridder Runtime"
      type: grouped_bars
      y_axis:
        metric: wall_time
```

### 4.3 Implement DegridderRunner
```python
# applications/degridder/experiment/run_experiments.py
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).parent.parent.parent / 'experiment-framework'))
from app_runner import AppExperimentRunner

class DegridderRunner(AppExperimentRunner):
    def create_test_directory(self, params):
        return f"XX-degridder-{params['cell_size']}_{params['grid_size']}_{params['scheduler']}"

    def get_parameter_exports(self, params):
        return {
            'CELL_SIZE': str(params['cell_size']),
            'GRID_SIZE': str(params['grid_size']),
            'SCHEDULER': params['scheduler'],
        }
```

---

## Phase 5: Testing & Validation

### 5.1 Unit Tests for Base Runner
- Test parameter validation
- Test constraint checking
- Test parameter derivation
- Test CSV parsing

### 5.2 Integration Tests
- Run full experiment pipeline for Cholesky (should work unchanged)
- Run full pipeline for SIFT (with fixes)
- Run full pipeline for Degridder (new)
- Verify visualization output identical between old/new

### 5.3 Validation Checklist
- [ ] Cholesky experiments still run and produce identical results
- [ ] SIFT experiments run without bugs
- [ ] Both Cholesky and SIFT generate identical visualizations with new framework
- [ ] Degridder experiments run successfully
- [ ] All three apps can use generic visualization tools
- [ ] Notebook generation works for all apps
- [ ] CSV parsing handles app-specific measurement names

---

## Implementation Order

### Week 1: Foundation (Phases 1 & 2)
1. Create base_runner.py with extracted shared code
2. Create app_runner.py with base class
3. Fix SIFT runner bugs and make it inherit from AppExperimentRunner
4. Update Cholesky runner to inherit from AppExperimentRunner
5. Create visualizer.py with generic Plotly engine

### Week 2: Schema & Standardization (Phase 3)
6. Update experiments.yaml schema documentation
7. Update Cholesky experiments.yaml with parameter_metadata and test_naming sections
8. Update SIFT experiments.yaml with full visualization config and metadata
9. Create notebook_generator.py and CLI visualization tool

### Week 3: Degridder Integration (Phase 4)
10. Create applications/degridder/ structure
11. Copy source code from experiment/degridder/
12. Create experiments.yaml for Degridder
13. Implement DegridderRunner
14. Update CMakeLists.txt with degridder test handling

### Week 4: Testing & Polish (Phase 5)
15. Run full validation suite
16. Create documentation for extending framework to new apps
17. Clean up and optimize code
18. Commit all changes

---

## Risk Mitigation

**Risk**: Breaking existing Cholesky/SIFT workflows
**Mitigation**: Keep old runners functional in parallel, then switch after validation

**Risk**: Visualization output differs between old and new
**Mitigation**: Generate side-by-side comparison and validate pixel-perfect match

**Risk**: Degridder requires different measurement parsing
**Mitigation**: Make measurement parsing fully configurable in YAML

**Risk**: Parameter naming conflicts across apps
**Mitigation**: Use parameter_metadata section to make explicit mappings

---

## Success Criteria

1. **Code Reduction**: Eliminate 300+ lines of duplicate code
2. **Functionality**: All three apps work identically to before (or better)
3. **Extensibility**: Adding a 4th app requires only:
   - YAML config file (~100 lines)
   - AppExperimentRunner subclass (~20 lines)
   - Source code in src/ directory
4. **Visualization**: Same plots work for all apps with appropriate parameter substitution
5. **Maintenance**: No hardcoded app names in framework code

---

## Files to Create
- `applications/experiment-framework/base_runner.py` (400 lines)
- `applications/experiment-framework/app_runner.py` (150 lines)
- `applications/experiment-framework/visualizer.py` (800 lines, from Cholesky)
- `applications/experiment-framework/notebook_generator.py` (300 lines, from Cholesky)
- `applications/experiment-framework/visualize.py` (100 lines, new CLI)

## Files to Modify
- `applications/cholesky/experiment/run_experiments.py` (simplify to 50 lines)
- `applications/sift/experiment/run_experiments.py` (simplify to 50 lines)
- `applications/cholesky/experiment/experiments.yaml` (add metadata sections)
- `applications/sift/experiment/experiments.yaml` (add metadata, fix visualization)
- `cmake/IaRaApplications.cmake` (add degridder if test entry differs)

## Files to Create (New Apps)
- `applications/degridder/experiment/experiments.yaml` (150 lines)
- `applications/degridder/experiment/run_experiments.py` (50 lines)
- `applications/degridder/experiment/generate_test_instances_degridder.py`
- `applications/degridder/experiment/generate_cmakelists_degridder.py`
