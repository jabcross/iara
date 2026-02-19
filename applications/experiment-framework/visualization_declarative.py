"""
Generic declarative visualization module for experiment results.

Reads visualization specifications from experiments.yaml and generates plots
accordingly. Supports any set of parameters and measurements.
"""

from pathlib import Path
from typing import Dict, List, Any, Optional
import pandas as pd
import plotly.graph_objects as go
from plotly.subplots import make_subplots
import yaml


def load_yaml_config(yaml_path: Path) -> dict:
    """Load YAML configuration."""
    with open(yaml_path, 'r') as f:
        return yaml.safe_load(f)


def generate_visualizations_from_yaml(csv_path: str, yaml_path: str, scheduler_order: list) -> None:
    """
    Generate visualizations based on declarative specifications in YAML.

    Args:
        csv_path: Path to results CSV file
        yaml_path: Path to experiments.yaml configuration
        scheduler_order: Ordered list of scheduler names
    """
    # Load configuration
    yaml_config = load_yaml_config(Path(yaml_path))
    plot_specs = yaml_config.get('visualization', {}).get('plots', {})

    if not plot_specs:
        print('No visualization specs found in YAML')
        return

    # Load results
    results = pd.read_csv(csv_path)

    # Convert value column to numeric
    if 'value' in results.columns:
        results['value'] = pd.to_numeric(results['value'], errors='coerce')

    print(f'Loaded {len(results)} measurements')
    print(f'Generating {len(plot_specs)} visualization(s) from YAML...\n')

    # Generate each plot specified in YAML
    for plot_name, plot_spec in plot_specs.items():
        try:
            _generate_plot(results, plot_name, plot_spec, scheduler_order)
        except Exception as e:
            print(f'Error generating plot "{plot_name}": {e}')
            continue


def _generate_plot(results: pd.DataFrame, plot_name: str, spec: Dict[str, Any], scheduler_order: list) -> None:
    """
    Generate a single plot based on specification.

    Spec format:
    {
        'title': str,
        'type': 'grouped_bars' | 'stacked_bar' | 'grouped_stacked_bars',
        'y_axis': {
            'metric': str,  # Column name or measurement name
            'filter': str   # Optional: filter expression
        },
        'x_axis': {
            'param1': 'separate_plots' | 'group_by',
            'param2': 'group_by'
        },
        'scheduler': 'ordered_within_group',
        'stack_by': str  # Optional: column to stack by
    }
    """
    print(f'Generating "{plot_name}": {spec.get("title", plot_name)}')

    plot_type = spec.get('type', 'grouped_bars')
    y_spec = spec.get('y_axis', {})
    x_spec = spec.get('x_axis', {})
    title = spec.get('title', plot_name)

    # Get metric (measurement type or column name)
    metric = y_spec.get('metric')
    if not metric:
        print(f'  Skipped: No y_axis.metric specified')
        return

    # Filter data
    filtered_data = results.copy()

    # Apply filter expression if specified
    filter_expr = y_spec.get('filter')
    if filter_expr:
        # For simple "column is not null" filters
        if ' is not null' in filter_expr:
            col = filter_expr.split(' is not null')[0].strip()
            filtered_data = filtered_data[filtered_data[col].notna()]

    # Filter by measurement type(s) if specified
    measurements = y_spec.get('measurements', [])
    if measurements:
        # Multiple measurements specified (e.g., for stacking)
        filtered_data = filtered_data[filtered_data['measurement'].isin(measurements)]
    elif 'measurement' in results.columns and metric in results['measurement'].unique():
        # Single measurement filter by metric name
        filtered_data = filtered_data[filtered_data['measurement'] == metric]
    elif metric == 'value' or (metric in filtered_data.columns):
        # Using value column directly - no measurement filter needed
        pass
    else:
        print(f'  Skipped: Metric "{metric}" not found in data')
        return

    if filtered_data.empty:
        print(f'  Skipped: No data available after filtering')
        return

    # Identify grouping parameters
    separate_params = [k for k, v in x_spec.items() if v == 'separate_plots']
    group_params = [k for k, v in x_spec.items() if v == 'group_by']
    scheduler_param = 'scheduler' if 'scheduler' in filtered_data.columns else None

    # Generate plots
    if plot_type == 'grouped_bars':
        _plot_grouped_bars(filtered_data, title, metric, group_params, separate_params,
                          scheduler_param, scheduler_order)
    elif plot_type == 'stacked_bar':
        stack_by = spec.get('stack_by')
        _plot_stacked_bars(filtered_data, title, metric, group_params, separate_params,
                          stack_by, scheduler_param, scheduler_order)
    elif plot_type == 'grouped_stacked_bars':
        stack_by = spec.get('stack_by')  # For composite metrics
        _plot_grouped_stacked_bars(filtered_data, title, metric, group_params, separate_params,
                                  stack_by, scheduler_param, scheduler_order)
    else:
        print(f'  Skipped: Unknown plot type "{plot_type}"')


def _plot_grouped_bars(data: pd.DataFrame, title: str, metric: str,
                       group_params: List[str], separate_params: List[str],
                       scheduler_param: Optional[str], scheduler_order: list) -> None:
    """Generate grouped bar charts."""
    if not separate_params:
        # Single plot with all data
        _create_single_grouped_bar_plot(data, title, metric, group_params,
                                       scheduler_param, scheduler_order)
    else:
        # Multiple plots, one for each unique value of separate_params
        for separate_param in separate_params:
            unique_vals = sorted(data[separate_param].dropna().unique())
            for val in unique_vals:
                plot_data = data[data[separate_param] == val]
                plot_title = f"{title} ({separate_param}={val})"
                _create_single_grouped_bar_plot(plot_data, plot_title, metric, group_params,
                                              scheduler_param, scheduler_order)


def _create_single_grouped_bar_plot(data: pd.DataFrame, title: str, metric: str,
                                   group_params: List[str], scheduler_param: Optional[str],
                                   scheduler_order: list) -> None:
    """Create a single grouped bar plot."""
    fig = go.Figure()

    if not group_params and not scheduler_param:
        print(f'  Skipped: No grouping parameters')
        return

    # Determine which column to use for values
    # If metric is a measurement name (in 'measurement' column), use 'value' column
    # Otherwise use metric as column name directly
    if 'measurement' in data.columns and metric in data['measurement'].unique():
        value_col = 'value'
    else:
        value_col = metric

    # Get unique groups based on group_params
    if group_params:
        # Get unique combinations of group parameters
        unique_group_dicts = []
        group_labels = []
        for _, group_row in data.drop_duplicates(subset=group_params).iterrows():
            group_key = '_'.join([f'{p}={group_row[p]}' for p in group_params])
            unique_group_dicts.append({p: group_row[p] for p in group_params})
            group_labels.append(group_key)
    else:
        unique_group_dicts = [{}]  # Single "group" with no parameters
        group_labels = ['data']

    if scheduler_param:
        schedulers = [s for s in scheduler_order if s in data[scheduler_param].unique()]
        if not schedulers:
            schedulers = list(data[scheduler_param].unique())

        # For each scheduler, create a trace with bars for each group
        for scheduler in schedulers:
            sched_data = data[data[scheduler_param] == scheduler]
            y_vals = []
            x_labels = []

            # Build bars: one for each group
            for group_dict in unique_group_dicts:
                # Filter to this group
                group_data = sched_data.copy()
                for p in group_params:
                    group_data = group_data[group_data[p] == group_dict[p]]

                # Get average value
                values = group_data[value_col].dropna()
                y_vals.append(values.mean() if len(values) > 0 else 0)
                x_labels.append(group_labels[unique_group_dicts.index(group_dict)])

            fig.add_trace(go.Bar(
                name=scheduler,
                x=x_labels,
                y=y_vals
            ))

    fig.update_layout(
        title=title,
        barmode='group',
        height=600,
        template='plotly_white'
    )
    fig.show()


def _plot_stacked_bars(data: pd.DataFrame, title: str, metric: str,
                       group_params: List[str], separate_params: List[str],
                       stack_by: Optional[str], scheduler_param: Optional[str],
                       scheduler_order: list) -> None:
    """Generate stacked bar charts."""
    if not stack_by:
        print(f'  Skipped: stack_by not specified for stacked plot')
        return

    if not separate_params:
        _create_single_stacked_bar_plot(data, title, metric, group_params, stack_by,
                                       scheduler_param, scheduler_order)
    else:
        for separate_param in separate_params:
            unique_vals = sorted(data[separate_param].dropna().unique())
            for val in unique_vals:
                plot_data = data[data[separate_param] == val]
                plot_title = f"{title} ({separate_param}={val})"
                _create_single_stacked_bar_plot(plot_data, plot_title, metric, group_params,
                                               stack_by, scheduler_param, scheduler_order)


def _create_single_stacked_bar_plot(data: pd.DataFrame, title: str, metric: str,
                                   group_params: List[str], stack_by: str,
                                   scheduler_param: Optional[str], scheduler_order: list) -> None:
    """Create a single stacked bar plot.

    Optionally groups by group_params on the x-axis, and stacks by stack_by.
    If scheduler_param is present, different schedulers appear as separate bars within each group.
    """
    fig = go.Figure()

    # Determine which column to use for values
    # If metric is a measurement name (in 'measurement' column), use 'value' column
    # Otherwise use metric as column name directly
    if 'measurement' in data.columns and metric in data['measurement'].unique():
        value_col = 'value'
    else:
        value_col = metric

    # Get unique stack values (sections or other stack_by values)
    stack_values = sorted(data[stack_by].dropna().unique())

    # Get unique groups (if any)
    if group_params:
        unique_group_dicts = []
        group_labels = []
        for _, group_row in data.drop_duplicates(subset=group_params).iterrows():
            group_key = '_'.join([f'{p}={group_row[p]}' for p in group_params])
            unique_group_dicts.append({p: group_row[p] for p in group_params})
            group_labels.append(group_key)
    else:
        unique_group_dicts = [{}]  # Single "group" with no parameters
        group_labels = ['data']

    x_positions = []
    x_labels_list = []
    tickvals = []
    annotations = []

    if scheduler_param:
        schedulers = [s for s in scheduler_order if s in data[scheduler_param].unique()]
        if not schedulers:
            schedulers = list(data[scheduler_param].unique())

        # Build x-axis positions for proper grouping
        group_position = 0
        for group_idx, group_dict in enumerate(unique_group_dicts):
            for scheduler in schedulers:
                x_positions.append(group_position)
                x_labels_list.append(scheduler)  # Just show scheduler name on tick
                tickvals.append(group_position)  # Track actual position for each tick
                group_position += 1
            group_position += 0.5  # Add spacing between groups

        # For each stack value, create a trace with all bars
        for stack_val in stack_values:
            stack_data = data[data[stack_by] == stack_val]
            y_vals = []

            # Build bars: for each group, add a bar for each scheduler
            for group_dict in unique_group_dicts:
                for scheduler in schedulers:
                    # Filter to this group and scheduler
                    group_sched_data = stack_data.copy()
                    for p in group_params:
                        group_sched_data = group_sched_data[group_sched_data[p] == group_dict[p]]
                    group_sched_data = group_sched_data[group_sched_data[scheduler_param] == scheduler]

                    # Get average value
                    if len(group_sched_data) > 0:
                        values = group_sched_data[value_col].dropna()
                        y_vals.append(values.mean() if len(values) > 0 else 0)
                    else:
                        y_vals.append(0)

            fig.add_trace(go.Bar(
                name=str(stack_val),
                x=x_positions,
                y=y_vals
            ))

        # Calculate group label positions (centered under each group of bars)
        group_label_positions = []
        group_label_text = []
        group_position = 0
        for group_idx, group_dict in enumerate(unique_group_dicts):
            group_center = group_position + (len(schedulers) - 1) / 2
            group_key = '_'.join([f'{p}={group_dict[p]}' for p in group_params])
            group_label_positions.append(group_center)
            group_label_text.append(group_key)
            group_position += len(schedulers) + 0.5

        # Add group labels as annotations below the x-axis
        for pos, label in zip(group_label_positions, group_label_text):
            annotations.append(
                dict(
                    text=label,
                    x=pos,
                    y=-0.15,
                    xref='x',
                    yref='paper',
                    showarrow=False,
                    xanchor='center',
                    font=dict(size=12, color='black')
                )
            )

    fig.update_layout(
        title=title,
        barmode='stack',
        height=600,
        xaxis=dict(
            tickmode='array',
            tickvals=tickvals,
            ticktext=x_labels_list
        ),
        template='plotly_white',
        annotations=annotations if annotations else None
    )
    fig.show()


def _plot_grouped_stacked_bars(data: pd.DataFrame, title: str, metric: str,
                               group_params: List[str], separate_params: List[str],
                               stack_by: Optional[str], scheduler_param: Optional[str],
                               scheduler_order: list) -> None:
    """Generate grouped and stacked bar charts."""
    if not separate_params:
        # Single plot with all data
        _create_single_grouped_stacked_bar_plot(data, title, metric, group_params, stack_by,
                                               scheduler_param, scheduler_order)
    else:
        # Multiple plots, one for each unique value of separate_params
        for separate_param in separate_params:
            unique_vals = sorted(data[separate_param].dropna().unique())
            for val in unique_vals:
                plot_data = data[data[separate_param] == val]
                plot_title = f"{title} ({separate_param}={val})"
                _create_single_grouped_stacked_bar_plot(plot_data, plot_title, metric, group_params,
                                                       stack_by, scheduler_param, scheduler_order)


def _create_single_grouped_stacked_bar_plot(data: pd.DataFrame, title: str, metric: str,
                                           group_params: List[str], stack_by: Optional[str],
                                           scheduler_param: Optional[str], scheduler_order: list) -> None:
    """Create a single grouped and stacked bar plot.

    Layout: x-axis shows groups (from group_params), within each group there are multiple
    bars (one per scheduler), and each bar is stacked by stack_by values (no averaging across schedulers).
    """
    fig = go.Figure()

    # Determine which column to use for values
    if 'measurement' in data.columns and metric in data['measurement'].unique():
        value_col = 'value'
    else:
        value_col = metric

    # Get unique stack values (measurements/components to stack)
    if not stack_by:
        print(f'  Skipped: stack_by not specified for grouped_stacked plot')
        return

    stack_values = sorted(data[stack_by].dropna().unique())

    if not group_params:
        print(f'  Skipped: No grouping parameters')
        return

    # Get unique groups
    unique_group_dicts = []
    group_labels = []
    for _, group_row in data.drop_duplicates(subset=group_params).iterrows():
        group_key = '_'.join([f'{p}={group_row[p]}' for p in group_params])
        unique_group_dicts.append({p: group_row[p] for p in group_params})
        group_labels.append(group_key)

    annotations = []
    x_labels_list = []
    tickvals = []

    if scheduler_param:
        schedulers = [s for s in scheduler_order if s in data[scheduler_param].unique()]
        if not schedulers:
            schedulers = list(data[scheduler_param].unique())

        # Build x-axis positions for proper grouping
        # Create x-axis with groups and scheduler subgroups
        x_positions = []
        x_labels_list = []
        tickvals = []
        group_position = 0
        for group_idx, group_dict in enumerate(unique_group_dicts):
            for scheduler in schedulers:
                x_positions.append(group_position)
                x_labels_list.append(scheduler)  # Just show scheduler name on tick
                tickvals.append(group_position)  # Track actual position for each tick
                group_position += 1
            group_position += 0.5  # Add spacing between groups

        # For each stack value, create a trace with grouped bars
        for stack_val in stack_values:
            stack_data = data[data[stack_by] == stack_val]

            y_vals = []

            # Build bars: for each group, add a bar for each scheduler
            for group_dict in unique_group_dicts:
                for scheduler in schedulers:
                    # Filter to this group and scheduler
                    group_sched_data = stack_data.copy()
                    for p in group_params:
                        group_sched_data = group_sched_data[group_sched_data[p] == group_dict[p]]
                    group_sched_data = group_sched_data[group_sched_data[scheduler_param] == scheduler]

                    # Get average value for this stack value, group, and scheduler combination
                    values = group_sched_data[value_col].dropna()
                    y_vals.append(values.mean() if len(values) > 0 else 0)

            fig.add_trace(go.Bar(
                name=str(stack_val),
                x=x_positions,
                y=y_vals,
                text=[f'{v:.2f}' for v in y_vals],
                textposition='inside'
            ))

        # Calculate group label positions (centered under each group of bars)
        group_label_positions = []
        group_label_text = []
        group_position = 0
        for group_idx, group_dict in enumerate(unique_group_dicts):
            group_center = group_position + (len(schedulers) - 1) / 2
            group_key = '_'.join([f'{p}={group_dict[p]}' for p in group_params])
            group_label_positions.append(group_center)
            group_label_text.append(group_key)
            group_position += len(schedulers) + 0.5

        # Add group labels as annotations below the x-axis
        for pos, label in zip(group_label_positions, group_label_text):
            annotations.append(
                dict(
                    text=label,
                    x=pos,
                    y=-0.15,
                    xref='x',
                    yref='paper',
                    showarrow=False,
                    xanchor='center',
                    font=dict(size=12, color='black')
                )
            )

    fig.update_layout(
        title=title,
        barmode='stack',
        height=600,
        xaxis=dict(
            tickmode='array',
            tickvals=tickvals,
            ticktext=x_labels_list
        ),
        template='plotly_white',
        annotations=annotations if annotations else None
    )
    fig.show()
