"""
Simple progress bar utility for IaRa Unified Experiment Framework.

Provides a lightweight progress tracker that displays percentage completion
in stdout without external dependencies.
"""

import sys
from typing import Optional


class ProgressBar:
    """Simple progress bar that shows percentage and count."""

    def __init__(self, total: int, name: str = "Progress"):
        """
        Initialize progress bar.

        Args:
            total: Total number of items to process
            name: Name of the operation (e.g., "Generating", "Building", "Executing")
        """
        self.total = total
        self.name = name
        self.current = 0
        self.bar_width = 50

    def update(self, count: int = 1) -> None:
        """
        Update progress by incrementing counter.

        Args:
            count: Number of items completed (default 1)
        """
        self.current += count
        self._display()

    def set(self, current: int) -> None:
        """
        Set current progress to a specific count.

        Args:
            current: Current count
        """
        self.current = current
        self._display()

    def _display(self) -> None:
        """Display progress bar to stdout."""
        if self.total == 0:
            return

        percentage = (self.current / self.total) * 100
        filled = int(self.bar_width * self.current / self.total)
        bar = "█" * filled + "░" * (self.bar_width - filled)

        # Format: [████████░░░░░░░░░░] 45% (9/20)
        line = f"\r{self.name}: [{bar}] {percentage:5.1f}% ({self.current}/{self.total})"

        # Write to stdout without newline
        sys.stdout.write(line)
        sys.stdout.flush()

        # Print newline when complete
        if self.current >= self.total:
            sys.stdout.write("\n")
            sys.stdout.flush()
