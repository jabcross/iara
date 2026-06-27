"""Vega-Lite JSON → Chart IR translation.

Parses a Vega-Lite v5/v6 spec and produces a framework-agnostic
intermediate representation that renderer.py consumes.

See spec.md §3.3 for IR schema and §4 for translation rules.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

from . import data as data_mod
from .data import _deep_get
from .colors import RGB, series_colors

# ── IR types ──────────────────────────────────────────────────────


@dataclass
class DataSeries:
    label: str
    color: RGB | None
    values: List[float | str]


@dataclass
class PanelIR:
    kind: str  # "bars" | "text-table" | "line" | "unknown"
    x_label: str | None
    y_label: str | None
    x_values: List[str]   # domain labels (x-axis)
    y_values: List[str]   # domain labels (y-axis, for bars)
    data: List[DataSeries]
    width_ratio: float = 1.0  # relative width in hconcat
    failures: dict = field(default_factory=dict)  # y_value → failure message


@dataclass
class ChartIR:
    title: str | None
    panels: List[PanelIR]

    @property
    def width(self) -> int:
        return 80  # will be scaled to terminal at render time

    @property
    def height(self) -> int:
        return 24


# ── Public API ────────────────────────────────────────────────────


def parse_vegalite(spec: dict, data_dir: Path | None = None) -> ChartIR:
    """Parse a Vega-Lite spec dict into a ChartIR.

    Handles:
        - ``hconcat`` → multiple PanelIR
        - ``mark`` → PanelIR.kind
        - ``encoding`` → axes, series
        - ``data.url`` → data loading + transforms

    Args:
        spec: Vega-Lite v5/v6 JSON specification dictionary.
        data_dir: Directory the .vl.json file is in (for relative
                  data URLs). If None, cwd is used.

    Returns:
        ChartIR ready for rendering.
    """
    spec_dir = data_dir or Path.cwd()

    # Extract title
    title = spec.get("title")

    # Load data if present at top level
    rows = _load_data(spec, spec_dir)

    # Check for hconcat
    panels: List[PanelIR] = []
    if "hconcat" in spec:
        for panel_spec in spec["hconcat"]:
            panel = _parse_panel(panel_spec, rows, spec_dir)
            panels.append(panel)
    else:
        panel = _parse_panel(spec, rows, spec_dir)
        panels.append(panel)

    return ChartIR(title=title, panels=panels)


def _load_data(spec: dict, spec_dir: Path) -> List[dict]:
    """Load and transform data from a Vega-Lite spec."""
    data_ref = spec.get("data")
    if data_ref is None:
        return []

    url = data_ref.get("url", "")
    if not url:
        return []

    try:
        raw = data_mod.resolve_data(spec, spec_dir)
    except (FileNotFoundError, ValueError) as e:
        raise DataLoadError(f"Cannot load data: {e}") from e

    # Extract property if specified
    fmt = data_ref.get("format", {})
    prop = fmt.get("property") if isinstance(fmt, dict) else None
    if prop and isinstance(raw, dict):
        raw = raw.get(prop, raw)

    if not isinstance(raw, list):
        records_key = _guess_records_key(raw)
        if records_key:
            raw = raw.get(records_key, [])
        else:
            raw = [raw] if isinstance(raw, dict) else []

    # Apply transforms
    transforms = spec.get("transform", [])
    if transforms:
        raw = data_mod.apply_transforms(raw, transforms)

    return raw


def _guess_records_key(data: dict) -> str | None:
    """Guess which key in a dict holds the array of records."""
    for key in data:
        if isinstance(data[key], list):
            return key
    return None


# ── Panel parsing ─────────────────────────────────────────────────


def _parse_panel(spec: dict, rows: List[dict],
                 spec_dir: Path) -> PanelIR:
    """Parse a single panel from a Vega-Lite spec fragment."""
    mark = spec.get("mark", {})
    if isinstance(mark, str):
        mark_type = mark
    elif isinstance(mark, dict):
        mark_type = mark.get("type", "bar")
    else:
        mark_type = "bar"

    encoding = spec.get("encoding", {})

    # Panel-local data overrides the top-level data (if any).
    # If the panel spec has its own ``data`` key, resolve it independently.
    panel_rows = rows
    if "data" in spec:
        try:
            panel_data = data_mod.resolve_data(spec, spec_dir)
            fmt = spec["data"].get("format", {})
            prop = fmt.get("property") if isinstance(fmt, dict) else None
            if prop and isinstance(panel_data, dict):
                panel_data = panel_data.get(prop, panel_data)
            if isinstance(panel_data, list):
                panel_rows = panel_data
            transforms = spec.get("transform", [])
            if transforms:
                panel_rows = data_mod.apply_transforms(panel_rows, transforms)
        except DataLoadError:
            pass  # fall through to top-level data
        except Exception:
            pass

    if mark_type == "text":
        return _parse_text_panel(spec, encoding, panel_rows)
    elif mark_type == "bar":
        return _parse_bar_panel(spec, encoding, panel_rows)
    elif mark_type == "line":
        return _parse_line_panel(spec, encoding, panel_rows)
    elif mark_type == "rect":
        return _parse_rect_panel(spec, encoding, panel_rows)
    else:
        return PanelIR(
            kind="unknown",
            x_label=None,
            y_label=None,
            x_values=[],
            y_values=[],
            data=[],
        )


# ── Bar panel ─────────────────────────────────────────────────────


def _parse_bar_panel(spec: dict, encoding: dict,
                     rows: List[dict]) -> PanelIR:
    """Parse a bar mark into PanelIR.

    Framework convention: y=ordinal (instance names), x=quantitative
    (metric value), color=nominal (scheduler/series).
    """
    x_enc = encoding.get("x", {})
    y_enc = encoding.get("y", {})
    color_enc = encoding.get("color", {})
    x_offset_enc = encoding.get("xOffset", {})

    # Y domain: instance labels
    y_field = y_enc.get("field", "")
    y_sort = y_enc.get("sort")

    # X domain: metric
    x_field = x_enc.get("field", "")
    x_title = x_enc.get("title", x_field)

    # Color: series grouping
    color_field = color_enc.get("field", "")
    color_legend = color_enc.get("legend")

    # xOffset: grouped bar positioning within a y row
    x_offset_field = x_offset_enc.get("field", "")

    if not rows:
        return PanelIR(kind="bars", x_label=x_title, y_label=y_enc.get("title", y_field),
                       x_values=[], y_values=[], data=[])

    # Extract Y labels in order
    y_labels = _extract_domain(rows, y_field, y_sort)

    # Determine series
    if color_field:
        series_labels = _unique_preserving_order(rows, color_field)
    else:
        series_labels = ["value"]

    colors = series_colors(series_labels)

    # Build dataset: one DataSeries per color group
    series_map: Dict[str, DataSeries] = {}
    for label in series_labels:
        series_map[label] = DataSeries(
            label=label,
            color=colors.get(label),
            values=[],
        )

    # For horizontal bars (the framework format), each row is one bar.
    # We group by y_field value and series label.
    if color_field:
        # Multiple series: each y_field value has N bars (one per series)
        for y_val in y_labels:
            grouped = _group_by(rows, y_field, y_val)
            for sl in series_labels:
                matching = [r for r in grouped
                            if _field_str(r, color_field) == sl]
                if matching:
                    val = _to_number(_field_val(matching[0], x_field))
                else:
                    val = 0.0
                series_map[sl].values.append(val)
    else:
        # Single series
        sl = series_labels[0]
        for y_val in y_labels:
            matching = [r for r in rows
                        if _field_str(r, y_field) == y_val]
            if matching:
                val = _to_number(_field_val(matching[0], x_field))
            else:
                val = 0.0
            series_map[sl].values.append(val)

    # Collect failure messages per y_value.
    failures: dict[str, str] = {}
    for row in rows:
        y_key = _field_str(row, y_field)
        ex = _field_val(row, "execution")
        if isinstance(ex, dict):
            failed = ex.get("failed_runs", 0)
            if failed > 0:
                msgs = ex.get("failures", [])
                if msgs:
                    failures[y_key] = "; ".join(str(m) for m in msgs)
                else:
                    failures[y_key] = f"{failed} run(s) failed"

    return PanelIR(
        kind="bars",
        x_label=x_title,
        y_label=y_enc.get("title", y_field),
        x_values=[],
        y_values=y_labels,
        data=[series_map[sl] for sl in series_labels],
        failures=failures,
    )


# ── Text panel ────────────────────────────────────────────────────


def _parse_text_panel(spec: dict, encoding: dict,
                      rows: List[dict]) -> PanelIR:
    """Parse a text mark (parameter table) into PanelIR."""
    text_enc = encoding.get("text", {})
    x_enc = encoding.get("x", {})  # column headers
    y_enc = encoding.get("y", {})  # row IDs

    text_field = text_enc.get("field", "param_value")
    x_field = x_enc.get("field", "param_label")
    y_field = y_enc.get("field", "short_name")
    x_sort = x_enc.get("sort")

    # Build grid: rows = y values, columns = x values
    col_labels = _extract_domain(rows, x_field, x_sort)
    row_labels = _extract_domain(rows, y_field, y_enc.get("sort"))

    # Very wide text tables are impractical — cap columns.
    if len(col_labels) > 10:
        col_labels = col_labels[:10]

    # Build data series: one per column
    series_list: List[DataSeries] = []
    for col in col_labels:
        values: List[float | str] = []
        for row_label in row_labels:
            matching = [r for r in rows
                        if _field_str(r, y_field) == row_label
                        and _field_str(r, x_field) == col]
            if matching:
                values.append(str(_field_val(matching[0], text_field) or ""))
            else:
                values.append("")
        series_list.append(DataSeries(label=col, color=None, values=values))

    return PanelIR(
        kind="text-table",
        x_label=None,
        y_label=None,
        x_values=col_labels,
        y_values=row_labels,
        data=series_list,
        width_ratio=0.25,  # text tables are narrower
    )


# ── Line panel ────────────────────────────────────────────────────


def _parse_line_panel(spec: dict, encoding: dict,
                      rows: List[dict]) -> PanelIR:
    """Parse a line mark into PanelIR."""
    x_enc = encoding.get("x", {})
    y_enc = encoding.get("y", {})
    color_enc = encoding.get("color", {})

    x_field = x_enc.get("field", "")
    y_field = y_enc.get("field", "")
    color_field = color_enc.get("field", "")

    x_title = x_enc.get("title", x_field)
    y_title = y_enc.get("title", y_field)

    if not rows:
        return PanelIR(kind="line", x_label=x_title, y_label=y_title,
                       x_values=[], y_values=[], data=[])

    if color_field:
        series_labels = _unique_preserving_order(rows, color_field)
    else:
        series_labels = ["value"]

    colors = series_colors(series_labels)

    series_list: List[DataSeries] = []
    for sl in series_labels:
        if color_field:
            matching = [r for r in rows
                        if _field_str(r, color_field) == sl]
        else:
            matching = rows

        # Sort by x value
        matching_sorted = sorted(matching,
                                 key=lambda r: _to_number(_field_val(r, x_field)))

        x_vals = [_field_str(r, x_field) for r in matching_sorted]
        y_vals: List[float | str] = []
        for r in matching_sorted:
            y_vals.append(_to_number(_field_val(r, y_field)))

        series_list.append(DataSeries(
            label=sl,
            color=colors.get(sl),
            values=y_vals,
        ))

    return PanelIR(
        kind="line",
        x_label=x_title,
        y_label=y_title,
        x_values=x_vals if series_list else [],
        y_values=[],
        data=series_list,
    )


# ── Rect panel (heatmap) ──────────────────────────────────────────


def _parse_rect_panel(spec: dict, encoding: dict,
                      rows: List[dict]) -> PanelIR:
    """Parse a rect mark (heatmap) into PanelIR."""
    # Stub: return unknown for now. Rect rendering is P2.
    return PanelIR(
        kind="unknown",
        x_label=None,
        y_label=None,
        x_values=[],
        y_values=[],
        data=[],
    )


# ── Helpers ───────────────────────────────────────────────────────


def _field_val(row: dict, field: str) -> Any:
    """Get a field value from a row, supporting dot-notation for nested access.

    Args:
        row: Data row dict.
        field: Field name, possibly with dots (e.g., ``execution.statistics.wall_time.mean``).

    Returns:
        The value at the given path, or None if any intermediate key is missing.
    """
    return _deep_get(row, field)


def _field_str(row: dict, field: str) -> str:
    """Get a field value as a string, supporting dot notation."""
    val = _deep_get(row, field)
    if val is None:
        return ""
    return str(val)


def _extract_domain(rows: List[dict], field: str,
                    sort_spec: Any = None) -> List[str]:
    """Extract unique domain values from rows, preserving order.

    Args:
        rows: Data rows.
        field: Field name to extract (supports dot notation).
        sort_spec: Vega-Lite sort specification (dict, list, or string).

    Returns:
        Ordered list of unique string values.
    """
    values: List[str] = []
    seen: set = set()
    for row in rows:
        val = _field_str(row, field)
        if val not in seen:
            seen.add(val)
            values.append(val)

    if sort_spec:
        values = _apply_sort(values, sort_spec, rows, field)

    return values


def _apply_sort(values: List[str], sort_spec: Any,
                rows: List[dict], field: str) -> List[str]:
    """Apply Vega-Lite sort to domain values."""
    if isinstance(sort_spec, list):
        # Explicit order: items in sort_spec should be in desired order.
        # Values not in sort_spec go last.
        order_map = {v: i for i, v in enumerate(sort_spec)}
        return sorted(values, key=lambda v: order_map.get(v, 999999))
    elif isinstance(sort_spec, dict):
        sort_field = sort_spec.get("field", "")
        sort_op = sort_spec.get("op", "min")
        order = sort_spec.get("order", "ascending")
        if sort_field and sort_field in {_field_str(row, field) for row in rows}:
            # Build value lookup
            val_scores: Dict[str, float] = {}
            for row in rows:
                key = _field_str(row, field)
                sf_val = _field_val(row, sort_field)
                try:
                    sf_val = float(sf_val) if sf_val is not None else 0.0
                except (ValueError, TypeError):
                    sf_val = 0.0
                if key not in val_scores:
                    val_scores[key] = sf_val
                elif sort_op == "min":
                    val_scores[key] = min(val_scores[key], sf_val)
                else:
                    val_scores[key] = max(val_scores[key], sf_val)
            reverse = order == "descending"
            return sorted(values, key=lambda v: val_scores.get(v, 0),
                          reverse=reverse)
    return values


def _unique_preserving_order(rows: List[dict], field: str) -> List[str]:
    """Extract unique values preserving first-appearance order."""
    seen: set = set()
    result: List[str] = []
    for row in rows:
        val = _field_str(row, field)
        if val not in seen:
            seen.add(val)
            result.append(val)
    return result


def _group_by(rows: List[dict], field: str, value: Any) -> List[dict]:
    """Filter rows where field matches value."""
    return [r for r in rows if _field_str(r, field) == str(value)]


def _to_number(val: Any) -> float:
    """Coerce a value to float, returning 0.0 on failure."""
    if val is None:
        return 0.0
    try:
        return float(val)
    except (ValueError, TypeError):
        return 0.0


# ── Exceptions ────────────────────────────────────────────────────


class DataLoadError(Exception):
    """Raised when Vega-Lite data cannot be loaded."""
    pass
