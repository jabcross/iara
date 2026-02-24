"""
Visualization module for IaRa Unified Experiment Framework.

This module generates Vega-Lite visualizations from experiment results.
"""

import copy
import json
import logging
from datetime import datetime, timezone
from pathlib import Path
from typing import Dict, List, Any, Tuple, Optional

from .config import ValidationError


# Configure logging
logger = logging.getLogger(__name__)


# Constants for validation
VALID_MARK_TYPES = [
    "bar", "line", "area", "point", "circle", "square",
    "text", "tick", "rule", "rect"
]

VALID_ENCODING_CHANNELS = [
    "x", "y", "color", "size", "shape", "opacity",
    "row", "column", "detail", "xOffset", "yOffset", "tooltip"
]

PLOTLY_MARK_TYPES = [
    "grouped_bars", "stacked_bar", "grouped_stacked_bars",
    "line_plot", "scatter"
]

# ELF sections that hold zero-filled (uninitialised) data — they occupy no
# space in the on-disk binary and are skipped by the "no-BSS" plot variant.
BSS_SECTION_NAMES = {"bss", "lbss", "tbss", "sbss"}

# Constants for unit conversion (Task 4.2)
TIME_MEASUREMENTS = {
    "wall_time_s", "user_time_s", "system_time_s", "compute_time_s",
    "init_time_s", "convert_time_s", "elapsed_time_s"
}

MEMORY_MEASUREMENTS = {
    "max_rss_bytes", "memory_usage_bytes", "peak_memory_bytes", "rss_bytes"
}

# Thresholds for time unit selection
TIME_THRESHOLDS = {
    "us": 0.001,    # Below 1ms → use microseconds
    "ms": 1.0,      # Below 1s → use milliseconds
    "s": float('inf')  # 1s or above → use seconds
}

# Conversion factors (unit label, scale factor)
MEMORY_SCALES = [
    (1024**4, "TB"),
    (1024**3, "GB"),
    (1024**2, "MB"),
    (1024, "KB"),
    (1, "B")
]

TIME_SCALES = [
    (1e6, "μs"),   # Microseconds
    (1e3, "ms"),   # Milliseconds
    (1.0, "s")     # Seconds
]


def _default_plot_specs(app_name: str) -> Dict[str, Dict[str, Any]]:
    """
    Standard plots generated for every app that has no visualization section.

    Covers the four base measurements always collected by the framework:
    wall_time, max_rss_mb, compilation time, and binary size by section.
    """
    title_prefix = app_name or "Application"
    return {
        "wall_time": {
            "title": f"{title_prefix} — Wall Time",
            "type": "grouped_bars",
            "y_axis": {"metric": "wall_time"},
            "x_axis": {},
            "description": "Total wall clock execution time per instance",
        },
        "max_rss": {
            "title": f"{title_prefix} — Peak Memory",
            "type": "grouped_bars",
            "y_axis": {"metric": "max_rss_mb"},
            "x_axis": {},
            "description": "Peak resident set size per instance",
        },
        "compilation_time": {
            "title": f"{title_prefix} — Compilation Time",
            "type": "grouped_bars",
            "y_axis": {"metric": "binary_compilation_time"},
            "x_axis": {},
            "description": "Time to compile and link the binary",
        },
        "binary_size": {
            "title": f"{title_prefix} — Binary Size Breakdown",
            "type": "stacked_bar",
            "y_axis": {"metric": "binary_size_breakdown"},
            "x_axis": {},
            "stack_by": "section",
            "description": "Binary size split by text/data/bss sections",
        },
        "binary_size_no_bss": {
            "title": f"{title_prefix} — Binary Size Breakdown (on-disk sections only)",
            "type": "stacked_bar",
            "y_axis": {"metric": "binary_size_breakdown"},
            "x_axis": {},
            "stack_by": "section",
            "exclude_bss": True,
            "description": "Binary size split by on-disk sections only (BSS/LBSS excluded)",
        },
    }


def load_plot_specs(yaml_config: Dict[str, Any]) -> Dict[str, Dict[str, Any]]:
    """
    Extract plot specifications from complete YAML config.

    Algorithm:
        1. Extract yaml_config.get("visualization", {}).get("plots", {})
        2. If visualization or plots missing: Log info, return empty dict
        3. Return dict of plot specs keyed by plot name

    Args:
        yaml_config: Complete YAML config from load_experiments_yaml()

    Returns:
        Dict of plot specs keyed by plot name. Example:
        {
          "runtime_performance": {
            "title": "Cholesky Runtime Performance",
            "type": "grouped_stacked_bars",
            "y_axis": {"metric": "value", "measurements": [...]},
            ...
          },
          "memory_usage": {...},
          ...
        }

    Error Handling:
        - Missing visualization section: Log info (graceful), return {}
        - Invalid spec structure: Log warning, skip that plot
        - Empty plots dict: Return {}
    """
    # Extract visualization section
    visualization = yaml_config.get("visualization", {})

    if not visualization:
        logger.info("No visualization section found in config")
        return {}

    # Extract plots
    plots = visualization.get("plots", {})

    if not plots:
        logger.info("No plots defined in visualization section")
        return {}

    # Validate each plot spec structure
    valid_plots = {}
    for plot_name, plot_spec in plots.items():
        if not isinstance(plot_spec, dict):
            logger.warning(f"Skipping plot '{plot_name}': spec is not a dictionary")
            continue

        # Basic validation - plots should have at least a title or type
        if "title" not in plot_spec and "type" not in plot_spec:
            logger.warning(f"Skipping plot '{plot_name}': missing both 'title' and 'type'")
            continue

        valid_plots[plot_name] = plot_spec

    logger.info(f"Loaded {len(valid_plots)} plot specifications")
    return valid_plots


def translate_plotly_to_vegalite(
    plot_name: str,
    plotly_spec: Dict[str, Any]
) -> Dict[str, Any]:
    """
    Translate Plotly-style declarative spec to Vega-Lite v5 JSON.

    Supported Plotly Types & Vega-Lite Equivalents:
        - grouped_bars: Groups of bars with different colors
        - stacked_bar: Bars stacked by a dimension
        - grouped_stacked_bars: Groups with stacked bars inside

    Args:
        plot_name: Name of the plot (used as fallback title)
        plotly_spec: Plotly-style specification dict

    Returns:
        Valid Vega-Lite v5 JSON specification

    Raises:
        ValidationError: If required fields are missing

    Error Handling:
        - Unknown plot type: Log warning, default to grouped_bars
        - Missing required fields (title, metric): Raise ValidationError
        - Invalid scheduler name: Log warning, use default
    """
    # Get plot type (default to grouped_bars)
    plot_type = plotly_spec.get("type", "grouped_bars")

    if plot_type not in PLOTLY_MARK_TYPES:
        logger.warning(
            f"Unknown plot type '{plot_type}' for plot '{plot_name}', "
            f"defaulting to 'grouped_bars'"
        )
        plot_type = "grouped_bars"

    # Create base Vega-Lite structure
    vl_spec = {
        "$schema": "https://vega.github.io/schema/vega-lite/v5.json",
        "title": plotly_spec.get("title", plot_name),
        "width": 800,
        "height": {"step": 30},
        # Bind interval selection to the chart scales so scroll zooms and
        # drag pans on both axes without requiring any extra widgets.
        "params": [{"name": "grid", "select": "interval", "bind": "scales"}],
        "data": {
            "url": "PLACEHOLDER",
            "format": {"type": "json", "property": "instances"}
        },
        "mark": "bar",
        "encoding": {},
        "transform": [
            {
                # Strip <app>_<set>_ — keeps the scheduler token so every
                # instance has a unique Y label.
                "calculate": "replace(datum.name, /^[^_]+_[^_]+_/, '')",
                "as": "short_name"
            },
            {
                # Strip <app>_<set>_<scheduler>_ — used for sorting so that
                # bars sharing the same configuration appear adjacent.
                "calculate": "replace(datum.name, /^[^_]+_[^_]+_[^_]+_/, '')",
                "as": "config_key"
            }
        ]
    }

    # Extract y_axis configuration
    y_axis = plotly_spec.get("y_axis", {})
    if not y_axis:
        raise ValidationError(f"Plot '{plot_name}' missing required 'y_axis' configuration")

    metric = y_axis.get("metric")
    if not metric:
        raise ValidationError(f"Plot '{plot_name}' missing required 'metric' in y_axis")

    measurements = y_axis.get("measurements", [])

    # Extract x_axis configuration
    x_axis = plotly_spec.get("x_axis", {})

    # Extract scheduler configuration
    scheduler_order = plotly_spec.get("scheduler", "ordered_within_group")

    # Determine faceting and grouping parameters
    facet_param = None
    group_param = None

    for param_name, param_action in x_axis.items():
        if param_action == "separate_plots":
            facet_param = param_name
        elif param_action == "group_by":
            group_param = param_name

    # Build encodings based on plot type
    if plot_type == "grouped_bars":
        # Simple grouped bars: x = group param, y = metric, color = scheduler
        _build_grouped_bars_encoding(
            vl_spec, metric, group_param, facet_param, scheduler_order
        )

    elif plot_type == "stacked_bar":
        # Stacked bars: x = group param, y = metric, color = stack_by field
        stack_by = plotly_spec.get("stack_by")
        _build_stacked_bar_encoding(
            vl_spec, metric, group_param, facet_param, stack_by
        )

    elif plot_type == "grouped_stacked_bars":
        # Grouped stacked bars: complex, use column faceting
        stack_by = plotly_spec.get("stack_by")
        _build_grouped_stacked_bars_encoding(
            vl_spec, metric, measurements, group_param, facet_param, stack_by, scheduler_order
        )

    elif plot_type == "line_plot":
        # Line plot: x = x_axis param, y = metric, color = scheduler
        vl_spec["mark"] = "line"
        _build_line_plot_encoding(
            vl_spec, metric, x_axis, scheduler_order
        )

    # Validate before returning
    validate_vegalite_spec(vl_spec)

    return vl_spec


def _build_grouped_bars_encoding(
    vl_spec: Dict[str, Any],
    metric: str,
    group_param: Optional[str],
    facet_param: Optional[str],
    scheduler_order: str
) -> None:
    """Build encoding for grouped_bars plot type (horizontal bars)."""
    # Y encoding: unique per instance (short_name keeps the scheduler token),
    # but sorted by config_key so same-config bars from different schedulers
    # appear adjacent — scheduler becomes the finest visual granularity.
    vl_spec["encoding"]["y"] = {
        "field": "short_name",
        "type": "ordinal",
        "title": "Instance",
        "sort": {"field": "config_key", "op": "min", "order": "ascending"},
        "axis": {"labelLimit": 300}
    }

    # X encoding: metric value — always starts at zero (all tracked metrics are non-negative)
    x_encoding = {
        "field": _get_metric_field(metric),
        "type": "quantitative",
        "title": _get_metric_title(metric),
        "scale": {"domainMin": 0}
    }

    # Add SI unit formatting for byte-based metrics
    if _is_byte_metric(metric):
        x_encoding["axis"] = {"format": ".2s"}

    vl_spec["encoding"]["x"] = x_encoding

    # Color encoding: scheduler
    vl_spec["encoding"]["color"] = {
        "field": "parameters.scheduler",
        "type": "nominal",
        "title": "Scheduler"
    }

    # Tooltip encoding
    tooltip_fields = [
        {"field": "short_name", "type": "nominal", "title": "Instance"},
        {"field": "parameters.scheduler", "type": "nominal", "title": "Scheduler"},
        {"field": _get_metric_field(metric), "type": "quantitative", "title": _format_title(metric)}
    ]
    vl_spec["encoding"]["tooltip"] = tooltip_fields


def _build_stacked_bar_encoding(
    vl_spec: Dict[str, Any],
    metric: str,
    group_param: Optional[str],
    facet_param: Optional[str],
    stack_by: Optional[str]
) -> None:
    """Build encoding for stacked_bar plot type (horizontal bars)."""
    # Y encoding: always use instance names (short_name) so bars are labeled with their identity
    vl_spec["encoding"]["y"] = {
        "field": "short_name",
        "type": "ordinal",
        "title": "Instance",
        "axis": {"labelLimit": 300}
    }

    # Special handling for binary section stacking
    if stack_by == "section":
        # Fold over binary.sections.* fields.  The actual list of fields is
        # left empty here and populated later by generate_vegalite_json once it
        # has computed the top-N sections from the real results data.
        if "transform" not in vl_spec:
            vl_spec["transform"] = []

        vl_spec["transform"].append({
            "fold": [],          # patched by generate_vegalite_json
            "as": ["section_field", "value"]
        })

        # Strip the "binary.sections." prefix to obtain the bare section name
        vl_spec["transform"].append({
            "calculate": "replace(datum.section_field, 'binary.sections.', '')",
            "as": "section_type"
        })

        # Add formula transform to format bytes as human-readable SI units
        vl_spec["transform"].append({
            "calculate": "format(datum.value, '.2s')",
            "as": "value_formatted"
        })

        # X encoding: raw value (will use SI units via format)
        vl_spec["encoding"]["x"] = {
            "field": "value",
            "type": "quantitative",
            "title": "Binary Section Size",
            "axis": {"format": ".2s"},
            "scale": {"domainMin": 0}
        }

        # Color encoding: section type
        vl_spec["encoding"]["color"] = {
            "field": "section_type",
            "type": "nominal",
            "title": "Section"
        }
    else:
        # Standard X encoding: metric value
        x_encoding = {
            "field": _get_metric_field(metric),
            "type": "quantitative",
            "title": _get_metric_title(metric)
        }

        # Add SI unit formatting for byte-based metrics
        if _is_byte_metric(metric):
            x_encoding["axis"] = {"format": ".2s"}

        vl_spec["encoding"]["x"] = x_encoding

        # Color encoding: stack_by field
        if stack_by:
            vl_spec["encoding"]["color"] = {
                "field": f"parameters.{stack_by}",
                "type": "nominal",
                "title": _format_title(stack_by)
            }
        else:
            # Default to scheduler
            vl_spec["encoding"]["color"] = {
                "field": "parameters.scheduler",
                "type": "nominal",
                "title": "Scheduler"
            }

    # Tooltip encoding
    tooltip_fields = [
        {"field": "short_name", "type": "nominal", "title": "Instance"},
        {"field": "parameters.scheduler", "type": "nominal", "title": "Scheduler"}
    ]
    if stack_by == "section":
        tooltip_fields.append({"field": "section_type", "type": "nominal", "title": "Section"})
        tooltip_fields.append({"field": "value_formatted", "type": "nominal", "title": "Size (SI Units)"})
    else:
        tooltip_fields.append({"field": _get_metric_field(metric), "type": "quantitative", "title": _format_title(metric)})
    vl_spec["encoding"]["tooltip"] = tooltip_fields


def _build_grouped_stacked_bars_encoding(
    vl_spec: Dict[str, Any],
    metric: str,
    measurements: List[str],
    group_param: Optional[str],
    facet_param: Optional[str],
    stack_by: Optional[str],
    scheduler_order: str
) -> None:
    """Build encoding for grouped_stacked_bars plot type (horizontal bars)."""
    # Y encoding: always use instance names (short_name) so bars are labeled with their identity
    vl_spec["encoding"]["y"] = {
        "field": "short_name",
        "type": "ordinal",
        "title": "Instance",
        "axis": {"labelLimit": 300}
    }


    # X encoding: metric value
    # For grouped_stacked_bars, we're often stacking measurements
    if stack_by == "measurement" and measurements:
        # Add fold transform to convert measurements into key-value pairs
        # Fold the execution.statistics.<measurement>.mean fields
        fold_fields = [f"execution.statistics.{m}.mean" for m in measurements]

        if "transform" not in vl_spec:
            vl_spec["transform"] = []

        vl_spec["transform"].append({
            "fold": fold_fields,
            "as": ["measurement_field", "value"]
        })

        # Add formula transform to extract clean measurement name from field path
        # Extract the measurement name from "execution.statistics.<name>.mean"
        vl_spec["transform"].append({
            "calculate": "split(datum.measurement_field, '.')[2]",
            "as": "measurement_type"
        })

        # Use folded value for x encoding
        vl_spec["encoding"]["x"] = {
            "field": "value",
            "type": "quantitative",
            "title": "Time (s)"
        }
    else:
        x_encoding = {
            "field": _get_metric_field(metric),
            "type": "quantitative",
            "title": _get_metric_title(metric)
        }

        # Add SI unit formatting for byte-based metrics
        if _is_byte_metric(metric):
            x_encoding["axis"] = {"format": ".2s"}

        vl_spec["encoding"]["x"] = x_encoding

    # Color encoding: stack field
    if stack_by:
        if stack_by == "measurement":
            # Stacking by measurement type (from fold transform)
            vl_spec["encoding"]["color"] = {
                "field": "measurement_type",
                "type": "nominal",
                "title": "Measurement"
            }
        else:
            vl_spec["encoding"]["color"] = {
                "field": f"parameters.{stack_by}",
                "type": "nominal",
                "title": _format_title(stack_by)
            }
    else:
        vl_spec["encoding"]["color"] = {
            "field": "parameters.scheduler",
            "type": "nominal",
            "title": "Scheduler"
        }

    # yOffset: ordinal scheduler field so Vega-Lite auto-subdivides the band
    if group_param and group_param != "instance":
        vl_spec["encoding"]["yOffset"] = {
            "field": "parameters.scheduler",
            "type": "ordinal"
        }

    # Tooltip encoding
    tooltip_fields = [
        {"field": "short_name", "type": "nominal", "title": "Instance"},
        {"field": "parameters.scheduler", "type": "nominal", "title": "Scheduler"}
    ]
    if stack_by == "measurement":
        tooltip_fields.append({"field": "measurement_type", "type": "nominal", "title": "Measurement"})
        tooltip_fields.append({"field": "value", "type": "quantitative", "title": "Time (s)"})
    else:
        tooltip_fields.append({"field": _get_metric_field(metric), "type": "quantitative", "title": _format_title(metric)})
    vl_spec["encoding"]["tooltip"] = tooltip_fields


def _build_line_plot_encoding(
    vl_spec: Dict[str, Any],
    metric: str,
    x_axis: Dict[str, str],
    scheduler_order: str
) -> None:
    """Build encoding for line_plot type."""
    # Find the x_axis parameter
    x_param = None
    for param_name, param_action in x_axis.items():
        if param_action == "x_axis":
            x_param = param_name
            break

    # X encoding: x_axis parameter
    if x_param:
        vl_spec["encoding"]["x"] = {
            "field": f"parameters.{x_param}",
            "type": "quantitative",  # Line plots typically use quantitative x
            "title": _format_title(x_param)
        }
    else:
        # Default to scheduler if no x_axis specified
        vl_spec["encoding"]["x"] = {
            "field": "parameters.scheduler",
            "type": "ordinal",
            "title": "Scheduler"
        }

    # Y encoding: metric value
    vl_spec["encoding"]["y"] = {
        "field": f"execution.statistics.{metric}.mean",
        "type": "quantitative",
        "title": _format_title(metric)
    }

    # Color encoding: scheduler for separate lines
    vl_spec["encoding"]["color"] = {
        "field": "parameters.scheduler",
        "type": "nominal",
        "title": "Scheduler"
    }


def _format_title(field_name: str) -> str:
    """Convert snake_case field name to Title Case."""
    return field_name.replace("_", " ").title()


def _get_metric_field(metric: str) -> str:
    """
    Get the correct field path for a metric.

    Maps metric names to their actual field locations in the data:
    - execution.statistics.* for most measurements
    - compilation.total_time_s for binary_compilation_time
    - binary.*_bytes for binary size breakdown metrics
    """
    if metric == "binary_compilation_time":
        return "compilation.total_time_s"
    elif metric.startswith("binary_"):
        # binary_size_breakdown -> binary.binary_size_breakdown_bytes
        return f"binary.{metric}_bytes"
    else:
        return f"execution.statistics.{metric}.mean"


def _is_byte_metric(metric: str) -> bool:
    """Check if a metric is in bytes and should use SI unit formatting."""
    return metric.startswith("binary_") or "rss" in metric.lower()


def _get_metric_title(metric: str) -> str:
    """Get formatted title for metric with units."""
    title = _format_title(metric)

    # Add units based on metric type
    if metric == "binary_compilation_time":
        return "Binary Compilation Time (s)"
    elif metric.startswith("binary_"):
        return f"{title} (Bytes)"
    elif "rss" in metric.lower():
        return "Memory (MB)"
    elif "time" in metric.lower() and metric != "binary_compilation_time":
        return f"{title} (s)"

    return title


def validate_vegalite_spec(spec: Dict[str, Any]) -> bool:
    """
    Validate Vega-Lite specification has required fields and valid structure.

    Validation Rules:
        - $schema: Must contain "vega-lite" and "v5"
        - data: Must exist and have "url" key
        - mark: Must exist and be valid mark type
        - encoding: Must exist and be dict with at least one encoding

    Args:
        spec: Vega-Lite specification to validate

    Returns:
        True if all checks pass

    Raises:
        ValidationError: If spec is invalid with specific reason

    Error Handling:
        - Missing required field: Raise ValidationError with specific field name
        - Invalid mark type: Raise ValidationError listing valid types
        - Invalid encoding channel: Log warning but pass (forward compatibility)
        - Malformed spec: Raise ValidationError with context
    """
    # Check $schema
    if "$schema" not in spec:
        raise ValidationError("Missing required field: $schema")

    schema = spec["$schema"]
    if not isinstance(schema, str):
        raise ValidationError("$schema must be a string")

    if "vega-lite" not in schema.lower():
        raise ValidationError("$schema must contain 'vega-lite'")

    if "v5" not in schema:
        raise ValidationError(
            f"$schema must specify v5, got: {schema}. "
            "Expected: https://vega.github.io/schema/vega-lite/v5.json"
        )

    # Check data
    if "data" not in spec:
        raise ValidationError("Missing required field: data")

    data = spec["data"]
    if not isinstance(data, dict):
        raise ValidationError("data must be a dictionary")

    if "url" not in data:
        raise ValidationError("data must have a 'url' field")

    # Check mark
    if "mark" not in spec:
        raise ValidationError("Missing required field: mark")

    mark = spec["mark"]
    mark_type = mark if isinstance(mark, str) else mark.get("type")

    if not mark_type:
        raise ValidationError("mark must be a string or dict with 'type' field")

    if mark_type not in VALID_MARK_TYPES:
        raise ValidationError(
            f"Invalid mark type: '{mark_type}'. "
            f"Valid types: {', '.join(VALID_MARK_TYPES)}"
        )

    # Check encoding
    if "encoding" not in spec:
        raise ValidationError("Missing required field: encoding")

    encoding = spec["encoding"]
    if not isinstance(encoding, dict):
        raise ValidationError("encoding must be a dictionary")

    if not encoding:
        raise ValidationError("encoding must have at least one channel")

    # Validate encoding channels (warn on unknown, don't fail)
    for channel in encoding.keys():
        if channel not in VALID_ENCODING_CHANNELS:
            logger.warning(
                f"Unknown encoding channel: '{channel}'. "
                f"Valid channels: {', '.join(VALID_ENCODING_CHANNELS)}"
            )

    return True


def load_vegalite_spec(
    yaml_config: Dict[str, Any],
    plot_name: str
) -> Dict[str, Any]:
    """
    Load Vega-Lite spec from YAML configuration.

    Expected YAML structure:
        visualization:
          plots:
            runtime_performance:
              $schema: "https://vega.github.io/schema/vega-lite/v5.json"
              title: "Runtime Performance"
              data:
                url: "PLACEHOLDER"  # Will be replaced
              mark: "bar"
              encoding:
                x: {...}
                y: {...}

    Args:
        yaml_config: Complete configuration dictionary
        plot_name: Name of the plot to load

    Returns:
        Complete Vega-Lite JSON specification

    Raises:
        ValidationError: If spec is incomplete or invalid

    Note:
        TODO: Implementation deferred to Phase 4
    """
    pass


def inject_data_url(
    vegalite_spec: Dict[str, Any],
    results_json_path: Path
) -> Dict[str, Any]:
    """
    Replace PLACEHOLDER data URL with actual results file path.

    Algorithm:
        1. Deep copy input spec (don't modify original)
        2. Convert results_json_path to absolute path
        3. Create file URL: f"file://{absolute_path}"
        4. Update spec["data"]["url"] = file_url
        5. Ensure spec["data"]["format"] = {"type": "json", "property": "instances"}
        6. Return modified spec

    Args:
        vegalite_spec: Vega-Lite specification with PLACEHOLDER URL
        results_json_path: Path to results JSON file

    Returns:
        Modified spec with injected data URL

    Raises:
        ValueError: If path is invalid

    Error Handling:
        - File doesn't exist: Log WARNING (don't fail, visualization may be generated before execution)
        - Invalid path: Raise ValueError
    """
    # Deep copy to avoid modifying original
    spec = copy.deepcopy(vegalite_spec)

    # Validate path
    if not results_json_path:
        raise ValueError("results_json_path cannot be None or empty")

    # Convert to absolute path
    try:
        absolute_path = results_json_path.resolve()
    except Exception as e:
        raise ValueError(f"Invalid path: {results_json_path}: {e}")

    # Check if file exists (warning only, don't fail)
    if not absolute_path.exists():
        logger.warning(
            f"Results file does not exist yet: {absolute_path}. "
            "This is expected if visualization is generated before execution."
        )

    # Create file:// URL
    file_url = f"file://{absolute_path}"

    # Update spec
    spec["data"]["url"] = file_url
    spec["data"]["format"] = {"type": "json", "property": "instances"}

    logger.debug(f"Injected data URL: {file_url}")

    return spec


def auto_select_display_unit(
    measurement_name: str,
    values: List[float]
) -> Tuple[str, float]:
    """
    Analyze value magnitudes and select appropriate display unit.

    Algorithm:
        1. Identify measurement type from name suffix:
           - Time: ends with "_s" (seconds)
           - Memory: ends with "_bytes"
           - Default: numeric
        2. Calculate max value
        3. Select unit based on magnitude thresholds
        4. Return (unit_label, scale_factor)

    Args:
        measurement_name: Name of the measurement field
        values: List of numeric values for magnitude analysis

    Returns:
        Tuple of (display_unit_string, scale_factor)
        - display_unit_string: "μs", "ms", "s", "B", "KB", "MB", "GB", "TB", or ""
        - scale_factor: Multiplier to convert base unit to display unit

    Examples:
        - ("compute_time_s", [0.0005, 0.0008]) → ("μs", 1e6)
        - ("max_rss_bytes", [2048000, 3072000]) → ("MB", 1048576)
        - ("keypoints_found", [100, 200]) → ("", 1.0)
    """
    # Calculate max value
    max_val = max(values) if values else 0

    # Identify measurement type from name
    # Time measurements (base unit: seconds)
    if measurement_name.endswith("_s"):
        if max_val < 0.001:
            return ("μs", 1e6)  # microseconds
        elif max_val < 1.0:
            return ("ms", 1000)  # milliseconds
        else:
            return ("s", 1.0)  # seconds

    # Memory measurements (base unit: bytes)
    elif measurement_name.endswith("_bytes"):
        if max_val < 1024:
            return ("B", 1)  # bytes
        elif max_val < 1024**2:
            return ("KB", 1024)  # kilobytes
        elif max_val < 1024**3:
            return ("MB", 1024**2)  # megabytes
        elif max_val < 1024**4:
            return ("GB", 1024**3)  # gigabytes
        else:
            return ("TB", 1024**4)  # terabytes

    # Unknown measurement type (no conversion)
    else:
        return ("", 1.0)


def add_unit_conversion_transform(
    spec: Dict[str, Any],
    measurement: str,
    display_unit: str,
    scale_factor: float
) -> Dict[str, Any]:
    """
    Add Vega-Lite calculate transform for unit conversion.

    Algorithm:
        1. Create calculate transform with division by scale_factor
        2. Initialize transforms list if needed
        3. Append transform to list
        4. Update encoding to use converted field and include unit in title
        5. Return modified spec

    Args:
        spec: Vega-Lite specification (modified in place)
        measurement: Name of the measurement field
        display_unit: Display unit string (e.g., "ms", "MB")
        scale_factor: Scale factor to divide by

    Returns:
        Modified spec with conversion transform and updated encoding

    Example:
        Input: measurement="compute_time_s", display_unit="ms", scale_factor=1000
        Adds transform:
        {
          "calculate": "datum['execution.statistics.compute_time_s.mean'] / 1000",
          "as": "compute_time_s_display"
        }
        Updates encoding field: "compute_time_s_display"
        Updates title: "Compute Time (ms)"
    """
    # Skip if no conversion needed (scale_factor == 1.0 and no unit)
    if scale_factor == 1.0 and not display_unit:
        return spec

    # Create the full field path for the measurement
    # Measurements are stored as: execution.statistics.{measurement}.mean
    full_field = f"execution.statistics.{measurement}.mean"
    display_field = f"{measurement}_display"

    # Create calculate transform
    transform = {
        "calculate": f"datum['{full_field}'] / {scale_factor}",
        "as": display_field
    }

    # Initialize transforms list if needed
    if "transform" not in spec:
        spec["transform"] = []

    # Append transform
    spec["transform"].append(transform)

    # Update encoding to use converted field
    for channel, encoding in spec.get("encoding", {}).items():
        if isinstance(encoding, dict) and "field" in encoding:
            # Check if this encoding uses our measurement
            if encoding["field"] == full_field:
                # Update to use display field
                encoding["field"] = display_field

                # Update title to include unit
                if "title" in encoding:
                    original_title = encoding["title"]
                    if display_unit:
                        encoding["title"] = f"{original_title} ({display_unit})"
                else:
                    # Generate title from field name
                    title = _format_title(measurement)
                    if display_unit:
                        encoding["title"] = f"{title} ({display_unit})"
                    else:
                        encoding["title"] = title

    logger.debug(
        f"Added unit conversion for {measurement}: "
        f"{full_field} → {display_field} (scale: {scale_factor}, unit: {display_unit})"
    )

    return spec


def apply_unit_conversions(
    spec: Dict[str, Any],
    results_data: Dict[str, Any]
) -> Dict[str, Any]:
    """
    Analyze measurements in spec and apply appropriate unit conversions.

    Algorithm:
        1. Extract measurement fields from encoding channels
        2. Load sample data from results to get value ranges
        3. For each measurement:
           - Call auto_select_display_unit(measurement, values)
           - Call add_unit_conversion_transform(spec, measurement, unit, scale)
        4. Return modified spec

    Args:
        spec: Vega-Lite specification
        results_data: Phase 3 results dict with experiment data

    Returns:
        Spec with unit conversion transforms applied

    Error Handling:
        - No data available: Use base units, log WARNING
        - Missing measurement in data: Skip conversion, log WARNING
    """
    # Extract measurement fields from encoding
    measurement_fields = set()

    for channel, encoding in spec.get("encoding", {}).items():
        if isinstance(encoding, dict) and "field" in encoding:
            field = encoding["field"]
            # Extract measurement name from field path
            # Fields are like: "execution.statistics.{measurement}.mean"
            if field.startswith("execution.statistics.") and field.endswith(".mean"):
                measurement = field.replace("execution.statistics.", "").replace(".mean", "")
                measurement_fields.add(measurement)

    if not measurement_fields:
        logger.debug("No measurement fields found in encoding")
        return spec

    # Load sample data from results
    instances = results_data.get("instances", [])

    if not instances:
        logger.warning(
            "No instances data available for unit conversion. "
            "Using base units without conversion."
        )
        return spec

    # For each measurement, collect values and apply conversion
    for measurement in measurement_fields:
        values = []

        # Extract values from instances
        for instance in instances:
            if "execution" in instance and "statistics" in instance["execution"]:
                stats = instance["execution"]["statistics"].get(measurement, {})
                if "mean" in stats:
                    try:
                        values.append(float(stats["mean"]))
                    except (ValueError, TypeError):
                        logger.warning(
                            f"Could not convert value to float for {measurement} "
                            f"in instance: {stats.get('mean')}"
                        )

        if not values:
            logger.warning(
                f"No values found for measurement '{measurement}'. "
                "Skipping unit conversion for this field."
            )
            continue

        # Select appropriate display unit
        display_unit, scale_factor = auto_select_display_unit(measurement, values)

        # Apply conversion transform
        if display_unit or scale_factor != 1.0:
            spec = add_unit_conversion_transform(spec, measurement, display_unit, scale_factor)
            logger.info(
                f"Applied unit conversion for {measurement}: "
                f"{len(values)} values, max={max(values):.6f}, unit={display_unit}, scale={scale_factor}"
            )

    return spec


def convert_units_for_display(
    data: Dict[str, Any],
    measurement: str,
    base_unit: str
) -> Tuple[Dict[str, Any], str, float]:
    """
    Convert from base SI to appropriate display unit.

    Conversion Strategy:
        Memory (bytes):
            < 1024        → bytes (1)
            < 1024²       → KB (1024)
            < 1024³       → MB (1024²)
            >= 1024³      → GB (1024³)

        Time (seconds):
            < 0.001       → μs (1e-6)
            < 1.0         → ms (0.001)
            >= 1.0        → s (1)

    Args:
        data: Data dictionary with values to convert
        measurement: Name of the measurement
        base_unit: Base unit (bytes or seconds)

    Returns:
        Tuple of (converted_data, display_unit, scale_factor)

    Example:
        data['value'] = [1048576, 2097152]  # bytes
        → ([1.0, 2.0], "MB", 1048576)

    Note:
        TODO: Implementation deferred to Phase 4 (deprecated in favor of auto_select_display_unit)
    """
    pass


def _wrap_with_parameter_table(
    bar_spec: Dict[str, Any],
    param_keys: List[str],
    instances: Optional[List[Dict[str, Any]]] = None,
) -> Dict[str, Any]:
    """
    Wrap a fully-processed bar-chart spec in an hconcat with a parameter table
    on the left.  The table uses a fold transform so all columns share a single
    view with no inter-column gaps; column headers are rendered as angled axis
    labels on a top-oriented ordinal X axis.

    The step width (pixels per column band) is computed from the longest cell
    value across all columns at ~8 px/char plus 16 px of horizontal padding.

    ``instances`` should be the raw list from the results JSON; it is used only
    for step-width estimation — if omitted the header widths are used.
    """
    param_fields = [f"parameters.{k}" for k in param_keys]

    # Carry over every transform except fold ones (those belong to the bar mark).
    shared_transforms = [t for t in bar_spec.get("transform", []) if "fold" not in t]

    # Mirror bar Y encoding; suppress axis labels (the bar chart provides them).
    table_y = {**copy.deepcopy(bar_spec["encoding"]["y"]), "axis": None}

    # Step width = widest cell value (headers are angled so they extend beyond
    # their band and don't constrain the step).
    px_per_char = 8
    padding = 8
    max_val_chars = 4  # minimum readable width
    if instances:
        for key in param_keys:
            for inst in instances:
                val = inst.get("parameters", {}).get(key, "")
                max_val_chars = max(max_val_chars, len(str(val)))
    else:
        max_val_chars = max(len(k) for k in param_keys)
    step = max_val_chars * px_per_char + padding

    table_spec: Dict[str, Any] = {
        "data": copy.deepcopy(bar_spec["data"]),
        "transform": shared_transforms + [
            {"fold": param_fields, "as": ["param_name", "param_value"]},
            {"calculate": "replace(datum.param_name, 'parameters.', '')", "as": "param_label"},
        ],
        "mark": {"type": "text", "align": "center"},
        "encoding": {
            "y": table_y,
            "x": {
                "field": "param_label",
                "type": "ordinal",
                "sort": param_keys,
                "axis": {"orient": "top", "labelAngle": -30, "title": None},
            },
            "text": {"field": "param_value", "type": "nominal"},
        },
        "width": {"step": step},
        "height": copy.deepcopy(bar_spec.get("height", {"step": 30})),
    }

    # $schema and title move to the compound level; everything else stays in
    # the bar sub-spec (including params for zoom/pan).
    bar_sub = {k: v for k, v in bar_spec.items() if k not in ("$schema", "title")}

    # Suppress Y axis on bar sub-spec — the parameter table provides the labels.
    if "encoding" in bar_sub and "y" in bar_sub["encoding"]:
        bar_sub["encoding"] = copy.deepcopy(bar_sub["encoding"])
        bar_sub["encoding"]["y"].pop("title", None)
        bar_sub["encoding"]["y"]["axis"] = None

    return {
        "$schema": "https://vega.github.io/schema/vega-lite/v5.json",
        "title": bar_spec.get("title", ""),
        "hconcat": [table_spec, bar_sub],
    }


def generate_vegalite_json(
    yaml_config: Dict[str, Any],
    results_json_path: Path,
    output_dir: Path
) -> List[Path]:
    """
    Orchestrate complete Vega-Lite generation workflow.

    Algorithm:
        1. Load plot specs from YAML config
        2. If no specs: log info, return []
        3. Load results data for unit conversion
        4. Create output_dir if needed
        5. Get timestamp from results or current time
        6. For each plot:
           - Translate Plotly spec to Vega-Lite
           - Validate spec
           - Inject data URL
           - Apply unit conversions
           - Write to plot_<name>_<timestamp>.vl.json
        7. Return list of generated files

    Args:
        yaml_config: Complete configuration dictionary
        results_json_path: Path to results JSON file
        output_dir: Directory to write visualization files

    Returns:
        List of generated .vl.json file paths

    Error Handling:
        - Per-plot errors: Skip plot, log WARNING, continue overall
        - Missing output dir: Create it
        - File write error: Raise IOError
    """
    # Step 1: Build plot specs — standard plots always first, yaml-defined plots added on top.
    app_name = yaml_config.get("application", {}).get("name", "")
    specs = _default_plot_specs(app_name)
    specs.update(load_plot_specs(yaml_config))

    # Step 2: Load results data (for unit conversion)
    results = {}
    if results_json_path.exists():
        try:
            with open(results_json_path, 'r') as f:
                results = json.load(f)
            logger.debug(f"Loaded results data from {results_json_path}")
        except Exception as e:
            logger.warning(
                f"Could not load results data from {results_json_path}: {e}. "
                "Unit conversions will not be applied."
            )
    else:
        logger.warning(
            f"Results file does not exist: {results_json_path}. "
            "Proceeding without unit conversions."
        )

    # Step 3: Create output directory
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    logger.debug(f"Output directory: {output_dir}")

    # Step 4: Get timestamp (sanitized for use in filenames: YYYY-MM-DDTHH-MM-SSZ)
    _raw_ts = results.get("experiment", {}).get("timestamp", "")
    if _raw_ts:
        # Convert ISO to filename format: 2026-02-20T08:58:44.123+00:00 → 2026-02-20T08-58-44Z
        timestamp = _raw_ts.replace(':', '-').replace('+', 'Z').split('.')[0] + 'Z'
    else:
        timestamp = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H-%M-%SZ")
        logger.debug(f"Generated timestamp: {timestamp}")

    # Step 5: Initialize generated files list
    generated_files = []

    # Step 6: Process each plot
    for plot_name, plotly_spec in specs.items():
        try:
            logger.info(f"Generating Vega-Lite spec for plot: {plot_name}")

            # Translate Plotly spec to Vega-Lite
            vl_spec = translate_plotly_to_vegalite(plot_name, plotly_spec)

            # For binary-size section plots, patch the fold transform with the
            # top-10 sections (by average size across instances).
            if plotly_spec.get("stack_by") == "section" and results.get("instances"):
                exclude_bss = plotly_spec.get("exclude_bss", False)
                section_totals: Dict[str, int] = {}
                section_counts: Dict[str, int] = {}
                for inst in results["instances"]:
                    for sec_name, sec_size in inst.get("binary", {}).get("sections", {}).items():
                        if exclude_bss and sec_name in BSS_SECTION_NAMES:
                            continue
                        section_totals[sec_name] = section_totals.get(sec_name, 0) + sec_size
                        section_counts[sec_name] = section_counts.get(sec_name, 0) + 1
                section_avgs = {
                    name: section_totals[name] / section_counts[name]
                    for name in section_totals
                }
                top_sections = [
                    name for name, _ in sorted(section_avgs.items(), key=lambda x: -x[1])[:10]
                ]
                top_fields = [f"binary.sections.{name}" for name in top_sections]
                # Patch the placeholder fold transform
                for transform in vl_spec.get("transform", []):
                    if isinstance(transform.get("fold"), list) and transform.get("as") == ["section_field", "value"]:
                        transform["fold"] = top_fields
                        break

            # Compute parameter table columns: non-scheduler keys in sort
            # precedence order first, then scheduler last (finest granularity).
            param_keys_for_table: List[str] = []
            const_title_suffix = ""
            if results.get("instances"):
                first_params = results["instances"][0].get("parameters", {})
                non_scheduler = sorted(k for k in first_params if k != "scheduler")
                if first_params:
                    param_keys_for_table = non_scheduler + ["scheduler"]

                # Remove constant params from the table and promote them to the
                # chart title as "key = value" suffixes separated by em-dashes.
                all_instances = results["instances"]
                const_parts: List[str] = []
                varying: List[str] = []
                for key in param_keys_for_table:
                    vals = {inst.get("parameters", {}).get(key) for inst in all_instances}
                    if len(vals) == 1:
                        raw = next(iter(vals))
                        if isinstance(raw, float) and raw == int(raw):
                            raw = int(raw)
                        const_parts.append(f"{key} = {raw}")
                    else:
                        varying.append(key)
                param_keys_for_table = varying
                if const_parts:
                    const_title_suffix = " \u2014 " + " \u2014 ".join(const_parts)

                # Build a numeric composite sort key from the numeric parameters
                # so that values sort by magnitude (8 < 64) rather than
                # lexicographically ('64' < '8').  Each numeric parameter
                # contributes toNumber(value) * 1000^(position_from_right).
                # String parameters are skipped (they don't affect ordering here).
                # Use datum.parameters['key'] for nested JSON access.
                sort_parts = []
                for key in non_scheduler:
                    val = first_params[key]
                    if isinstance(val, (int, float)):
                        sort_parts.append(f"toNumber(datum.parameters['{key}'])")

                if sort_parts:
                    n = len(sort_parts)
                    weighted = []
                    for i, part in enumerate(sort_parts):
                        power = n - 1 - i
                        if power > 0:
                            weighted.append(f"{part} * {1000 ** power}")
                        else:
                            weighted.append(part)
                    sort_key_expr = " + ".join(weighted)
                    if "transform" not in vl_spec:
                        vl_spec["transform"] = []
                    vl_spec["transform"].append(
                        {"calculate": sort_key_expr, "as": "sort_key"}
                    )
                    # Update Y encoding sort to use the new numeric sort key.
                    y_enc = vl_spec.get("encoding", {}).get("y", {})
                    if isinstance(y_enc.get("sort"), dict):
                        y_enc["sort"]["field"] = "sort_key"
                    elif "y" in vl_spec.get("encoding", {}):
                        vl_spec["encoding"]["y"]["sort"] = {
                            "field": "sort_key",
                            "op": "min",
                            "order": "ascending",
                        }

            # Determine if we need to create separate files for each facet value
            x_axis = plotly_spec.get("x_axis", {})
            facet_param = None
            for param_name, param_action in x_axis.items():
                if param_action == "separate_plots":
                    facet_param = param_name
                    break

            # Get unique values of the facet parameter from results
            facet_values = []
            if facet_param and results.get("instances"):
                facet_set = set()
                for instance in results["instances"]:
                    value = instance.get("parameters", {}).get(facet_param)
                    if value is not None:
                        facet_set.add(value)
                facet_values = sorted(list(facet_set))

            # If we have facet values, create separate files for each
            if facet_values:
                for facet_value in facet_values:
                    # Create a copy of the spec for this facet value
                    facet_spec = copy.deepcopy(vl_spec)

                    # Remove column encoding (we're making separate files instead)
                    if "column" in facet_spec.get("encoding", {}):
                        del facet_spec["encoding"]["column"]

                    # Add filter transform for this facet value
                    if "transform" not in facet_spec:
                        facet_spec["transform"] = []

                    # Insert filter at the beginning (after the existing transforms)
                    facet_spec["transform"].insert(0, {
                        "filter": f"datum.parameters.{facet_param} == {facet_value}"
                    })

                    # Validate spec
                    validate_vegalite_spec(facet_spec)

                    # Inject data URL
                    facet_spec = inject_data_url(facet_spec, results_json_path)

                    # Apply unit conversions (if results data available)
                    if results:
                        facet_spec = apply_unit_conversions(facet_spec, results)

                    # Append constant-param suffix to title
                    if const_title_suffix and "title" in facet_spec:
                        facet_spec["title"] = str(facet_spec["title"]) + const_title_suffix

                    # Wrap with parameter table on the left
                    if param_keys_for_table:
                        facet_spec = _wrap_with_parameter_table(
                            facet_spec, param_keys_for_table, results.get("instances")
                        )

                    # Write to file with facet value in name
                    output_file = output_dir / f"plot_{plot_name}__{facet_param}_{facet_value}_{timestamp}.vl.json"
                    with open(output_file, 'w') as f:
                        json.dump(facet_spec, f, indent=2, sort_keys=True)

                    generated_files.append(output_file)
                    logger.info(f"Generated Vega-Lite spec: {output_file}")
            else:
                # No facet parameter, just generate a single file
                # Validate spec
                validate_vegalite_spec(vl_spec)

                # Inject data URL
                vl_spec = inject_data_url(vl_spec, results_json_path)

                # Apply unit conversions (if results data available)
                if results:
                    vl_spec = apply_unit_conversions(vl_spec, results)

                # Append constant-param suffix to title
                if const_title_suffix and "title" in vl_spec:
                    vl_spec["title"] = str(vl_spec["title"]) + const_title_suffix

                # Wrap with parameter table on the left
                if param_keys_for_table:
                    vl_spec = _wrap_with_parameter_table(
                        vl_spec, param_keys_for_table, results.get("instances")
                    )

                # Write to file
                output_file = output_dir / f"plot_{plot_name}_{timestamp}.vl.json"
                with open(output_file, 'w') as f:
                    json.dump(vl_spec, f, indent=2, sort_keys=True)

                generated_files.append(output_file)
                logger.info(f"Generated Vega-Lite spec: {output_file}")

        except (ValidationError, ValueError) as e:
            logger.warning(
                f"Skipping plot '{plot_name}' due to validation error: {e}. "
                "Continuing with other plots."
            )
            continue
        except IOError as e:
            logger.error(f"Failed to write visualization file for plot '{plot_name}': {e}")
            raise
        except Exception as e:
            logger.warning(
                f"Unexpected error processing plot '{plot_name}': {e}. "
                "Continuing with other plots."
            )
            continue

    # Step 7: Return generated files
    logger.info(f"Generated {len(generated_files)} Vega-Lite specification(s)")
    return generated_files


def generate_html_from_vegalite(vegalite_files: List[Path], output_dir: Path) -> List[Path]:
    """
    Generate standalone HTML files from Vega-Lite JSON specifications.

    Algorithm:
        1. For each Vega-Lite JSON file:
           - Load the JSON spec
           - Create HTML template with vega-embed library
           - Embed the Vega-Lite spec in the HTML
           - Write to output directory with .html extension
        2. Return list of generated HTML file paths

    Args:
        vegalite_files: List of paths to Vega-Lite JSON files
        output_dir: Directory to write HTML files

    Returns:
        List of paths to generated HTML files

    HTML Template:
        Uses Vega-Embed library from CDN for standalone rendering.
        Each HTML file is self-contained and can be opened in any browser.

    Examples:
        >>> vl_files = [Path("plot.vl.json")]
        >>> html_files = generate_html_from_vegalite(vl_files, Path("output"))
        >>> len(html_files) == 1
        True
    """
    generated_files = []

    # Ensure output directory exists
    output_dir.mkdir(parents=True, exist_ok=True)

    for vl_file in vegalite_files:
        if not vl_file.exists():
            logger.warning(f"Vega-Lite file not found: {vl_file}")
            continue

        try:
            # Load Vega-Lite spec
            with open(vl_file, 'r') as f:
                spec = json.load(f)

            # Generate output filename (replace .vl.json with .html)
            html_filename = vl_file.stem.replace('.vl', '') + '.html'
            html_path = output_dir / html_filename

            # Create HTML template with embedded Vega-Lite spec
            html_content = f"""<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <script src="https://cdn.jsdelivr.net/npm/vega@5"></script>
    <script src="https://cdn.jsdelivr.net/npm/vega-lite@5"></script>
    <script src="https://cdn.jsdelivr.net/npm/vega-embed@6"></script>
    <style>
        body {{
            font-family: sans-serif;
            margin: 20px;
        }}
        #vis {{
            display: inline-block;
        }}
    </style>
</head>
<body>
    <div id="vis"></div>
    <script type="text/javascript">
        var spec = {json.dumps(spec)};
        vegaEmbed('#vis', spec);
    </script>
</body>
</html>
"""

            # Write HTML file
            with open(html_path, 'w') as f:
                f.write(html_content)

            logger.info(f"Generated HTML visualization: {html_path}")
            generated_files.append(html_path)

        except json.JSONDecodeError as e:
            logger.error(f"Invalid JSON in Vega-Lite file {vl_file}: {e}")
            continue
        except IOError as e:
            logger.error(f"Failed to write HTML file for {vl_file}: {e}")
            continue
        except Exception as e:
            logger.warning(f"Unexpected error generating HTML for {vl_file}: {e}")
            continue

    logger.info(f"Generated {len(generated_files)} HTML visualization(s)")
    return generated_files
