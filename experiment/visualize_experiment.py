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
import plotly.express as px
import plotly.graph_objects as go
import plotly.io as pio


class ExperimentVisualizer:
    """Generate visualizations from experiment results."""

    def __init__(self, experiment_dir: Path, scheduler_order: Optional[List[str]] = None, suffix: Optional[str] = None):
        """Initialize visualizer.

        Args:
            experiment_dir: Path to experiment directory (e.g., experiment/cholesky)
            scheduler_order: Optional list of schedulers in desired order for plots
            suffix: Optional suffix to add to image filenames (e.g., "20250121_123456")
        """
        self.experiment_dir = Path(experiment_dir)
        self.results_dir = self.experiment_dir / "results"
        self.build_metrics_dir = self.experiment_dir / "build_metrics"
        self.instances_dir = self.experiment_dir / "instances"
        self.images_dir = self.experiment_dir / "images"
        self.images_dir.mkdir(exist_ok=True)
        self.suffix = suffix

        # Default scheduler order and colors
        self.scheduler_order = scheduler_order or ['sequential', 'omp-for', 'omp-task', 'enkits-task', 'vf-omp', 'vf-enkits']
        self.scheduler_colors = {
            'sequential': '#2ca02c',
            'omp-task': '#1f77b4',
            'omp-for': "#20b39f",
            'enkits-task': '#9467bd',
            'vf-omp': '#ff7f0e',
            'vf-enkits': '#d62728'
        }

        # Set matplotlib style
        plt.style.use('seaborn-v0_8-darkgrid')
        plt.rcParams['figure.figsize'] = (12, 6)
        plt.rcParams['font.size'] = 10

    def _make_filename(self, base_name: str) -> str:
        """Create filename with optional suffix.

        Args:
            base_name: Base filename (e.g., "runtime_performance.png")

        Returns:
            Filename with suffix if provided (e.g., "runtime_performance_20250121_123456.png")
        """
        if self.suffix:
            name_parts = base_name.rsplit('.', 1)
            if len(name_parts) == 2:
                return f"{name_parts[0]}_{self.suffix}.{name_parts[1]}"
            else:
                return f"{base_name}_{self.suffix}"
        return base_name

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
        binary_dir = f"cholesky-{scheduler}"
        return self.instances_dir / config_name / "build" / binary_dir / "a.out"

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
            return None

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
                filename = self.images_dir / self._make_filename(f'binary_size_overhead_{matrix_size}.png')
                plt.savefig(filename, dpi=300, bbox_inches='tight')
                print(f"  Saved: {filename}")
                
                # Return the figure for notebook display
                return fig
        except Exception as e:
            print(f"  ERROR generating binary size overhead plots: {e}")
            import traceback
            traceback.print_exc()
            return None

    def plot_binary_size_overhead_plotly(self):
        """Plotly duplicate for `plot_binary_size_overhead`."""
        if not self.has_binary_size:
            return None

        try:
            matrix_sizes_list = sorted(self.build_metrics['matrix_size'].unique())
            for matrix_size in matrix_sizes_list:
                size_data = self.build_metrics[self.build_metrics['matrix_size'] == matrix_size].copy()
                num_blocks_list = sorted(size_data['num_blocks'].unique())

                rows = []
                for num_blocks in num_blocks_list:
                    seq_data = size_data[(size_data['num_blocks'] == num_blocks) & (size_data['scheduler'] == 'sequential')]
                    if len(seq_data) == 0:
                        continue
                    seq_size = seq_data.iloc[0]['total_size']
                    for sched in self.scheduler_order:
                        sched_data = size_data[(size_data['num_blocks'] == num_blocks) & (size_data['scheduler'] == sched)]
                        if len(sched_data) > 0:
                            sched_size = sched_data.iloc[0]['total_size']
                            pct_change = ((sched_size - seq_size) / seq_size) * 100
                            rows.append({'num_blocks': str(num_blocks), 'scheduler': sched, 'pct': pct_change})

                if not rows:
                    continue

                df = pd.DataFrame(rows)
                fig = px.bar(
                    df, x='num_blocks', y='pct', color='scheduler', barmode='group',
                    category_orders={'scheduler': self.scheduler_order},
                    color_discrete_map=self.scheduler_colors,
                    labels={'pct': 'Size Change vs Sequential (%)', 'num_blocks': 'Number of Blocks'}
                )
                fig.update_layout(title_text=f'Binary Size Overhead - {matrix_size}×{matrix_size}', xaxis={'type': 'category'})

                # Save interactive HTML and optional PNG
                try:
                    html_path = self.images_dir / self._make_filename(f'binary_size_overhead_{matrix_size}_plotly.html')
                    pio.write_html(fig, file=str(html_path), include_plotlyjs='cdn', full_html=False)
                    print(f"  Saved interactive: {html_path}")
                except Exception:
                    pass
                try:
                    png_path = self.images_dir / self._make_filename(f'binary_size_overhead_{matrix_size}_plotly.png')
                    pio.write_image(fig, str(png_path))
                    print(f"  Saved static: {png_path}")
                except Exception:
                    pass

                return fig
        except Exception as e:
            print(f"  ERROR generating Plotly binary size overhead: {e}")
            import traceback
            traceback.print_exc()
            return None

    def plot_binary_size_grouped(self):
        """Plot layered binary size comparison with all sections."""
        if not self.has_binary_size:
            print("Skipping binary size grouped plot (no data)")
            return None

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
                print("  WARNING: No binary section data found, creating placeholder plot")
                # Create a placeholder figure with informative message
                fig, ax = plt.subplots(figsize=(12, 6))
                ax.text(0.5, 0.5, 'Binary Size Comparison - All Sections (Layered)\n\nNo binary section data available\n\nThis plot requires binary files to be present in the instances directory\nand the "size" command to be available to analyze binary sections.',
                       ha='center', va='center', transform=ax.transAxes, fontsize=12,
                       bbox=dict(boxstyle='round', facecolor='lightgray', alpha=0.8))
                ax.set_xlim(0, 1)
                ax.set_ylim(0, 1)
                ax.set_xticks([])
                ax.set_yticks([])
                ax.set_title('Binary Size Comparison - All Sections (Layered)\n(No Data Available)', fontweight='bold', fontsize=14)
                
                plt.tight_layout()
                filename = self.images_dir / self._make_filename('binary_size_grouped.png')
                plt.savefig(filename, dpi=300, bbox_inches='tight')
                print(f"  Saved placeholder: {filename}")
                
                # Return the figure for notebook display
                return fig

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
            width = 1/len(self.scheduler_order)
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

            # Create second x-axis for num_blocks labels
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
            filename = self.images_dir / self._make_filename('binary_size_grouped.png')
            plt.savefig(filename, dpi=300, bbox_inches='tight')
            print(f"  Saved: {filename}")
            
            # Return the figure for notebook display
            return fig
        except Exception as e:
            print(f"  ERROR generating binary size grouped plot: {e}")
            import traceback
            traceback.print_exc()
            return None

    def plot_compilation_time(self):
        """Plot compilation time comparison if available."""
        if not self.has_compile_time:
            print("Skipping compilation time plot (no data)")
            return None

        try:
            print("Generating compilation time plot...")
            fig, ax = plt.subplots(figsize=(12, 6))

            # Each config has exactly one scheduler, so just plot them directly
            configs = self.build_metrics['config_name'].values
            compile_times = self.build_metrics['compile_time'].values
            schedulers = self.build_metrics['scheduler'].values

            x = np.arange(len(configs))

            # Color bars by scheduler
            colors = [self.scheduler_colors.get(sched, 'gray') for sched in schedulers]

            bars = ax.bar(x, compile_times, alpha=0.8, color=colors)

            # Add value labels on bars
            for bar, val in zip(bars, compile_times):
                if not np.isnan(val):
                    height = bar.get_height()
                    ax.text(bar.get_x() + bar.get_width()/2., height,
                           f'{val:.2f}s', ha='center', va='bottom', fontsize=8)

            ax.set_ylabel('Compilation Time (seconds)', fontweight='bold', fontsize=12)
            ax.set_xlabel('Configuration', fontweight='bold', fontsize=12)
            ax.set_title('Compilation Time by Configuration', fontweight='bold', fontsize=14)
            ax.set_xticks(x)
            ax.set_xticklabels(configs, rotation=45, ha='right')

            # Create legend from unique schedulers
            unique_schedulers = self.build_metrics['scheduler'].unique()
            legend_handles = [plt.Rectangle((0,0),1,1, fc=self.scheduler_colors.get(s, 'gray'), alpha=0.8)
                            for s in unique_schedulers]
            ax.legend(legend_handles, unique_schedulers)
            ax.grid(True, alpha=0.3, axis='y')

            plt.tight_layout()
            
            # Save the figure for file output
            filename = self.images_dir / self._make_filename('compilation_time.png')
            plt.savefig(filename, dpi=300, bbox_inches='tight')
            print(f"  Saved: {filename}")
            
            # Return the figure for notebook display
            return fig
        except Exception as e:
            print(f"  ERROR generating compilation time plot: {e}")
            import traceback
            traceback.print_exc()
            plt.close()
            return None

    def plot_runtime_performance(self):
        """Plot runtime performance with stacked bars showing initialization, conversion, and compute time."""
        try:
            print("Generating runtime performance plot (stacked)...")

            # Check if we have init_time and convert_time columns
            has_init_time = 'init_time' in self.results.columns
            has_convert_time = 'convert_time' in self.results.columns

            # Compute bootstrap statistics
            runtime_rows = []
            for (matrix_size, num_blocks, scheduler), group in self.results.groupby(['matrix_size', 'num_blocks', 'scheduler']):
                wall_values = group['wall_time'].values
                real_values = group['real_time'].values

                wall_mean, _, _ = self.bootstrap_ci(wall_values, func=np.mean, n_boot=1000)
                real_mean, _, _ = self.bootstrap_ci(real_values, func=np.mean, n_boot=1000)

                row_data = {
                    'matrix_size': matrix_size,
                    'num_blocks': num_blocks,
                    'scheduler': scheduler,
                    'wall_time_mean': wall_mean,
                    'real_time_mean': real_mean,
                }

                # Add detailed setup breakdown if available
                if has_init_time and has_convert_time:
                    init_values = group['init_time'].values
                    convert_values = group['convert_time'].values
                    init_mean, _, _ = self.bootstrap_ci(init_values, func=np.mean, n_boot=1000)
                    convert_mean, _, _ = self.bootstrap_ci(convert_values, func=np.mean, n_boot=1000)

                    row_data['init_time_mean'] = init_mean
                    row_data['convert_time_mean'] = convert_mean
                    # Remaining overhead after init and convert
                    row_data['other_setup_mean'] = real_mean - wall_mean - init_mean - convert_mean
                else:
                    # Fallback to single setup phase
                    row_data['setup_mean'] = real_mean - wall_mean

                runtime_rows.append(row_data)

            runtime_stats = pd.DataFrame(runtime_rows)
            runtime_stats = runtime_stats.sort_values(['matrix_size', 'num_blocks'])

            available_schedulers = [s for s in self.scheduler_order if s in runtime_stats['scheduler'].unique()]
            if not available_schedulers:
                print("  WARNING: No schedulers found in data, skipping runtime performance plot")
                return None

            # Create stacked plot with detailed breakdown
            if has_init_time and has_convert_time:
                # Pivot data for each component
                pivot_wall = runtime_stats.pivot_table(
                    index=['matrix_size', 'num_blocks'], columns='scheduler', values='wall_time_mean')
                pivot_init = runtime_stats.pivot_table(
                    index=['matrix_size', 'num_blocks'], columns='scheduler', values='init_time_mean')
                pivot_convert = runtime_stats.pivot_table(
                    index=['matrix_size', 'num_blocks'], columns='scheduler', values='convert_time_mean')
                pivot_other = runtime_stats.pivot_table(
                    index=['matrix_size', 'num_blocks'], columns='scheduler', values='other_setup_mean')
                pivot_real = runtime_stats.pivot_table(
                    index=['matrix_size', 'num_blocks'], columns='scheduler', values='real_time_mean')

                pivot_wall = pivot_wall[available_schedulers]
                pivot_init = pivot_init[available_schedulers]
                pivot_convert = pivot_convert[available_schedulers]
                pivot_other = pivot_other[available_schedulers]
                pivot_real = pivot_real[available_schedulers]

                return self._plot_detailed_stacked_bars(
                    pivot_wall.values,
                    pivot_init.values,
                    pivot_convert.values,
                    pivot_other.values,
                    pivot_real.values,
                    pivot_wall.index,
                    available_schedulers,
                    'Time (seconds)',
                    'Runtime Performance: Breakdown by Phase\nInit (bottom) + Convert + Other Setup + Compute (top)',
                    'runtime_performance.png'
                )
            else:
                # Fallback to simple 2-component stacking
                pivot_wall = runtime_stats.pivot_table(
                    index=['matrix_size', 'num_blocks'], columns='scheduler', values='wall_time_mean')
                pivot_setup = runtime_stats.pivot_table(
                    index=['matrix_size', 'num_blocks'], columns='scheduler', values='setup_mean')
                pivot_real = runtime_stats.pivot_table(
                    index=['matrix_size', 'num_blocks'], columns='scheduler', values='real_time_mean')

                pivot_wall = pivot_wall[available_schedulers]
                pivot_setup = pivot_setup[available_schedulers]
                pivot_real = pivot_real[available_schedulers]

                return self._plot_stacked_bars(
                    pivot_wall.values,
                    pivot_setup.values,
                    pivot_real.values,
                    pivot_wall.index,
                    available_schedulers,
                    'Time (seconds)',
                    'Runtime Performance: Setup Phase + Compute Time (Stacked)\nSetup (hatched, bottom) vs Compute (solid, top)',
                    'runtime_performance.png'
                )

            # Print setup phase analysis
            print("\n  Runtime Breakdown:")
            for _, row in runtime_stats.iterrows():
                if has_init_time and has_convert_time:
                    print(f"    {row['matrix_size']}×{row['matrix_size']} {row['num_blocks']}blk {row['scheduler']:15s}: "
                          f"init={row['init_time_mean']:5.2f}s, convert={row['convert_time_mean']:5.2f}s, "
                          f"other_setup={row['other_setup_mean']:5.2f}s, compute={row['wall_time_mean']:6.2f}s, "
                          f"total={row['real_time_mean']:6.2f}s")
                else:
                    setup_pct = (row['setup_mean'] / row['wall_time_mean']) * 100 if row['wall_time_mean'] > 0 else 0
                    print(f"    {row['matrix_size']}×{row['matrix_size']} {row['num_blocks']}blk {row['scheduler']:15s}: "
                          f"compute={row['wall_time_mean']:6.2f}s, total={row['real_time_mean']:6.2f}s, "
                          f"setup={row['setup_mean']:6.2f}s ({setup_pct:5.1f}%)")

        except Exception as e:
            print(f"  ERROR generating runtime performance plot: {e}")
            import traceback
            traceback.print_exc()
            return None

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

            # Calculate normalized change relative to sequential (fractional, not percent)
            relative_data = []
            for (matrix_size, num_blocks), group in runtime_stats.groupby(['matrix_size', 'num_blocks']):
                seq_data = group[group['scheduler'] == 'sequential']
                if len(seq_data) == 0:
                    continue

                seq_time = seq_data['wall_time_mean'].values[0]

                for _, row in group.iterrows():
                    ratio = (row['wall_time_mean'] / seq_time)
                    relative_data.append({
                        'matrix_size': matrix_size,
                        'num_blocks': num_blocks,
                        'scheduler': row['scheduler'],
                        'ratio': ratio
                    })

            runtime_relative_df = pd.DataFrame(relative_data)
            runtime_relative_df = runtime_relative_df.sort_values(['matrix_size', 'num_blocks'])

            # Pivot for plotting (values are multiplicative ratios)
            pivot_pct = runtime_relative_df.pivot_table(
                index=['matrix_size', 'num_blocks'], columns='scheduler', values='ratio')

            # Only use available schedulers
            available_schedulers = [s for s in self.scheduler_order if s in pivot_pct.columns]
            if not available_schedulers:
                print("  WARNING: No schedulers found, skipping")
                return None
            pivot_pct = pivot_pct[available_schedulers]

            return self._plot_relative_bars(
                pivot_pct.values,
                pivot_pct.index,
                available_schedulers,
                'Runtime Ratio vs Sequential',
                'Runtime Performance Ratio Relative to Sequential\n(Values > 1 = Slower, < 1 = Faster)',
                'runtime_performance_relative.png'
            )

        except Exception as e:
            print(f"  ERROR generating runtime performance relative plot: {e}")
            import traceback
            traceback.print_exc()
            return None

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
                return None
            pivot_rss = pivot_rss[available_schedulers]
            pivot_rss_low = pivot_rss_low.reindex(columns=available_schedulers).fillna(pivot_rss)
            pivot_rss_high = pivot_rss_high.reindex(columns=available_schedulers).fillna(pivot_rss)

            # Create plot
            return self._plot_grouped_bars(
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
            return None

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

            # Calculate normalized change relative to sequential (fractional, not percent)
            relative_data = []
            for (matrix_size, num_blocks), group in memory_stats.groupby(['matrix_size', 'num_blocks']):
                seq_data = group[group['scheduler'] == 'sequential']
                if len(seq_data) == 0:
                    continue

                seq_memory = seq_data['max_rss_kb_mean'].values[0]

                for _, row in group.iterrows():
                    ratio = (row['max_rss_kb_mean'] / seq_memory)
                    relative_data.append({
                        'matrix_size': matrix_size,
                        'num_blocks': num_blocks,
                        'scheduler': row['scheduler'],
                        'ratio': ratio
                    })

            memory_relative_df = pd.DataFrame(relative_data)
            memory_relative_df = memory_relative_df.sort_values(['matrix_size', 'num_blocks'])

            # Pivot for plotting (values are multiplicative ratios)
            pivot_pct = memory_relative_df.pivot_table(
                index=['matrix_size', 'num_blocks'], columns='scheduler', values='ratio')

            # Only use available schedulers
            available_schedulers = [s for s in self.scheduler_order if s in pivot_pct.columns]
            if not available_schedulers:
                print("  WARNING: No schedulers found, skipping")
                return None
            pivot_pct = pivot_pct[available_schedulers]

            return self._plot_relative_bars(
                pivot_pct.values,
                pivot_pct.index,
                available_schedulers,
                'Memory Ratio vs Sequential',
                'Memory Usage Ratio Relative to Sequential\n(Values > 1 = More Memory)',
                'memory_usage_relative.png'
            )

        except Exception as e:
            print(f"  ERROR generating memory usage relative plot: {e}")
            import traceback
            traceback.print_exc()
            return None

    # --- Plotly duplicates for all plots ---
    def plot_binary_size_grouped_plotly(self):
        """Plotly binary size grouped - one plot per matrix size."""
        if not self.has_binary_size:
            return None
        try:
            # Get unique matrix sizes
            matrix_sizes_list = sorted(self.build_metrics['matrix_size'].unique())

            # Generate one plot per matrix size
            for matrix_size in matrix_sizes_list:
                size_data = self.build_metrics[self.build_metrics['matrix_size'] == matrix_size]

                all_sections = set()
                plot_rows = []
                for _, row in size_data.iterrows():
                    num_blocks = row['num_blocks']
                    scheduler = row['scheduler']
                    binary_path = self.get_binary_path(matrix_size, num_blocks, scheduler)
                    if binary_path.exists():
                        sections = self.get_section_sizes(binary_path)
                        all_sections.update(sections.keys())
                        config_label = f"{num_blocks}b\n{scheduler}"
                        for section, size in sections.items():
                            plot_rows.append({'config_label': config_label, 'section': section, 'size_kb': size / 1024.0})

                if not plot_rows:
                    continue

                df = pd.DataFrame(plot_rows)
                fig = px.bar(df, x='config_label', y='size_kb', color='section', labels={'size_kb': 'Size (KB)'} )
                fig.update_layout(title_text=f'Binary Size Comparison - Sections (stacked) - {matrix_size}×{matrix_size}', xaxis_tickangle=-45)

                try:
                    html_path = self.images_dir / self._make_filename(f'binary_size_grouped_{matrix_size}_plotly.html')
                    pio.write_html(fig, file=str(html_path), include_plotlyjs='cdn', full_html=False)
                    print(f"  Saved interactive: {html_path}")
                except Exception:
                    pass
                try:
                    png_path = self.images_dir / self._make_filename(f'binary_size_grouped_{matrix_size}_plotly.png')
                    pio.write_image(fig, str(png_path))
                    print(f"  Saved static: {png_path}")
                except Exception:
                    pass

            return None  # Multiple figures generated
        except Exception as e:
            print(f"  ERROR generating Plotly grouped binary size plot: {e}")
            import traceback
            traceback.print_exc()
            return None

    def plot_compilation_time_plotly(self):
        """Plotly compilation time - one plot per matrix size."""
        if not self.has_compile_time:
            return None
        try:
            df = self.build_metrics.copy()
            df['num_blocks_str'] = df['num_blocks'].astype(str)

            # Get unique matrix sizes
            matrix_sizes_list = sorted(df['matrix_size'].unique())

            # Generate one plot per matrix size
            for matrix_size in matrix_sizes_list:
                size_data = df[df['matrix_size'] == matrix_size].copy()

                # Ensure scheduler order and numeric ordering of num_blocks
                num_blocks_order = sorted(size_data['num_blocks_str'].unique(), key=lambda x: int(x))
                category_orders = {'num_blocks_str': num_blocks_order, 'scheduler': self.scheduler_order}

                fig = px.bar(
                    size_data,
                    x='num_blocks_str',
                    y='compile_time',
                    color='scheduler',
                    barmode='group',
                    category_orders=category_orders,
                    color_discrete_map=self.scheduler_colors,
                    labels={'compile_time': 'Compilation Time (s)', 'num_blocks_str': 'Number of Blocks'}
                )
                fig.update_layout(title_text=f'Compilation Time by Number of Blocks - {matrix_size}×{matrix_size}')

                try:
                    html_path = self.images_dir / self._make_filename(f'compilation_time_{matrix_size}_plotly.html')
                    pio.write_html(fig, file=str(html_path), include_plotlyjs='cdn', full_html=False)
                    print(f"  Saved interactive: {html_path}")
                except Exception:
                    pass
                try:
                    png_path = self.images_dir / self._make_filename(f'compilation_time_{matrix_size}_plotly.png')
                    pio.write_image(fig, str(png_path))
                    print(f"  Saved static: {png_path}")
                except Exception:
                    pass

            return None  # Multiple figures generated
        except Exception as e:
            print(f"  ERROR generating Plotly compilation time plot: {e}")
            import traceback
            traceback.print_exc()
            return None

    def plot_runtime_performance_plotly(self):
        """Plotly duplicate for runtime performance (stacked) - one plot per matrix size."""
        try:
            has_init_time = 'init_time' in self.results.columns
            has_convert_time = 'convert_time' in self.results.columns

            # Get unique matrix sizes
            matrix_sizes_list = sorted(self.results['matrix_size'].unique())

            # Generate one plot per matrix size
            for matrix_size in matrix_sizes_list:
                size_data = self.results[self.results['matrix_size'] == matrix_size]

                runtime_rows = []
                for (num_blocks, scheduler), group in size_data.groupby(['num_blocks', 'scheduler']):
                    wall_mean, _, _ = self.bootstrap_ci(group['wall_time'].values, func=np.mean, n_boot=1000)
                    real_mean, _, _ = self.bootstrap_ci(group['real_time'].values, func=np.mean, n_boot=1000)
                    row = {'num_blocks': num_blocks, 'scheduler': scheduler, 'wall_time_mean': wall_mean, 'real_time_mean': real_mean}
                    if has_init_time and has_convert_time:
                        init_mean, _, _ = self.bootstrap_ci(group['init_time'].values, func=np.mean, n_boot=1000)
                        convert_mean, _, _ = self.bootstrap_ci(group['convert_time'].values, func=np.mean, n_boot=1000)
                        other = real_mean - wall_mean - init_mean - convert_mean
                        row.update({'init': init_mean, 'convert': convert_mean, 'other': other, 'compute': wall_mean})
                    else:
                        setup_mean = real_mean - wall_mean
                        row.update({'setup': setup_mean, 'compute': wall_mean})
                    runtime_rows.append(row)

                if not runtime_rows:
                    continue

                df = pd.DataFrame(runtime_rows)
                df = df.sort_values(['num_blocks', 'scheduler'])

                # Build config label
                df['config_label'] = df['num_blocks'].astype(str) + 'b\n' + df['scheduler']

                # Create figure with manual trace construction
                fig = go.Figure()

                # Define opacity values for each phase
                if 'init' in df.columns:
                    phase_styles = {
                        'init': {'opacity': 0.3},
                        'convert': {'opacity': 0.4},
                        'other': {'opacity': 0.5},
                        'compute': {'opacity': 0.8}
                    }
                    phases = ['init', 'convert', 'other', 'compute']
                    phase_names = ['Init', 'Convert', 'Other Setup', 'Compute']
                else:
                    phase_styles = {
                        'setup': {'opacity': 0.4},
                        'compute': {'opacity': 0.8}
                    }
                    phases = ['setup', 'compute']
                    phase_names = ['Setup', 'Compute']

                # Get schedulers in correct order
                available_schedulers = [s for s in self.scheduler_order if s in df['scheduler'].unique()]

                # Get config labels ordered by num_blocks and scheduler
                config_labels = []
                for num_blocks in sorted(df['num_blocks'].unique()):
                    for scheduler in available_schedulers:
                        label = f'{num_blocks}b\n{scheduler}'
                        if label in df['config_label'].values:
                            config_labels.append(label)

                # Add traces for each scheduler and phase combination
                for scheduler in available_schedulers:
                    base_color = self.scheduler_colors.get(scheduler, '#808080')
                    scheduler_data = df[df['scheduler'] == scheduler]

                    for phase, phase_name in zip(phases, phase_names):
                        values = [scheduler_data[scheduler_data['config_label'] == label][phase].values[0]
                                 if len(scheduler_data[scheduler_data['config_label'] == label]) > 0
                                 else 0
                                 for label in config_labels]

                        # Convert hex color to rgba with opacity
                        opacity = phase_styles[phase]['opacity']
                        hex_color = base_color.lstrip('#')
                        rgb = tuple(int(hex_color[i:i+2], 16) for i in (0, 2, 4))
                        rgba_color = f'rgba({rgb[0]}, {rgb[1]}, {rgb[2]}, {opacity})'

                        fig.add_trace(go.Bar(
                            name=f'{scheduler} ({phase_name})',
                            x=config_labels,
                            y=values,
                            marker=dict(
                                color=rgba_color,
                                line=dict(color='black', width=0.5)
                            ),
                            legendgroup=scheduler,
                            showlegend=True
                        ))

                fig.update_layout(
                    barmode='stack',
                    title_text=f'Runtime Performance: Breakdown by Phase - {matrix_size}×{matrix_size}',
                    xaxis_title='Configuration',
                    yaxis_title='Time (seconds)',
                    xaxis_tickangle=-45,
                    legend=dict(
                        title='Scheduler & Phase',
                        tracegroupgap=10
                    )
                )

                try:
                    html_path = self.images_dir / self._make_filename(f'runtime_performance_{matrix_size}_plotly.html')
                    pio.write_html(fig, file=str(html_path), include_plotlyjs='cdn', full_html=False)
                    print(f"  Saved interactive: {html_path}")
                except Exception:
                    pass
                try:
                    png_path = self.images_dir / self._make_filename(f'runtime_performance_{matrix_size}_plotly.png')
                    pio.write_image(fig, str(png_path))
                    print(f"  Saved static: {png_path}")
                except Exception:
                    pass

            return None  # Multiple figures generated
        except Exception as e:
            print(f"  ERROR generating Plotly runtime performance: {e}")
            import traceback
            traceback.print_exc()
            return None

    def plot_runtime_performance_relative_plotly(self):
        """Plotly runtime performance relative to sequential - one plot per matrix size."""
        try:
            runtime_rows = []
            for (matrix_size, num_blocks, scheduler), group in self.results.groupby(['matrix_size', 'num_blocks', 'scheduler']):
                mean, _, _ = self.bootstrap_ci(group['wall_time'].values, func=np.mean, n_boot=1000)
                runtime_rows.append({'matrix_size':matrix_size,'num_blocks':num_blocks,'scheduler':scheduler,'wall_time_mean':mean})
            runtime_stats = pd.DataFrame(runtime_rows)

            # Get unique matrix sizes
            matrix_sizes_list = sorted(runtime_stats['matrix_size'].unique())

            # Generate one plot per matrix size
            for matrix_size in matrix_sizes_list:
                size_data = runtime_stats[runtime_stats['matrix_size'] == matrix_size]

                relative = []
                for num_blocks, group in size_data.groupby('num_blocks'):
                    seq = group[group['scheduler']=='sequential']
                    if len(seq)==0:
                        continue
                    seq_time = seq['wall_time_mean'].values[0]
                    for _, row in group.iterrows():
                        ratio = (row['wall_time_mean'] / seq_time)
                        relative.append({'num_blocks':num_blocks,'scheduler':row['scheduler'],'ratio':ratio})

                if not relative:
                    continue

                df = pd.DataFrame(relative)
                df['config_label'] = df['num_blocks'].astype(str)+'b'
                category_orders = {'scheduler': self.scheduler_order}
                df['label'] = df['ratio'].apply(lambda v: f'{v:.2f}')
                fig = px.bar(df, x='config_label', y='ratio', color='scheduler', barmode='group',
                            category_orders=category_orders, color_discrete_map=self.scheduler_colors, text='label')
                fig.update_traces(textposition='outside')
                fig.update_layout(title_text=f'Runtime Ratio vs Sequential - {matrix_size}×{matrix_size}', xaxis_tickangle=-45)

                try:
                    html_path = self.images_dir / self._make_filename(f'runtime_performance_relative_{matrix_size}_plotly.html')
                    pio.write_html(fig, file=str(html_path), include_plotlyjs='cdn', full_html=False)
                    print(f"  Saved interactive: {html_path}")
                except Exception:
                    pass
                try:
                    png_path = self.images_dir / self._make_filename(f'runtime_performance_relative_{matrix_size}_plotly.png')
                    pio.write_image(fig, str(png_path))
                    print(f"  Saved static: {png_path}")
                except Exception:
                    pass

            return None  # Multiple figures generated
        except Exception as e:
            print(f"  ERROR generating Plotly runtime relative: {e}")
            import traceback
            traceback.print_exc()
            return None

    def plot_memory_usage_plotly(self):
        """Plotly memory usage - one plot per matrix size."""
        try:
            memory_rows = []
            for (matrix_size, num_blocks, scheduler), group in self.results.groupby(['matrix_size','num_blocks','scheduler']):
                mean, _, _ = self.bootstrap_ci(group['max_rss_kb'].values, func=np.mean, n_boot=1000)
                memory_rows.append({'matrix_size':matrix_size,'num_blocks':num_blocks,'scheduler':scheduler,'max_rss_kb_mean':mean})
            df = pd.DataFrame(memory_rows)
            if df.empty:
                return None

            # Get unique matrix sizes
            matrix_sizes_list = sorted(df['matrix_size'].unique())

            # Generate one plot per matrix size
            for matrix_size in matrix_sizes_list:
                size_data = df[df['matrix_size'] == matrix_size].copy()
                size_data['num_blocks_str'] = size_data['num_blocks'].astype(str)
                size_data['size_mb'] = size_data['max_rss_kb_mean']/1024.0
                category_orders = {'scheduler': self.scheduler_order,
                                 'num_blocks_str': sorted(size_data['num_blocks_str'].unique(), key=lambda x: int(x))}

                fig = px.bar(
                    size_data,
                    x='num_blocks_str',
                    y='size_mb',
                    color='scheduler',
                    barmode='group',
                    category_orders=category_orders,
                    color_discrete_map=self.scheduler_colors,
                    labels={'size_mb':'Max RSS (MB)', 'num_blocks_str': 'Number of Blocks'}
                )
                fig.update_layout(title_text=f'Memory Usage by Number of Blocks - {matrix_size}×{matrix_size}')

                try:
                    html_path = self.images_dir / self._make_filename(f'memory_usage_{matrix_size}_plotly.html')
                    pio.write_html(fig, file=str(html_path), include_plotlyjs='cdn', full_html=False)
                    print(f"  Saved interactive: {html_path}")
                except Exception:
                    pass
                try:
                    png_path = self.images_dir / self._make_filename(f'memory_usage_{matrix_size}_plotly.png')
                    pio.write_image(fig, str(png_path))
                    print(f"  Saved static: {png_path}")
                except Exception:
                    pass

            return None  # Multiple figures generated
        except Exception as e:
            print(f"  ERROR generating Plotly memory usage: {e}")
            import traceback
            traceback.print_exc()
            return None

    def plot_memory_usage_relative_plotly(self):
        """Plotly memory usage relative to sequential - one plot per matrix size."""
        try:
            memory_rows = []
            for (matrix_size, num_blocks, scheduler), group in self.results.groupby(['matrix_size','num_blocks','scheduler']):
                mean, _, _ = self.bootstrap_ci(group['max_rss_kb'].values, func=np.mean, n_boot=1000)
                memory_rows.append({'matrix_size':matrix_size,'num_blocks':num_blocks,'scheduler':scheduler,'max_rss_kb_mean':mean})
            mem = pd.DataFrame(memory_rows)

            # Get unique matrix sizes
            matrix_sizes_list = sorted(mem['matrix_size'].unique())

            # Generate one plot per matrix size
            for matrix_size in matrix_sizes_list:
                size_data = mem[mem['matrix_size'] == matrix_size]

                relative = []
                for num_blocks, group in size_data.groupby('num_blocks'):
                    seq = group[group['scheduler']=='sequential']
                    if len(seq)==0:
                        continue
                    seq_mem = seq['max_rss_kb_mean'].values[0]
                    for _, row in group.iterrows():
                        ratio = (row['max_rss_kb_mean'] / seq_mem)
                        relative.append({'num_blocks':num_blocks,'scheduler':row['scheduler'],'ratio':ratio})

                if not relative:
                    continue

                df = pd.DataFrame(relative)
                df['config_label'] = df['num_blocks'].astype(str)+'b'
                df['label'] = df['ratio'].apply(lambda v: f'{v:.2f}')
                fig = px.bar(df, x='config_label', y='ratio', color='scheduler', barmode='group',
                            color_discrete_map=self.scheduler_colors, text='label')
                fig.update_traces(textposition='outside')
                fig.update_layout(title_text=f'Memory Ratio vs Sequential - {matrix_size}×{matrix_size}', xaxis_tickangle=-45)

                try:
                    html_path = self.images_dir / self._make_filename(f'memory_usage_relative_{matrix_size}_plotly.html')
                    pio.write_html(fig, file=str(html_path), include_plotlyjs='cdn', full_html=False)
                    print(f"  Saved interactive: {html_path}")
                except Exception:
                    pass
                try:
                    png_path = self.images_dir / self._make_filename(f'memory_usage_relative_{matrix_size}_plotly.png')
                    pio.write_image(fig, str(png_path))
                    print(f"  Saved static: {png_path}")
                except Exception:
                    pass

            return None  # Multiple figures generated
        except Exception as e:
            print(f"  ERROR generating Plotly memory relative: {e}")
            import traceback
            traceback.print_exc()
            return None

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
        width = 1/len(schedulers)
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
        filepath = self.images_dir / self._make_filename(filename)
        plt.savefig(filepath, dpi=300, bbox_inches='tight')
        print(f"  Saved: {filepath}")
        
        # Return the figure for notebook display
        return fig

    def _plot_relative_bars(self, values, index, schedulers, ylabel, title, filename):
        """Helper to create relative performance bar plots."""
        fig, ax = plt.subplots(figsize=(16, 7))

        # Calculate positions
        matrix_sizes = sorted(index.get_level_values(0).unique())
        x_positions = []
        tick_positions = []
        tick_labels = []
        matrix_group_centers = []

        print(f"schedulers: {len(schedulers)}")

        current_x = 0
        width = 0.1
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

        # Compute max deviation from baseline (1.0) for sensible label offsets
        try:
            max_dev = float(np.nanmax(np.abs(values - 1.0)))
        except Exception:
            max_dev = 0.1

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
                    # Only label if value is meaningfully different from baseline (1.0)
                    if abs(val - 1.0) > 0.005:
                        # place label a small fraction above/below the bar end
                        offset_val = max(0.005, max_dev * 0.03)
                        y_pos = val + (offset_val if val >= 1.0 else -offset_val)
                        va = 'bottom' if val >= 1.0 else 'top'
                        ax.text(xp, y_pos, f'{val:.2f}', ha='center', va=va,
                               fontsize=8, fontweight='bold')

        # Add reference line at baseline (ratio == 1.0)
        ax.axhline(y=1.0, color='black', linestyle='-', linewidth=1.5, alpha=0.7)

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
        filepath = self.images_dir / self._make_filename(filename)
        plt.savefig(filepath, dpi=300, bbox_inches='tight')
        print(f"  Saved: {filepath}")
        
        # Return the figure for notebook display
        return fig

    def _plot_stacked_bars(self, wall_means, setup_means, real_means, index, schedulers, ylabel, title, filename):
        """Helper to create stacked bar plots showing compute time and setup phase."""
        fig, ax = plt.subplots(figsize=(16, 7))

        # Calculate positions
        matrix_sizes = sorted(index.get_level_values(0).unique())
        x_positions = []
        tick_positions = []
        tick_labels = []
        matrix_group_centers = []

        print(f'{len(scheduler)=}')

        current_x = 0
        width = 1/len(schedulers)
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

        # Define alpha values for stacked components
        stack_alphas = {
            'compute': 0.8,  # Lighter shade for compute time
            'setup': 0.4,    # Darker shade for setup phase
        }

        # Plot stacked bars for each scheduler
        for i, scheduler in enumerate(schedulers):
            offset = (i - len(schedulers)//2) * width if len(schedulers) % 2 == 1 else (i - len(schedulers)/2 + 0.5) * width
            base_color = self.scheduler_colors.get(scheduler, f'C{i}')

            # Bottom stack: setup phase (real_time - wall_time)
            ax.bar(x_positions + offset, setup_means[:, i], width,
                   label=f'{scheduler} (setup)',
                   color=base_color, alpha=stack_alphas['setup'],
                   edgecolor='black', linewidth=0.5,
                   hatch='//')

            # Top stack: compute time (reported wall_time)
            ax.bar(x_positions + offset, wall_means[:, i], width,
                   bottom=setup_means[:, i],
                   label=f'{scheduler} (compute)',
                   color=base_color, alpha=stack_alphas['compute'],
                   edgecolor='black', linewidth=0.5)

            # Add total time labels on top of stacked bars with compute/total fraction
            for x_pos, total_time, compute_time in zip(x_positions + offset, real_means[:, i], wall_means[:, i]):
                if not np.isnan(total_time) and not np.isnan(compute_time):
                    ax.text(x_pos, total_time + 0.2, f'{compute_time:.1f}/{total_time:.1f}s',
                           ha='center', va='bottom', fontsize=7, fontweight='bold')

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

        # Create custom legend grouping by scheduler (bottom to top order)
        legend_elements = []
        for scheduler in schedulers:
            base_color = self.scheduler_colors.get(scheduler, 'gray')
            legend_elements.append(Patch(facecolor=base_color, alpha=0.4, edgecolor='black',
                                        label=f'{scheduler} (setup)', linewidth=0.5, hatch='//'))
            legend_elements.append(Patch(facecolor=base_color, alpha=0.8, edgecolor='black',
                                        label=f'{scheduler} (compute)', linewidth=0.5))

        ax.legend(handles=legend_elements, loc='best', fontsize=9, ncol=2)
        ax.grid(True, alpha=0.3, axis='y')

        plt.tight_layout()
        filepath = self.images_dir / self._make_filename(filename)
        plt.savefig(filepath, dpi=300, bbox_inches='tight')
        print(f"  Saved: {filepath}")
        
        # Return the figure for notebook display
        return fig

    def _plot_detailed_stacked_bars(self, wall_means, init_means, convert_means, other_means, real_means, index, schedulers, ylabel, title, filename):
        """Helper to create detailed stacked bar plots showing all phases: init + convert + other_setup + compute."""
        fig, ax = plt.subplots(figsize=(16, 7))

        # Calculate positions
        matrix_sizes = sorted(index.get_level_values(0).unique())
        x_positions = []
        tick_positions = []
        tick_labels = []
        matrix_group_centers = []

        print(f"{len(schedulers)=}")

        current_x = 0
        width = 1/len(schedulers)
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

        # Define alpha values and hatches for stacked components (bottom to top)
        stack_styles = {
            'init': {'alpha': 0.3, 'hatch': '///'},       # Darkest, triple hatch
            'convert': {'alpha': 0.4, 'hatch': '//'},     # Medium-dark, double hatch
            'other': {'alpha': 0.5, 'hatch': '/'},        # Medium-light, single hatch
            'compute': {'alpha': 0.8, 'hatch': None},     # Lightest, no hatch
        }

        # Plot stacked bars for each scheduler
        for i, scheduler in enumerate(schedulers):
            offset = (i - len(schedulers)//2) * width if len(schedulers) % 2 == 1 else (i - len(schedulers)/2 + 0.5) * width
            base_color = self.scheduler_colors.get(scheduler, f'C{i}')

            # Bottom layer: initialization
            ax.bar(x_positions + offset, init_means[:, i], width,
                   color=base_color, alpha=stack_styles['init']['alpha'],
                   edgecolor='black', linewidth=0.5,
                   hatch=stack_styles['init']['hatch'])

            # Second layer: block conversion
            ax.bar(x_positions + offset, convert_means[:, i], width,
                   bottom=init_means[:, i],
                   color=base_color, alpha=stack_styles['convert']['alpha'],
                   edgecolor='black', linewidth=0.5,
                   hatch=stack_styles['convert']['hatch'])

            # Third layer: other setup
            ax.bar(x_positions + offset, other_means[:, i], width,
                   bottom=init_means[:, i] + convert_means[:, i],
                   color=base_color, alpha=stack_styles['other']['alpha'],
                   edgecolor='black', linewidth=0.5,
                   hatch=stack_styles['other']['hatch'])

            # Top layer: compute time
            ax.bar(x_positions + offset, wall_means[:, i], width,
                   bottom=init_means[:, i] + convert_means[:, i] + other_means[:, i],
                   color=base_color, alpha=stack_styles['compute']['alpha'],
                   edgecolor='black', linewidth=0.5)

            # Add total time labels on top of stacked bars with compute/total fraction
            for x_pos, total_time, compute_time in zip(x_positions + offset, real_means[:, i], wall_means[:, i]):
                if not np.isnan(total_time) and not np.isnan(compute_time):
                    ax.text(x_pos, total_time + 0.2, f'{compute_time:.1f}/{total_time:.1f}s',
                           ha='center', va='bottom', fontsize=7, fontweight='bold')

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

        # Create custom legend: schedulers as colored boxes, stages as gray boxes with hatches
        legend_elements = []

        # First section: Schedulers (colored boxes)
        for scheduler in schedulers:
            scheduler_color = self.scheduler_colors.get(scheduler, 'gray')
            legend_elements.append(Patch(facecolor=scheduler_color, edgecolor='black',
                                        label=scheduler, linewidth=0.5))

        # Add separator (empty patch)
        if legend_elements:
            legend_elements.append(Patch(facecolor='none', edgecolor='none', label=''))

        # Second section: Stages (gray with hatches)
        legend_elements.append(Patch(facecolor='gray', edgecolor='black',
                                    label='Initialization', linewidth=0.5, hatch='///'))
        legend_elements.append(Patch(facecolor='gray', edgecolor='black',
                                    label='Block Conversion', linewidth=0.5, hatch='//'))
        legend_elements.append(Patch(facecolor='gray', edgecolor='black',
                                    label='Other Setup', linewidth=0.5, hatch='/'))
        legend_elements.append(Patch(facecolor='gray', edgecolor='black',
                                    label='Compute', linewidth=0.5))

        ax.legend(handles=legend_elements, loc='best', fontsize=9, ncol=2, title='Schedulers & Stages', title_fontsize=10)
        ax.grid(True, alpha=0.3, axis='y')

        plt.tight_layout()
        filepath = self.images_dir / self._make_filename(filename)
        plt.savefig(filepath, dpi=300, bbox_inches='tight')
        print(f"  Saved: {filepath}")
        
        # Return the figure for notebook display
        return fig

    def generate_all(self):
        """Generate all visualizations."""
        print(f"\n{'='*80}")
        print(f"Generating visualizations for {self.experiment_dir.name}")
        print(f"{'='*80}\n")

        self.load_data()

        # Binary size visualizations (matplotlib + Plotly duplicates)
        self.plot_binary_size_overhead()
        try:
            plotly_func = getattr(self, 'plot_binary_size_overhead_plotly', None)
            if plotly_func is not None:
                plotly_func()
        except Exception:
            pass

        self.plot_binary_size_grouped()
        try:
            plotly_func = getattr(self, 'plot_binary_size_grouped_plotly', None)
            if plotly_func is not None:
                plotly_func()
        except Exception:
            pass

        # Compilation time (matplotlib + Plotly)
        self.plot_compilation_time()
        try:
            plotly_func = getattr(self, 'plot_compilation_time_plotly', None)
            if plotly_func is not None:
                plotly_func()
        except Exception:
            pass

        # Runtime performance (absolute and relative) - matplotlib + Plotly
        self.plot_runtime_performance()
        try:
            plotly_func = getattr(self, 'plot_runtime_performance_plotly', None)
            if plotly_func is not None:
                plotly_func()
        except Exception:
            pass

        self.plot_runtime_performance_relative()
        try:
            plotly_func = getattr(self, 'plot_runtime_performance_relative_plotly', None)
            if plotly_func is not None:
                plotly_func()
        except Exception:
            pass

        # Memory usage (absolute and relative) - matplotlib + Plotly
        self.plot_memory_usage()
        try:
            plotly_func = getattr(self, 'plot_memory_usage_plotly', None)
            if plotly_func is not None:
                plotly_func()
        except Exception:
            pass

        self.plot_memory_usage_relative()
        try:
            plotly_func = getattr(self, 'plot_memory_usage_relative_plotly', None)
            if plotly_func is not None:
                plotly_func()
        except Exception:
            pass

        print(f"\n{'='*80}")
        print(f"Visualization complete! Images saved to:")
        print(f"  {self.images_dir}")
        print(f"{'='*80}\n")

        return self.images_dir


def visualize_experiment(experiment_dir: Path, scheduler_order: Optional[List[str]] = None, suffix: Optional[str] = None) -> Path:
    """Run visualization for an experiment directory.

    Args:
        experiment_dir: Path to experiment directory
        scheduler_order: Optional list of schedulers in desired order
        suffix: Optional suffix to add to image filenames (e.g., "20250121_123456")

    Returns:
        Path to images directory
    """
    visualizer = ExperimentVisualizer(experiment_dir, scheduler_order, suffix)
    return visualizer.generate_all()


def main():
    """CLI entry point."""
    import argparse

    parser = argparse.ArgumentParser(description="Generate visualizations for experiment results")
    parser.add_argument("experiment_dir", type=Path,
                       help="Path to experiment directory (e.g., experiment/cholesky)")
    parser.add_argument("--schedulers", type=str, nargs='+',
                       help="Scheduler order for plots (default: sequential omp-task virtual-fifo)")
    parser.add_argument("--suffix", type=str,
                       help="Optional suffix to add to image filenames (e.g., 20250121_123456)")

    args = parser.parse_args()

    visualize_experiment(args.experiment_dir, args.schedulers, args.suffix)


if __name__ == "__main__":
    main()
