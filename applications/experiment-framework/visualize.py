#!/usr/bin/env python3
"""
Generic visualization tool for experiment results.

Generates Plotly visualizations from experiment CSV results using configuration
from experiments.yaml. Works with any application that follows the experiment
framework schema.

Usage:
  python3 visualize.py <csv_file> [--yaml <config.yaml>] [--output <dir>]
"""

import sys
import argparse
from pathlib import Path

# Try to import visualization module from framework or local directory
try:
    from visualization_declarative import generate_visualizations_from_yaml
except ImportError:
    # Try relative import if running from application directory
    sys.path.insert(0, str(Path(__file__).parent))
    from visualization_declarative import generate_visualizations_from_yaml

from base_runner import load_yaml_config


def main():
    parser = argparse.ArgumentParser(
        description='Generate visualizations from experiment results CSV'
    )
    parser.add_argument('csv_file', help='Path to results CSV file')
    parser.add_argument('--yaml', '-y', help='Path to experiments.yaml config (default: same dir as CSV)')
    parser.add_argument('--output', '-o', help='Output directory for plots (default: results/plots)')

    args = parser.parse_args()

    csv_path = Path(args.csv_file)

    if not csv_path.exists():
        print(f"ERROR: CSV file not found: {csv_path}", file=sys.stderr)
        sys.exit(1)

    # Find YAML config
    if args.yaml:
        yaml_path = Path(args.yaml)
    else:
        # Look in same directory as CSV
        yaml_path = csv_path.parent / 'experiments.yaml'
        if not yaml_path.exists():
            # Try parent directory
            yaml_path = csv_path.parent.parent / 'experiments.yaml'

    if not yaml_path.exists():
        print(f"ERROR: experiments.yaml not found", file=sys.stderr)
        print(f"  Searched: {csv_path.parent}/experiments.yaml", file=sys.stderr)
        sys.exit(1)

    print(f"Loading configuration from: {yaml_path}")
    print(f"Loading results from: {csv_path}")

    try:
        yaml_config = load_yaml_config(yaml_path)

        # Extract scheduler order from config
        scheduler_order = []
        for param in yaml_config.get('parameters', []):
            if param.get('name') == 'scheduler':
                scheduler_order = param.get('choices', [])
                break

        if not scheduler_order:
            scheduler_order = []

        print(f"Scheduler order: {scheduler_order}")
        print()

        # Generate visualizations using declarative specs from YAML
        generate_visualizations_from_yaml(str(csv_path), str(yaml_path), scheduler_order)

        print("\nVisualizations generated successfully!")

    except Exception as e:
        print(f"ERROR: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == '__main__':
    main()
