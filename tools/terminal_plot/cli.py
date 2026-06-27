"""Terminal-based Vega-Lite plot renderer — CLI.

Usage:
    python -m tools.terminal_plot plot.vl.json
    python -m tools.terminal_plot plot.vl.json --width 120 --height 30
    python -m tools.terminal_plot results/*.vl.json
    cat plot.vl.json | python -m tools.terminal_plot -
"""

from __future__ import annotations

import argparse
import json
import logging
import os
import shutil
import sys
from pathlib import Path
from typing import List

from . import render, render_file

logger = logging.getLogger(__name__)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="python -m tools.terminal_plot",
        description="Terminal-based Vega-Lite plot renderer.",
    )
    parser.add_argument(
        "files", nargs="*", default=["-"],
        help="Vega-Lite JSON files (.vl.json). Use '-' for stdin. "
             "Default: stdin.",
    )
    parser.add_argument(
        "--width", type=int, default=None,
        help="Canvas width in characters (default: terminal width).",
    )
    parser.add_argument(
        "--height", type=int, default=None,
        help="Canvas height in rows (default: terminal height - 2).",
    )
    parser.add_argument(
        "--no-color", action="store_true",
        help="Disable ANSI color output.",
    )
    parser.add_argument(
        "--title", type=str, default=None,
        help="Override chart title.",
    )
    parser.add_argument(
        "--mono", action="store_true",
        help="Monochrome mode (no color, uses shading characters).",
    )
    parser.add_argument(
        "--ascii", action="store_true",
        help="Fallback to ASCII only (no Unicode box-drawing).",
    )
    parser.add_argument(
        "--list", action="store_true",
        help="List supported mark types and exit.",
    )
    return parser


def terminal_width() -> int:
    """Get terminal width, with fallback to 80."""
    try:
        return max(40, shutil.get_terminal_size().columns)
    except Exception:
        return 80


def main(argv: List[str] | None = None) -> int:
    """CLI entry point. Returns exit code."""
    parser = build_parser()
    args = parser.parse_args(argv)

    if args.list:
        print("Supported mark types:")
        print("  bar          — horizontal bar charts")
        print("  text         — text tables (parameter table)")
        print("  line         — line charts")
        print("  hconcat      — horizontal concatenation")
        print("  rect         — heatmaps (planned)")
        return 0

    # Resolve width/height
    width = args.width or terminal_width()
    height = args.height  # None = fit to content

    color = not (args.no_color or args.mono)

    # Process files
    exit_code = 0
    first = True

    for file_arg in args.files:
        if not first:
            print()  # separator between files
        first = False

        try:
            if file_arg == "-":
                # Read from stdin
                raw = sys.stdin.read()
                if not raw.strip():
                    logger.warning("stdin is empty")
                    continue
                spec = json.loads(raw)
                if args.title:
                    spec["title"] = args.title
                try:
                    result = render(json.dumps(spec), width=width,
                                    height=height, color=color)
                    print(result)
                except Exception as e:
                    logger.error(f"Render error (stdin): {e}")
                    exit_code = 1
            else:
                path = Path(file_arg)
                if not path.exists():
                    logger.error(f"File not found: {path}")
                    exit_code = 1
                    continue
                try:
                    result = render_file(path, width=width,
                                         height=height, color=color,
                                         title_override=args.title)
                    print(result)
                except json.JSONDecodeError as e:
                    logger.error(f"Invalid JSON in {path}: {e}")
                    exit_code = 1
                except Exception as e:
                    logger.error(f"Render error ({path}): {e}")
                    exit_code = 1

        except json.JSONDecodeError as e:
            logger.error(f"Invalid JSON (stdin): {e}")
            exit_code = 1
        except KeyboardInterrupt:
            print(file=sys.stderr)
            return 130

    return exit_code


if __name__ == "__main__":
    sys.exit(main())
