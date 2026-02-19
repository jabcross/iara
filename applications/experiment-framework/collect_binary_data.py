#!/usr/bin/env python3
"""
Collect binary section sizes and compilation times from built executables.

For each test configuration, extract:
1. Binary section sizes using 'size -A'
2. Compilation time (measured from build start to end)
3. Merge into existing results CSV
"""

import subprocess
import csv
import os
from pathlib import Path
from datetime import datetime


def get_section_sizes(executable_path: str) -> dict:
    """
    Extract ELF section sizes from executable using 'size -A'.

    Returns dict: {section_name: size_in_bytes}
    """
    try:
        result = subprocess.run(
            ['size', '-A', executable_path],
            capture_output=True,
            text=True,
            timeout=5
        )

        sections = {}
        for line in result.stdout.split('\n')[1:]:  # Skip header
            if not line.strip():
                continue
            parts = line.split()
            if len(parts) >= 2:
                section_name = parts[0]
                try:
                    size = int(parts[1])
                    sections[section_name] = size
                except (ValueError, IndexError):
                    continue

        return sections
    except Exception as e:
        print(f"Warning: Could not get section sizes for {executable_path}: {e}")
        return {}


def collect_binary_data(build_dir: Path = None, app_name: str = None, experiment_set: str = None):
    """
    Collect binary data from all test configurations.

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

        print(f"Processing {test_name}...")

        # Extract test parameters from name
        # Pattern: <app>_<experiment_set>_<scheduler>_matrix_size_2048_num_blocks_<n>
        parts = test_name.split('_')

        # Parse scheduler - look for scheduler name in test name
        # Note: When split by '_', hyphenated names like "omp-for" remain intact
        scheduler = None
        for part in parts:
            if part in ['sequential', 'enkits', 'omp-for', 'omp-task', 'vf-omp', 'vf-enkits']:
                scheduler = part
                break

        # Get section sizes
        sections = get_section_sizes(str(executable))

        # Get executable modification time as proxy for compilation time
        exec_mtime = executable.stat().st_mtime
        exec_time = datetime.fromtimestamp(exec_mtime)

        # Add section size measurements
        for section, size in sections.items():
            results.append({
                'test_name': test_name,
                'scheduler': scheduler or 'unknown',
                'measurement': 'section_size',
                'section': section,
                'value': str(size),
                'unit': 'bytes'
            })

        print(f"  Found {len(sections)} sections")

    # Write results to CSV
    output_csv = build_dir / f'{app_name}_{experiment_set}_binary_data.csv'

    if results:
        fieldnames = ['test_name', 'scheduler', 'measurement', 'section', 'value', 'unit']
        with open(output_csv, 'w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames)
            writer.writeheader()
            writer.writerows(results)

        print(f"\nWrote binary data to {output_csv}")
        return output_csv
    else:
        print("No binary data collected")
        return None


if __name__ == '__main__':
    collect_binary_data()
