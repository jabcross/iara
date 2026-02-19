#!/usr/bin/env python3
"""
Run all test executables and collect runtime measurements.

For each test, run the executable and extract measurements:
- Initialization time
- Block conversion time
- Compute time (wall time)
- Memory usage (via /usr/bin/time)
"""

import subprocess
import re
import csv
from pathlib import Path


def extract_measurements_from_output(stdout: str, time_output: str = None) -> dict:
    """
    Extract timing and memory measurements from test executable output and time output.

    Args:
        stdout: Standard output from the test executable
        time_output: Output from /usr/bin/time -v command

    Returns dict with keys: init_time, convert_time, compute_time, max_rss_mb
    """
    measurements = {}

    # Extract initialization time
    match = re.search(r'Initialization time:\s+([\d.]+)\s*s', stdout)
    if match:
        measurements['init_time'] = float(match.group(1))

    # Extract block conversion time
    match = re.search(r'Block conversion time:\s+([\d.]+)\s*s', stdout)
    if match:
        measurements['convert_time'] = float(match.group(1))

    # Extract wall time (compute time)
    match = re.search(r'Wall time:\s+([\d.]+)\s*s', stdout)
    if match:
        measurements['compute_time'] = float(match.group(1))

    # Extract maximum resident set size from /usr/bin/time output
    # GNU time outputs "Maximum resident set size (kbytes): XXXX"
    if time_output:
        match = re.search(r'Maximum resident set size\s*\(([^)]+)\):\s+(\d+)', time_output)
        if match:
            unit = match.group(1).lower()
            value = int(match.group(2))
            # Convert to megabytes based on unit
            if 'kbytes' in unit or 'kb' in unit:
                measurements['max_rss_mb'] = value / 1024.0
            elif 'mbytes' in unit or 'mb' in unit:
                measurements['max_rss_mb'] = value
            elif 'gbytes' in unit or 'gb' in unit:
                measurements['max_rss_mb'] = value * 1024.0
            else:
                # Assume kilobytes as default for GNU time
                measurements['max_rss_mb'] = value / 1024.0

    return measurements


def collect_runtime_data(build_dir: Path = None, app_name: str = None, experiment_set: str = None):
    """
    Run all test executables and collect runtime measurements.

    Args:
        build_dir: Path to build directory (defaults to build_experiments)
        app_name: Application name (e.g., '05-cholesky')
        experiment_set: Experiment set name (defaults to 'regression')
    """
    if build_dir is None:
        build_dir = Path('/scratch/pedro.ciambra/repos/iara/build_experiments')

    if app_name is None:
        app_name = '05-cholesky'

    if experiment_set is None:
        experiment_set = 'regression'

    test_dir = build_dir / app_name / experiment_set

    if not test_dir.exists():
        print(f"Error: {test_dir} does not exist")
        return

    results = []

    # Find all test directories
    for test_case_dir in sorted(test_dir.iterdir()):
        if not test_case_dir.is_dir():
            continue

        test_name = test_case_dir.name
        executable = test_case_dir / 'a.out'

        if not executable.exists():
            print(f"Skipping {test_name}: executable not found")
            continue

        print(f"Running {test_name}...")

        # Extract test parameters from name
        parts = test_name.split('_')

        # Parse scheduler, matrix_size, and num_blocks
        # Note: When split by '_', hyphenated names like "omp-for" remain intact
        scheduler = None
        matrix_size = None
        num_blocks = None

        for i, part in enumerate(parts):
            # Parse scheduler - look for any known scheduler name
            if part in ['sequential', 'enkits', 'omp-for', 'omp-task', 'vf-omp', 'vf-enkits']:
                scheduler = part

            # Parse parameters
            if part == 'matrix_size' and i + 1 < len(parts):
                try:
                    matrix_size = int(parts[i + 1])
                except ValueError:
                    pass
            elif part == 'num_blocks' and i + 1 < len(parts):
                try:
                    num_blocks = int(parts[i + 1])
                except ValueError:
                    pass

        # Run executable with /usr/bin/time to collect memory measurements
        try:
            # Run with GNU time to capture memory statistics
            result = subprocess.run(
                ['/usr/bin/time', '-v', str(executable.resolve())],
                capture_output=True,
                text=True,
                timeout=600
            )

            # Extract measurements from both program output and time output
            # time -v puts its output to stderr
            measurements = extract_measurements_from_output(result.stdout, result.stderr)

            if not measurements:
                print(f"  Warning: Could not extract measurements")
                continue

            # Add to results
            for measurement_name, value in measurements.items():
                results.append({
                    'test_name': test_name,
                    'scheduler': scheduler or 'unknown',
                    'matrix_size': matrix_size,
                    'num_blocks': num_blocks,
                    'measurement': measurement_name,
                    'value': str(value),
                    'unit': 's' if measurement_name != 'max_rss_mb' else 'MB'
                })

            print(f"  Extracted measurements: {list(measurements.keys())}")

        except subprocess.TimeoutExpired:
            print(f"  Error: Execution timed out")
        except Exception as e:
            print(f"  Error: {e}")

    # Write results to CSV
    output_csv = build_dir / f'{app_name}_{experiment_set}_runtime_measurements.csv'

    if results:
        fieldnames = ['test_name', 'scheduler', 'matrix_size', 'num_blocks', 'measurement', 'value', 'unit']
        with open(output_csv, 'w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames)
            writer.writeheader()
            writer.writerows(results)

        print(f"\nWrote runtime measurements to {output_csv}")
        return output_csv
    else:
        print("No measurements collected")
        return None


if __name__ == '__main__':
    collect_runtime_data()
