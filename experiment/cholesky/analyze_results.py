#!/usr/bin/env python3
"""
Analyze experiment results with statistical rigor.

This script:
- Loads results from CSV
- Computes statistics (mean, std, CI)
- Detects outliers
- Generates comparison tables
- Creates visualizations
"""

import pandas as pd
import numpy as np
from pathlib import Path
import argparse
import sys


def compute_statistics(df: pd.DataFrame) -> pd.DataFrame:
    """Compute statistics for each configuration."""
    
    grouped = df.groupby(['matrix_size', 'num_blocks', 'scheduler'])
    
    stats = grouped['wall_time'].agg([
        ('count', 'count'),
        ('mean', 'mean'),
        ('std', 'std'),
        ('min', 'min'),
        ('max', 'max'),
        ('median', 'median'),
    ]).reset_index()
    
    # Compute 95% confidence interval
    stats['ci_lower'] = grouped['wall_time'].apply(
        lambda x: x.mean() - 1.96 * x.std() / np.sqrt(len(x))
    ).values
    stats['ci_upper'] = grouped['wall_time'].apply(
        lambda x: x.mean() + 1.96 * x.std() / np.sqrt(len(x))
    ).values
    
    # Coefficient of variation (%)
    stats['cv_percent'] = (stats['std'] / stats['mean']) * 100
    
    return stats


def detect_outliers(df: pd.DataFrame) -> pd.DataFrame:
    """Detect outliers using IQR method."""
    
    grouped = df.groupby(['matrix_size', 'num_blocks', 'scheduler'])
    
    def is_outlier(group):
        Q1 = group['wall_time'].quantile(0.25)
        Q3 = group['wall_time'].quantile(0.75)
        IQR = Q3 - Q1
        lower_bound = Q1 - 1.5 * IQR
        upper_bound = Q3 + 1.5 * IQR
        return (group['wall_time'] < lower_bound) | (group['wall_time'] > upper_bound)
    
    df['is_outlier'] = grouped.apply(is_outlier).reset_index(level=[0,1,2], drop=True)
    
    return df


def compare_schedulers(stats: pd.DataFrame) -> pd.DataFrame:
    """Compare schedulers for each matrix size and num_blocks."""
    
    # Pivot to get schedulers as columns
    pivot = stats.pivot_table(
        index=['matrix_size', 'num_blocks'],
        columns='scheduler',
        values='mean'
    )
    
    # Compute speedup relative to sequential
    if 'sequential' in pivot.columns:
        for col in pivot.columns:
            if col != 'sequential':
                pivot[f'{col}_speedup'] = pivot['sequential'] / pivot[col]
    
    return pivot


def main():
    parser = argparse.ArgumentParser(description="Analyze experiment results")
    parser.add_argument("--input", type=str, default="results.csv",
                       help="Input CSV file with results")
    parser.add_argument("--output-stats", type=str, default="statistics.csv",
                       help="Output file for statistics")
    parser.add_argument("--output-comparison", type=str, default="comparison.csv",
                       help="Output file for scheduler comparison")
    parser.add_argument("--show-outliers", action="store_true",
                       help="Print detected outliers")
    
    args = parser.parse_args()
    
    results_file = Path(args.input)
    if not results_file.exists():
        print(f"ERROR: Results file not found: {results_file}", file=sys.stderr)
        sys.exit(1)
    
    # Load results
    print(f"Loading results from {results_file}...")
    df = pd.read_csv(results_file)
    print(f"Loaded {len(df)} measurements")
    
    # Detect outliers
    df = detect_outliers(df)
    outliers = df[df['is_outlier']]
    
    if args.show_outliers and len(outliers) > 0:
        print(f"\nDetected {len(outliers)} outliers:")
        print(outliers[['matrix_size', 'num_blocks', 'scheduler', 'wall_time', 'run_number']])
    
    # Compute statistics
    print("\nComputing statistics...")
    stats = compute_statistics(df[~df['is_outlier']])  # Exclude outliers
    
    # Save statistics
    stats.to_csv(args.output_stats, index=False)
    print(f"Saved statistics to {args.output_stats}")
    
    # Compare schedulers
    comparison = compare_schedulers(stats)
    comparison.to_csv(args.output_comparison)
    print(f"Saved scheduler comparison to {args.output_comparison}")
    
    # Print summary
    print("\n=== Summary ===")
    print(f"Total configurations: {len(stats)}")
    print(f"Matrix sizes: {sorted(df['matrix_size'].unique())}")
    print(f"Num blocks: {sorted(df['num_blocks'].unique())}")
    print(f"Schedulers: {sorted(df['scheduler'].unique())}")
    
    print("\n=== Statistics Summary ===")
    print("\nCoefficient of Variation by scheduler:")
    cv_by_scheduler = stats.groupby('scheduler')['cv_percent'].agg(['mean', 'max'])
    print(cv_by_scheduler)
    
    if 'sequential' in stats['scheduler'].values:
        print("\n=== Speedup vs Sequential ===")
        speedup_cols = [c for c in comparison.columns if c.endswith('_speedup')]
        if speedup_cols:
            print(comparison[speedup_cols].describe())


if __name__ == "__main__":
    main()
