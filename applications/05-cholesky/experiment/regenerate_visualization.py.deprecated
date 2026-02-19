#!/usr/bin/env python3
"""
Regenerate the visualization notebook from the most recent experiment results.

This script finds the latest results CSV file and regenerates the Jupyter notebook
with interactive Plotly visualizations, without re-running the experiments.

Usage:
    python3 regenerate_visualization.py              # Use latest results
    python3 regenerate_visualization.py <csv_file>  # Use specific CSV file
"""

import sys
import argparse
from pathlib import Path
from datetime import datetime

# Import the visualization generator
sys.path.insert(0, str(Path(__file__).parent))
from visualize_notebook import create_notebook


def find_latest_results() -> Path:
    """Find the most recent results CSV file."""
    results_dir = Path(__file__).parent / 'results'

    if not results_dir.exists():
        raise FileNotFoundError(f"Results directory not found: {results_dir}")

    # Find all CSV files matching the pattern results_YYYYMMDD_HHMMSS.csv
    csv_files = sorted(results_dir.glob('results_[0-9]*.csv'))

    if not csv_files:
        raise FileNotFoundError(f"No results CSV files found in {results_dir}")

    latest = csv_files[-1]
    return latest


def main():
    parser = argparse.ArgumentParser(
        description='Regenerate visualization notebook from experiment results'
    )
    parser.add_argument(
        'csv_file',
        nargs='?',
        help='Path to results CSV file (defaults to most recent)'
    )
    parser.add_argument(
        '--output', '-o',
        help='Output notebook path (defaults to analysis_TIMESTAMP.ipynb in results dir)'
    )
    parser.add_argument(
        '--build-dir', '-b',
        default='build_experiments',
        help='Path to build directory for binary size analysis (default: build_experiments)'
    )

    args = parser.parse_args()

    # Find CSV file
    if args.csv_file:
        csv_path = Path(args.csv_file)
        if not csv_path.exists():
            print(f"ERROR: CSV file not found: {csv_path}", file=sys.stderr)
            sys.exit(1)
    else:
        try:
            csv_path = find_latest_results()
            print(f"Using latest results: {csv_path.name}")
        except FileNotFoundError as e:
            print(f"ERROR: {e}", file=sys.stderr)
            sys.exit(1)

    # Determine output path
    if args.output:
        output_path = Path(args.output)
    else:
        # Extract timestamp from CSV filename and use same timestamp for notebook
        csv_name = csv_path.stem  # e.g., 'results_20251201_140043'
        if 'results_' in csv_name:
            timestamp = csv_name.replace('results_', '')  # Extract '20251201_140043'
        else:
            timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
        output_path = csv_path.parent / f'analysis_{timestamp}.ipynb'

    # Build directory
    build_dir = Path(args.build_dir)
    if not build_dir.exists():
        print(f"WARNING: Build directory not found: {build_dir}")
        build_dir = None

    # YAML config in same directory as script
    yaml_config = Path(__file__).parent / 'experiments.yaml'
    if not yaml_config.exists():
        print(f"ERROR: experiments.yaml not found at {yaml_config}", file=sys.stderr)
        sys.exit(1)

    # Generate notebook
    print(f"Regenerating visualization from: {csv_path}")
    print(f"Output notebook: {output_path}")

    try:
        notebook_path = create_notebook(csv_path, output_path, build_dir, yaml_config)
        print(f"\n✓ Notebook regenerated successfully!")
        print(f"\nTo view the notebook:")
        print(f"  jupyter notebook {notebook_path}")
    except Exception as e:
        print(f"ERROR: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == '__main__':
    main()
