"""
IaRa Unified Experiment Framework

This package provides a comprehensive framework for declarative, parameterized
experiment generation, execution, and visualization for the IaRa compiler project.
"""

from .config import (
    load_experiments_yaml,
    ConfigError,
    ValidationError,
    compute_yaml_hash,
    check_yaml_changed,
    store_yaml_hash,
    get_parameter_combinations,
)
from .generator import generate_cmakelists
from .builder import (
    build_all_instances,
    build_instance,
    parse_time_output,
    get_binary_size,
    BuildResult,
    BuildResults,
    read_build_results,
    update_instance_execution,
    write_build_results,
)
from .collector import collect_all_measurements
from .visualizer import generate_vegalite_json, generate_html_from_vegalite
from .notebook import (
    extract_failure_summary,
    generate_failure_markdown,
    get_failure_report_cell,
    create_metadata_cell,
    create_data_loading_cell,
    create_statistics_cell,
    create_visualization_cell,
    generate_notebook,
    execute_notebook,
)

__version__ = "1.0.0"

__all__ = [
    "load_experiments_yaml",
    "ConfigError",
    "ValidationError",
    "compute_yaml_hash",
    "check_yaml_changed",
    "store_yaml_hash",
    "get_parameter_combinations",
    "generate_cmakelists",
    "build_all_instances",
    "build_instance",
    "parse_time_output",
    "get_binary_size",
    "BuildResult",
    "BuildResults",
    "read_build_results",
    "update_instance_execution",
    "write_build_results",
    "collect_all_measurements",
    "generate_vegalite_json",
    "generate_html_from_vegalite",
    "extract_failure_summary",
    "generate_failure_markdown",
    "get_failure_report_cell",
    "create_metadata_cell",
    "create_data_loading_cell",
    "create_statistics_cell",
    "create_visualization_cell",
    "generate_notebook",
    "execute_notebook",
]
