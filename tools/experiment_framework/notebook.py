"""
Jupyter notebook generation module for IaRa Unified Experiment Framework.

This module generates and executes Jupyter notebooks with visualizations
and failure reports.
"""

import json
import logging
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Dict, List, Any, Optional


# Configure logging
logger = logging.getLogger(__name__)


# =============================================================================
# Task 4.3: Failure Report Generation
# =============================================================================


def extract_failure_summary(failed_instances: List[Dict[str, Any]]) -> List[Dict[str, str]]:
    """
    Parse failed instances and extract structured error summaries.

    Algorithm:
        1. If empty: return []
        2. For each failed instance:
           - Extract: name, attempts, errors list
           - Get error summary: first 100 chars of last error message (truncate with "..." if longer)
           - Get full error: format all attempts with error details
           - Append summary dict with keys: instance_name, attempts, error_summary, full_error

    Args:
        failed_instances: List of failed instance dicts with keys:
            - instance_name: str
            - errors: List[Dict] with keys 'phase', 'message'

    Returns:
        List of summary dicts with keys:
            - instance_name: Instance name
            - attempts: Number of attempts (as string)
            - error_summary: Truncated error message (first 100 chars)
            - full_error: Complete formatted error details

    Examples:
        >>> failed = [{"instance_name": "test", "errors": [{"phase": "build", "message": "Error!"}]}]
        >>> result = extract_failure_summary(failed)
        >>> result[0]["instance_name"]
        'test'
    """
    if not failed_instances:
        return []

    summaries = []

    for instance in failed_instances:
        instance_name = instance.get("instance_name", "unknown")
        errors = instance.get("errors", [])

        # Handle missing or empty errors
        if not errors:
            summaries.append({
                "instance_name": instance_name,
                "attempts": "0",
                "error_summary": "No error details available",
                "full_error": "No error details available"
            })
            continue

        attempts = str(len(errors))

        # Get last error message for summary
        last_error = errors[-1] if errors else {}
        last_message = last_error.get("message", "No error message")

        # Truncate to 100 chars with "..." if longer
        if len(last_message) > 100:
            error_summary = last_message[:100] + "..."
        else:
            error_summary = last_message

        # Build full error details with all attempts
        full_error_lines = []
        for i, error in enumerate(errors, 1):
            phase = error.get("phase", "unknown")
            message = error.get("message", "No error message")
            full_error_lines.append(f"Attempt {i}: {phase}\n  {message}")

        full_error = "\n\n".join(full_error_lines)

        summaries.append({
            "instance_name": instance_name,
            "attempts": attempts,
            "error_summary": error_summary,
            "full_error": full_error
        })

    return summaries


def generate_failure_markdown(failed_instances: List[Dict[str, Any]]) -> str:
    """
    Create markdown table with collapsible error details.

    Algorithm:
        1. If empty: return "## Build Failures\n\nNo build failures.\n"
        2. Extract summaries using extract_failure_summary()
        3. Build markdown lines:
           - Header: "## Build Failures"
           - Stat: "**Total Failed:** {count} instances"
           - Table header and rows
           - Each row: Instance | Attempts | Summary | <details><summary>View</summary>Full Error</details>
        4. Return joined markdown

    Args:
        failed_instances: List of failed instance dicts

    Returns:
        Markdown string with table and collapsible details

    HTML Details Format:
        <details><summary>View</summary><pre>Error details here</pre></details>

    Examples:
        >>> markdown = generate_failure_markdown([])
        >>> "No build failures" in markdown
        True
    """
    if not failed_instances:
        return "## Build Failures\n\nNo build failures.\n"

    summaries = extract_failure_summary(failed_instances)

    markdown_lines = [
        "## Build Failures",
        "",
        f"**Total Failed:** {len(summaries)} instances",
        "",
        "| Instance Name | Attempts | Error Summary | Full Error |",
        "|---------------|----------|---------------|------------|"
    ]

    for summary in summaries:
        instance_name = summary["instance_name"]
        attempts = summary["attempts"]
        error_summary = summary["error_summary"]
        full_error = summary["full_error"]

        # Escape pipe characters in error messages for markdown table
        error_summary_escaped = error_summary.replace("|", "\\|").replace("\n", " ")

        # Create collapsible details section
        details_html = f'<details><summary>View</summary><pre>{full_error}</pre></details>'

        row = f"| {instance_name} | {attempts} | {error_summary_escaped} | {details_html} |"
        markdown_lines.append(row)

    return "\n".join(markdown_lines) + "\n"


def get_failure_report_cell(failed_instances: List[Dict[str, Any]]) -> Dict[str, Any]:
    """
    Create Jupyter notebook markdown cell with failure report.

    Algorithm:
        1. Generate markdown: md_content = generate_failure_markdown(failed_instances)
        2. Create nbformat v4 cell:
           {
               "cell_type": "markdown",
               "metadata": {},
               "source": md_content.split("\n")  # List of lines
           }
        3. Return cell

    Args:
        failed_instances: List of failed instance dicts

    Returns:
        Jupyter notebook cell dict (nbformat v4) with:
            - cell_type: "markdown"
            - metadata: {}
            - source: List of strings (one per line)

    Examples:
        >>> cell = get_failure_report_cell([])
        >>> cell["cell_type"]
        'markdown'
    """
    md_content = generate_failure_markdown(failed_instances)

    # Split into list of lines for nbformat v4 (preserving newlines)
    lines = md_content.split("\n")
    # Add newlines back (except for last line)
    source = [line + "\n" for line in lines[:-1]]
    if lines[-1]:  # Add last line without newline if non-empty
        source.append(lines[-1])

    return {
        "cell_type": "markdown",
        "metadata": {},
        "source": source
    }


# =============================================================================
# Task 4.4: Jupyter Notebook Generation
# =============================================================================


def create_metadata_cell(config: Dict[str, Any], results: Dict[str, Any]) -> Dict[str, Any]:
    """
    Create notebook header with experiment metadata.

    Algorithm:
        1. Extract from config: app_name, app_description
        2. Extract from results: experiment_set, timestamp, git_commit, yaml_hash, instance counts
        3. Build markdown lines:
           - Title: "# {app_name} - Experiment Results"
           - Description
           - Metadata section with bullets
           - Summary statistics
        4. Create markdown cell and return

    Args:
        config: Complete YAML configuration dict
        results: Results JSON dict with experiment metadata

    Returns:
        Jupyter notebook markdown cell dict

    Examples:
        >>> config = {"application": {"name": "test", "description": "Test app"}}
        >>> results = {"experiment": {"set": "regression", "timestamp": "2024-01-15T10:00:00Z"}}
        >>> cell = create_metadata_cell(config, results)
        >>> cell["cell_type"]
        'markdown'
    """
    # Extract application info
    app_name = config.get("application", {}).get("name", "Unknown Application")
    app_description = config.get("application", {}).get("description", "")

    # Extract experiment metadata
    experiment = results.get("experiment", {})
    experiment_set = experiment.get("set", "unknown")
    timestamp = experiment.get("timestamp", "unknown")
    git_commit = experiment.get("git_commit", "unknown")
    yaml_hash = experiment.get("yaml_hash", "unknown")

    # Extract instance counts
    instances = results.get("instances", [])
    failed_instances = results.get("failed_instances", [])
    total_instances = len(instances) + len(failed_instances)
    successful_instances = len(instances)
    failed_count = len(failed_instances)

    # Calculate success percentage
    success_pct = (successful_instances / total_instances * 100) if total_instances > 0 else 0

    # Build markdown
    markdown_lines = [
        f"# {app_name} - Experiment Results",
        ""
    ]

    if app_description:
        markdown_lines.extend([app_description, ""])

    markdown_lines.extend([
        "## Experiment Metadata",
        "",
        f"- **Experiment Set:** {experiment_set}",
        f"- **Timestamp:** {timestamp}",
        f"- **Git Commit:** {git_commit}",
        f"- **YAML Hash:** {yaml_hash[:16]}...",
        "",
        "## Summary Statistics",
        "",
        f"- **Total Instances:** {total_instances}",
        f"- **Successful:** {successful_instances} ({success_pct:.1f}%)",
        f"- **Failed:** {failed_count}",
        ""
    ])

    markdown_content = "\n".join(markdown_lines)

    # Convert to list of lines for nbformat
    lines = markdown_content.split("\n")
    source = [line + "\n" for line in lines[:-1]]
    if lines[-1]:
        source.append(lines[-1])

    return {
        "cell_type": "markdown",
        "metadata": {},
        "source": source
    }


def create_data_loading_cell(results_json_path: Path) -> Dict[str, Any]:
    """
    Create code cell to load Phase 3 JSON data.

    Algorithm:
        1. Create Python code to:
           - Import json and Path
           - Load results JSON
           - Print summary
        2. Create code cell with source as list of lines
        3. Return cell

    Args:
        results_json_path: Path to results JSON file

    Returns:
        Jupyter notebook code cell dict

    Examples:
        >>> cell = create_data_loading_cell(Path("/tmp/results.json"))
        >>> cell["cell_type"]
        'code'
    """
    # Convert to absolute path if it isn't already
    abs_path = results_json_path.resolve() if isinstance(results_json_path, Path) else Path(results_json_path).resolve()

    code = f"""import json
from pathlib import Path

results_path = Path("{abs_path}")
with open(results_path, 'r') as f:
    results_data = json.load(f)

print(f"Loaded {{len(results_data['instances'])}} instances")
print(f"Schema version: {{results_data['schema_version']}}")
"""

    # Convert to list of lines for nbformat
    lines = code.split("\n")
    source = [line + "\n" for line in lines[:-1]]
    if lines[-1]:
        source.append(lines[-1])

    return {
        "cell_type": "code",
        "execution_count": None,
        "metadata": {},
        "outputs": [],
        "source": source
    }


def create_statistics_cell(measurements: List[str]) -> Dict[str, Any]:
    """
    Create code cell for computing summary statistics with pandas.

    Algorithm:
        1. Create Python code that:
           - Extracts instances into DataFrame
           - Groups by scheduler
           - Shows mean/std/min/max for each measurement
        2. Return code cell

    Args:
        measurements: List of measurement names to compute statistics for

    Returns:
        Jupyter notebook code cell dict

    Examples:
        >>> cell = create_statistics_cell(["wall_time_s", "max_rss_bytes"])
        >>> cell["cell_type"]
        'code'
    """
    # Build list of measurements for code
    measurements_str = ", ".join(f'"{m}"' for m in measurements) if measurements else ""

    code = f"""import pandas as pd

# Extract instances into DataFrame
data_rows = []
for instance in results_data['instances']:
    row = {{
        'name': instance['name'],
        'scheduler': instance.get('scheduler', 'unknown')
    }}

    # Extract execution data if available
    execution = instance.get('execution', {{}})
    if execution:
        statistics = execution.get('statistics', {{}})
        for metric_name, metric_stats in statistics.items():
            row[f'{{metric_name}}_mean'] = metric_stats.get('mean', None)
            row[f'{{metric_name}}_std'] = metric_stats.get('std', None)
            row[f'{{metric_name}}_min'] = metric_stats.get('min', None)
            row[f'{{metric_name}}_max'] = metric_stats.get('max', None)

    data_rows.append(row)

df = pd.DataFrame(data_rows)

# Display summary statistics grouped by scheduler
if len(df) > 0:
    print("Summary Statistics by Scheduler:")
    print("=" * 80)

    # Get all measurement columns
    measurement_cols = [col for col in df.columns if col.endswith('_mean')]

    if measurement_cols:
        for scheduler in df['scheduler'].unique():
            scheduler_df = df[df['scheduler'] == scheduler]
            print(f"\\n{{scheduler}}:")
            print(f"  Instances: {{len(scheduler_df)}}")

            for col in measurement_cols:
                metric_name = col.replace('_mean', '')
                mean_col = f'{{metric_name}}_mean'
                std_col = f'{{metric_name}}_std'

                if mean_col in scheduler_df.columns:
                    mean_val = scheduler_df[mean_col].mean()
                    std_val = scheduler_df[std_col].mean() if std_col in scheduler_df.columns else 0
                    print(f"  {{metric_name}}: {{mean_val:.4f}} ± {{std_val:.4f}}")
    else:
        print("No execution statistics found in results.")
else:
    print("No data available.")
"""

    # Convert to list of lines for nbformat
    lines = code.split("\n")
    source = [line + "\n" for line in lines[:-1]]
    if lines[-1]:
        source.append(lines[-1])

    return {
        "cell_type": "code",
        "execution_count": None,
        "metadata": {},
        "outputs": [],
        "source": source
    }


def create_visualization_cell(plot_name: str, vegalite_path: Path) -> Dict[str, Any]:
    """
    Create code cell to load and render Vega-Lite spec using altair.

    Args:
        plot_name: Name of the plot (for display)
        vegalite_path: Path to Vega-Lite JSON file

    Returns:
        Jupyter notebook code cell dict that renders interactive chart

    Examples:
        >>> cell = create_visualization_cell("runtime", Path("/tmp/plot.vl.json"))
        >>> cell["cell_type"]
        'code'
    """
    # Use absolute path to ensure notebook can find files regardless of working directory
    abs_vegalite_path = vegalite_path.resolve() if isinstance(vegalite_path, Path) else Path(vegalite_path).resolve()
    code = f"""# {plot_name}
import altair as alt
import json
from pathlib import Path

with open("{abs_vegalite_path}", "r") as f:
    vl_spec = json.load(f)

# Load and embed file:// data URLs for a spec part (flat or compound).
# Compound specs (hconcat/vconcat/layer) store data inside each sub-spec,
# not at the top level, so we recurse into them.
_loaded_data = {{}}

def _embed_file_data(spec_part):
    data = spec_part.get("data", {{}})
    url = data.get("url", "")
    if url.startswith("file://"):
        path = url[7:]  # strip "file://"
        if path not in _loaded_data:
            try:
                with open(path, "r") as f:
                    _loaded_data[path] = json.load(f)
            except Exception as e:
                print(f"Warning: Could not load data from {{path}}: {{e}}")
                return
        content = _loaded_data[path]
        spec_part["data"]["values"] = content.get("instances", content)
        del spec_part["data"]["url"]
        spec_part["data"].pop("format", None)

_embed_file_data(vl_spec)
for _compound_key in ("hconcat", "vconcat", "layer", "concat"):
    for _sub_spec in vl_spec.get(_compound_key, []):
        _embed_file_data(_sub_spec)

# Display using Altair (native Jupyter support)
chart = alt.Chart.from_dict(vl_spec)
chart
"""

    # Convert to list of lines for nbformat
    lines = code.split("\n")
    source = [line + "\n" for line in lines[:-1]]
    if lines[-1]:
        source.append(lines[-1])

    return {
        "cell_type": "code",
        "execution_count": None,
        "metadata": {},
        "outputs": [],
        "source": source
    }


def generate_notebook(
    config: Dict[str, Any],
    results_path: Path,
    vegalite_files: List[Path],
    output_path: Path,
    notebook_relative_path: Optional[str] = None
) -> Path:
    """
    Generate complete Jupyter notebook.

    Notebook Structure:
        1. Title and metadata
        2. Failure report (if any failures)
        3. Data loading
        4. Summary statistics
        5. Visualizations (one per plot)

    Algorithm:
        1. Load results: results = json.load(open(results_path))
        2. Create cells list
        3. Add metadata cell
        4. If failures: add failure report cell
        5. Add data loading cell
        6. Add statistics cell
        7. For each vegalite file: add visualization cell
        8. Create notebook structure with metadata
        9. Write notebook to output_path
        10. Return output_path

    Args:
        config: Complete YAML configuration dictionary
        results_path: Path to results JSON file
        vegalite_files: List of generated Vega-Lite visualization files
        output_path: Path where notebook should be written

    Returns:
        Path to generated .ipynb file

    Examples:
        >>> config = {"application": {"name": "test"}}
        >>> results_path = Path("/tmp/results.json")
        >>> output = Path("/tmp/notebook.ipynb")
        >>> # generate_notebook(config, results_path, [], output)
    """
    # Load results
    with open(results_path, 'r') as f:
        results = json.load(f)

    # Initialize cells list
    cells = []

    # Compute relative paths for notebook code if provided
    if notebook_relative_path is not None:
        # If notebook_relative_path is empty string, use just filename
        # Otherwise use notebook_relative_path/filename
        if notebook_relative_path:
            results_path_notebook = Path(notebook_relative_path) / results_path.name
            vegalite_files_notebook = [Path(notebook_relative_path) / f.name for f in vegalite_files]
        else:
            results_path_notebook = Path(results_path.name)
            vegalite_files_notebook = [Path(f.name) for f in vegalite_files]
    else:
        results_path_notebook = results_path
        vegalite_files_notebook = vegalite_files

    # 1. Add metadata cell
    cells.append(create_metadata_cell(config, results))

    # 2. Add failure report if there are failures
    failed_instances = results.get("failed_instances", [])
    if failed_instances:
        cells.append(get_failure_report_cell(failed_instances))

    # 3. Add data loading cell
    cells.append(create_data_loading_cell(results_path_notebook))

    # 4. Add statistics cell
    # Extract measurement names from config
    measurements = [m.get("name", "") for m in config.get("measurements", [])]
    cells.append(create_statistics_cell(measurements))

    # 5. Add visualization cells
    for vegalite_file in vegalite_files_notebook:
        # Extract plot name from filename (e.g., "plot_runtime.vl.json" -> "runtime")
        plot_name = vegalite_file.stem.replace("plot_", "").replace(".vl", "")
        cells.append(create_visualization_cell(plot_name, vegalite_file))

    # Create notebook structure (nbformat v4)
    kernelspec = {
        "display_name": "Python 3",
        "language": "python",
        "name": "python3"
    }

    notebook = {
        "cells": cells,
        "metadata": {
            "kernelspec": kernelspec,
            "language_info": {
                "name": "python",
                "version": "3.13.0"
            }
        },
        "nbformat": 4,
        "nbformat_minor": 5
    }

    # Write notebook to file
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with open(output_path, 'w') as f:
        json.dump(notebook, f, indent=2)

    logger.info(f"Generated notebook with {len(cells)} cells: {output_path}")

    return output_path


def execute_notebook(notebook_path: Path, timeout: int = 300) -> None:
    """
    Execute notebook using nbconvert.

    Command:
        python3 -m nbconvert --execute --inplace <notebook_path>

    Algorithm:
        1. Check if venv_notebooks exists (has all dependencies)
        2. If yes, use venv Python; otherwise use system python3
        3. Construct command using -m nbconvert
        4. Execute subprocess.run()
        5. If returncode != 0: raise RuntimeError with helpful message
        6. Log success

    Args:
        notebook_path: Path to .ipynb file to execute
        timeout: Execution timeout in seconds (default: 300)

    Raises:
        RuntimeError: If notebook execution fails
        FileNotFoundError: If jupyter not found

    Error Handling:
        - jupyter not found: Raise with install hint
        - Execution timeout: Raise with timeout context
        - Cell error: Raise with cell error info

    Examples:
        >>> # execute_notebook(Path("/tmp/notebook.ipynb"))
    """
    # Ensure sys.executable is used for nbconvert to respect the active venv
    python_executable = sys.executable

    # Construct command to run nbconvert within Spack environment using cached env
    framework_dir = Path(__file__).parent.parent.parent  # tools/experiment_framework/notebook.py -> repo root
    env_cached = framework_dir / ".env.cached"

    # Use cached environment snapshot for fast Spack environment activation
    if env_cached.exists():
        # Run within cached Spack environment with explicit kernel
        activation_script = f"""
source {env_cached}
"{python_executable}" -m nbconvert --execute --inplace --ExecutePreprocessor.timeout={timeout} --ExecutePreprocessor.kernel_name=python3 {notebook_path}
"""
        cmd = ["bash", "-c", activation_script]
        logger.info(f"Executing notebook with Spack cached environment: {notebook_path}")
        logger.debug(f"Using cached Spack environment: {env_cached}")
    else:
        # Fall back to direct execution
        cmd = [
            python_executable, "-m", "nbconvert",
            "--execute",
            "--inplace",
            f"--ExecutePreprocessor.timeout={timeout}",
            str(notebook_path)
        ]
        logger.info(f"Executing notebook: {notebook_path}")
        logger.debug(f"Fallback: No cached Spack environment at {env_cached}")

    logger.info(f"Notebook execution timeout: {timeout}s")
    logger.debug(f"Execution command: {' '.join(cmd)}")

    try:
        # Execute with timeout + 10s buffer for nbconvert overhead
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=timeout + 10
        )

        if result.returncode != 0:
            logger.error(f"Notebook execution failed with exit code {result.returncode}")
            error_msg = f"Notebook execution failed (exit code {result.returncode})"
            if result.stderr:
                logger.error(f"Stderr: {result.stderr[:500]}")
                error_msg += f"\nStderr: {result.stderr}"
            if result.stdout:
                logger.debug(f"Stdout: {result.stdout[:500]}")
                error_msg += f"\nStdout: {result.stdout}"

            # Check for common issues
            if "timeout" in result.stderr.lower():
                error_msg += f"\n\nExecution timed out after {timeout}s. Try increasing --timeout."
            elif "not found" in result.stderr.lower() or "command not found" in result.stderr.lower():
                error_msg += "\n\nJupyter not found. Install with: pip install jupyter nbconvert"

            raise RuntimeError(error_msg)

        logger.info(f"Successfully executed notebook: {notebook_path}")

    except FileNotFoundError as e:
        logger.error(f"jupyter/nbconvert not found: {e}")
        raise FileNotFoundError(
            "jupyter command not found. Install Jupyter with:\n"
            "  pip install jupyter nbconvert\n"
            "Or skip notebook execution with --skip-notebook-execution"
        )
    except subprocess.TimeoutExpired as e:
        logger.error(f"Notebook execution timed out after {timeout + 10}s")
        logger.error(f"Notebook path: {notebook_path}")
        raise RuntimeError(
            f"Notebook execution timed out after {timeout + 10}s.\n"
            f"Try increasing timeout with a longer value."
        )
