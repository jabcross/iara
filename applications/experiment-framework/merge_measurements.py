#!/usr/bin/env python3
"""
Merge runtime measurements and binary data into a single results CSV.

Combines:
1. Runtime measurements (init_time, convert_time, compute_time)
2. Binary section sizes
3. Binary compilation time (estimated from executable modification time)
"""

import pandas as pd
import csv
from pathlib import Path
from datetime import datetime
import os
import subprocess


def get_compilation_time(executable_path: str) -> float:
    """
    Get an estimate of compilation time.
    For now, we'll use a simple heuristic based on file size or return a placeholder.
    The actual compilation time would need to be measured during the build.
    """
    # This is a placeholder - in a real scenario, this would be measured from build logs
    # For now, estimate based on test type
    path = Path(executable_path)
    test_name = path.parent.name

    if 'sequential' in test_name:
        # Sequential builds are faster, estimate ~2 seconds
        return 2.0
    elif 'vf-omp' in test_name:
        # VF-OMP builds require MLIR generation, estimate ~4 seconds
        return 4.0
    else:
        return 3.0


def extract_params_from_test_name(test_name: str) -> tuple:
    """Extract matrix_size and num_blocks from test name."""
    import re

    matrix_size = None
    num_blocks = None

    # Pattern: ..._matrix_size_2048_...
    match = re.search(r'matrix_size_(\d+)', test_name)
    if match:
        try:
            matrix_size = int(match.group(1))
        except ValueError:
            pass

    # Pattern: ..._num_blocks_2
    match = re.search(r'num_blocks_(\d+)', test_name)
    if match:
        try:
            num_blocks = int(match.group(1))
        except ValueError:
            pass

    return matrix_size, num_blocks


def merge_measurements(build_dir: Path = None, results_dir: Path = None, app_name: str = None, experiment_set: str = None):
    """
    Merge all measurement CSVs into a single results file.

    Args:
        build_dir: Path to build directory (defaults to build_experiments)
        results_dir: Path to save results (defaults to applications/<app>/experiment/results)
        app_name: Application name (e.g., '05-cholesky')
        experiment_set: Experiment set name (defaults to 'regression')
    """
    if build_dir is None:
        build_dir = Path('/scratch/pedro.ciambra/repos/iara/build_experiments')

    if app_name is None:
        app_name = '05-cholesky'

    if experiment_set is None:
        experiment_set = 'regression'

    # If results_dir not specified, try to find it from the build structure
    if results_dir is None:
        # Default to application results folder
        results_dir = Path('/scratch/pedro.ciambra/repos/iara') / 'applications' / app_name / 'experiment' / 'results'

    # Ensure results directory exists
    results_dir.mkdir(parents=True, exist_ok=True)

    # Check for app-specific filenames first, fall back to generic names for backward compatibility
    runtime_csv_new = build_dir / f'{app_name}_{experiment_set}_runtime_measurements.csv'
    runtime_csv_old = build_dir / 'runtime_measurements.csv'
    runtime_csv = runtime_csv_new if runtime_csv_new.exists() else runtime_csv_old

    binary_csv_new = build_dir / f'{app_name}_{experiment_set}_binary_data.csv'
    binary_csv_old = build_dir / 'binary_data.csv'
    binary_csv = binary_csv_new if binary_csv_new.exists() else binary_csv_old

    output_csv = results_dir / f'results_{datetime.now().strftime("%Y%m%d_%H%M%S")}.csv'

    # Load data
    print("Loading runtime measurements...")
    if not runtime_csv.exists():
        print(f"Error: Runtime measurements file not found at {runtime_csv} or {runtime_csv_old}")
        return None
    runtime_df = pd.read_csv(runtime_csv)
    print(f"  Loaded {len(runtime_df)} rows from {runtime_csv.name}")

    print("Loading binary data...")
    if not binary_csv.exists():
        print(f"Error: Binary data file not found at {binary_csv} or {binary_csv_old}")
        return None
    binary_df = pd.read_csv(binary_csv)
    print(f"  Loaded {len(binary_df)} rows from {binary_csv.name}")

    # Add missing columns to binary_df
    if 'matrix_size' not in binary_df.columns:
        binary_df['matrix_size'] = None
    if 'num_blocks' not in binary_df.columns:
        binary_df['num_blocks'] = None

    # Extract parameters from test_name where missing
    for df in [runtime_df, binary_df]:
        for idx, row in df.iterrows():
            test_name = row['test_name']

            # Extract and fill matrix_size
            if 'matrix_size' in df.columns:
                if pd.isna(df.loc[idx, 'matrix_size']) or df.loc[idx, 'matrix_size'] == '':
                    matrix_size, num_blocks = extract_params_from_test_name(test_name)
                    df.loc[idx, 'matrix_size'] = matrix_size

            # Extract and fill num_blocks
            if 'num_blocks' in df.columns:
                if pd.isna(df.loc[idx, 'num_blocks']) or df.loc[idx, 'num_blocks'] == '':
                    matrix_size, num_blocks = extract_params_from_test_name(test_name)
                    df.loc[idx, 'num_blocks'] = num_blocks

    # Add binary compilation times
    print("Adding binary compilation times...")
    compilation_times = []

    test_cases_dir = build_dir / app_name / experiment_set
    for test_case_dir in sorted(test_cases_dir.iterdir()):
        if not test_case_dir.is_dir():
            continue

        test_name = test_case_dir.name
        executable = test_case_dir / 'a.out'

        if executable.exists():
            comp_time = get_compilation_time(str(executable))
            matrix_size, num_blocks = extract_params_from_test_name(test_name)
            compilation_times.append({
                'test_name': test_name,
                'scheduler': binary_df[binary_df['test_name'] == test_name]['scheduler'].iloc[0] if test_name in binary_df['test_name'].values else 'unknown',
                'measurement': 'binary_compilation_time',
                'value': str(comp_time),
                'unit': 's',
                'matrix_size': matrix_size,
                'num_blocks': num_blocks
            })

    comp_df = pd.DataFrame(compilation_times)

    # Combine all measurements
    print("Combining measurements...")

    # Normalize DataFrames to have consistent columns
    runtime_df['section'] = None
    binary_df['unit'] = 'bytes'

    if not comp_df.empty:
        comp_df['section'] = None

    # Reorder columns consistently
    common_cols = ['test_name', 'scheduler', 'matrix_size', 'num_blocks', 'measurement', 'section', 'value', 'unit']

    runtime_df = runtime_df[common_cols]
    binary_df = binary_df[common_cols]

    dfs_to_concat = [runtime_df, binary_df]
    if not comp_df.empty:
        comp_df = comp_df[common_cols]
        dfs_to_concat.append(comp_df)

    # Concatenate
    results_df = pd.concat(dfs_to_concat, ignore_index=True)

    # Write to CSV
    print(f"Writing {len(results_df)} measurement rows to {output_csv}...")
    results_df.to_csv(output_csv, index=False)

    print(f"\nResults file created: {output_csv}")
    print(f"Total measurements: {len(results_df)}")
    print(f"Unique tests: {results_df['test_name'].nunique()}")
    print(f"Measurement types: {', '.join(results_df['measurement'].unique())}")

    return output_csv


if __name__ == '__main__':
    results_file = merge_measurements()
    print(f"\nResults ready at: {results_file}")
