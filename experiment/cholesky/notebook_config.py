#!/usr/bin/env python3
"""
Cholesky experiment-specific notebook configuration.

This module provides experiment-specific plot ordering and titles for notebook generation.
"""

from typing import List, Tuple


def get_plot_info() -> List[Tuple[str, str]]:
    """Get ordered list of (plot_method, title) pairs for plots.

    Returns:
        List of (plot_method, title) tuples in desired display order
    """
    plot_info = [
        ("plot_binary_size_overhead", "Binary Size Overhead"),
        ("plot_binary_size_grouped", "Binary Size Comparison - All Sections (Layered)"),
        ("plot_compilation_time", "Compilation Time"),
        ("plot_runtime_performance", "Runtime Performance (Absolute)"),
        ("plot_runtime_performance_relative", "Runtime Performance (Relative to Sequential)"),
        ("plot_memory_usage", "Memory Usage (Absolute)"),
        ("plot_memory_usage_relative", "Memory Usage (Relative to Sequential)"),
    ]
    
    return plot_info


def get_image_info(image_files: List[str]) -> List[Tuple[str, str]]:
    """Get ordered list of (filename, title) pairs for images.
    
    This function is kept for backward compatibility with older versions.
    
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
        # Strip timestamp suffix if present (format: matrixsize_YYYYMMDD_HHMMSS)
        parts = config_part.split("_")
        # Try to parse first part as integer (new format: binary_size_overhead_10080_timestamp.png)
        try:
            return int(parts[0])
        except (ValueError, IndexError):
            # Old format with blocks suffix (binary_size_overhead_1blocks.png) - skip these
            return -1

    overhead_images = sorted(overhead_images, key=get_matrix_size_key)

    for img in overhead_images:
        # Extract matrix size (e.g., "10080" from "binary_size_overhead_10080_20251121_082803.png")
        config_part = img.replace("binary_size_overhead_", "").replace(".png", "")
        # Strip timestamp suffix if present
        parts = config_part.split("_")

        # Only include new format images (pure numbers, not "Nblocks")
        try:
            matrix_size = int(parts[0])
            title = f"Binary Size Overhead - {matrix_size}×{matrix_size}"
            image_info.append((img, title))
        except (ValueError, IndexError):
            # Skip old format files
            pass

    # Standard images (base names without timestamp suffix)
    standard_images_base = [
        ("binary_size_grouped", "Binary Size Comparison - All Sections (Layered)"),
        ("compilation_time", "Compilation Time"),
        ("runtime_performance", "Runtime Performance (Absolute)"),
        ("runtime_performance_relative", "Runtime Performance (Relative to Sequential)"),
        ("memory_usage", "Memory Usage (Absolute)"),
        ("memory_usage_relative", "Memory Usage (Relative to Sequential)"),
    ]

    for base_name, title in standard_images_base:
        # Find matching file (with or without timestamp suffix)
        matching = [f for f in image_files if f.startswith(base_name) and f.endswith(".png")]
        if matching:
            # Use the first match (should only be one with timestamp suffix)
            image_info.append((matching[0], title))

    return image_info
