#!/usr/bin/env python3
"""
Jupyter notebook generator for Cholesky experiment results.

Creates an interactive Jupyter notebook with plotly visualizations
embedded directly, allowing exploration of results within the notebook.
Visualization configuration is read from experiments.yaml.
"""

import sys
import json
import yaml
from pathlib import Path
from datetime import datetime
from typing import Optional, Dict, List
import pandas as pd

try:
    import plotly.graph_objects as go
    import plotly.express as px
    from plotly.subplots import make_subplots
    PLOTLY_AVAILABLE = True
except ImportError:
    PLOTLY_AVAILABLE = False


def load_yaml_config(yaml_path: Path) -> dict:
    """
    Load and parse the experiments.yaml configuration file.

    Args:
        yaml_path: Path to experiments.yaml

    Returns:
        Parsed YAML configuration dictionary
    """
    with open(yaml_path, 'r') as f:
        return yaml.safe_load(f)


def get_scheduler_order(yaml_config: dict) -> List[str]:
    """
    Extract scheduler ordering from YAML configuration.

    Args:
        yaml_config: Parsed YAML configuration

    Returns:
        List of scheduler names in definition order
    """
    for param in yaml_config.get('parameters', []):
        if param.get('name') == 'scheduler':
            return param.get('choices', [])
    return []


def create_notebook(csv_file: Path, output_notebook: Optional[Path] = None,
                   yaml_config_path: Optional[Path] = None) -> Path:
    """
    Create a Jupyter notebook with interactive visualizations.

    Args:
        csv_file: Path to CSV results file
        output_notebook: Path to output notebook (defaults to results_analysis.ipynb)
        yaml_config_path: Optional path to experiments.yaml (defaults to same dir as script)

    Returns:
        Path to created notebook
    """
    if not PLOTLY_AVAILABLE:
        print("ERROR: plotly not installed. Install with: pip install plotly", file=sys.stderr)
        sys.exit(1)

    if not csv_file.exists():
        print(f"ERROR: Results file not found: {csv_file}", file=sys.stderr)
        sys.exit(1)

    # Load YAML configuration
    if yaml_config_path is None:
        yaml_config_path = Path(__file__).parent / "experiments.yaml"

    if not yaml_config_path.exists():
        print(f"ERROR: experiments.yaml not found at {yaml_config_path}", file=sys.stderr)
        sys.exit(1)

    yaml_config = load_yaml_config(yaml_config_path)
    scheduler_order = get_scheduler_order(yaml_config)
    plot_configs = yaml_config.get('visualization', {}).get('plots', {})

    # Get application name and description
    app_config = yaml_config.get('application', {})
    app_name = app_config.get('name', 'Unknown Application')
    app_description = app_config.get('description', '')

    print(f"Loaded YAML config from {yaml_config_path}")
    print(f"Application: {app_name}")
    print(f"Scheduler order: {scheduler_order}")

    # Load results
    results = pd.read_csv(csv_file)
    print(f"Loaded {len(results)} result rows from {csv_file.name}")

    # Process results generically - only process columns that exist and have valid data
    # Convert run_num if it exists (standard framework column)
    if 'run_num' in results.columns:
        results['run_num'] = pd.to_numeric(results['run_num'], errors='coerce').astype('Int64')

    # Convert integer-type parameters if they exist and have valid data
    for col in results.columns:
        if col in ['matrix_size', 'num_blocks', 'block_size', 'matrix_dim', 'num_cores']:
            # Try to convert to int, but be lenient with NaN values
            if results[col].notna().any():
                results[col] = pd.to_numeric(results[col], errors='coerce')
                # Only convert to int if all non-null values are whole numbers
                if (results[col].dropna() % 1 == 0).all():
                    results[col] = results[col].astype('Int64')

    # Determine output notebook path
    if output_notebook is None:
        # Extract timestamp from CSV filename if it follows the pattern results_YYYYMMDD_HHMMSS.csv
        csv_stem = csv_file.stem  # e.g., "results_20251209_034301"
        if csv_stem.startswith("results_") and len(csv_stem) >= 16:
            timestamp = csv_stem[8:]  # Extract YYYYMMDD_HHMMSS
            output_notebook = csv_file.parent / f"results_analysis_{timestamp}.ipynb"
        else:
            output_notebook = csv_file.parent / "results_analysis.ipynb"
    else:
        output_notebook = Path(output_notebook)

    # Create notebook structure
    cells = []

    # Title cell
    title = f"{app_name} Experiment Results Analysis"
    cells.append({
        "cell_type": "markdown",
        "metadata": {},
        "source": [
            f"# {title}\n",
            f"\n",
            f"Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n",
            f"\n",
            f"Results file: `{csv_file.name}`\n",
            f"\n",
            f"Visualization configuration: `{yaml_config_path.name}`"
        ]
    })

    # Setup cell with imports and data loading
    framework_dir = Path(__file__).parent.absolute()

    cells.append({
        "cell_type": "code",
        "execution_count": None,
        "metadata": {},
        "outputs": [],
        "source": [
            "import sys\n",
            "import pandas as pd\n",
            "\n",
            "# Add framework path for visualization module\n",
            f"sys.path.insert(0, '{framework_dir}')\n",
            "from visualization_declarative import generate_visualizations_from_yaml\n",
            "from visualization import print_statistics\n",
            "\n",
            f"csv_file = '{csv_file.absolute()}'\n",
            f"yaml_file = '{yaml_config_path.absolute()}'\n",
            f"scheduler_order = {scheduler_order}\n",
            f"results = pd.read_csv(csv_file)\n",
            "print(f'Loaded {{len(results)}} measurements from {{csv_file}}')\n",
            "print(f'Schedulers: {{sorted(results[\"scheduler\"].unique()) if \"scheduler\" in results.columns else []}}')\n",
            "print(f'Parameters: {{sorted(set(results.columns) - {{\"test_name\", \"measurement\", \"value\", \"unit\", \"run_num\", \"scheduler\"}})}}')\n"
        ]
    })

    # Data summary cell - build summary dynamically based on what's in the results
    summary_items = [f"- **Total measurements**: {len(results)}"]

    # Add scheduler info if available
    if 'scheduler' in results.columns:
        schedulers = sorted(results['scheduler'].dropna().unique())
        summary_items.append(f"- **Schedulers**: {', '.join(schedulers)}")

    # Add measurement types
    if 'measurement' in results.columns:
        measurements = sorted(results['measurement'].dropna().unique())
        summary_items.append(f"- **Measurement types**: {', '.join(measurements)}")

    # Add parameter info for all non-standard columns
    parameter_cols = set(results.columns) - {'test_name', 'measurement', 'value', 'unit', 'run_num', 'scheduler', 'section'}
    for col in sorted(parameter_cols):
        unique_vals = results[col].dropna().unique()
        if len(unique_vals) <= 10:  # Only show if reasonable number of unique values
            summary_items.append(f"- **{col}**: {', '.join(map(str, sorted(unique_vals)))}")

    cells.append({
        "cell_type": "markdown",
        "metadata": {},
        "source": ["## Data Summary\n", "\n"] + [item + "\n" for item in summary_items]
    })

    # Create visualization cell
    cells.append({
        "cell_type": "markdown",
        "metadata": {},
        "source": ["## Interactive Visualizations"]
    })

    cells.append({
        "cell_type": "code",
        "execution_count": None,
        "metadata": {},
        "outputs": [],
        "source": [
            "generate_visualizations_from_yaml(csv_file, yaml_file, scheduler_order)\n"
        ]
    })

    # Statistics cell
    cells.append({
        "cell_type": "markdown",
        "metadata": {},
        "source": ["## Performance Statistics"]
    })

    cells.append({
        "cell_type": "code",
        "execution_count": None,
        "metadata": {},
        "outputs": [],
        "source": [
            "print_statistics(results, scheduler_order)\n"
        ]
    })

    # Raw data cell
    cells.append({
        "cell_type": "markdown",
        "metadata": {},
        "source": ["## Raw Data\n", "\nDisplay first 20 measurement rows"]
    })

    cells.append({
        "cell_type": "code",
        "execution_count": None,
        "metadata": {},
        "outputs": [],
        "source": ["results.head(20)"]
    })

    # Create notebook JSON structure
    notebook = {
        "cells": cells,
        "metadata": {
            "kernelspec": {
                "display_name": "Python 3",
                "language": "python",
                "name": "python3"
            },
            "language_info": {
                "name": "python",
                "version": "3.9.0"
            }
        },
        "nbformat": 4,
        "nbformat_minor": 2
    }

    # Write notebook
    with open(output_notebook, 'w') as f:
        json.dump(notebook, f, indent=2)

    print(f"Notebook saved to: {output_notebook}")
    return output_notebook


if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser(
        description='Create interactive Jupyter notebook for experiment analysis'
    )
    parser.add_argument('csv_file', help='Path to results CSV file')
    parser.add_argument('--output', '-o', help='Output notebook path (default: results_analysis.ipynb)')
    parser.add_argument('--yaml', '-y', help='Path to experiments.yaml (default: same dir as CSV)')

    args = parser.parse_args()

    csv_path = Path(args.csv_file)
    output_path = Path(args.output) if args.output else None
    yaml_path = Path(args.yaml) if args.yaml else None

    try:
        notebook_path = create_notebook(csv_path, output_path, yaml_path)
        print(f"\nNotebook created successfully!")
        print(f"To view: jupyter notebook {notebook_path}")
    except Exception as e:
        print(f"ERROR: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc()
        sys.exit(1)
