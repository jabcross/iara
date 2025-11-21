#!/usr/bin/env python3
"""
Cholesky experiment-specific notebook configuration.

This module provides experiment-specific image ordering and titles for notebook generation.
"""

from typing import List, Tuple


def get_image_info(image_files: List[str]) -> List[Tuple[str, str]]:
    """Get ordered list of (filename, title) pairs for images.

    Args:
        image_files: List of available image filenames

    Returns:
        List of (filename, title) tuples in desired display order
    """
    image_info = []

    # Binary size overhead images (one per matrix size)
    overhead_images = [f for f in image_files if f.startswith("binary_size_overhead_")]

    # Sort numerically by matrix size (extract numeric part)
    def get_matrix_size_key(filename):
        """Extract numeric matrix size from filename for sorting."""
        config_part = filename.replace("binary_size_overhead_", "").replace(".png", "")
        # Try to parse as integer (new format: binary_size_overhead_10080.png)
        try:
            return int(config_part)
        except ValueError:
            # Old format with blocks suffix (binary_size_overhead_1blocks.png) - skip these
            return -1

    overhead_images = sorted(overhead_images, key=get_matrix_size_key)

    for img in overhead_images:
        # Extract matrix size (e.g., "10080" from "binary_size_overhead_10080.png")
        config_part = img.replace("binary_size_overhead_", "").replace(".png", "")

        # Only include new format images (pure numbers, not "Nblocks")
        try:
            matrix_size = int(config_part)
            title = f"Binary Size Overhead - {matrix_size}×{matrix_size}"
            image_info.append((img, title))
        except ValueError:
            # Skip old format files
            pass

    # Standard images
    standard_images = [
        ("binary_size_grouped.png", "Binary Size Comparison - All Sections (Layered)"),
        ("compilation_time.png", "Compilation Time"),
        ("runtime_performance.png", "Runtime Performance (Absolute)"),
        ("runtime_performance_relative.png", "Runtime Performance (Relative to Sequential)"),
        ("memory_usage.png", "Memory Usage (Absolute)"),
        ("memory_usage_relative.png", "Memory Usage (Relative to Sequential)"),
    ]

    for filename, title in standard_images:
        if filename in image_files:
            image_info.append((filename, title))

    return image_info
