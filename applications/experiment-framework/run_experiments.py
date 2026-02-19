#!/usr/bin/env python3
"""
Master front-end script for running IaRa experiments end-to-end.

This script orchestrates the complete experiment workflow:
1. Generate CMakeLists.txt from experiments.yaml
2. Clean build directories to force full rebuilds
3. Configure CMake build system
4. Build all test instances sequentially
5. Collect binary data and runtime measurements
6. Merge results and generate analysis notebook

Usage:
    python3 run_experiments.py [OPTIONS]

Options:
    --iara-dir DIR              Path to IaRa repository (default: auto-detect)
    --build-dir DIR             Path to build directory (default: build_experiments)
    --app-name NAME             Application to test (default: 05-cholesky)
    --experiment-set SET        Experiment set name (default: regression)
    --skip-analysis             Skip measurement collection and analysis
    --help                      Show this help message
"""

import sys
import os
import argparse
import subprocess
from pathlib import Path
from datetime import datetime


def log_info(msg):
    print(f"[INFO] {msg}")


def log_success(msg):
    print(f"[SUCCESS] {msg}")


def log_error(msg):
    print(f"[ERROR] {msg}", file=sys.stderr)


def run_command(cmd, description, cwd=None, check=True):
    """Run a command and report results."""
    log_info(f"Running: {description}")
    log_info(f"  Command: {' '.join(cmd)}")
    try:
        result = subprocess.run(
            cmd,
            cwd=cwd,
            check=check,
            capture_output=False,
            text=True
        )
        if result.returncode == 0:
            log_success(f"{description} completed")
        else:
            log_error(f"{description} failed with exit code {result.returncode}")
            return False
        return True
    except Exception as e:
        log_error(f"{description} error: {e}")
        return False


def find_iara_dir():
    """Auto-detect IARA_DIR by looking for cmake/IaRaApplications.cmake."""
    # Try environment variable first
    if 'IARA_DIR' in os.environ:
        return Path(os.environ['IARA_DIR'])

    # Try relative to this script
    script_dir = Path(__file__).parent.parent.parent
    if (script_dir / 'cmake' / 'IaRaApplications.cmake').exists():
        return script_dir

    # Try current directory
    if (Path.cwd() / 'cmake' / 'IaRaApplications.cmake').exists():
        return Path.cwd()

    return None


def main():
    parser = argparse.ArgumentParser(
        description="Run IaRa experiments end-to-end",
        formatter_class=argparse.RawDescriptionHelpFormatter
    )

    parser.add_argument("--iara-dir", type=Path, default=None,
                        help="Path to IaRa repository (default: auto-detect)")
    parser.add_argument("--build-dir", type=Path, default=None,
                        help="Path to build directory (default: build_experiments)")
    parser.add_argument("--app-name", type=str, default="05-cholesky",
                        help="Application to test (default: 05-cholesky)")
    parser.add_argument("--experiment-set", type=str, default="regression",
                        help="Experiment set name (default: regression)")
    parser.add_argument("--skip-analysis", action="store_true",
                        help="Skip measurement collection and analysis")

    args = parser.parse_args()

    # Find IaRa directory
    if args.iara_dir is None:
        args.iara_dir = find_iara_dir()
        if args.iara_dir is None:
            log_error("Could not find IaRa repository. Set IARA_DIR or use --iara-dir")
            return 1

    args.iara_dir = args.iara_dir.resolve()
    log_info(f"Using IARA_DIR: {args.iara_dir}")

    # Set build directory
    if args.build_dir is None:
        args.build_dir = args.iara_dir / "build_experiments"
    args.build_dir = args.build_dir.resolve()

    # Verify application exists
    app_dir = args.iara_dir / "applications" / args.app_name
    if not app_dir.exists():
        log_error(f"Application directory not found: {app_dir}")
        return 1

    yaml_path = app_dir / "experiment" / "experiments.yaml"
    if not yaml_path.exists():
        log_error(f"experiments.yaml not found: {yaml_path}")
        return 1

    log_info("="*70)
    log_info("IaRa Experiment Workflow")
    log_info("="*70)
    log_info(f"Application: {args.app_name}")
    log_info(f"Experiment set: {args.experiment_set}")
    log_info(f"Build directory: {args.build_dir}")

    # Step 1: Generate CMakeLists.txt from experiments.yaml
    log_info("\n" + "="*70)
    log_info("Step 1: Generating CMakeLists.txt from experiments.yaml")
    log_info("="*70)

    generate_script = args.iara_dir / "scripts" / "generate_experiments.py"
    if not generate_script.exists():
        log_error(f"generate_experiments.py not found: {generate_script}")
        return 1

    env = os.environ.copy()
    env['IARA_DIR'] = str(args.iara_dir)

    result = subprocess.run(
        [sys.executable, str(generate_script), str(app_dir)],
        cwd=args.iara_dir,
        env=env,
        capture_output=False
    )
    if result.returncode != 0:
        log_error("CMakeLists.txt generation failed")
        return 1

    # Step 2: Clean build directory to force full rebuilds
    log_info("\n" + "="*70)
    log_info("Step 2: Cleaning build directory for full rebuilds")
    log_info("="*70)

    app_build_dir = args.build_dir / args.app_name
    if app_build_dir.exists():
        log_info(f"Removing: {app_build_dir}")
        import shutil
        shutil.rmtree(app_build_dir)
        log_success(f"Cleaned {app_build_dir}")
    else:
        log_info(f"Build directory does not exist yet: {app_build_dir}")

    # Step 3: Configure CMake (always run fresh configuration)
    log_info("\n" + "="*70)
    log_info("Step 3: Configuring CMake")
    log_info("="*70)

    # Create build directory
    args.build_dir.mkdir(parents=True, exist_ok=True)

    cmake_cmd = [
        "cmake",
        "-S", str(args.iara_dir),
        "-B", str(args.build_dir),
        "-DCMAKE_BUILD_TYPE=Release",
        "-DIARA_BUILD_TESTS=ON",
        f"-DIARA_DIR={args.iara_dir}"
    ]

    if not run_command(cmake_cmd, "CMake configure", check=True):
        return 1

    # Step 4: Build tests
    log_info("\n" + "="*70)
    log_info("Step 4: Building test instances")
    log_info("="*70)
    log_info(f"Building all {args.app_name}_{args.experiment_set} tests")
    log_info("Tests will be built sequentially (one at a time) for accurate timing")

    # Build all test targets for this app/experiment set using ctest
    log_info("\nBuilding tests via ctest...")

    # Use ctest to build tests sequentially (only the build-* targets, not run-* targets)
    ctest_cmd = [
        "ctest",
        "-j", "1",  # Sequential (one at a time)
        "-V",  # Verbose output
        "-R", f"^build-{args.app_name}_{args.experiment_set}_"  # Only match build targets
    ]

    # Run ctest from the build directory
    result = subprocess.run(
        ctest_cmd,
        cwd=str(args.build_dir),
        capture_output=False
    )

    if result.returncode != 0:
        log_error(f"Some tests failed to build (this may be expected if some schedulers are not implemented)")
    else:
        log_success(f"Successfully built all {args.app_name}_{args.experiment_set} tests")

    # Step 5: Collect measurements and generate analysis
    if not args.skip_analysis:
        log_info("\n" + "="*70)
        log_info("Step 5: Collecting measurements and generating analysis")
        log_info("="*70)

        # Import and run the analysis workflow
        sys.path.insert(0, str(args.iara_dir / "applications" / "experiment-framework"))

        from collect_binary_data import collect_binary_data
        from collect_runtime_measurements import collect_runtime_data
        from merge_measurements import merge_measurements
        from notebook_generator import create_notebook

        # Collect binary data
        log_info("\nCollecting binary data...")
        binary_csv = collect_binary_data(
            build_dir=args.build_dir,
            app_name=args.app_name,
            experiment_set=args.experiment_set
        )
        if not binary_csv:
            log_error("Failed to collect binary data")
            return 1
        log_success(f"Binary data collected: {binary_csv}")

        # Collect runtime measurements
        log_info("\nCollecting runtime measurements...")
        runtime_csv = collect_runtime_data(
            build_dir=args.build_dir,
            app_name=args.app_name,
            experiment_set=args.experiment_set
        )
        if not runtime_csv:
            log_error("Failed to collect runtime measurements")
            return 1
        log_success(f"Runtime measurements collected: {runtime_csv}")

        # Merge measurements
        log_info("\nMerging measurements...")
        results_csv = merge_measurements(
            build_dir=args.build_dir,
            app_name=args.app_name,
            experiment_set=args.experiment_set
        )
        if not results_csv:
            log_error("Failed to merge measurements")
            return 1
        log_success(f"Measurements merged: {results_csv}")

        # Generate notebook
        log_info("\nGenerating analysis notebook...")
        try:
            notebook_path = create_notebook(
                csv_file=Path(results_csv),
                yaml_config_path=yaml_path
            )
            log_success(f"Notebook generated: {notebook_path}")

        except Exception as e:
            log_error(f"Error generating notebook: {e}")
            import traceback
            traceback.print_exc()
            return 1
    else:
        log_info("\nStep 5: Skipping analysis (--skip-analysis)")

    log_info("\n" + "="*70)
    log_success("Experiment workflow completed successfully!")
    log_info("="*70)

    return 0


if __name__ == '__main__':
    sys.exit(main())
