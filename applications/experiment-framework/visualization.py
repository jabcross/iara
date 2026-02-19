"""
Statistical analysis for experiment results.

Provides analysis functions for experiment results.
Visualization generation is handled by the declarative visualization_declarative module.
"""

import pandas as pd


def print_statistics(results: pd.DataFrame, scheduler_order: list) -> None:
    """
    Print performance statistics and speedup analysis.

    Args:
        results: DataFrame with all measurement results
        scheduler_order: Ordered list of scheduler names
    """
    compute_data = results[results['measurement'] == 'compute_time'].copy()
    # Convert value to numeric (handles both numeric strings and non-numeric values)
    compute_data['value'] = pd.to_numeric(compute_data['value'], errors='coerce')
    # Remove NaN values (non-numeric rows)
    compute_data = compute_data.dropna(subset=['value'])

    compute_stats = compute_data.groupby('scheduler')['value'].agg([
        'mean', 'std', 'min', 'max', 'count'
    ]).round(4)

    # Reorder by scheduler_order
    available_schedulers = [s for s in scheduler_order if s in compute_stats.index]
    compute_stats = compute_stats.reindex(available_schedulers)

    print('\n=== Compute Time Statistics (ordered by scheduler definition) ===')
    print(compute_stats)

    # Speedup analysis
    print('\n=== Speedup vs Sequential ===')
    seq_data = results[(results['scheduler'] == 'sequential') & (results['measurement'] == 'compute_time')].copy()
    seq_data['value'] = pd.to_numeric(seq_data['value'], errors='coerce')
    seq_time = seq_data['value'].mean()

    for scheduler in available_schedulers:
        sched_data = results[(results['scheduler'] == scheduler) & (results['measurement'] == 'compute_time')].copy()
        sched_data['value'] = pd.to_numeric(sched_data['value'], errors='coerce')
        sched_time = sched_data['value'].mean()
        speedup = seq_time / sched_time if sched_time > 0 else 0
        print(f'{scheduler:15} : {speedup:6.2f}x')
