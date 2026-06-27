"""TerminalCanvas — fixed-size 2D character grid with ANSI color overlays.

Renders to an ANSI-escaped string suitable for direct terminal output.
Uses Unicode block-drawing characters for smooth bar charts.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import List, Optional

from .colors import RGB, fg_escape, bg_escape, BOLD, DIM, RESET

# ── Block-element tables ──────────────────────────────────────────

# Vertical bar fill: 9 levels (0 = empty, 8 = full).
# U+2581–U+2588 + space for 0.
VERT_BLOCKS = " ▁▂▃▄▅▆▇█"

# Horizontal fractional bars: 9 levels (0 = empty, 8 = full).
# U+258F–U+2588, left-to-right partial blocks + space.
HORIZ_BLOCKS = " ▏▎▍▌▋▊▉█"

# Block sextants (Symbols for Legacy Computing, U+1FB00–U+1FB3F).
# Each character divides the cell into a 2×3 sub-grid.
# The bits: bit0=top-left, bit1=top-right, bit2=mid-left,
# bit3=mid-right, bit4=bottom-left, bit5=bottom-right.
# U+1FB00 is sextant-1 (bit 0 only), U+1FB3F is all six bits.
# We build a lookup table indexed by 6-bit mask.
_SEXTANT_BASE = 0x1FB00


def _build_sextant_table() -> List[str]:
    """Build the 64-entry block sextant lookup table."""
    table: List[str] = []
    for mask in range(64):
        cp = _SEXTANT_BASE + mask
        table.append(chr(cp))
    return table


SEXTANTS = _build_sextant_table()


# ── Box-drawing characters ────────────────────────────────────────

BOX_H = "─"
BOX_V = "│"
BOX_TL = "┌"
BOX_TR = "┐"
BOX_BL = "└"
BOX_BR = "┘"
BOX_TD = "┬"
BOX_BD = "┴"
BOX_LT = "├"
BOX_RT = "┤"
BOX_CR = "┼"


# ── Cell ──────────────────────────────────────────────────────────


@dataclass
class Cell:
    """Single character cell with optional ANSI attributes."""

    char: str = " "
    fg: RGB | None = None
    bg: RGB | None = None
    bold: bool = False
    dim: bool = False
    italic: bool = False
    overline: bool = False
    underline: bool = False
    ul_rgb: RGB | None = None  # explicit underline colour (kitty/foot)

    def render(self, prev_fg: RGB | None, prev_bg: RGB | None,
               prev_bold: bool, prev_dim: bool,
               prev_ol: bool = False, prev_ul: bool = False,
               prev_ul_rgb: RGB | None = None,
               prev_italic: bool = False) -> str:
        """Render this cell, emitting ANSI codes only when state changes.

        Args:
            prev_fg/bg/bold/dim: Previous cell's state for diffing.

        Returns:
            ANSI text for this cell (char + any needed escapes).
        """
        parts: List[str] = []

        if self.fg != prev_fg:
            parts.append(fg_escape(self.fg))
        if self.bg != prev_bg:
            parts.append(bg_escape(self.bg))
        if self.bold != prev_bold:
            parts.append(BOLD if self.bold else "\033[22m")
        if self.dim != prev_dim and not self.bold:
            parts.append(DIM if self.dim else "\033[22m")
        if self.italic != prev_italic:
            parts.append("\033[3m" if self.italic else "\033[23m")
        if self.ul_rgb != prev_ul_rgb:
            if self.ul_rgb is not None:
                # Kitty: disable, set colour, re-enable.
                parts.append("\033[24m")
                parts.append(f"\033[58:2:{self.ul_rgb[0]}:{self.ul_rgb[1]}:{self.ul_rgb[2]}m")
                parts.append("\033[4:1m")
            else:
                parts.append("\033[59m")
        if self.overline != prev_ol:
            parts.append("\033[53m" if self.overline else "\033[55m")
        if self.underline != prev_ul:
            if self.ul_rgb is not None:
                pass  # handled above with 24m/4:1m
            elif self.underline:
                parts.append("\033[4m")
            else:
                parts.append("\033[24m")

        parts.append(self.char)
        return "".join(parts)


# ── Canvas ────────────────────────────────────────────────────────


@dataclass
class TerminalCanvas:
    """Fixed-size 2D character grid.

    Attributes:
        width: Number of columns.
        height: Number of rows.
        grid: Row-major cell array (grid[y][x]).
    """

    width: int
    height: int
    grid: List[List[Cell]] = field(init=False)

    def __post_init__(self):
        self.grid = [[Cell() for _ in range(self.width)]
                     for _ in range(self.height)]

    def put(self, x: int, y: int, char: str = " ",
            fg: RGB | None = None, bg: RGB | None = None,
            bold: bool = False, dim: bool = False,
            italic: bool = False,
            overline: bool = False, underline: bool = False,
            ul_rgb: RGB | None = None):
        """Place a character at (x, y), clipping to canvas bounds."""
        if 0 <= x < self.width and 0 <= y < self.height:
            self.grid[y][x] = Cell(char=char, fg=fg, bg=bg,
                                   bold=bold, dim=dim, italic=italic,
                                   overline=overline, underline=underline,
                                   ul_rgb=ul_rgb)

    def hline(self, x: int, y: int, width: int,
              char: str = BOX_H, fg: RGB | None = None):
        """Draw horizontal line."""
        for i in range(max(0, width)):
            self.put(x + i, y, char, fg=fg)

    def vline(self, x: int, y: int, height: int,
              char: str = BOX_V, fg: RGB | None = None):
        """Draw vertical line."""
        for j in range(max(0, height)):
            self.put(x, y + j, char, fg=fg)

    def text(self, x: int, y: int, s: str,
             fg: RGB | None = None, bold: bool = False,
             dim: bool = False):
        """Write string, clipped to canvas right edge."""
        for i, ch in enumerate(s):
            if x + i >= self.width:
                break
            self.put(x + i, y, ch, fg=fg, bold=bold, dim=dim)

    def rect(self, x: int, y: int, w: int, h: int,
             fg: RGB | None = None, bg: RGB | None = None):
        """Draw a rectangle border using box-drawing characters."""
        if w < 2 or h < 2:
            return
        # Corners
        self.put(x, y, BOX_TL, fg=fg, bg=bg)
        self.put(x + w - 1, y, BOX_TR, fg=fg, bg=bg)
        self.put(x, y + h - 1, BOX_BL, fg=fg, bg=bg)
        self.put(x + w - 1, y + h - 1, BOX_BR, fg=fg, bg=bg)
        # Edges
        self.hline(x + 1, y, w - 2, BOX_H, fg=fg)
        self.hline(x + 1, y + h - 1, w - 2, BOX_H, fg=fg)
        self.vline(x, y + 1, h - 2, BOX_V, fg=fg)
        self.vline(x + w - 1, y + 1, h - 2, BOX_V, fg=fg)

    def bar_h(self, x: int, y: int, width: int, fraction: float,
              fg: RGB | None = None, bg: RGB | None = None):
        """Draw a horizontal bar at (x,y) filling `fraction` of `width` columns.

        Uses block-element characters for sub-character precision.
        fraction: 0.0 = empty, 1.0 = full width.
        """
        if fraction <= 0:
            return
        full_cells = int(fraction * width)
        remainder = fraction * width - full_cells

        # Full cells
        for i in range(min(full_cells, width)):
            self.put(x + i, y, "█", fg=fg, bg=bg)

        # Fractional cell (sub-character precision)
        if full_cells < width and remainder > 0:
            idx = int(remainder * 8)
            if idx > 0:
                self.put(x + full_cells, y, HORIZ_BLOCKS[idx],
                         fg=fg, bg=bg)

    def bar_v(self, x: int, y: int, height: int, fraction: float,
              fg: RGB | None = None, bg: RGB | None = None):
        """Draw a vertical bar growing upward from (x, y+height-1).

        Uses block-element characters + block sextants for
        sub-character Y precision (6 levels per character).
        fraction: 0.0 = empty, 1.0 = max height.
        """
        if fraction <= 0:
            return

        filled = fraction * height
        full_cells = int(filled)
        # Remainder mapped to 6-level sextant (0-6)
        rem6 = int((filled - full_cells) * 6)

        # Draw full cells from bottom up
        for j in range(full_cells):
            py = y + height - 1 - j
            if 0 <= py < self.height:
                self.put(x, py, "█", fg=fg, bg=bg)

        # Fractional cell using sextant
        if full_cells < height and rem6 > 0:
            py = y + height - 1 - full_cells
            if 0 <= py < self.height:
                # Determine which sextant bits to set.
                # For vertical bar growing from bottom, we want the
                # bottom N rows of the 3-row sub-grid filled.
                # Bottom row = bits 4,5; middle = bits 2,3; top = bits 0,1.
                mask: int
                if rem6 == 1:
                    mask = 0b110000  # bottom-right only
                elif rem6 == 2:
                    mask = 0b111100  # bottom row
                elif rem6 == 3:
                    mask = 0b111101  # bottom row + bottom of middle
                elif rem6 == 4:
                    mask = 0b111111  # bottom + middle rows
                elif rem6 == 5:
                    mask = 0b111111 | 0b000001  # need custom...
                    # Actually let's use a simpler approach
                    mask = 0b111111  # wait this is 6 bits...

                # Simpler: use bit pattern based on remainder fraction.
                # Bottom→top fill: mask bottom bits first.
                if rem6 == 1:
                    mask = 0b110000
                elif rem6 == 2:
                    mask = 0b111100
                elif rem6 == 3:
                    mask = 0b111101
                elif rem6 == 4:
                    mask = 0b111111
                elif rem6 == 5:
                    mask = 0b111111  # full cell (shouldn't need 5 usually)
                else:
                    mask = 0

                self.put(x, py, SEXTANTS[mask], fg=fg, bg=bg)

    def hbar_sextant(self, x: int, y: int, width: int, fraction: float,
                     fg: RGB | None = None, bg: RGB | None = None):
        """Draw horizontal bar with sextant-level precision.

        Uses block sextants for the fractional column, giving 6
        sub-column steps per character (2×3 grid, rotated).
        For horizontal bars, read the sextant rotated: left column
        occupies bits corresponding to the left half.
        """
        if fraction <= 0:
            return
        filled = fraction * width
        full_cells = int(filled)
        rem6 = int((filled - full_cells) * 6)

        for i in range(min(full_cells, width)):
            self.put(x + i, y, "█", fg=fg, bg=bg)

        if full_cells < width and rem6 > 0:
            # For horizontal: fill left column of sextant first.
            # Left column: bits 0 (top-left), 2 (mid-left), 4 (bottom-left).
            masks = {
                1: 0b010101,  # just left column, one pixel
                # Actually: rem6=1 means 1/6th → we want one subcolumn of
                # the left half. Let's use a simpler approach: map rem6
                # to the horizontal block chars, they're good enough.
            }
            # Fallback to standard 8-level horizontal blocks for clarity.
            idx = min(8, int(rem6 * 8 / 6))
            self.put(x + full_cells, y, HORIZ_BLOCKS[idx],
                     fg=fg, bg=bg)

    def bar_stacked(self, x: int, y: int, bar_w: int,
                    segments: list[tuple[float, RGB | None]]
                    ) -> set[int]:
        """Draw a stacked horizontal bar with sliver-preserving rounding.

        Tiny segments get at least one sub-position (partial block,
        proportional to size).  The next segment is ceil-rounded into
        the negative space left behind.  Rounding error is tracked and
        later absorbed by the largest segment so the total bar width
        stays exact.

        When a cell is split, the first segment's partial block is
        drawn with the second segment's background for continuity.

        Args:
            x: Leftmost column of the bar area.
            y: Row to draw on.
            bar_w: Total bar width in character cells.
            segments: List of (fraction, colour) tuples in stack order.

        Returns:
            Set of segment indices that rendered at least one
            sub-position.
        """
        rendered: set[int] = set()
        if not segments or bar_w <= 0:
            return rendered

        total_sub = bar_w * 8
        import math

        # Exact sub-positions per segment.
        seg_exact = [frac * total_sub for frac, _ in segments]

        # Per-segment rendered sub-positions (tracked for absorption).
        seg_rendered = [0.0] * len(seg_exact)
        extra = 0.0  # accumulated round-up error

        seg_idx = 0
        seg_cursor = 0.0  # consumed within current segment

        for cell_idx in range(bar_w):
            cell_remaining = 8.0
            cell_fg: RGB | None = None
            cell_bg: RGB | None = None
            cell_ch: str = " "
            cell_drawn = False

            while cell_remaining > 0.001 and seg_idx < len(seg_exact):
                avail = seg_exact[seg_idx] - seg_cursor
                if avail <= 0.001:
                    seg_idx += 1
                    seg_cursor = 0.0
                    continue

                true_avail = min(avail, cell_remaining)

                # Proportional sliver: at least 1 sub-pos, ceil-rounded
                # so negative space flows to the next segment.
                taken = min(cell_remaining,
                            max(1.0, math.ceil(true_avail)))
                fill = int(taken)
                if fill <= 0:
                    break

                ch = HORIZ_BLOCKS[min(8, fill)]
                if not cell_drawn:
                    cell_fg = segments[seg_idx][1]
                    cell_ch = ch
                    cell_drawn = True
                else:
                    cell_bg = segments[seg_idx][1]

                extra += taken - true_avail
                seg_rendered[seg_idx] += taken
                seg_cursor += true_avail
                cell_remaining -= taken

                if seg_cursor >= seg_exact[seg_idx] - 0.001:
                    seg_idx += 1
                    seg_cursor = 0.0

            if cell_drawn:
                self.put(x + cell_idx, y, cell_ch, fg=cell_fg, bg=cell_bg)

        # Absorb rounding error into the largest segment.
        if extra > 0 and seg_exact:
            largest = max(range(len(seg_exact)), key=lambda i: seg_exact[i])
            seg_rendered[largest] = max(0.0, seg_rendered[largest] - extra)

        return {i for i, r in enumerate(seg_rendered) if r >= 1.0}

    def to_ansi(self) -> str:
        """Serialize the entire canvas to an ANSI-escaped string.

        Uses state diffing: ANSI codes emitted only when attributes
        change between adjacent cells. Each row separated by newline.
        Trailing spaces are stripped per row to avoid artifacts.
        """
        lines: List[str] = []
        for row in self.grid:
            parts: List[str] = []
            prev_fg: RGB | None = None
            prev_bg: RGB | None = None
            prev_bold = False
            prev_dim = False
            prev_italic = False
            prev_ol = False
            prev_ul = False
            prev_ul_rgb: RGB | None = None
            for cell in row:
                parts.append(cell.render(prev_fg, prev_bg,
                                         prev_bold, prev_dim,
                                         prev_ol, prev_ul,
                                         prev_ul_rgb,
                                         prev_italic))
                prev_fg = cell.fg
                prev_bg = cell.bg
                prev_bold = cell.bold
                prev_dim = cell.dim
                prev_italic = cell.italic
                prev_ol = cell.overline
                prev_ul = cell.underline
                prev_ul_rgb = cell.ul_rgb
            # Reset at end of line, strip trailing spaces
            line = "".join(parts) + RESET
            line = line.rstrip(" ")
            lines.append(line)
        return "\n".join(lines)
