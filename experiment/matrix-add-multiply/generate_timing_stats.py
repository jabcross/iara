#!/usr/bin/env python3
"""
Generate timing statistics from timing_results.csv
"""

import csv
import statistics
import sys
import os
import random


def bootstrap_confidence_interval(data, n_bootstrap=10000, confidence=0.95):
    """Calculate bootstrap confidence interval for the mean"""
    if len(data) <= 1:
        return None, None

    bootstrap_means = []
    n = len(data)

    for _ in range(n_bootstrap):
        # Resample with replacement
        bootstrap_sample = [random.choice(data) for _ in range(n)]
        bootstrap_means.append(statistics.mean(bootstrap_sample))

    # Sort bootstrap means and find confidence interval
    bootstrap_means.sort()
    alpha = 1 - confidence
    lower_idx = int(alpha / 2 * n_bootstrap)
    upper_idx = int((1 - alpha / 2) * n_bootstrap)

    return bootstrap_means[lower_idx], bootstrap_means[upper_idx]


def generate_timing_stats(csv_file='timing_results.csv', output_file='timing_summary.txt'):
    """Generate timing statistics from CSV file and write to output file"""

    if not os.path.exists(csv_file):
        print(f"Error: {csv_file} not found")
        sys.exit(1)

    wall_times = []
    max_residents = []

    try:
        with open(csv_file, 'r') as f:
            reader = csv.DictReader(f)
            for row in reader:
                wall_times.append(float(row['wall_time_seconds']))
                max_residents.append(int(row['max_resident_size_kb']))
    except Exception as e:
        print(f"Error reading {csv_file}: {e}")
        sys.exit(1)

    if not wall_times:
        print(f"Error: No data found in {csv_file}")
        sys.exit(1)

    # Generate statistics report
    report = []
    report.append('Wall Time Statistics:')
    report.append(f'  Mean: {statistics.mean(wall_times):.6f} seconds')
    report.append(f'  Median: {statistics.median(wall_times):.6f} seconds')
    report.append(f'  Min: {min(wall_times):.6f} seconds')
    report.append(f'  Max: {max(wall_times):.6f} seconds')
    if len(wall_times) > 1:
        report.append(f'  StdDev: {statistics.stdev(wall_times):.6f} seconds')

        # Bootstrap confidence interval for mean
        lower_ci, upper_ci = bootstrap_confidence_interval(wall_times)
        if lower_ci is not None and upper_ci is not None:
            report.append(
                f'  Bootstrap 95% CI: [{lower_ci:.6f}, {upper_ci:.6f}] seconds')
    else:
        report.append('  StdDev: N/A (only one sample)')
        report.append('  Bootstrap 95% CI: N/A (only one sample)')

    report.append('')
    report.append('Max Resident Size Statistics:')
    report.append(f'  Mean: {statistics.mean(max_residents):.0f} KB')
    report.append(f'  Median: {statistics.median(max_residents):.0f} KB')
    report.append(f'  Min: {min(max_residents)} KB')
    report.append(f'  Max: {max(max_residents)} KB')
    if len(max_residents) > 1:
        report.append(f'  StdDev: {statistics.stdev(max_residents):.2f} KB')
    else:
        report.append('  StdDev: N/A (only one sample)')

    # Write to output file
    with open(output_file, 'w') as f:
        f.write('\n'.join(report) + '\n')

    # Also print to stdout
    for line in report:
        print(line)


if __name__ == '__main__':
    # Allow command line arguments for input and output files
    csv_file = sys.argv[1] if len(sys.argv) > 1 else 'timing_results.csv'
    output_file = sys.argv[2] if len(sys.argv) > 2 else 'timing_summary.txt'

    generate_timing_stats(csv_file, output_file)
