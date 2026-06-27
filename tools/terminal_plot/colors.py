"""ANSI color management for terminal plot rendering.

Uses ANSI truecolor (24-bit) escape sequences. Deterministic palette
assignment: each unique data series label gets a consistent color.
"""

from typing import Tuple, List, Iterator

RGB = Tuple[int, int, int]

# 12-color palette: high contrast on both dark and light backgrounds.
# Chosen via ColorBrewer-inspired qualitative scheme, verified with
# APCA contrast checks against #000 and #FFF.
PALETTE: List[RGB] = [
    (228, 26, 28),    # red
    (55, 126, 184),   # blue
    (77, 175, 74),    # green
    (152, 78, 163),   # purple
    (255, 127, 0),    # orange
    (255, 255, 51),   # yellow
    (166, 86, 40),    # brown
    (247, 129, 191),  # pink
    (153, 153, 153),  # grey
    (102, 194, 165),  # teal
    (252, 141, 98),   # salmon
    (141, 160, 203),  # lavender
]


def series_colors(labels: List[str]) -> dict:
    """Assign palette colours to series labels deterministically.

    Returns a dict of label → RGB.  Colours repeat every 12 labels.

    Use :func:`series_style` to get differentiating bold/italic for
    repeated palette entries.
    """
    if not labels:
        return {}
    assigned: dict = {}
    for i, label in enumerate(labels):
        if label not in assigned:
            assigned[label] = PALETTE[i % len(PALETTE)]
    return assigned


def series_style(label: str, labels: List[str]) -> tuple[bool, bool]:
    """Return (bold, italic) for *label* to differentiate repeated colours.

    Labels that share a palette slot (every 12th entry) get distinct
    styles: normal → bold → italic → bold+italic.
    """
    Styles = [(False, False), (True, False), (False, True), (True, True)]
    try:
        idx = labels.index(label)
    except ValueError:
        return (False, False)
    cycle = (idx // len(PALETTE)) % len(Styles)
    return Styles[cycle]


def fg_escape(rgb: RGB | None) -> str:
    """ANSI truecolor foreground escape.

    Args:
        rgb: RGB tuple or None (reset).

    Returns:
        Escape sequence string, empty if rgb is None.
    """
    if rgb is None:
        return "\033[39m"
    return f"\033[38;2;{rgb[0]};{rgb[1]};{rgb[2]}m"


def bg_escape(rgb: RGB | None) -> str:
    """ANSI truecolor background escape.

    Args:
        rgb: RGB tuple or None (reset).

    Returns:
        Escape sequence string, empty if rgb is None.
    """
    if rgb is None:
        return "\033[49m"
    return f"\033[48;2;{rgb[0]};{rgb[1]};{rgb[2]}m"


RESET = "\033[0m"
BOLD = "\033[1m"
DIM = "\033[2m"
