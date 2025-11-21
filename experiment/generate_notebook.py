#!/usr/bin/env python3
"""
Generate a Jupyter notebook containing visualization results.

This script creates a notebook with embedded images from an experiment run.
"""

import json
from pathlib import Path
from datetime import datetime
from typing import List, Optional


def create_visualization_notebook(
    experiment_dir: Path,
    run_timestamp: str,
    images_dir: Path,
    image_files: List[str]
) -> Path:
    """Create a Jupyter notebook with embedded visualization images.

    Args:
        experiment_dir: Path to experiment directory
        run_timestamp: Timestamp of the experiment run
        images_dir: Path to images directory
        image_files: List of image filenames to include

    Returns:
        Path to the generated notebook
    """
    # Create notebooks directory
    notebooks_dir = experiment_dir / "notebooks"
    notebooks_dir.mkdir(exist_ok=True)

    # Notebook filename
    notebook_path = notebooks_dir / f"results_{run_timestamp}.ipynb"

    # Load metadata if available
    metadata_file = experiment_dir / "metadata.json"
    metadata = {}
    if metadata_file.exists():
        with open(metadata_file, 'r') as f:
            metadata = json.load(f)

    # Build notebook structure
    cells = []

    # Title cell
    cells.append({
        "cell_type": "markdown",
        "metadata": {},
        "source": [
            f"# Experiment Results: {experiment_dir.name}\n",
            f"\n",
            f"**Run Timestamp:** {run_timestamp}\n",
            f"**Generated:** {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n"
        ]
    })

    # Experiment info cell
    if metadata:
        info_lines = ["## Experiment Configuration\n", "\n"]
        info_lines.append(f"- **Experiment:** {metadata.get('experiment_name', 'N/A')}\n")
        info_lines.append(f"- **Configurations:** {metadata.get('num_configurations', 'N/A')}\n")
        info_lines.append(f"- **Repetitions:** {metadata.get('num_repetitions', 'N/A')}\n")
        info_lines.append(f"- **Warmup runs:** {metadata.get('warmup_runs', 'N/A')}\n")
        if 'git_commit' in metadata:
            info_lines.append(f"- **Git commit:** `{metadata['git_commit']}`\n")

        cells.append({
            "cell_type": "markdown",
            "metadata": {},
            "source": info_lines
        })

    # Add images
    cells.append({
        "cell_type": "markdown",
        "metadata": {},
        "source": ["## Visualizations\n"]
    })

    # Try to import experiment-specific notebook configuration
    # This allows each experiment to define custom image ordering and titles
    image_info = []
    try:
        import sys
        sys.path.insert(0, str(experiment_dir))
        from notebook_config import get_image_info
        image_info = get_image_info(image_files)
        print(f"Using experiment-specific notebook configuration from {experiment_dir / 'notebook_config.py'}")
    except (ImportError, ModuleNotFoundError):
        # Fall back to generic behavior: display all images in alphabetical order
        print(f"No experiment-specific notebook configuration found, using default ordering")
        for img in sorted(image_files):
            # Generate title from filename: remove extension and replace underscores with spaces
            title = img.replace('.png', '').replace('_', ' ').title()
            image_info.append((img, title))

    for image_file, title in image_info:
        if image_file in image_files:
            image_path = images_dir / image_file

            # Add title
            cells.append({
                "cell_type": "markdown",
                "metadata": {},
                "source": [f"### {title}\n"]
            })

            # Add image using markdown
            # Use relative path from notebook to image
            rel_path = Path("..") / "images" / image_file
            cells.append({
                "cell_type": "markdown",
                "metadata": {},
                "source": [f"![]({rel_path})\n"]
            })

    # Data files cell
    cells.append({
        "cell_type": "markdown",
        "metadata": {},
        "source": [
            "## Data Files\n",
            "\n",
            f"- **Results:** `results/results_{run_timestamp}.csv`\n",
            f"- **Build Metrics:** `build_metrics/build_metrics_{run_timestamp}.csv`\n",
            f"- **Log:** `logs/experiment_{run_timestamp}.log`\n"
        ]
    })

    # Create code cell to load data
    cells.append({
        "cell_type": "code",
        "execution_count": None,
        "metadata": {},
        "outputs": [],
        "source": [
            "# Load results data\n",
            "import pandas as pd\n",
            "from pathlib import Path\n",
            "\n",
            f"results = pd.read_csv('../results/results_{run_timestamp}.csv')\n",
            f"build_metrics = pd.read_csv('../build_metrics/build_metrics_{run_timestamp}.csv')\n",
            "\n",
            "print(f'Results shape: {results.shape}')\n",
            "print(f'Build metrics shape: {build_metrics.shape}')\n"
        ]
    })

    # Summary statistics cell
    cells.append({
        "cell_type": "code",
        "execution_count": None,
        "metadata": {},
        "outputs": [],
        "source": [
            "# Summary statistics\n",
            "print('=== Runtime Summary ===')\n",
            "print(results.groupby('scheduler')['wall_time'].describe())\n",
            "print('\\n=== Memory Summary ===')\n",
            "print(results.groupby('scheduler')['max_rss_kb'].describe())\n"
        ]
    })

    # Build the notebook structure
    notebook = {
        "cells": cells,
        "metadata": {
            "kernelspec": {
                "display_name": "Python 3",
                "language": "python",
                "name": "python3"
            },
            "language_info": {
                "codemirror_mode": {
                    "name": "ipython",
                    "version": 3
                },
                "file_extension": ".py",
                "mimetype": "text/x-python",
                "name": "python",
                "nbconvert_exporter": "python",
                "pygments_lexer": "ipython3",
                "version": "3.10.0"
            }
        },
        "nbformat": 4,
        "nbformat_minor": 4
    }

    # Write notebook
    with open(notebook_path, 'w') as f:
        json.dump(notebook, f, indent=2)

    return notebook_path


def main():
    """CLI entry point."""
    import argparse

    parser = argparse.ArgumentParser(description="Generate visualization notebook")
    parser.add_argument("experiment_dir", type=Path,
                       help="Path to experiment directory")
    parser.add_argument("--timestamp", type=str, required=True,
                       help="Run timestamp")
    parser.add_argument("--images", type=str, nargs='+',
                       help="Image files to include")

    args = parser.parse_args()

    images_dir = args.experiment_dir / "images"
    image_files = args.images or []

    notebook_path = create_visualization_notebook(
        args.experiment_dir,
        args.timestamp,
        images_dir,
        image_files
    )

    print(f"Generated notebook: {notebook_path}")


if __name__ == "__main__":
    main()
