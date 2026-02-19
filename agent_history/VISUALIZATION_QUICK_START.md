# Visualization Quick Start Guide

## Overview

The IaRa Experiment Framework Phase 4 provides automated Jupyter notebook generation with embedded visualizations and failure reports.

## Basic Usage

### Complete Pipeline

```bash
# 1. Generate CMakeLists.txt
python3 -m tools.experiment_framework generate --app 05-cholesky --set regression

# 2. Build instances
python3 -m tools.experiment_framework build --app 05-cholesky --set regression

# 3. Execute and collect measurements
python3 -m tools.experiment_framework execute --app 05-cholesky --set regression

# 4. Generate visualizations and notebook
python3 -m tools.experiment_framework visualize --app 05-cholesky --set regression
```

### Visualize Command Only

If you already have results from a previous execute run:

```bash
python3 -m tools.experiment_framework visualize --app 05-cholesky --set regression
```

### Using Custom Results File

```bash
python3 -m tools.experiment_framework visualize \
    --app 05-cholesky \
    --set regression \
    --results applications/05-cholesky/experiment/results/results_2024-01-15T100000Z.json
```

## Command Options

```
usage: python3 -m tools.experiment_framework visualize [-h] --app NAME --set NAME [--results PATH]

Required:
  --app NAME      Application name (e.g., '05-cholesky')
  --set NAME      Experiment set name (e.g., 'regression')

Optional:
  --results PATH  Path to specific results JSON file (default: latest in results/)
  -h, --help      Show help message
```

## What Gets Generated

### 1. Vega-Lite JSON Files

Location: `applications/<app>/experiment/results/plot_*.vl.json`

Example:
```
plot_runtime_performance.vl.json
plot_memory_usage.vl.json
plot_scheduler_comparison.vl.json
```

### 2. Jupyter Notebook

Location: `applications/<app>/experiment/results/analysis_<timestamp>.ipynb`

Example: `analysis_20240115T120000Z.ipynb`

## Notebook Structure

The generated notebook contains:

### 1. Experiment Metadata
- Application name and description
- Experiment set and timestamp
- Git commit hash
- Instance statistics (total, successful, failed)

### 2. Failure Report (if failures exist)
- Table with instance names and error summaries
- Collapsible full error details

### 3. Data Loading
- Python code to load results JSON
- Summary statistics

### 4. Statistical Analysis
- Pandas-based groupby analysis
- Mean, std, min, max by scheduler
- Formatted output

### 5. Visualizations
- One cell per plot defined in YAML
- Altair/Vega-Lite interactive charts
- Embedded directly in notebook

## Notebook Execution

By default, the notebook is automatically executed after generation. This:
- Runs all code cells
- Generates all plots
- Produces a complete analysis document

### Requirements

To execute notebooks, you need:

```bash
pip install jupyter nbconvert pandas altair
```

### If Jupyter Not Installed

The command will:
1. Generate the notebook successfully
2. Log a warning about missing jupyter
3. Exit with code 1 (partial success)
4. You can still open the notebook manually and execute it later

## Exit Codes

- **0:** Success (all steps completed)
- **1:** Partial success (notebook generated but not executed)
- **2:** Critical failure (missing data, config error, generation failure)

## Example Output

```
Generating Vega-Lite visualizations...
  Generated 3 plot(s)
Generating Jupyter notebook...
  Notebook created: applications/05-cholesky/experiment/results/analysis_20240115T120000Z.ipynb
Executing notebook...
  Notebook executed successfully

Visualization complete!
  Results: applications/05-cholesky/experiment/results/results_20240115T100000Z.json
  Notebook: applications/05-cholesky/experiment/results/analysis_20240115T120000Z.ipynb
  Plots: 3 Vega-Lite JSON files in applications/05-cholesky/experiment/results
```

## YAML Configuration

To enable visualizations, add a `visualization` section to your `experiments.yaml`:

```yaml
application:
  name: "05-cholesky"
  description: "Cholesky Decomposition Performance Study"

# ... parameters, experiment_sets, etc ...

visualization:
  plots:
    runtime_performance:
      title: "Runtime Performance by Scheduler"
      type: grouped_bars
      y_axis:
        metric: value
        measurements:
          - wall_time_s
          - user_time_s
      x_axis:
        field: scheduler

    memory_usage:
      title: "Memory Usage"
      type: grouped_bars
      y_axis:
        metric: value
        measurements:
          - max_rss_bytes
      x_axis:
        field: scheduler
```

## Viewing Notebooks

### In Jupyter

```bash
cd applications/05-cholesky/experiment/results
jupyter notebook analysis_20240115T120000Z.ipynb
```

### In JupyterLab

```bash
cd applications/05-cholesky/experiment/results
jupyter lab analysis_20240115T120000Z.ipynb
```

### In VS Code

Open the `.ipynb` file directly in VS Code with the Jupyter extension installed.

## Troubleshooting

### No Results Found

```
ERROR: No results JSON found in applications/05-cholesky/experiment/results
       Run the execute command first
```

**Solution:** Run the execute command before visualize:
```bash
python3 -m tools.experiment_framework execute --app 05-cholesky --set regression
```

### No Visualization Configuration

```
WARNING: No visualization configuration found in YAML
         Skipping plot generation
```

**Solution:** Add a `visualization.plots` section to your `experiments.yaml` file.

### Jupyter Not Found

```
WARNING: jupyter command not found. Install Jupyter with:
           pip install jupyter nbconvert
         Notebook generated but not executed: applications/.../analysis.ipynb
```

**Solution:** Install Jupyter:
```bash
pip install jupyter nbconvert
```

Or execute the notebook manually later.

### Execution Timeout

If notebook execution times out:
- The notebook is still generated
- You can execute it manually with a longer timeout:
```bash
jupyter nbconvert --execute --inplace --ExecutePreprocessor.timeout=600 analysis.ipynb
```

## Programmatic Usage

### Python API

```python
from pathlib import Path
from tools.experiment_framework.notebook import generate_notebook, execute_notebook
from tools.experiment_framework.visualizer import generate_vegalite_json
from tools.experiment_framework.config import load_experiments_yaml

# Load config and results
config = load_experiments_yaml(Path("applications/05-cholesky/experiment/experiments.yaml"))
results_path = Path("applications/05-cholesky/experiment/results/results_latest.json")
results_dir = results_path.parent

# Generate visualizations
vegalite_files = generate_vegalite_json(config, results_path, results_dir)

# Generate notebook
notebook_path = results_dir / "analysis.ipynb"
generate_notebook(config, results_path, vegalite_files, notebook_path)

# Execute notebook
execute_notebook(notebook_path, timeout=300)
```

### Individual Functions

```python
from tools.experiment_framework.notebook import (
    extract_failure_summary,
    generate_failure_markdown,
    get_failure_report_cell,
    create_metadata_cell,
    create_data_loading_cell,
    create_statistics_cell,
    create_visualization_cell,
)

# Extract failure summaries
failed_instances = [...]  # From results JSON
summaries = extract_failure_summary(failed_instances)

# Generate markdown report
markdown = generate_failure_markdown(failed_instances)

# Create individual cells
metadata_cell = create_metadata_cell(config, results)
data_cell = create_data_loading_cell(results_path)
stats_cell = create_statistics_cell(["wall_time_s", "max_rss_bytes"])
viz_cell = create_visualization_cell("runtime", Path("plot_runtime.vl.json"))
```

## Best Practices

1. **Run Full Pipeline:** Always run generate → build → execute → visualize in order
2. **Check YAML First:** Ensure your `experiments.yaml` has a `visualization.plots` section
3. **Use Latest Results:** The command auto-finds the latest results by default
4. **Review Failures:** Check the failure report section if any builds failed
5. **Version Control:** Consider committing executed notebooks for reproducibility

## Advanced Features

### Custom Statistics

Edit the generated notebook to add custom analysis:
1. Generate the notebook
2. Open in Jupyter
3. Add new cells with custom pandas/numpy analysis
4. Re-execute the notebook

### Custom Visualizations

Add more plot specifications to your YAML:
```yaml
visualization:
  plots:
    custom_scatter:
      title: "Custom Analysis"
      type: scatter
      # ... configuration
```

### Batch Processing

Visualize multiple experiments:
```bash
for app in 05-cholesky 06-qr 07-lu; do
    python3 -m tools.experiment_framework visualize --app $app --set regression
done
```

## Next Steps

1. Explore the generated notebook
2. Customize visualizations by editing YAML
3. Add custom analysis cells to notebooks
4. Share notebooks with collaborators
5. Export to HTML for static viewing: `jupyter nbconvert --to html analysis.ipynb`

## Support

For issues or questions:
1. Check this guide's troubleshooting section
2. Review the completion report: `PHASE_4_TASKS_4.3-4.5_COMPLETION_REPORT.md`
3. Examine test files for usage examples: `tests/test_notebook.py`
4. Check function docstrings: `help(generate_notebook)`
