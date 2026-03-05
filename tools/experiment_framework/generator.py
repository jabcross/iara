"""
CMakeLists.txt generation module for IaRa Unified Experiment Framework.

This module generates CMakeLists.txt files and supporting CMake code from
experiments.yaml configurations.
"""

import logging
import re
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Any

from .config import (
    load_experiments_yaml,
    compute_yaml_hash,
    check_yaml_changed,
    store_yaml_hash,
    get_parameter_combinations,
    ConfigError,
)
from .progress import ProgressBar

logger = logging.getLogger(__name__)



def generate_instance_name(
    app_name: str,
    set_name: str,
    params: Dict[str, Any],
    computed_param_names: set = None,
    param_labels: Dict[str, str] = None
) -> str:
    """
    Generate instance name following naming convention.

    Format:
        <XX-app-name>_<set-name>_<scheduler-value>_<param-name-1>_<value-1>_...

    Rules:
        - Application name must start with two digits
        - Set name follows app name
        - Scheduler value only (no "scheduler=" prefix)
        - All other parameters include name and value
        - Parameters separated by underscores
        - Values sanitized (spaces → hyphens, special chars removed)
        - Parameter names use underscores (matrix_size)
        - Computed parameters are excluded from instance name

    Args:
        app_name: Application name (e.g., "05-cholesky")
        set_name: Experiment set name (e.g., "quick", "vs-preesm")
        params: Parameter dictionary with parameter names and values
        computed_param_names: Optional set of computed parameter names to exclude

    Returns:
        Generated instance name string

    Example:
        app_name = "05-cholesky"
        set_name = "quick"
        params = {"scheduler": "vf-omp", "matrix_size": 2048, "num_blocks": 4, "block_size": 512}
        computed_param_names = {"block_size"}
        → "05-cholesky_quick_vf-omp_matrix_size_2048_num_blocks_4"
    """
    if computed_param_names is None:
        computed_param_names = set()

    # Start with app name and set name
    parts = [app_name, set_name]

    # Add scheduler value (without "scheduler=" prefix)
    scheduler = str(params.get('scheduler', '')).strip()
    if scheduler:
        parts.append(scheduler)

    # Add other parameters (sorted, excluding scheduler and computed params)
    for param_name in sorted(params.keys()):
        if param_name == 'scheduler' or param_name in computed_param_names:
            continue

        value = params[param_name]

        # Get label for instance naming: use label (lowercased, spaces→hyphens)
        # or fall back to lowercase of parameter name
        if param_labels and param_name in param_labels:
            label_str = param_labels[param_name].lower().replace(' ', '-')
            label_str = ''.join(c for c in label_str if c.isalnum() or c == '-')
        else:
            label_str = param_name.lower()

        # Sanitize value: replace spaces with hyphens, keep alphanumeric and hyphens
        value_str = str(value).strip().lower()
        value_str = value_str.replace(' ', '-')
        value_str = ''.join(c for c in value_str if c.isalnum() or c == '-')

        parts.append(f"{label_str}_{value_str}")

    return '_'.join(parts)


def _get_scheduler_define(scheduler: str) -> str:
    """
    Get the CMake define for a scheduler.

    Args:
        scheduler: Scheduler name (e.g., "vf-omp", "sequential")

    Returns:
        CMake define name (e.g., "SCHEDULER_IARA", "SCHEDULER_SEQUENTIAL")
    """
    if scheduler.startswith('vf-'):
        return 'SCHEDULER_IARA=1'
    elif scheduler.startswith('enkits-'):
        return 'SCHEDULER_ENKITS=1'
    elif scheduler == 'sequential':
        return 'SCHEDULER_SEQUENTIAL=1'
    elif scheduler.startswith('omp-'):
        # Distinguish between omp-for and omp-task
        if 'task' in scheduler:
            return 'SCHEDULER_OMP_TASK=1'
        else:
            return 'SCHEDULER_OMP_FOR=1'
    elif scheduler == 'preesm':
        return 'SCHEDULER_PREESM=1'
    else:
        # Unknown scheduler, try to infer
        logger.warning(f"Unknown scheduler '{scheduler}', assuming SCHEDULER_IARA")
        return 'SCHEDULER_IARA=1'


def generate_cmake_instance(
    instance_name: str,
    params: Dict[str, Any],
    config: Dict[str, Any],
    experiment_set: str
) -> str:
    """
    Generate CMake code for one test instance.

    Args:
        instance_name: Generated instance name
        params: Parameter dictionary for this instance
        config: Complete configuration dictionary
        experiment_set: Name of the experiment set

    Returns:
        CMake code string with iara_add_test_instance() call

    Example output:
        iara_add_test_instance(
            NAME "05-cholesky_vf-omp_matrix_size_2048_num_blocks_2"
            EXPERIMENT_SET "regression_test"
            APPLICATION_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.."
            ENTRY "05-cholesky"
            SCHEDULER "vf-omp"
            PARAMETERS "matrix_size=2048" "num_blocks=2"
            BUILD_DIR "${CMAKE_BINARY_DIR}/05-cholesky/regression_test/..."
            DEFINES "MATRIX_SIZE=2048" "NUM_BLOCKS=2" "SCHEDULER_IARA"
            LINKER_ARGS "-lopenblas" "-lm"
            TIMEOUT "300"
        )
    """
    # Extract application entry name
    app_name = config.get('application', {}).get('name', 'unknown')
    entry = config.get('application', {}).get('entry', app_name)
    main_actor = config.get('application', {}).get('build', {}).get('main_actor', 'run')

    # Scheduler
    scheduler = str(params.get('scheduler', 'sequential'))

    # Build directory - use CMAKE_BINARY_DIR directly
    # The build system configures cmake with -B <instance-specific-dir> for each instance,
    # so CMAKE_BINARY_DIR is already set to the correct per-instance location.
    # We use "${CMAKE_BINARY_DIR}" to refer to that location.
    build_dir = "${CMAKE_BINARY_DIR}"

    # Application directory - use relative path from experiment directory
    # The experiment directory is applications/<app>/experiment/
    # The application source is at applications/<app>/ (one level up)
    # iara_add_test_instance will use this as: ${CMAKE_SOURCE_DIR}/${app_dir}
    # When CMAKE_SOURCE_DIR is the experiment dir, this gives: experiment/../ = parent dir
    app_dir = "../"

    # Parameters list (name=value for non-computed params)
    # Identify computed parameters to exclude from PARAMETERS list
    computed_param_names = set()
    for comp in config.get('computed_parameters', []):
        computed_param_names.add(comp.get('name'))

    parameters_list = []
    for param_name in sorted(params.keys()):
        if param_name == 'scheduler' or param_name in computed_param_names:
            continue
        value = params[param_name]
        parameters_list.append(f'"{param_name}={value}"')

    # DEFINES: All parameters as uppercase, computed parameters, and scheduler define
    defines_list = []

    # Add all parameters as defines (parameter names are already ALL_CAPS)
    for param_name in sorted(params.keys()):
        if param_name == 'scheduler':
            continue
        value = params[param_name]
        defines_list.append(f'"{param_name}={value}"')

    # Add scheduler define
    scheduler_define = _get_scheduler_define(scheduler)
    defines_list.append(f'"{scheduler_define}"')

    # Add application-specific defines
    app_defines = config.get('application', {}).get('build', {}).get('defines', [])
    for define in app_defines:
        if isinstance(define, str):
            # Simple string define like "VERBOSE"
            defines_list.append(f'"{define}"')
        elif isinstance(define, dict):
            # Define with value like {"DEBUG_LEVEL": "2"}
            for key, val in define.items():
                defines_list.append(f'"{key}={val}"')


    # LINKER_ARGS
    linker_args = config.get('application', {}).get('build', {}).get('extra_linker_args', [])
    linker_args_list = [f'"{arg}"' for arg in linker_args]

    # TIMEOUT
    timeout = config.get('execution', {}).get('timeout', 300)

    # Build the CMake code - wrap in conditional to only build selected instance
    # The build system will pass -DIARA_EXPERIMENT_INSTANCE=<instance_name>
    # Only the matching instance will be configured
    lines = [
        f"# Instance: {instance_name}",
        f'if(NOT DEFINED IARA_EXPERIMENT_INSTANCE OR IARA_EXPERIMENT_INSTANCE STREQUAL "{instance_name}")',
        "iara_add_test_instance(",
        f'    NAME "{instance_name}"',
        f'    EXPERIMENT_SET "{experiment_set}"',
        f'    APPLICATION_DIR "{app_dir}"',
        f'    ENTRY "{entry}"',
        f'    SCHEDULER "{scheduler}"',
        f'    MAIN_ACTOR "{main_actor}"',
    ]

    # Add PARAMETERS if present
    if parameters_list:
        lines.append(f"    PARAMETERS {' '.join(parameters_list)}")

    # Add remaining fields
    lines.extend([
        f'    BUILD_DIR "{build_dir}"',
        f"    DEFINES {' '.join(defines_list)}" if defines_list else f'    DEFINES ""',
    ])

    # Add LINKER_ARGS if present
    if linker_args_list:
        lines.append(f"    LINKER_ARGS {' '.join(linker_args_list)}")

    lines.append(")")
    lines.append("endif()")

    return '\n'.join(lines)



def generate_cmakelists(
    yaml_path: Path,
    output_path: Path,
    experiment_set: str
) -> int:
    """
    Generate complete CMakeLists.txt for an experiment set.

    Process:
        1. Load and validate YAML
        2. Check hash (warn if changed, don't fail in Phase 1)
        3. Generate parameter combinations
        4. Generate CMake code for each instance
        5. Write CMakeLists.txt with header and all instances

    Args:
        yaml_path: Path to experiments.yaml
        output_path: Path where CMakeLists.txt should be written
        experiment_set: Name of the experiment set to generate for

    Returns:
        Number of instances generated

    Raises:
        ConfigError: If YAML is invalid
        ValidationError: If schema validation fails
    """
    # Step 1: Load and validate YAML
    logger.debug(f"Loading YAML from {yaml_path}")
    config = load_experiments_yaml(yaml_path)

    # Step 2: Check hash (only warn in Phase 1, don't block)
    hash_file = yaml_path.parent / '.experiments.yaml.hash'
    if check_yaml_changed(yaml_path, hash_file):
        logger.warning(
            f"YAML content has changed since last generation. "
            f"Consider running 'generate' to update hash."
        )

    # Step 3: Generate parameter combinations
    logger.debug(f"Generating parameter combinations for experiment set '{experiment_set}'")
    combinations = get_parameter_combinations(config, experiment_set)
    logger.info(f"Generated {len(combinations)} parameter combinations")

    # Step 4: Generate CMake code for each instance
    app_name = config.get('application', {}).get('name')
    if not app_name:
        # Try to extract from yaml_path (should be applications/<app>/experiment/experiments.yaml)
        app_name = yaml_path.parent.parent.name

    # Get computed parameter names to exclude from instance naming
    computed_param_names = set()
    for comp in config.get('computed_parameters', []):
        computed_param_names.add(comp.get('name'))

    # Build param_labels dict for human-readable instance naming
    param_labels = {}
    for param in config.get('parameters', []):
        if 'label' in param:
            param_labels[param['name']] = param['label']

    # Generate CMake code with progress tracking
    cmake_code_blocks = []
    progress = ProgressBar(len(combinations), "Generating")
    for combo in combinations:
        instance_name = generate_instance_name(app_name, experiment_set, combo, computed_param_names, param_labels)
        cmake_code = generate_cmake_instance(instance_name, combo, config, experiment_set)
        cmake_code_blocks.append(cmake_code)
        progress.update()

    # Step 5: Compute hash and timestamp
    yaml_hash = compute_yaml_hash(yaml_path)
    timestamp = datetime.utcnow().strftime("%Y-%m-%d %H:%M:%S UTC")

    # Step 6: Build CMakeLists.txt content with header
    header_lines = [
        "# Auto-generated by tools.experiment_framework.generator",
        f"# Source: {_get_relative_path(yaml_path)}",
        f"# Experiment Set: {experiment_set}",
        f"# Generated: {timestamp}",
        f"# YAML Hash: {yaml_hash}",
        "# DO NOT EDIT - Regenerate with: python3 -m tools.experiment_framework generate",
        "",
        "cmake_minimum_required(VERSION 3.20)",
        "",
        "# Set compilers to use LLVM/Clang toolchain (must be before project() call)",
        "if(DEFINED ENV{LLVM_INSTALL})",
        "    set(CMAKE_C_COMPILER \"$ENV{LLVM_INSTALL}/bin/clang\")",
        "    set(CMAKE_CXX_COMPILER \"$ENV{LLVM_INSTALL}/bin/clang++\")",
        "endif()",
        "",
        "# Project definition - must come after compiler setup but names are relative to this file",
        "project(iara-experiment-instances LANGUAGES C CXX)",
        "",
        "# For IaRa build system compatibility: set PROJECT_SOURCE_DIR to point to repo root",
        "# (project() above sets it to the experiment directory, but we need the repo root)",
        "# Compute absolute path to repo root: experiment dir is applications/<app>/experiment/",
        "# so repo root is 3 levels up: ../../..",
        "get_filename_component(REPO_ROOT \"${CMAKE_CURRENT_SOURCE_DIR}/../../..\" ABSOLUTE)",
        "set(PROJECT_SOURCE_DIR \"${REPO_ROOT}\")",
        "",
        "include(${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/IaRaApplications.cmake)",
        "",
        "enable_testing()",
        "",
    ]

    # Combine all parts
    all_lines = header_lines + [""] + ["\n".join([f"# Instance {i+1}"]) + "\n" + block
                                        for i, block in enumerate(cmake_code_blocks)]

    content = '\n'.join(header_lines) + '\n'.join([
        f"\n# Instance {i+1}\n{block}"
        for i, block in enumerate(cmake_code_blocks)
    ])

    # Step 7: Write to file
    logger.debug(f"Writing CMakeLists.txt to {output_path}")
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(content + '\n')

    # Store hash for future comparisons
    store_yaml_hash(yaml_path, hash_file)

    logger.info(f"Generated CMakeLists.txt with {len(combinations)} instances")
    return len(combinations)


def _get_relative_path(path: Path) -> str:
    """
    Get a relative path for a file, relative to repo root if possible.

    For paths like /path/to/iara/applications/05-cholesky/experiment/experiments.yaml,
    returns "applications/05-cholesky/experiment/experiments.yaml"
    """
    try:
        # Try to find 'applications' in the path
        parts = path.parts
        if 'applications' in parts:
            idx = parts.index('applications')
            return '/'.join(parts[idx:])
        return str(path)
    except Exception:
        return str(path)
