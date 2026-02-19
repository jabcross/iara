#!/usr/bin/env python3
"""
Master orchestration script for experiment analysis.

Runs the complete measurement collection and analysis workflow:
1. Collect binary section sizes
2. Collect runtime measurements
3. Merge all measurements into results CSV
4. Generate Jupyter notebook with visualizations
5. Execute notebook with nbconvert

Usage:
    python3 run_experiment_analysis.py [OPTIONS]

Options:
    --build-dir DIR         Path to build directory
    --results-dir DIR       Path to save results
    --app-name NAME         Application name (e.g., '05-cholesky')
    --experiment-set SET    Experiment set name (defaults to 'regression')
    --skip-binary           Skip binary data collection
    --skip-runtime          Skip runtime measurement collection
    --skip-merge            Skip measurement merging
    --skip-notebook         Skip notebook generation and execution
    --help                  Show this help message
"""

import sys
import argparse
import subprocess
from pathlib import Path
from datetime import datetime

# Add applications/experiment-framework to path for imports
sys.path.insert(0, str(Path(__file__).parent))

from collect_binary_data import collect_binary_data
from collect_runtime_measurements import collect_runtime_data
from merge_measurements import merge_measurements
from notebook_generator import create_notebook


def log_info(msg):
    print(f"[INFO] {msg}")


def log_success(msg):
    print(f"[SUCCESS] {msg}")


def log_error(msg):
    print(f"[ERROR] {msg}", file=sys.stderr)


def main():
    parser = argparse.ArgumentParser(
        description="Run complete experiment analysis workflow",
        formatter_class=argparse.RawDescriptionHelpFormatter
    )

    parser.add_argument("--build-dir", type=Path, default=None,
                        help="Path to build directory (default: ./build_experiments)")
    parser.add_argument("--results-dir", type=Path, default=None,
                        help="Path to save results (default: applications/<app>/experiment/results)")
    parser.add_argument("--app-name", type=str, default="05-cholesky",
                        help="Application name (default: 05-cholesky)")
    parser.add_argument("--experiment-set", type=str, default="regression",
                        help="Experiment set name (default: regression)")
    parser.add_argument("--skip-binary", action="store_true",
                        help="Skip binary data collection")
    parser.add_argument("--skip-runtime", action="store_true",
                        help="Skip runtime measurement collection")
    parser.add_argument("--skip-merge", action="store_true",
                        help="Skip measurement merging")
    parser.add_argument("--skip-notebook", action="store_true",
                        help="Skip notebook generation and execution")

    args = parser.parse_args()

    # Set build_dir to default if not provided
    if args.build_dir is None:
        # Try to find build_experiments relative to this script
        script_dir = Path(__file__).parent.parent.parent
        args.build_dir = script_dir / "build_experiments"

    log_info("="*70)
    log_info("Experiment Analysis Workflow")
    log_info("="*70)
    log_info(f"Build directory: {args.build_dir}")
    log_info(f"Application: {args.app_name}")
    log_info(f"Experiment set: {args.experiment_set}")
    if args.results_dir:
        log_info(f"Results directory: {args.results_dir}")

    # Step 1: Collect binary data
    if not args.skip_binary:
        log_info("\nStep 1: Collecting binary data...")
        try:
            binary_csv = collect_binary_data(
                build_dir=args.build_dir,
                app_name=args.app_name,
                experiment_set=args.experiment_set
            )
            if binary_csv:
                log_success(f"Binary data collected: {binary_csv}")
            else:
                log_error("Failed to collect binary data")
                return 1
        except Exception as e:
            log_error(f"Error collecting binary data: {e}")
            import traceback
            traceback.print_exc()
            return 1
    else:
        log_info("\nStep 1: Skipping binary data collection (--skip-binary)")

    # Step 2: Collect runtime measurements
    if not args.skip_runtime:
        log_info("\nStep 2: Collecting runtime measurements...")
        try:
            runtime_csv = collect_runtime_data(
                build_dir=args.build_dir,
                app_name=args.app_name,
                experiment_set=args.experiment_set
            )
            if runtime_csv:
                log_success(f"Runtime measurements collected: {runtime_csv}")
            else:
                log_error("Failed to collect runtime measurements")
                return 1
        except Exception as e:
            log_error(f"Error collecting runtime measurements: {e}")
            import traceback
            traceback.print_exc()
            return 1
    else:
        log_info("\nStep 2: Skipping runtime measurement collection (--skip-runtime)")

    # Step 3: Merge measurements
    if not args.skip_merge:
        log_info("\nStep 3: Merging measurements...")
        try:
            results_csv = merge_measurements(
                build_dir=args.build_dir,
                results_dir=args.results_dir,
                app_name=args.app_name,
                experiment_set=args.experiment_set
            )
            if results_csv:
                log_success(f"Measurements merged: {results_csv}")
            else:
                log_error("Failed to merge measurements")
                return 1
        except Exception as e:
            log_error(f"Error merging measurements: {e}")
            import traceback
            traceback.print_exc()
            return 1
    else:
        log_info("\nStep 3: Skipping measurement merging (--skip-merge)")

    # Step 4: Generate and execute notebook
    if not args.skip_notebook:
        log_info("\nStep 4: Generating Jupyter notebook with visualizations...")
        try:
            # Get the results CSV that was just created
            if not args.skip_merge:
                # Find the most recent results CSV
                if args.results_dir is None:
                    results_dir = Path('/scratch/pedro.ciambra/repos/iara') / 'applications' / args.app_name / 'experiment' / 'results'
                else:
                    results_dir = args.results_dir

                # Find latest results CSV
                csv_files = sorted(results_dir.glob('results_*.csv'), key=lambda p: p.stat().st_mtime, reverse=True)
                if not csv_files:
                    log_error("No results CSV found to generate notebook from")
                    return 1

                results_csv = csv_files[0]

                # Get path to experiments.yaml for the application
                yaml_path = Path('/scratch/pedro.ciambra/repos/iara') / 'applications' / args.app_name / 'experiment' / 'experiments.yaml'

                # Generate notebook
                notebook_path = create_notebook(
                    csv_file=results_csv,
                    output_notebook=results_dir / f'results_analysis_{datetime.now().strftime("%Y%m%d_%H%M%S")}.ipynb',
                    yaml_config_path=yaml_path
                )
                log_success(f"Notebook generated: {notebook_path}")

                # Execute notebook with nbconvert
                log_info("Executing notebook with nbconvert...")
                output_html = notebook_path.with_suffix('.html')
                try:
                    subprocess.run([
                        'jupyter', 'nbconvert',
                        '--to', 'html',
                        '--execute',
                        '--ExecutePreprocessor.timeout=300',
                        '--output', str(output_html),
                        str(notebook_path)
                    ], check=True, capture_output=True, text=True)
                    log_success(f"Notebook executed and converted to HTML: {output_html}")
                except subprocess.CalledProcessError as e:
                    log_error(f"Failed to execute notebook: {e.stderr}")
                    return 1
                except FileNotFoundError:
                    log_error("jupyter or nbconvert not found. Install with: pip install jupyter nbconvert")
                    return 1
            else:
                log_info("Skipping notebook generation (measurement merging was skipped)")
        except Exception as e:
            log_error(f"Error generating notebook: {e}")
            import traceback
            traceback.print_exc()
            return 1
    else:
        log_info("\nStep 4: Skipping notebook generation (--skip-notebook)")

    log_info("\n" + "="*70)
    log_success("Experiment analysis workflow completed!")
    log_info("="*70)

    return 0


if __name__ == '__main__':
    sys.exit(main())
