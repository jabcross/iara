"""Chart IR → TerminalCanvas → ANSI string.

The rendering pipeline:
    1. Receive ChartIR from vega.py
    2. Allocate canvas regions for each panel
    3. Draw axes, labels, bars, text tables
    4. Serialize canvas to ANSI string
"""

from __future__ import annotations

from typing import List, Optional

from .canvas import TerminalCanvas
from .colors import RGB, DIM, series_style
from .vega import ChartIR, DataSeries, PanelIR


def _panels_share_y_axis(panels: List[PanelIR]) -> bool:
    """Check if multiple hconcat panels share the same Y axis (y_values)."""
    if len(panels) < 2:
        return False
    y_sets = [set(p.y_values) for p in panels if p.y_values]
    if len(y_sets) < 2:
        return False
    return y_sets[0] and all(s == y_sets[0] for s in y_sets[1:])


def _compute_min_height(chart: ChartIR, shared_y: bool = False,
                        width: int = 80) -> int:
    """Compute minimum canvas height to fit chart content without padding."""
    title_rows = 2 if chart.title else 0
    body_rows = 0
    for p in chart.panels:
        if p.kind == "text-table":
            body_rows = max(body_rows, 2 + len(p.y_values))
        elif p.kind == "bars":
            shift = 1 if shared_y else 0
            bar_body = 1 + shift + len(p.y_values) + 1  # x_title + data + x_axis
            # Colour legend rows (wrapping).
            legend_rows = 0
            # Only stacked charts get a colour legend.
            if len(p.data) > 1:
                stacked = False
                for ri in range(len(p.y_values)):
                    nz = sum(1 for ds in p.data
                             if ri < len(ds.values) and isinstance(ds.values[ri], (int, float)) and float(ds.values[ri]) > 0)
                    if nz > 1:
                        stacked = True
                        break
            if len(p.data) > 1 and stacked:
                legend_rows = 1
                lx_test = 1
                for ds in p.data:
                    has_val = any(isinstance(ds.values[ri], (int, float)) and float(ds.values[ri]) > 0
                                 for ri in range(min(len(p.y_values), len(ds.values))))
                    if has_val:
                        label = f" {ds.label} "
                        if lx_test + len(label) + 1 >= width:
                            legend_rows += 1
                            lx_test = 1
                        lx_test += len(label) + 1
            body_rows = max(body_rows, bar_body + legend_rows)
        elif p.kind == "line":
            body_rows = max(body_rows, 1 + 6 + 2)
        else:
            body_rows = max(body_rows, 3)
    return max(10, title_rows + body_rows)


def render_ir(chart: ChartIR, width: int = 80,
              height: int = 24) -> str:
    """Render a ChartIR to an ANSI terminal string.

    Args:
        chart: Parsed chart IR.
        width: Terminal width in columns.
        height: Terminal height in rows (0 = fit to content).

    Returns:
        ANSI-escaped string ready for stdout.
    """
    width = max(40, width)
    shared_y = _panels_share_y_axis(chart.panels)
    min_h = _compute_min_height(chart, shared_y, width)
    height = max(min_h, height)

    canvas = TerminalCanvas(width, height)

    if not chart.panels:
        canvas.text(2, 0, "(empty chart)", fg=(128, 128, 128))
        return canvas.to_ansi()

    # ── Title ──
    title_row = 0
    if chart.title:
        canvas.text(2, title_row, chart.title, bold=True)
        title_row = 2

    # ── Compute panel widths ──
    total_ratio = sum(p.width_ratio for p in chart.panels)
    available_w = width - 2  # left margin
    panel_widths: List[int] = []
    if total_ratio > 0:
        for p in chart.panels:
            pw = max(10, int(available_w * p.width_ratio / total_ratio))
            panel_widths.append(pw)
    else:
        panel_widths = [available_w // len(chart.panels)] * len(chart.panels)

    # Adjust to fit exactly
    delta = available_w - sum(panel_widths)
    for i in range(abs(delta)):
        if delta > 0:
            panel_widths[i % len(panel_widths)] += 1
        else:
            idx = len(panel_widths) - 1 - (i % len(panel_widths))
            if panel_widths[idx] > 10:
                panel_widths[idx] -= 1

    # ── Size text-table panels to actual content width ──
    for i, panel in enumerate(chart.panels):
        if panel.kind != "text-table" or not panel.x_values:
            continue
        # Max width needed: column headers + data values.
        col_widths: list[int] = []
        for ci, col_label in enumerate(panel.x_values):
            max_w = len(col_label)
            if ci < len(panel.data):
                for v in panel.data[ci].values:
                    max_w = max(max_w, len(str(v)))
            col_widths.append(max_w + 2)  # padding
        content_w = sum(col_widths) + 2  # left margin
        if content_w < panel_widths[i]:
            # Panel is wider than needed — shrink.
            extra = panel_widths[i] - content_w
            panel_widths[i] = content_w
            for j in range(i + 1, len(panel_widths)):
                if chart.panels[j].kind != "text-table":
                    panel_widths[j] += extra
                    break
        elif content_w > panel_widths[i]:
            # Panel is too narrow — expand.
            needed = content_w - panel_widths[i]
            panel_widths[i] = content_w
            # Take space from the next non-text-table panel.
            for j in range(i + 1, len(panel_widths)):
                if chart.panels[j].kind != "text-table" and panel_widths[j] > needed + 10:
                    panel_widths[j] -= needed
                    break

    # ── Render panels ──
    x_offset = 1
    separator_x: List[int] = []  # positions between panels
    body_height = height - title_row  # no bottom padding
    show_y_labels = True  # first panel always shows labels

    for i, (panel, pw) in enumerate(zip(chart.panels, panel_widths)):
        if i > 0:
            separator_x.append(x_offset)
            # Draw vertical separator
            for yy in range(title_row, height):
                canvas.put(x_offset, yy, "│", dim=True)
            # Hide Y labels for panels sharing axis with previous panel
            if shared_y:
                show_y_labels = False

        _render_panel(canvas, panel, x_offset + 1, title_row,
                      pw - 1, body_height, show_y_labels=show_y_labels)
        x_offset += pw

    return canvas.to_ansi()


def _render_panel(canvas: TerminalCanvas, panel: PanelIR,
                  x: int, y: int, w: int, h: int,
                  show_y_labels: bool = True):
    """Dispatch panel rendering by kind."""
    if panel.kind == "bars":
        _render_bars(canvas, panel, x, y, w, h,
                     show_y_labels=show_y_labels)
    elif panel.kind == "text-table":
        _render_text_table(canvas, panel, x, y, w, h,
                           show_y_labels=show_y_labels)
    elif panel.kind == "line":
        _render_line(canvas, panel, x, y, w, h)
    elif panel.kind == "unknown":
        canvas.text(x, y + 1, f"(unsupported: {panel.kind})",
                    fg=(128, 128, 128))
    else:
        _render_bars(canvas, panel, x, y, w, h,
                     show_y_labels=show_y_labels)


# ── Bar chart rendering ───────────────────────────────────────────


def _render_bars(canvas: TerminalCanvas, panel: PanelIR,
                 x: int, y: int, w: int, h: int,
                 show_y_labels: bool = True):
    """Render a horizontal bar chart panel.

    Layout (assuming bar area width >= 20):
        ┌─────────────────────────────────┐
        │  X Label (title)                │
        │  instance_1  ████████████ 42.3  │
        │  instance_2  ██████ 18.1        │
        │  ...                            │
        │          0 ─┴──┴──┴──┴──┴─ max │
        └─────────────────────────────────┘

    Y labels on left, bars on right, values after bars.
    Multiple series: group bars per Y label, stacked or side-by-side.
    """
    if not panel.data or not panel.y_values:
        canvas.text(x, y, "(no data)", fg=(128, 128, 128))
        return

    n_series = len(panel.data)
    n_rows = len(panel.y_values)

    # ── Region sizing ──
    label_w = _max_strlen(panel.y_values) + 2 if show_y_labels else 0
    value_w = 8  # space for numeric labels

    # How much space for bars?
    min_bar_area = 20
    bar_area_x = x + label_w
    bar_area_w = max(min_bar_area, w - label_w - value_w - 2)
    if bar_area_w < 10:
        bar_area_w = w - label_w - 2
        value_w = 0

    available_rows = h - 3  # title row + x-axis row + padding
    # Use single-line rows when sharing axis with left text table,
    # otherwise allow double-height rows for readability.
    if not show_y_labels:
        row_height = 1
    else:
        row_height = max(1, min(2, available_rows // max(1, n_rows)))

    # ── X-axis title ──
    if panel.x_label:
        canvas.text(x, y, panel.x_label, bold=True)
        y += 1

    # ── Y-axis label ──
    if panel.y_label and show_y_labels:
        canvas.text(x, y, panel.y_label, dim=True)
        y += 1

    # ── Data bars ──
    # Find global max for scaling
    all_vals: List[float] = []
    for ds in panel.data:
        for v in ds.values:
            if isinstance(v, (int, float)):
                all_vals.append(float(v))
    max_val = max(all_vals) if all_vals else 1.0
    if max_val == 0:
        max_val = 1.0

    # If Y labels are hidden (shared axis with text table left panel),
    # shift data rows down by 1 to compensate for the text table's extra
    # header row (text table has header + separator = 2, bar has 1 title row).
    if not show_y_labels:
        y += 1

    # Precompute per-row totals for value labels (before row loop)
    row_sums: List[float] = []
    for ri in range(n_rows):
        row_total = 0.0
        for ds in panel.data:
            if ri < len(ds.values):
                v = ds.values[ri]
                if isinstance(v, (int, float)):
                    row_total += float(v)
        row_sums.append(row_total)

    # ── Determine which sections to show: greedy, add sections in
    #     stack order until one row can't fit its legend (either
    #     inside the largest bar section or in the empty space).
    # Determine if this is a stacked chart (multiple series per row).
    is_stacked = False
    if n_series > 1:
        for ri in range(n_rows):
            non_zero = 0
            for ds in panel.data:
                if ri < len(ds.values):
                    v = ds.values[ri]
                    if isinstance(v, (int, float)) and float(v) > 0:
                        non_zero += 1
            if non_zero > 1:
                is_stacked = True
                break

    # All non-zero section labels (for legend below, stacked only).
    all_section_labels: list[str] = []
    if n_series > 1 and is_stacked:
        for si, ds in enumerate(panel.data):
            has_val = any(isinstance(ds.values[ri], (int, float)) and float(ds.values[ri]) > 0
                         for ri in range(min(n_rows, len(ds.values))))
            if has_val:
                all_section_labels.append(ds.label)

    # Draw each row
    chart_y = y
    for row_idx, label in enumerate(panel.y_values):
        row_y = chart_y + row_idx * row_height

        if row_y >= canvas.height:
            break

        # Y-axis label (only if not shared with left panel)
        if show_y_labels and label_w > 0:
            display_label = label[:label_w - 1] if len(label) >= label_w else label
            canvas.text(x + label_w - len(display_label) - 1, row_y,
                        display_label, dim=True)

        # Build stacked segments for this row (full width, legend appended after).
        bar_usable_w = max(1, bar_area_w - 2)

        # Check for failure — render message instead of bar.
        failure_msg = panel.failures.get(label, "")
        row_total = row_sums[row_idx]
        if failure_msg:
            fail_text = f"[FAILED] {failure_msg}"
            canvas.text(bar_area_x, row_y, fail_text[:bar_usable_w],
                        fg=(255, 80, 80), dim=True)
            if row_total > 0:
                canvas.text(bar_area_x + bar_usable_w + 1, row_y,
                            _format_number(row_total, max_val),
                            fg=(255, 80, 80), bold=True)
            continue

        segments: list[tuple[float, RGB | None]] = []
        for si, ds in enumerate(panel.data):
            if row_idx < len(ds.values):
                val = ds.values[row_idx]
                if isinstance(val, (int, float)):
                    val_f = float(val)
                else:
                    val_f = 0.0
            else:
                val_f = 0.0

            frac = val_f / max_val if max_val > 0 else 0.0
            if frac > 0:
                segments.append((frac, ds.color))

        canvas.bar_stacked(bar_area_x, row_y, bar_usable_w, segments)

        # Per-row legend: visible sections only.
        row_total = row_sums[row_idx]
        bar_fill = int(row_total / max_val * bar_usable_w) if max_val > 0 else 0

        ranked: list[tuple[float, str, RGB | None]] = []
        if n_series > 1 and row_total > 0:
            for si, ds in enumerate(panel.data):
                if row_idx < len(ds.values):
                    v = ds.values[row_idx]
                    if isinstance(v, (int, float)) and float(v) > 0:
                        ranked.append((float(v), ds.label, ds.color))
            # Keep stack order (left-to-right), not sorted by value.
            # Filter to sections actually rendered in the bar.
            # Keep stack order (no pre-filter — fit checked at render time).

        # Value label.
        val_str = ""
        if value_w > 0 and row_total > 0:
            val_str = _format_number(row_total, max_val)

        if not is_stacked:
            # Grouped chart — just total at right.
            if val_str:
                canvas.text(bar_area_x + bar_usable_w + 1, row_y, val_str,
                            fg=panel.data[-1].color, bold=True)
        elif bar_fill >= 15 and ranked:
            # Wide bar — overlay compact legend inside the largest
            # section's cell range (centered).  Overline+underline
            # on all bar cells for continuity.
            for bx in range(bar_area_x, bar_area_x + bar_fill):
                canvas.grid[row_y][bx].overline = True
                canvas.grid[row_y][bx].underline = True

            # In-bar: show values only (coloured), limit to what fits.
            vals: list[tuple[str, RGB | None]] = []
            cum_frac = 0.0
            best_start = bar_area_x
            best_end = bar_area_x + bar_fill
            best_width = 0
            dominant_color: RGB | None = None
            for si, ds in enumerate(panel.data):
                if row_idx < len(ds.values):
                    v = ds.values[row_idx]
                    vf = float(v) if isinstance(v, (int, float)) else 0.0
                else:
                    vf = 0.0
                sec_frac = vf / max_val if max_val > 0 else 0.0
                start_x = bar_area_x + int(cum_frac * bar_usable_w)
                end_x = bar_area_x + int((cum_frac + sec_frac) * bar_usable_w)
                w = end_x - start_x
                if w > best_width:
                    best_width = w
                    best_start = start_x
                    best_end = end_x
                    dominant_color = ds.color
                cum_frac += sec_frac
                if vf > 0:
                    sv = _format_number(vf, max_val)
                    cur_w = sum(len(v) + 1 for v, *_ in vals) if vals else 0
                    if cur_w + len(sv) < best_width - 2:
                        bld, itl = series_style(ds.label, [d.label for d in panel.data])
                        vals.append((sv, ds.color, bld, itl))

            total_w = sum(len(v) + 1 for v, *_ in vals) - 1 if vals else 0
            if total_w > 0 and best_width >= 3 and total_w < best_width:
                center_x = best_start + (best_width - total_w) // 2
                ox = max(bar_area_x, center_x)
                for vi, (v, c, bld, itl) in enumerate(vals):
                    for ci, ch in enumerate(v):
                        if ox + ci < best_end:
                            canvas.put(ox + ci, row_y, ch,
                                       fg=c, bg=(0, 0, 0),
                                       bold=bld, italic=itl,
                                       underline=True,
                                       ul_rgb=dominant_color)
                    if ox + len(v) < best_end:
                        canvas.put(ox + len(v), row_y, " ",
                                   fg=dominant_color,
                                   overline=True, underline=True,
                                   ul_rgb=dominant_color)
                    ox += len(v) + 1

            # Total always at the right edge of the bar area.
            total_x = bar_area_x + bar_usable_w + 1
            if val_str:
                canvas.text(total_x, row_y, val_str,
                            fg=panel.data[-1].color, bold=True)
        else:
            # Narrow bar — total at right edge, values centered between
            # bar end and total.
            total_x = bar_area_x + bar_usable_w + 1
            vals: list[tuple[str, RGB | None, bool, bool]] = []
            if ranked:
                for (sec_val, sec_name, sec_color) in ranked:
                    bld, itl = series_style(sec_name, [d.label for d in panel.data])
                    vals.append((_format_number(sec_val, max_val), sec_color, bld, itl))
                vals_w = sum(len(v) + 1 for v, _, _, _ in vals) - 1 if vals else 0
                avail = total_x - bar_area_x - max(0, bar_fill) - 1
                lx = bar_area_x + max(0, bar_fill) + 1
                if vals_w < avail:
                    lx += (avail - vals_w) // 2
                for v, c, bld, itl in vals:
                    for ci, ch in enumerate(v):
                        if lx + ci < total_x:
                            canvas.put(lx + ci, row_y, ch, fg=c,
                                       bold=bld, italic=itl)
                    lx += len(v) + 1
            if val_str:
                canvas.text(total_x, row_y, val_str,
                            fg=panel.data[-1].color, bold=True)

    # ── X-axis baseline ──
    axis_y = min(chart_y + n_rows * row_height, canvas.height - 1)
    canvas.hline(bar_area_x, axis_y, bar_area_w, "─", fg=None)
    canvas.text(bar_area_x, axis_y, "0", dim=True)
    max_str = _format_number(max_val, max_val)
    canvas.text(bar_area_x + bar_area_w - len(max_str), axis_y,
                max_str, dim=True)

    # ── Section colour legend (below x-axis, all sections, wrapping) ──
    if n_series > 1 and all_section_labels:
        legend_y = axis_y + 1
        lx = 1  # full width under table + bar
        for si, ds in enumerate(panel.data):
            if ds.label not in all_section_labels:
                continue
            label = f" {ds.label} "
            if lx + len(label) + 1 >= canvas.width:
                legend_y += 1
                lx = 1
            if legend_y >= canvas.height:
                break
            canvas.put(lx, legend_y, " ", bg=ds.color)
            canvas.put(lx + 1, legend_y, " ", bg=ds.color)
            bld, itl = series_style(ds.label, [d.label for d in panel.data])
            canvas.text(lx + 2, legend_y, ds.label, fg=ds.color, bold=bld)
            if itl:
                for ci in range(len(ds.label)):
                    canvas.grid[legend_y][lx + 2 + ci].italic = True
            lx += len(label) + 1


def _format_number(value: float, max_val: float) -> str:
    """Format a number for display in the chart.

    Chooses precision based on magnitude.
    """
    abs_v = abs(value)
    if abs_v >= 1_000_000:
        return f"{value / 1_000_000:.1f}M"
    elif abs_v >= 1_000:
        return f"{value / 1_000:.1f}k"
    elif abs_v >= 100:
        return f"{value:.1f}"
    elif abs_v >= 1:
        return f"{value:.2f}"
    elif abs_v > 0:
        return f"{value:.3f}"
    else:
        return "0"


def _max_strlen(strings: List[str]) -> int:
    """Max display width of a list of strings."""
    max_len = 0
    for s in strings:
        # Approximate: count characters
        w = sum(2 if ord(c) > 0x7FF else 1 for c in s)
        max_len = max(max_len, w)
    return max_len


# ── Text table rendering ──────────────────────────────────────────


def _render_text_table(canvas: TerminalCanvas, panel: PanelIR,
                       x: int, y: int, w: int, h: int,
                       show_y_labels: bool = True):
    """Render a text mark panel (parameter table).

    Columns: x_values (parameter names)
    Rows: y_values (instance short names)
    Cells: data series values (param values)
    """
    del show_y_labels  # text table always shows its content
    if not panel.x_values or not panel.y_values:
        canvas.text(x, y, "(empty table)", fg=(128, 128, 128))
        return

    # Compute column widths, clipped to available space
    n_cols = len(panel.x_values)
    max_col_w = max(4, w // max(1, n_cols))
    col_widths: List[int] = []
    for ci, col_label in enumerate(panel.x_values):
        max_w = len(col_label) + 2
        if ci < len(panel.data):
            for v in panel.data[ci].values:
                max_w = max(max_w, len(str(v)) + 2)
        col_widths.append(min(max_w, max_col_w))

    # Total width: clip to panel width
    total_w = min(sum(col_widths), w)

    # Header row
    cx = x
    for ci, (label, cw) in enumerate(zip(panel.x_values, col_widths)):
        canvas.text(cx + 1, y, label[:cw - 1], bold=True)
        cx += cw
    y += 1

    # Separator (clipped to panel width)
    canvas.hline(x, y, min(total_w, w), "─")
    y += 1

    # Data rows
    for ri, row_label in enumerate(panel.y_values):
        if y >= canvas.height:
            break
        cx = x
        for ci, cw in enumerate(col_widths):
            if ci < len(panel.data) and ri < len(panel.data[ci].values):
                val = str(panel.data[ci].values[ri])
                canvas.text(cx + 1, y, val[:cw - 1])
            cx += cw
        y += 1


# ── Line chart rendering ──────────────────────────────────────────


def _render_line(canvas: TerminalCanvas, panel: PanelIR,
                 x: int, y: int, w: int, h: int):
    """Render a line chart panel.

    Uses Unicode characters for line segments and markers.
    """
    if not panel.data:
        canvas.text(x, y, "(no data)", fg=(128, 128, 128))
        return

    # Markers (cycle per series)
    markers = ["●", "◆", "▲", "■", "★", "♦", "▴", "▪"]

    # Find global min/max
    all_vals: List[float] = []
    max_x = 0
    for ds in panel.data:
        max_x = max(max_x, len(ds.values))
        for v in ds.values:
            if isinstance(v, (int, float)):
                all_vals.append(float(v))
    min_val = min(all_vals) if all_vals else 0
    max_val = max(all_vals) if all_vals else 1
    val_range = max_val - min_val
    if val_range == 0:
        val_range = 1

    # Title
    if panel.y_label:
        canvas.text(x, y, panel.y_label, bold=True)
        y += 1

    # Chart area
    chart_h = max(5, h - 3)
    chart_w = max(20, w - 2)
    chart_y = y + 1
    plot_x = x + 2

    # Y-axis labels
    for i in range(3):
        tick_y = chart_y + i * (chart_h - 1) // 2
        val = max_val - (max_val - min_val) * i / 2
        canvas.text(x, tick_y, _format_number(val, max_val), dim=True)

    # Draw each series
    for si, ds in enumerate(panel.data):
        marker = markers[si % len(markers)]
        n = len(ds.values)
        if n < 2:
            continue

        prev_plot_x: int | None = None
        prev_plot_y: int | None = None

        for i, val in enumerate(ds.values):
            if isinstance(val, (int, float)):
                v = float(val)
            else:
                v = 0.0

            frac_x = i / max(1, n - 1)
            frac_y = (v - min_val) / val_range

            px = plot_x + int(frac_x * (chart_w - 1))
            py = chart_y + chart_h - 1 - int(frac_y * (chart_h - 1))

            if px < 0 or px >= canvas.width:
                continue
            if py < 0 or py >= canvas.height:
                continue

            # Draw marker
            canvas.put(px, py, marker, fg=ds.color, bold=True)

            # Draw line segment from previous point
            if prev_plot_x is not None and prev_plot_y is not None:
                _draw_line_segment(canvas, prev_plot_x, prev_plot_y,
                                   px, py, ds.color)

            prev_plot_x = px
            prev_plot_y = py

    # Legend
    legend_y = chart_y + chart_h + 1
    lx = plot_x
    for si, ds in enumerate(panel.data):
        marker = markers[si % len(markers)]
        entry = f"{marker} {ds.label}"
        if lx + len(entry) < canvas.width:
            canvas.text(lx, legend_y, entry, fg=ds.color)
            lx += len(entry) + 2


def _draw_line_segment(canvas: TerminalCanvas,
                       x1: int, y1: int, x2: int, y2: int,
                       color: RGB | None):
    """Draw a line segment using Unicode box-drawing characters.

    Uses Bresenham-like approach with directional box chars:
    ─ (horizontal), │ (vertical), ╱ (diagonal up-right),
    ╲ (diagonal down-right).
    """
    dx = x2 - x1
    dy = y2 - y1

    if dx == 0 and dy == 0:
        return

    # Horizontal
    if dy == 0:
        step = 1 if dx > 0 else -1
        for x in range(x1 + step, x2, step):
            if 0 <= x < canvas.width and 0 <= y1 < canvas.height:
                existing = canvas.grid[y1][x].char
                if existing in (" ", "", "─"):
                    canvas.put(x, y1, "─", fg=color)
        return

    # Vertical
    if dx == 0:
        step = 1 if dy > 0 else -1
        for y in range(y1 + step, y2, step):
            if 0 <= x1 < canvas.width and 0 <= y < canvas.height:
                existing = canvas.grid[y][x1].char
                if existing in (" ", "", "│"):
                    canvas.put(x1, y, "│", fg=color)
        return

    # Diagonal: use Bresenham
    steep = abs(dy) > abs(dx)
    if steep:
        x1, y1 = y1, x1
        x2, y2 = y2, x2

    if x1 > x2:
        x1, x2 = x2, x1
        y1, y2 = y2, y1

    dx = x2 - x1
    dy = abs(y2 - y1)
    err = dx // 2
    y_step = 1 if y2 > y1 else -1
    y_cur = y1

    for x in range(x1 + 1, x2):
        if err - dy >= 0:
            if steep:
                if 0 <= y_cur < canvas.width and 0 <= x < canvas.height:
                    existing = canvas.grid[x][y_cur].char
                    if existing in (" ", ""):
                        canvas.put(y_cur, x, "╱" if y_step > 0 else "╲",
                                   fg=color)
            else:
                if 0 <= x < canvas.width and 0 <= y_cur < canvas.height:
                    existing = canvas.grid[y_cur][x].char
                    if existing in (" ", ""):
                        canvas.put(x, y_cur, "╱" if y_step > 0 else "╲",
                                   fg=color)
            y_cur += y_step
            err -= dy
        err += dy
        y_cur = y1 + y_step * (x - x1) * dy // dx
