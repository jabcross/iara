#!/usr/bin/env python3
"""
Generic experiment visualizer - works with any application using the framework.

This module provides a declarative visualization system where all plots are
specified in experiments.yaml. Applications don't need custom visualization
code - just configure what they want to plot in the YAML.

Key features:
- Parameter-agnostic: Works with any parameter names via YAML configuration
- Declarative plotting: Define plots in YAML, not Python code
- Automatic parameter mapping: Finds the right columns based on parameter definitions
- Flexible plot types: Supports grouped bars, stacked bars, line plots, scatter plots
"""

import sys
from pathlib import Path
from typing import Dict, List, Any, Optional, Tuple
import pandas as pd
import plotly.graph_objects as go
import plotly.express as px
from plotly.subplots import make_subplots


def get_parameter_columns(yaml_config: Dict[str, Any]) -> Dict[str, str]:
    """
    Extract parameter column names from YAML config.

    Returns a mapping of parameter names to their CSV column names.
    If not specified, assumes parameter name = column name.
    """
    columns = {}

    # First, try parameter_metadata section
    metadata = yaml_config.get('parameter_metadata', {})
    for param_name, param_info in metadata.items():
        csv_col = param_info.get('csv_column', param_name)
        columns[param_name] = csv_col

    # Fallback: use parameter names directly
    for param in yaml_config.get('parameters', []):
        param_name = param['name']
        if param_name not in columns:
            columns[param_name] = param_name

    return columns


class GenericVisualizer:
    """Generic visualizer that works with any experiment application."""

    def __init__(self, csv_path: str, yaml_config: Dict[str, Any]):
        """
        Initialize visualizer.

        Args:
            csv_path: Path to results CSV file
            yaml_config: Parsed experiments.yaml configuration
        """
        self.csv_path = Path(csv_path)
        self.config = yaml_config
        self.results = pd.read_csv(csv_path)

        # Get parameter to column mapping
        self.param_columns = get_parameter_columns(yaml_config)

        # Get scheduler order
        self.scheduler_order = []
        for param in yaml_config.get('parameters', []):
            if param['name'] == 'scheduler':
                self.scheduler_order = param.get('choices', [])
                break

        # Get visualization config
        self.viz_config = yaml_config.get('visualization', {})
        self.plots = self.viz_config.get('plots', {})

    def generate_all(self) -> None:
        """Generate all plots specified in the YAML configuration."""
        if not self.viz_config.get('enabled', True):
            print("Visualization disabled in configuration")
            return

        print(f"Generating {len(self.plots)} visualization(s)...")

        for plot_name, plot_config in self.plots.items():
            print(f"  - {plot_name}")
            plot_type = plot_config.get('type', 'grouped_bars')

            try:
                if plot_type == 'grouped_bars':
                    self._plot_grouped_bars(plot_name, plot_config)
                elif plot_type == 'grouped_stacked_bars':
                    self._plot_grouped_stacked_bars(plot_name, plot_config)
                elif plot_type == 'stacked_bar':
                    self._plot_stacked_bar(plot_name, plot_config)
                elif plot_type == 'line_plot':
                    self._plot_line(plot_name, plot_config)
                elif plot_type == 'scatter':
                    self._plot_scatter(plot_name, plot_config)
                else:
                    print(f"    WARNING: Unknown plot type '{plot_type}'")
            except Exception as e:
                print(f"    ERROR: {e}")

        print("Done!")

    def _plot_grouped_bars(self, plot_name: str, config: Dict[str, Any]) -> None:
        """Generate grouped bar chart based on YAML configuration."""
        # Extract configuration
        y_metric = config.get('y_axis', {}).get('metric')
        x_axis_config = config.get('x_axis', {})
        title = config.get('title', plot_name)

        # Get data for the metric
        data = self.results[self.results['measurement'] == y_metric].copy()
        data['value'] = pd.to_numeric(data['value'], errors='coerce')
        data = data.dropna(subset=['value'])

        if data.empty:
            print(f"    No data for metric '{y_metric}'")
            return

        # Aggregate by scheduler and other dimensions
        group_by = ['scheduler']
        for param_name, axis_role in x_axis_config.items():
            if axis_role == 'group_by' or axis_role == 'separate_plots':
                if param_name in self.param_columns:
                    param_col = self.param_columns[param_name]
                    if param_col in data.columns:
                        group_by.append(param_col)

        agg_data = data.groupby(group_by).agg({
            'value': ['mean', 'std']
        }).reset_index()
        agg_data.columns = [c[0] if c[1] == '' else c[1] for c in agg_data.columns]

        # Create plot
        fig = px.bar(
            agg_data,
            x='scheduler',
            y='mean',
            color='scheduler' if len(group_by) == 1 else group_by[-1],
            title=title,
            barmode='group',
            error_y='std' if 'std' in agg_data.columns else None,
            labels={'value': f'{y_metric} ({self._get_unit(y_metric)})', 'scheduler': 'Scheduler'}
        )

        output_file = self.csv_path.parent / f"{plot_name}.html"
        fig.write_html(str(output_file))
        print(f"    Saved to {output_file}")

    def _plot_grouped_stacked_bars(self, plot_name: str, config: Dict[str, Any]) -> None:
        """Generate grouped stacked bar chart based on YAML configuration."""
        # This is Cholesky-specific pattern, need to generalize
        # For now, use the original Cholesky visualization if wall_time metric
        y_config = config.get('y_axis', {})

        if y_config.get('metric') == 'wall_time' and y_config.get('stack_composite'):
            # This is a composite stacked plot - need timing components
            timing_data = self.results[
                self.results['measurement'].isin(['init_time', 'convert_time', 'compute_time'])
            ].copy()
            timing_data['value'] = pd.to_numeric(timing_data['value'], errors='coerce')
            timing_data = timing_data.dropna(subset=['value'])

            if timing_data.empty:
                return

            # Find the parameter to separate plots by
            x_axis_config = config.get('x_axis', {})
            separate_param = None
            group_param = None
            for param, role in x_axis_config.items():
                if role == 'separate_plots':
                    separate_param = self.param_columns.get(param, param)
                elif role == 'group_by':
                    group_param = self.param_columns.get(param, param)

            if not separate_param or separate_param not in timing_data.columns:
                print(f"    ERROR: Separator parameter not found in data")
                return

            # Get unique values for separation
            sep_values = sorted(timing_data[separate_param].unique())
            component_order = ['init_time', 'convert_time', 'compute_time']
            component_colors = {
                'init_time': '#636EFA',
                'convert_time': '#EF553B',
                'compute_time': '#00CC96'
            }

            fig = make_subplots(
                rows=1, cols=len(sep_values),
                subplot_titles=[f'{separate_param.title()}: {val}' for val in sep_values],
                x_title=group_param.replace('_', ' ').title() if group_param else 'Value',
                y_title='Time (s)',
                horizontal_spacing=0.08
            )

            available_schedulers = [s for s in self.scheduler_order if s in timing_data['scheduler'].unique()]

            for col_idx, sep_val in enumerate(sep_values, start=1):
                size_data = timing_data[timing_data[separate_param] == sep_val]

                if group_param and group_param in size_data.columns:
                    group_values = sorted(size_data[group_param].unique())
                else:
                    group_values = [None]

                agg_data = size_data.groupby(['scheduler', 'measurement']).agg({
                    'value': ['mean', 'std']
                }).reset_index()
                agg_data.columns = ['scheduler', 'measurement', 'mean_time', 'std_time']

                bar_width = 0.8 / len(available_schedulers)

                for comp_idx, component in enumerate(component_order):
                    for sched_idx, scheduler in enumerate(available_schedulers):
                        scheduler_data = agg_data[
                            (agg_data['scheduler'] == scheduler) &
                            (agg_data['measurement'] == component)
                        ]

                        if scheduler_data.empty:
                            continue

                        x_pos = sched_idx * bar_width
                        show_legend = (col_idx == 1)

                        fig.add_trace(
                            go.Bar(
                                x=[scheduler],
                                y=scheduler_data['mean_time'].values,
                                name=f'{component}',
                                marker_color=component_colors[component],
                                showlegend=show_legend,
                                legendgroup=component,
                                hovertemplate=f'<b>{scheduler}</b><br>{component}<br>Time: %{{y:.4f}}s<extra></extra>'
                            ),
                            row=1, col=col_idx
                        )

            fig.update_layout(
                title_text=config.get('title', plot_name),
                barmode='stack',
                height=600
            )

            output_file = self.csv_path.parent / f"{plot_name}.html"
            fig.write_html(str(output_file))
            print(f"    Saved to {output_file}")
        else:
            # Fallback to regular grouped bars
            self._plot_grouped_bars(plot_name, config)

    def _plot_stacked_bar(self, plot_name: str, config: Dict[str, Any]) -> None:
        """Generate stacked bar chart based on YAML configuration."""
        # For binary size breakdown and other stacked plots
        y_metric = config.get('y_axis', {}).get('metric')

        if y_metric == 'section':
            # Binary size stacking
            section_data = self.results[self.results['measurement'] == 'section'].copy()
            section_data['value'] = pd.to_numeric(section_data['value'], errors='coerce')
            section_data = section_data.dropna(subset=['value'])

            if section_data.empty:
                return

            # Pivot to get sections as columns
            pivot_data = section_data.pivot_table(
                index='scheduler',
                columns='section',
                values='value',
                aggfunc='mean'
            ).fillna(0)

            fig = go.Figure()
            for section in pivot_data.columns:
                fig.add_trace(go.Bar(
                    x=pivot_data.index,
                    y=pivot_data[section],
                    name=section
                ))

            fig.update_layout(
                barmode='stack',
                title=config.get('title', plot_name),
                xaxis_title='Scheduler',
                yaxis_title='Size (bytes)'
            )

            output_file = self.csv_path.parent / f"{plot_name}.html"
            fig.write_html(str(output_file))
            print(f"    Saved to {output_file}")

    def _plot_line(self, plot_name: str, config: Dict[str, Any]) -> None:
        """Generate line plot based on YAML configuration."""
        y_metric = config.get('y_axis', {}).get('metric')

        data = self.results[self.results['measurement'] == y_metric].copy()
        data['value'] = pd.to_numeric(data['value'], errors='coerce')
        data = data.dropna(subset=['value'])

        if data.empty:
            return

        # Group and aggregate
        agg_data = data.groupby('scheduler').agg({
            'value': ['mean', 'std']
        }).reset_index()
        agg_data.columns = ['scheduler', 'mean', 'std']

        fig = px.line(
            agg_data,
            x='scheduler',
            y='mean',
            title=config.get('title', plot_name),
            markers=True,
            error_y='std',
            labels={'value': f'{y_metric} ({self._get_unit(y_metric)})'}
        )

        output_file = self.csv_path.parent / f"{plot_name}.html"
        fig.write_html(str(output_file))
        print(f"    Saved to {output_file}")

    def _plot_scatter(self, plot_name: str, config: Dict[str, Any]) -> None:
        """Generate scatter plot based on YAML configuration."""
        y_metric = config.get('y_axis', {}).get('metric')

        data = self.results[self.results['measurement'] == y_metric].copy()
        data['value'] = pd.to_numeric(data['value'], errors='coerce')
        data = data.dropna(subset=['value'])

        if data.empty:
            return

        fig = px.scatter(
            data,
            x='scheduler',
            y='value',
            color='scheduler',
            title=config.get('title', plot_name),
            labels={'value': f'{y_metric} ({self._get_unit(y_metric)})'}
        )

        output_file = self.csv_path.parent / f"{plot_name}.html"
        fig.write_html(str(output_file))
        print(f"    Saved to {output_file}")

    def _get_unit(self, metric_name: str) -> str:
        """Get the unit for a measurement from YAML config."""
        for meas in self.config.get('measurements', []):
            if meas.get('name') == metric_name:
                return meas.get('unit', '')
        return ''
