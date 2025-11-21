#!/usr/bin/env python3
"""
Standalone visualization script for experiment results.

This script generates all visualization plots from experiment results CSV files
and saves them to the experiment's images directory.
"""

import pandas as pd
import matplotlib
matplotlib.use('Agg')  # Use non-interactive backend
import matplotlib.pyplot as plt
import matplotlib.cm as cm
import matplotlib.colors as mcolors
from matplotlib.patches import Patch
import numpy as np
from pathlib import Path
from datetime import datetime
import glob
import os
import subprocess
import re
from typing import Optional, List, Dict, Any


class ExperimentVisualizer:
    """Generate visualizations from experiment results."""

    def __init__(self, experiment_dir: Path, scheduler_order: Optional[List[str]] = None):
        """Initialize visualizer.

        Args:
            experiment_dir: Path to experiment directory (e.g., experiment/cholesky)
            scheduler_order: Optional list of schedulers in desired order for plots
        """
        self.experiment_dir = Path(experiment_dir)
        self.results_dir = self.experiment_dir / "results"
        self.build_metrics_dir = self.experiment_dir / "build_metrics"
        self.instances_dir = self.experiment_dir / "instances"
        self.images_dir = self.experiment_dir / "images"
        self.images_dir.mkdir(exist_ok=True)

        # Default scheduler order and colors
        self.scheduler_order = scheduler_order or ['sequential', 'omp-task', 'virtual-fifo']
        self.scheduler_colors = {
            'sequential': '#2ca02c',
            'omp-task': '#1f77b4',
            'virtual-fifo': '#ff7f0e'
        }

        # Set matplotlib style
        plt.style.use('seaborn-v0_8-darkgrid')
        plt.rcParams['figure.figsize'] = (12, 6)
        plt.rcParams['font.size'] = 10

    def get_latest_file(self, pattern: str) -> Optional[str]:
        """Get the most recent file matching the pattern."""
        files = glob.glob(str(self.experiment_dir / pattern))
        if not files:
            return None
        return max(files, key=os.path.getmtime)

    def load_data(self):
        """Load experiment results and build metrics from CSV files."""
        # Find latest files
        build_metrics_file = self.get_latest_file('build_metrics/build_metrics_*.csv')
        results_file = self.get_latest_file('results/results_*.csv')

        if not build_metrics_file or not results_file:
            raise FileNotFoundError(
                f"Could not find results files in {self.experiment_dir}\n"
                f"  Build metrics: {build_metrics_file}\n"
                f"  Results: {results_file}"
            )

        print(f"Loading build metrics from: {build_metrics_file}")
        print(f"Loading results from: {results_file}")

        self.build_metrics = pd.read_csv(build_metrics_file)
        self.results = pd.read_csv(results_file)

        # Parse config_name in build_metrics if needed
        if 'config_name' in self.build_metrics.columns and 'scheduler' not in self.build_metrics.columns:
            parts = self.build_metrics['config_name'].str.split('_', expand=True)
            self.build_metrics['matrix_size'] = parts[0].astype(int)
            self.build_metrics['num_blocks'] = parts[1].astype(int)
            self.build_metrics['scheduler'] = parts[2]

        self.has_compile_time = 'compile_time' in self.build_metrics.columns
        self.has_binary_size = 'total_size' in self.build_metrics.columns

        print(f"Loaded {len(self.build_metrics)} build metrics entries")
        print(f"Loaded {len(self.results)} runtime results entries")
        print(f"Has compile_time: {self.has_compile_time}")
        print(f"Has binary_size: {self.has_binary_size}")

    def bootstrap_ci(self, data, func=np.mean, n_boot=1000, ci=95, random_state=0):
        """Compute bootstrap mean and two-sided percentile CI."""
        arr = np.asarray(data)
        if arr.size == 0:
            return np.nan, np.nan, (np.nan, np.nan)
        rng = np.random.default_rng(random_state)
        boots = np.empty(n_boot, dtype=float)
        for i in range(n_boot):
            sample = rng.choice(arr, size=arr.size, replace=True)
            boots[i] = func(sample)
        mean = func(arr)
        se = boots.std(ddof=1)
        low = np.percentile(boots, (100 - ci) / 2)
        high = np.percentile(boots, 100 - (100 - ci) / 2)
        return mean, se, (low, high)

    def get_binary_path(self, matrix_size: int, num_blocks: int, scheduler: str) -> Path:
        """Construct path to binary for a given configuration."""
        config_name = f"{matrix_size}_{num_blocks}_{scheduler}"
        return self.instances_dir / config_name / "build" / "test" / "Iara" / "05-cholesky" / f"build-{scheduler}" / "a.out"

    def get_section_sizes(self, binary_path: Path) -> Dict[str, int]:
        """Run 'size -A' and parse all section sizes."""
        try:
            result = subprocess.run(['size', '-A', str(binary_path)],
                                  capture_output=True, text=True, check=True)
            lines = result.stdout.splitlines()
            section_sizes = {}
            for line in lines:
                m = re.match(r'\s*(\S+)\s+(\d+)', line)
                if m:
                    section = m.group(1)
                    size = int(m.group(2))
                    if section not in ['section', 'Total']:  # Skip header and total
                        section_sizes[section] = size
            return section_sizes
        except Exception as e:
            print(f"  Warning: Could not read binary {binary_path}: {e}")
            return {}

    def plot_binary_size_overhead(self):
        """Plot binary size overhead relative to sequential, one plot per matrix size showing all num_blocks."""
        if not self.has_binary_size:
            print("Skipping binary size overhead plots (no data)")
            return

        try:
            print("Generating binary size overhead plots...")

            # Get unique matrix sizes
            matrix_sizes_list = sorted(self.build_metrics['matrix_size'].unique())

            # Create one graph for each matrix size configuration
            for matrix_size in matrix_sizes_list:
                size_data = self.build_metrics[self.build_metrics['matrix_size'] == matrix_size].copy()
                num_blocks_list = sorted(size_data['num_blocks'].unique())
                num_groups = len(num_blocks_list)

                if num_groups == 0:
                    continue

                # Set figure width
                fig_width = min(max(8, num_groups * 3), 16)
                fig, ax = plt.subplots(figsize=(fig_width, 7))

                # Track data for plotting
                scheduler_data = {sched: [] for sched in self.scheduler_order}
                sequential_values = []
                has_data = False
                schedulers_present = set()

                # For each num_blocks configuration
                for num_blocks in num_blocks_list:
                    # Get sequential baseline
                    seq_data = size_data[
                        (size_data['num_blocks'] == num_blocks) &
                        (size_data['scheduler'] == 'sequential')
                    ]

                    if len(seq_data) > 0:
                        seq_size = seq_data.iloc[0]['total_size']
                        sequential_values.append(seq_size / 1024)

                        # For each scheduler
                        for sched in self.scheduler_order:
                            sched_data = size_data[
                                (size_data['num_blocks'] == num_blocks) &
                                (size_data['scheduler'] == sched)
                            ]

                            if len(sched_data) > 0:
                                sched_size = sched_data.iloc[0]['total_size']
                                pct_change = ((sched_size - seq_size) / seq_size) * 100
                                scheduler_data[sched].append(pct_change)
                                has_data = True
                                schedulers_present.add(sched)
                            else:
                                scheduler_data[sched].append(np.nan)
                    else:
                        sequential_values.append(0)
                        for sched in self.scheduler_order:
                            scheduler_data[sched].append(np.nan)

                if not has_data:
                    plt.close(fig)
                    continue

                # Calculate positions
                bar_width = 0.3
                group_width = len(self.scheduler_order) * bar_width + 0.2
                x_group_centers = np.arange(num_groups) * group_width

                # Plot bars for each scheduler
                for sched_idx, sched in enumerate(self.scheduler_order):
                    offset = (sched_idx - 1) * bar_width
                    x_pos = x_group_centers + offset
                    values = np.array(scheduler_data[sched])

                    valid_mask = ~np.isnan(values)
                    if not valid_mask.any():
                        continue

                    valid_x = x_pos[valid_mask]
                    valid_values = values[valid_mask]

                    bars = ax.bar(valid_x, valid_values, bar_width, label=sched,
                                 color=self.scheduler_colors[sched], alpha=0.8,
                                 edgecolor='black', linewidth=0.5)

                    # Add value labels on bars
                    for xp, val in zip(valid_x, valid_values):
                        if abs(val) > 0.1:
                            y_pos = val + (3 if val >= 0 else -3)
                            va = 'bottom' if val >= 0 else 'top'
                            ax.text(xp, y_pos, f'{val:+.0f}%', ha='center', va=va,
                                   fontsize=8, fontweight='bold')

                # Add reference line at 0%
                ax.axhline(y=0, color='black', linestyle='-', linewidth=1.5, alpha=0.7)

                # Customize the plot
                ax.set_ylabel('Size Change vs Sequential (%)', fontweight='bold', fontsize=12)

                missing_schedulers = set(self.scheduler_order) - schedulers_present
                title = f'Binary Size Overhead - {matrix_size}×{matrix_size}'
                if missing_schedulers:
                    title += f'\n(Note: {", ".join(sorted(missing_schedulers))} data not available)'
                ax.set_title(title, fontweight='bold', fontsize=14)

                ax.set_xticks(x_group_centers)
                ax.set_xticklabels([f'{nb} block{"s" if nb > 1 else ""}' for nb in num_blocks_list], fontsize=11)
                ax.set_xlabel('Number of Blocks', fontweight='bold', fontsize=12)
                ax.grid(True, alpha=0.3, axis='y')
                ax.legend(loc='best', fontsize=10)

                # Add sequential baseline sizes as text below
                textstr = 'Sequential baseline (KB): ' + ', '.join([
                    f'{nb}blocks={val:.0f}'
                    for nb, val in zip(num_blocks_list, sequential_values) if val > 0
                ])
                ax.text(0.5, -0.12, textstr, transform=ax.transAxes, fontsize=9,
                       verticalalignment='top', horizontalalignment='center',
                       bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.3))

                plt.subplots_adjust(bottom=0.15)
                filename = self.images_dir / f'binary_size_overhead_{matrix_size}.png'
                plt.savefig(filename, dpi=300, bbox_inches='tight')
                plt.close()
                print(f"  Saved: {filename}")
        except Exception as e:
            print(f"  ERROR generating binary size overhead plots: {e}")
            import traceback
            traceback.print_exc()

    def plot_binary_size_grouped(self):
        """Plot layered binary size comparison with all sections."""
        if not self.has_binary_size:
            print("Skipping binary size grouped plot (no data)")
            return

        try:
            print("Generating binary size grouped plot...")

            # Collect all section data from binaries
            all_sections = set()
            config_data = []

            for _, row in self.build_metrics.iterrows():
                matrix_size = row['matrix_size']
                num_blocks = row['num_blocks']
                scheduler = row['scheduler']

                binary_path = self.get_binary_path(matrix_size, num_blocks, scheduler)
                if binary_path.exists():
                    sections = self.get_section_sizes(binary_path)
                    config_data.append({
                        'matrix_size': matrix_size,
                        'num_blocks': num_blocks,
                        'scheduler': scheduler,
                        'sections': sections
                    })
                    all_sections.update(sections.keys())

            if not config_data:
                print("  WARNING: No binary section data found, skipping plot")
                return

            # Calculate total size for each section
            section_totals = {}
            for section in all_sections:
                total = sum(config['sections'].get(section, 0) for config in config_data)
                section_totals[section] = total

            # Sort sections by total size (largest first)
            sorted_sections = sorted(all_sections, key=lambda s: section_totals[s], reverse=True)

            # Build DataFrame
            plot_data = []
            for config in config_data:
                row_data = {
                    'matrix_size': config['matrix_size'],
                    'num_blocks': config['num_blocks'],
                    'scheduler': config['scheduler']
                }
                for section in sorted_sections:
                    row_data[section] = config['sections'].get(section, 0) / 1024  # Convert to KB
                plot_data.append(row_data)

            df = pd.DataFrame(plot_data)
            df = df.sort_values(['matrix_size', 'num_blocks', 'scheduler'])

            # Calculate positions with spacing
            matrix_sizes_numeric = sorted(df['matrix_size'].unique())
            x_positions = []
            scheduler_tick_positions = []
            scheduler_tick_labels = []
            num_blocks_tick_positions = []
            num_blocks_tick_labels = []
            matrix_group_centers = []
            config_info = []

            current_x = 0
            width = 0.25
            scheduler_spacing = 0.05
            num_blocks_spacing = 0.3
            matrix_spacing = 0.6

            for matrix_size in matrix_sizes_numeric:
                matrix_df = df[df['matrix_size'] == matrix_size]
                num_blocks_list = sorted(matrix_df['num_blocks'].unique())

                group_start = current_x

                for num_blocks in num_blocks_list:
                    block_df = matrix_df[matrix_df['num_blocks'] == num_blocks]
                    num_blocks_start = current_x

                    for scheduler in self.scheduler_order:
                        sched_df = block_df[block_df['scheduler'] == scheduler]
                        if len(sched_df) > 0:
                            x_positions.append(current_x)
                            scheduler_tick_positions.append(current_x)
                            scheduler_tick_labels.append(scheduler)
                            config_info.append((matrix_size, num_blocks, scheduler))
                            current_x += width + scheduler_spacing

                    num_blocks_center = (num_blocks_start + current_x - scheduler_spacing) / 2
                    num_blocks_tick_positions.append(num_blocks_center)
                    num_blocks_tick_labels.append(f"{num_blocks} block{'s' if num_blocks > 1 else ''}")

                    current_x += num_blocks_spacing - scheduler_spacing

                group_end = current_x - num_blocks_spacing
                matrix_group_centers.append((group_start + group_end) / 2)
                current_x += matrix_spacing - num_blocks_spacing

            x_positions = np.array(x_positions)

            # Define colors for sections
            color_palette = [
                '#3498db', '#e74c3c', '#2ecc71', '#f39c12', '#9b59b6', '#1abc9c',
                '#e67e22', '#34495e', '#16a085', '#c0392b', '#8e44ad', '#27ae60',
                '#2980b9', '#d35400', '#7f8c8d', '#95a5a6',
            ]

            if len(sorted_sections) > len(color_palette):
                extra_colors = cm.tab20(np.linspace(0, 1, len(sorted_sections) - len(color_palette)))
                color_palette.extend([mcolors.rgb2hex(c) for c in extra_colors])

            section_colors = {section: color_palette[i] for i, section in enumerate(sorted_sections)}

            # Create plot
            fig, ax = plt.subplots(figsize=(18, 8))

            # Prepare stacked data
            bottoms = np.zeros(len(x_positions))

            # Plot each section
            for section in sorted_sections:
                heights = []

                for i, (ms, nb, sched) in enumerate(config_info):
                    row = df[(df['matrix_size'] == ms) &
                            (df['num_blocks'] == nb) &
                            (df['scheduler'] == sched)]
                    if len(row) > 0:
                        heights.append(row.iloc[0][section])
                    else:
                        heights.append(0)

                heights = np.array(heights)

                ax.bar(x_positions, heights, width,
                      bottom=bottoms,
                      color=section_colors[section],
                      edgecolor='none')

                bottoms += heights

            # Add vertical separators
            current_x = 0
            for matrix_idx, matrix_size in enumerate(matrix_sizes_numeric):
                if matrix_idx > 0:
                    separator_x = current_x - matrix_spacing / 2
                    ax.axvline(x=separator_x, color='gray',
                              linestyle='--', linewidth=1.5, alpha=0.5)

                matrix_df = df[df['matrix_size'] == matrix_size]
                num_blocks_list = sorted(matrix_df['num_blocks'].unique())

                for nb_idx, num_blocks in enumerate(num_blocks_list):
                    if nb_idx > 0:
                        separator_x = current_x - num_blocks_spacing / 2
                        ax.axvline(x=separator_x, color='gray',
                                  linestyle=':', linewidth=1, alpha=0.3)
                    current_x += (width + scheduler_spacing) * len(self.scheduler_order) + num_blocks_spacing - scheduler_spacing

                current_x += matrix_spacing - num_blocks_spacing

            # Create custom legend
            legend_elements = []
            legend_elements.append(Patch(facecolor='none', edgecolor='none', label='Sections (by size):'))

            num_to_show = 10
            for section in sorted_sections[:num_to_show]:
                total_kb = section_totals[section] / 1024
                legend_elements.append(
                    Patch(facecolor=section_colors[section], edgecolor='black',
                          linewidth=0.5, label=f'{section} ({total_kb:.1f}KB)')
                )

            if len(sorted_sections) > num_to_show:
                remaining = len(sorted_sections) - num_to_show
                legend_elements.append(
                    Patch(facecolor='none', edgecolor='none',
                          label=f'... +{remaining} more')
                )

            ax.set_ylabel('Binary Size (KB)', fontweight='bold', fontsize=12)
            ax.set_title('Binary Size Comparison - All Sections', fontweight='bold', fontsize=14)

            # Set up three-tiered x-axis labels
            ax.set_xticks(scheduler_tick_positions)
            ax.set_xticklabels(scheduler_tick_labels, fontsize=8, rotation=45, ha='right')
            ax.set_xlabel('Scheduler', fontweight='bold', fontsize=11, labelpad=10)

            ax2 = ax.twiny()
            ax2.set_xlim(ax.get_xlim())
            ax2.set_xticks(num_blocks_tick_positions)
            ax2.set_xticklabels(num_blocks_tick_labels, fontsize=9)
            ax2.set_xlabel('Configuration', fontweight='bold', fontsize=11)
            ax2.tick_params(axis='x', which='both', length=0, pad=5)

            ax3 = ax.twiny()
            ax3.set_xlim(ax.get_xlim())
            ax3.set_xticks(matrix_group_centers)
            ax3.set_xticklabels([f'{ms}×{ms}' for ms in matrix_sizes_numeric],
                                fontweight='bold', fontsize=12)
            ax3.set_xlabel('Matrix Size', fontweight='bold', fontsize=12)
            ax3.tick_params(axis='x', which='both', length=0, pad=25)
            ax3.spines['top'].set_position(('outward', 40))

            ax.legend(handles=legend_elements, loc='upper left', ncol=1, fontsize=8,
                      handlelength=2)
            ax.grid(True, alpha=0.3, axis='y')

            plt.tight_layout()
            filename = self.images_dir / 'binary_size_grouped.png'
            plt.savefig(filename, dpi=300, bbox_inches='tight')
            plt.close()
            print(f"  Saved: {filename}")

        except Exception as e:
            print(f"  ERROR generating binary size grouped plot: {e}")
            import traceback
            traceback.print_exc()

    def plot_compilation_time(self):
        """Plot compilation time comparison if available."""
        if not self.has_compile_time:
            print("Skipping compilation time plot (no data)")
            return

        try:
            print("Generating compilation time plot...")
            fig, ax = plt.subplots(figsize=(12, 6))

            pivot_compile = self.build_metrics.pivot_table(
                index='config_name', columns='scheduler', values='compile_time')

            # Only use schedulers that exist in the data
            available_schedulers = [s for s in self.scheduler_order if s in pivot_compile.columns]
            if not available_schedulers:
                print("  WARNING: No schedulers found in data, skipping compilation time plot")
                plt.close()
                return
            pivot_compile = pivot_compile[available_schedulers]

            x = np.arange(len(pivot_compile.index))
            width = 0.25

            for i, scheduler in enumerate(available_schedulers):
                offset = (i - 1) * width
                bars = ax.bar(x + offset, pivot_compile[scheduler], width,
                             label=scheduler, alpha=0.8,
                             color=self.scheduler_colors.get(scheduler, f'C{i}'))

                for j, (bar, val) in enumerate(zip(bars, pivot_compile[scheduler])):
                    if not np.isnan(val):
                        height = bar.get_height()
                        ax.text(bar.get_x() + bar.get_width()/2., height,
                               f'{val:.2f}s', ha='center', va='bottom', fontsize=8)

            ax.set_ylabel('Compilation Time (seconds)', fontweight='bold', fontsize=12)
            ax.set_xlabel('Configuration', fontweight='bold', fontsize=12)
            ax.set_title('Compilation Time by Scheduler and Configuration', fontweight='bold', fontsize=14)
            ax.set_xticks(x)
            ax.set_xticklabels(pivot_compile.index, rotation=45, ha='right')
            ax.legend()
            ax.grid(True, alpha=0.3, axis='y')

            plt.tight_layout()
            filename = self.images_dir / 'compilation_time.png'
            plt.savefig(filename, dpi=300, bbox_inches='tight')
            plt.close()
            print(f"  Saved: {filename}")
        except Exception as e:
            print(f"  ERROR generating compilation time plot: {e}")
            import traceback
            traceback.print_exc()
            plt.close()

    def plot_runtime_performance(self):
        """Plot runtime performance with bootstrap CI."""
        try:
            print("Generating runtime performance plot...")

            # Compute bootstrap statistics
            runtime_rows = []
            for (matrix_size, num_blocks, scheduler), group in self.results.groupby(['matrix_size', 'num_blocks', 'scheduler']):
                values = group['wall_time'].values
                mean, se, (low, high) = self.bootstrap_ci(values, func=np.mean, n_boot=1000)
                runtime_rows.append({
                    'matrix_size': matrix_size,
                    'num_blocks': num_blocks,
                    'scheduler': scheduler,
                    'wall_time_mean': mean,
                    'wall_time_ci_low': low,
                    'wall_time_ci_high': high,
                })

            runtime_stats = pd.DataFrame(runtime_rows)
            runtime_stats = runtime_stats.sort_values(['matrix_size', 'num_blocks'])

            # Pivot
            pivot_wall = runtime_stats.pivot_table(
                index=['matrix_size', 'num_blocks'], columns='scheduler', values='wall_time_mean')
            pivot_wall_low = runtime_stats.pivot_table(
                index=['matrix_size', 'num_blocks'], columns='scheduler', values='wall_time_ci_low')
            pivot_wall_high = runtime_stats.pivot_table(
                index=['matrix_size', 'num_blocks'], columns='scheduler', values='wall_time_ci_high')

            available_schedulers = [s for s in self.scheduler_order if s in pivot_wall.columns]
            if not available_schedulers:
                print("  WARNING: No schedulers found in data, skipping runtime performance plot")
                return
            pivot_wall = pivot_wall[available_schedulers]
            pivot_wall_low = pivot_wall_low.reindex(columns=available_schedulers).fillna(pivot_wall)
            pivot_wall_high = pivot_wall_high.reindex(columns=available_schedulers).fillna(pivot_wall)

            # Create plot
            self._plot_grouped_bars(
                pivot_wall.values,
                pivot_wall_low.values,
                pivot_wall_high.values,
                pivot_wall.index,
                available_schedulers,
                'Wall Time (seconds)',
                'Runtime Performance by Scheduler and Configuration',
                'runtime_performance.png'
            )
        except Exception as e:
            print(f"  ERROR generating runtime performance plot: {e}")
            import traceback
            traceback.print_exc()

    def plot_runtime_performance_relative(self):
        """Plot runtime performance relative to sequential."""
        try:
            print("Generating runtime performance relative plot...")

            # Compute bootstrap statistics
            runtime_rows = []
            for (matrix_size, num_blocks, scheduler), group in self.results.groupby(['matrix_size', 'num_blocks', 'scheduler']):
                values = group['wall_time'].values
                mean, se, (low, high) = self.bootstrap_ci(values, func=np.mean, n_boot=1000)
                runtime_rows.append({
                    'matrix_size': matrix_size,
                    'num_blocks': num_blocks,
                    'scheduler': scheduler,
                    'wall_time_mean': mean,
                })

            runtime_stats = pd.DataFrame(runtime_rows)

            # Calculate percentage change relative to sequential
            relative_data = []
            for (matrix_size, num_blocks), group in runtime_stats.groupby(['matrix_size', 'num_blocks']):
                seq_data = group[group['scheduler'] == 'sequential']
                if len(seq_data) == 0:
                    continue

                seq_time = seq_data['wall_time_mean'].values[0]

                for _, row in group.iterrows():
                    pct_change = ((row['wall_time_mean'] - seq_time) / seq_time) * 100
                    relative_data.append({
                        'matrix_size': matrix_size,
                        'num_blocks': num_blocks,
                        'scheduler': row['scheduler'],
                        'pct_change': pct_change
                    })

            runtime_relative_df = pd.DataFrame(relative_data)
            runtime_relative_df = runtime_relative_df.sort_values(['matrix_size', 'num_blocks'])

            # Pivot for plotting
            pivot_pct = runtime_relative_df.pivot_table(
                index=['matrix_size', 'num_blocks'], columns='scheduler', values='pct_change')

            # Only use available schedulers
            available_schedulers = [s for s in self.scheduler_order if s in pivot_pct.columns]
            if not available_schedulers:
                print("  WARNING: No schedulers found, skipping")
                return
            pivot_pct = pivot_pct[available_schedulers]

            self._plot_relative_bars(
                pivot_pct.values,
                pivot_pct.index,
                available_schedulers,
                'Runtime Change vs Sequential (%)',
                'Runtime Performance Change Relative to Sequential\n(Negative = Faster, Positive = Slower)',
                'runtime_performance_relative.png'
            )

        except Exception as e:
            print(f"  ERROR generating runtime performance relative plot: {e}")
            import traceback
            traceback.print_exc()

    def plot_memory_usage(self):
        """Plot memory usage with bootstrap CI."""
        try:
            print("Generating memory usage plot...")

            # Compute bootstrap statistics
            memory_rows = []
            for (matrix_size, num_blocks, scheduler), group in self.results.groupby(['matrix_size', 'num_blocks', 'scheduler']):
                values = group['max_rss_kb'].values
                mean, se, (low, high) = self.bootstrap_ci(values, func=np.mean, n_boot=1000)
                memory_rows.append({
                    'matrix_size': matrix_size,
                    'num_blocks': num_blocks,
                    'scheduler': scheduler,
                    'max_rss_kb_mean': mean,
                    'max_rss_kb_ci_low': low,
                    'max_rss_kb_ci_high': high,
                })

            memory_stats = pd.DataFrame(memory_rows)
            memory_stats = memory_stats.sort_values(['matrix_size', 'num_blocks'])

            # Pivot and convert to MB
            pivot_rss = memory_stats.pivot_table(
                index=['matrix_size', 'num_blocks'], columns='scheduler', values='max_rss_kb_mean') / 1024
            pivot_rss_low = memory_stats.pivot_table(
                index=['matrix_size', 'num_blocks'], columns='scheduler', values='max_rss_kb_ci_low') / 1024
            pivot_rss_high = memory_stats.pivot_table(
                index=['matrix_size', 'num_blocks'], columns='scheduler', values='max_rss_kb_ci_high') / 1024

            available_schedulers = [s for s in self.scheduler_order if s in pivot_rss.columns]
            if not available_schedulers:
                print("  WARNING: No schedulers found in data, skipping memory usage plot")
                return
            pivot_rss = pivot_rss[available_schedulers]
            pivot_rss_low = pivot_rss_low.reindex(columns=available_schedulers).fillna(pivot_rss)
            pivot_rss_high = pivot_rss_high.reindex(columns=available_schedulers).fillna(pivot_rss)

            # Create plot
            self._plot_grouped_bars(
                pivot_rss.values,
                pivot_rss_low.values,
                pivot_rss_high.values,
                pivot_rss.index,
                available_schedulers,
                'Max RSS (MB)',
                'Memory Usage by Scheduler and Configuration',
                'memory_usage.png'
            )
        except Exception as e:
            print(f"  ERROR generating memory usage plot: {e}")
            import traceback
            traceback.print_exc()

    def plot_memory_usage_relative(self):
        """Plot memory usage relative to sequential."""
        try:
            print("Generating memory usage relative plot...")

            # Compute bootstrap statistics
            memory_rows = []
            for (matrix_size, num_blocks, scheduler), group in self.results.groupby(['matrix_size', 'num_blocks', 'scheduler']):
                values = group['max_rss_kb'].values
                mean, se, (low, high) = self.bootstrap_ci(values, func=np.mean, n_boot=1000)
                memory_rows.append({
                    'matrix_size': matrix_size,
                    'num_blocks': num_blocks,
                    'scheduler': scheduler,
                    'max_rss_kb_mean': mean,
                })

            memory_stats = pd.DataFrame(memory_rows)

            # Calculate percentage change relative to sequential
            relative_data = []
            for (matrix_size, num_blocks), group in memory_stats.groupby(['matrix_size', 'num_blocks']):
                seq_data = group[group['scheduler'] == 'sequential']
                if len(seq_data) == 0:
                    continue

                seq_memory = seq_data['max_rss_kb_mean'].values[0]

                for _, row in group.iterrows():
                    pct_change = ((row['max_rss_kb_mean'] - seq_memory) / seq_memory) * 100
                    relative_data.append({
                        'matrix_size': matrix_size,
                        'num_blocks': num_blocks,
                        'scheduler': row['scheduler'],
                        'pct_change': pct_change
                    })

            memory_relative_df = pd.DataFrame(relative_data)
            memory_relative_df = memory_relative_df.sort_values(['matrix_size', 'num_blocks'])

            # Pivot for plotting
            pivot_pct = memory_relative_df.pivot_table(
                index=['matrix_size', 'num_blocks'], columns='scheduler', values='pct_change')

            # Only use available schedulers
            available_schedulers = [s for s in self.scheduler_order if s in pivot_pct.columns]
            if not available_schedulers:
                print("  WARNING: No schedulers found, skipping")
                return
            pivot_pct = pivot_pct[available_schedulers]

            self._plot_relative_bars(
                pivot_pct.values,
                pivot_pct.index,
                available_schedulers,
                'Memory Change vs Sequential (%)',
                'Memory Usage Change Relative to Sequential\n(Positive = More Memory)',
                'memory_usage_relative.png'
            )

        except Exception as e:
            print(f"  ERROR generating memory usage relative plot: {e}")
            import traceback
            traceback.print_exc()

    def _plot_grouped_bars(self, means, lows, highs, index, schedulers, ylabel, title, filename):
        """Helper to create grouped bar plots with error bars."""
        fig, ax = plt.subplots(figsize=(16, 7))

        # Calculate positions
        matrix_sizes = sorted(index.get_level_values(0).unique())
        x_positions = []
        tick_positions = []
        tick_labels = []
        matrix_group_centers = []

        current_x = 0
        width = 0.25
        group_spacing = 0.5

        for matrix_size in matrix_sizes:
            matrix_data = [nb for ms, nb in index if ms == matrix_size]
            group_start = current_x

            for num_blocks in matrix_data:
                x_positions.append(current_x)
                tick_positions.append(current_x)
                tick_labels.append(f"{num_blocks} block{'s' if num_blocks > 1 else ''}")
                current_x += 1

            group_end = current_x - 1
            matrix_group_centers.append((group_start + group_end) / 2)
            current_x += group_spacing

        x_positions = np.array(x_positions)

        # Plot bars
        for i, scheduler in enumerate(schedulers):
            offset = (i - len(schedulers)//2) * width if len(schedulers) % 2 == 1 else (i - len(schedulers)/2 + 0.5) * width
            color = self.scheduler_colors.get(scheduler, f'C{i}')

            lower = means[:, i] - lows[:, i]
            upper = highs[:, i] - means[:, i]
            lower = np.maximum(lower, 0)
            upper = np.maximum(upper, 0)
            yerr = np.vstack([lower, upper])

            ax.bar(x_positions + offset, means[:, i], width,
                  yerr=yerr, label=scheduler, color=color, alpha=0.8,
                  capsize=3, edgecolor='black', linewidth=0.5)

        # Add vertical separators
        current_x = 0
        for idx, matrix_size in enumerate(matrix_sizes):
            if idx > 0:
                separator_x = current_x - group_spacing / 2
                ax.axvline(x=separator_x, color='gray', linestyle='--', linewidth=1.5, alpha=0.5)
            matrix_data = [nb for ms, nb in index if ms == matrix_size]
            current_x += len(matrix_data) + group_spacing

        ax.set_ylabel(ylabel, fontweight='bold', fontsize=12)
        ax.set_xlabel('Number of Blocks (grouped by Matrix Size)', fontweight='bold', fontsize=12)
        ax.set_title(title, fontweight='bold', fontsize=14)
        ax.set_xticks(tick_positions)
        ax.set_xticklabels(tick_labels, fontsize=8, rotation=45, ha='right')

        # Add matrix size labels
        ax2 = ax.twiny()
        ax2.set_xlim(ax.get_xlim())
        ax2.set_xticks(matrix_group_centers)
        ax2.set_xticklabels([f'{ms}×{ms}' for ms in matrix_sizes], fontweight='bold', fontsize=11)
        ax2.set_xlabel('Matrix Size', fontweight='bold', fontsize=12)
        ax2.tick_params(axis='x', which='both', length=0)

        ax.legend(loc='best', fontsize=10)
        ax.grid(True, alpha=0.3, axis='y')

        plt.tight_layout()
        filepath = self.images_dir / filename
        plt.savefig(filepath, dpi=300, bbox_inches='tight')
        plt.close()
        print(f"  Saved: {filepath}")

    def _plot_relative_bars(self, values, index, schedulers, ylabel, title, filename):
        """Helper to create relative performance bar plots."""
        fig, ax = plt.subplots(figsize=(16, 7))

        # Calculate positions
        matrix_sizes = sorted(index.get_level_values(0).unique())
        x_positions = []
        tick_positions = []
        tick_labels = []
        matrix_group_centers = []

        current_x = 0
        width = 0.25
        group_spacing = 0.5

        for matrix_size in matrix_sizes:
            matrix_data = [nb for ms, nb in index if ms == matrix_size]
            group_start = current_x

            for num_blocks in matrix_data:
                x_positions.append(current_x)
                tick_positions.append(current_x)
                tick_labels.append(f"{num_blocks} block{'s' if num_blocks > 1 else ''}")
                current_x += 1

            group_end = current_x - 1
            matrix_group_centers.append((group_start + group_end) / 2)
            current_x += group_spacing

        x_positions = np.array(x_positions)

        # Plot bars for each scheduler
        for i, scheduler in enumerate(schedulers):
            offset = (i - 1) * width
            color = self.scheduler_colors.get(scheduler, f'C{i}')

            vals = values[:, i]
            valid_mask = ~np.isnan(vals)

            if valid_mask.any():
                valid_x = (x_positions + offset)[valid_mask]
                valid_values = vals[valid_mask]

                bars = ax.bar(valid_x, valid_values, width,
                              label=scheduler, color=color, alpha=0.8,
                              edgecolor='black', linewidth=0.5)

                # Add value labels on bars
                for xp, val in zip(valid_x, valid_values):
                    if abs(val) > 0.5:
                        y_pos = val + (2 if val >= 0 else -2)
                        va = 'bottom' if val >= 0 else 'top'
                        ax.text(xp, y_pos, f'{val:+.0f}%', ha='center', va=va,
                               fontsize=8, fontweight='bold')

        # Add reference line at 0%
        ax.axhline(y=0, color='black', linestyle='-', linewidth=1.5, alpha=0.7)

        # Add vertical separators
        current_x = 0
        for idx, matrix_size in enumerate(matrix_sizes):
            if idx > 0:
                separator_x = current_x - group_spacing / 2
                ax.axvline(x=separator_x, color='gray', linestyle='--', linewidth=1.5, alpha=0.5)
            matrix_data = [nb for ms, nb in index if ms == matrix_size]
            current_x += len(matrix_data) + group_spacing

        ax.set_ylabel(ylabel, fontweight='bold', fontsize=12)
        ax.set_xlabel('Number of Blocks (grouped by Matrix Size)', fontweight='bold', fontsize=12)
        ax.set_title(title, fontweight='bold', fontsize=14)
        ax.set_xticks(tick_positions)
        ax.set_xticklabels(tick_labels, fontsize=8, rotation=45, ha='right')

        # Add matrix size labels
        ax2 = ax.twiny()
        ax2.set_xlim(ax.get_xlim())
        ax2.set_xticks(matrix_group_centers)
        ax2.set_xticklabels([f'{ms}×{ms}' for ms in matrix_sizes], fontweight='bold', fontsize=11)
        ax2.set_xlabel('Matrix Size', fontweight='bold', fontsize=12)
        ax2.tick_params(axis='x', which='both', length=0)

        ax.legend(loc='best', fontsize=10)
        ax.grid(True, alpha=0.3, axis='y')

        plt.tight_layout()
        filepath = self.images_dir / filename
        plt.savefig(filepath, dpi=300, bbox_inches='tight')
        plt.close()
        print(f"  Saved: {filepath}")

    def generate_all(self):
        """Generate all visualizations."""
        print(f"\n{'='*80}")
        print(f"Generating visualizations for {self.experiment_dir.name}")
        print(f"{'='*80}\n")

        self.load_data()

        # Binary size visualizations
        self.plot_binary_size_overhead()
        self.plot_binary_size_grouped()

        # Compilation time
        self.plot_compilation_time()

        # Runtime performance (absolute and relative)
        self.plot_runtime_performance()
        self.plot_runtime_performance_relative()

        # Memory usage (absolute and relative)
        self.plot_memory_usage()
        self.plot_memory_usage_relative()

        print(f"\n{'='*80}")
        print(f"Visualization complete! Images saved to:")
        print(f"  {self.images_dir}")
        print(f"{'='*80}\n")

        return self.images_dir


def visualize_experiment(experiment_dir: Path, scheduler_order: Optional[List[str]] = None) -> Path:
    """Run visualization for an experiment directory.

    Args:
        experiment_dir: Path to experiment directory
        scheduler_order: Optional list of schedulers in desired order

    Returns:
        Path to images directory
    """
    visualizer = ExperimentVisualizer(experiment_dir, scheduler_order)
    return visualizer.generate_all()


def main():
    """CLI entry point."""
    import argparse

    parser = argparse.ArgumentParser(description="Generate visualizations for experiment results")
    parser.add_argument("experiment_dir", type=Path,
                       help="Path to experiment directory (e.g., experiment/cholesky)")
    parser.add_argument("--schedulers", type=str, nargs='+',
                       help="Scheduler order for plots (default: sequential omp-task virtual-fifo)")

    args = parser.parse_args()

    visualize_experiment(args.experiment_dir, args.schedulers)


if __name__ == "__main__":
    main()
