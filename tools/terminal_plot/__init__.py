"""Terminal-based Vega-Lite plot renderer.

Public API:
    - render(vl_json_str) → ANSI string
    - render_file(path) → ANSI string
    - main(argv) → CLI entry point
"""

from __future__ import annotations

import json
import shutil
from pathlib import Path

from .vega import parse_vegalite, ChartIR, DataLoadError
from .renderer import render_ir


def _term_width(default_w: int = 80) -> int:
    """Get terminal width, falling back to default."""
    try:
        return max(40, shutil.get_terminal_size().columns)
    except Exception:
        return default_w


def render(vl_json: str, *, width: int | None = None,
           height: int | None = None,
           color: bool = True) -> str:
    """Render a Vega-Lite JSON string to an ANSI terminal string.

    Args:
        vl_json: Vega-Lite v5/v6 JSON specification string.
        width: Terminal width in columns (None = auto-detect).
        height: Terminal height in rows (None = fit content, no padding).
        color: If False, strip all ANSI color (monochrome output).

    Returns:
        ANSI-escaped string suitable for terminal output.

    Raises:
        json.JSONDecodeError: If vl_json is not valid JSON.
        DataLoadError: If referenced data cannot be loaded.
    """
    if width is None:
        width = _term_width()
    if height is None:
        height = 0  # sentinel: fit to content

    spec = json.loads(vl_json)
    chart = parse_vegalite(spec, data_dir=Path.cwd())
    result = render_ir(chart, width=width, height=height)

    if not color:
        result = _strip_ansi(result)

    return result


def render_file(path: Path, *, width: int | None = None,
                height: int | None = None,
                color: bool = True,
                title_override: str | None = None) -> str:
    """Render a Vega-Lite JSON file to an ANSI terminal string.

    Args:
        path: Path to a .vl.json file.
        width: Terminal width in columns (None = auto-detect).
        height: Terminal height in rows (None = fit content, no padding).
        color: If False, strip all ANSI color.
        title_override: Override the chart title.

    Returns:
        ANSI-escaped string suitable for terminal output.

    Raises:
        FileNotFoundError: If path does not exist.
        json.JSONDecodeError: If file is not valid JSON.
        DataLoadError: If referenced data cannot be loaded.
    """
    if width is None:
        width = _term_width()
    if height is None:
        height = 0  # sentinel: fit to content

    with open(path, "r") as f:
        spec = json.load(f)

    if title_override:
        spec["title"] = title_override

    data_dir = path.parent.resolve()
    chart = parse_vegalite(spec, data_dir=data_dir)
    result = render_ir(chart, width=width, height=height)

    if not color:
        result = _strip_ansi(result)

    return result


def _strip_ansi(text: str) -> str:
    """Remove ANSI escape sequences from a string."""
    import re
    return re.sub(r"\033\[[0-9;]*m", "", text)


__all__ = ["render", "render_file", "render_ir",
           "parse_vegalite", "ChartIR", "DataLoadError"]
