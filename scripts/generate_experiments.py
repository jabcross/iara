#!/usr/bin/env python3
"""
Generate CMakeLists.txt files from experiments.yaml configurations.

This script processes all applications/*/experiment/experiments.yaml files and
generates one CMakeLists.txt per application that encodes all test instances
for experiment sets marked with generate_tests: true.

The generated CMakeLists.txt files use the iara_add_test_instance() CMake function
to define all build-* and run-* test targets.
"""

import sys
import os
import yaml
import itertools
from pathlib import Path
from typing import Dict, List, Any, Tuple, Optional


def find_yaml_files(iara_dir: Path) -> List[Path]:
    """Find all experiments.yaml files in applications/*/experiment/."""
    yaml_files = []
    apps_dir = iara_dir / "applications"

    if not apps_dir.exists():
        print(f"ERROR: applications directory not found at {apps_dir}", file=sys.stderr)
        return []

    for app_dir in sorted(apps_dir.iterdir()):
        if not app_dir.is_dir():
            continue

        yaml_file = app_dir / "experiment" / "experiments.yaml"
        if yaml_file.exists():
            yaml_files.append(yaml_file)

    return yaml_files


def parse_yaml_config(yaml_path: Path) -> Optional[Dict[str, Any]]:
    """Load and parse a YAML configuration file."""
    try:
        with open(yaml_path, 'r') as f:
            return yaml.safe_load(f)
    except Exception as e:
        print(f"ERROR: Failed to parse {yaml_path}: {e}", file=sys.stderr)
        return None


def get_computed_params(config: Dict[str, Any]) -> Dict[str, str]:
    """Extract computed parameter expressions from config."""
    computed = {}
    for param in config.get('computed_parameters', []):
        computed[param['name']] = param['expression']
    return computed


def compute_derived_params(combo: Dict[str, Any], computed_params: Dict[str, str]) -> Dict[str, Any]:
    """Compute derived parameters based on expressions."""
    derived = {}
    for param_name, expression in computed_params.items():
        try:
            value = eval(expression, {}, combo)
            derived[param_name] = value
        except Exception as e:
            print(f"Warning: Error computing {param_name}: {e}", file=sys.stderr)
    return derived


def validate_combination(params: Dict[str, Any], constraints: List[Dict[str, str]]) -> bool:
    """Check if a parameter combination satisfies all constraints."""
    for constraint in constraints:
        expression = constraint['expression']
        try:
            if not eval(expression, {}, params):
                return False
        except Exception as e:
            print(f"Warning: Error evaluating constraint '{expression}': {e}", file=sys.stderr)
            return False
    return True


def generate_combinations(exp_set: Dict[str, Any], constraints: List[Dict[str, str]],
                         computed_params: Dict[str, str]) -> List[Dict[str, Any]]:
    """Generate all valid parameter combinations for an experiment set."""
    params = exp_set['parameters']
    param_names = list(params.keys())
    param_values = [params[name] for name in param_names]

    combinations = []
    for combo_tuple in itertools.product(*param_values):
        combo = dict(zip(param_names, combo_tuple))

        # Compute derived parameters before validating constraints
        derived = compute_derived_params(combo, computed_params)
        combo.update(derived)

        # Validate against constraints
        if validate_combination(combo, constraints):
            combinations.append(combo)

    return combinations


def generate_instance_name(app_name: str, params: Dict[str, Any], param_order: List[str], exp_set_name: str = None) -> str:
    """
    Generate instance name following the naming convention.

    Format: <XX-app-name>_<experiment-set>_<scheduler>_<param-name-1>_<param-value-1>_...

    Note: scheduler shows only the value, not the name.
    """
    parts = [app_name]

    # Add experiment set name if provided
    if exp_set_name:
        parts.append(exp_set_name)

    # Add scheduler first if it exists
    if 'scheduler' in params:
        parts.append(str(params['scheduler']))

    # Add remaining parameters in order (excluding scheduler)
    for param_name in param_order:
        if param_name != 'scheduler' and param_name in params:
            parts.append(param_name)
            parts.append(str(params[param_name]))

    return '_'.join(parts)


def generate_cmake_call(instance: Dict[str, Any], config: Dict[str, Any],
                       experiment_set_name: str, app_name: str) -> str:
    """Generate a single iara_add_test_instance() CMake call."""

    # Extract application configuration
    app_config = config.get('application', {})
    build_config = app_config.get('build', {})

    # Extract instance information
    instance_name = instance['name']
    scheduler = instance['params'].get('scheduler', '')

    # Build directory path (relative to CMAKE_BINARY_DIR, which is already set to build_experiments)
    build_dir = f"${{CMAKE_BINARY_DIR}}/{app_name}/{experiment_set_name}/{instance_name}"

    # Collect defines
    defines = list(build_config.get('defines', []))

    # Add parameter defines
    param_metadata = config.get('parameter_metadata', {})
    for param_name, param_value in instance['params'].items():
        if param_name in param_metadata:
            export_as = param_metadata[param_name].get('export_as')
            if export_as:
                defines.append(f"{export_as}={param_value}")
        else:
            # Fallback: use uppercase parameter name
            defines.append(f"{param_name.upper()}={param_value}")

    # Collect linker args
    linker_args = list(build_config.get('extra_linker_args', []))
    linker_args_str = ' '.join(linker_args) if linker_args else ''

    # Build parameters list for CMake
    params_cmake = []
    for param_name, param_value in instance['params'].items():
        params_cmake.append(f'"{param_name}={param_value}"')

    defines_str = ';'.join(defines) if defines else ''

    cmake_code = f"""
iara_add_test_instance(
    NAME "{instance_name}"
    EXPERIMENT_SET "{experiment_set_name}"
    APPLICATION_DIR "applications/{app_name}"
    ENTRY "{app_name}"
    SCHEDULER "{scheduler}"
    PARAMETERS {' '.join(params_cmake)}
    BUILD_DIR "{build_dir}"
    DEFINES "{defines_str}"
    LINKER_ARGS "{linker_args_str}"
)
"""

    return cmake_code


def extract_app_dir_from_name(app_name: str) -> str:
    """Extract application directory name from entry name.

    Examples:
    - "05-cholesky" -> "cholesky"
    - "03-two-nodes" -> "03-two-nodes"
    """
    # If it starts with digits, strip them and the hyphen
    if app_name and app_name[0].isdigit():
        parts = app_name.split('-', 1)
        if len(parts) == 2:
            return parts[1]
    return app_name


def generate_cmake_file(yaml_path: Path, config: Dict[str, Any]) -> Optional[Tuple[Path, str]]:
    """
    Generate CMakeLists.txt content for an application.

    Returns (output_path, cmake_content) or None if no tests to generate.
    """
    # Get application configuration
    app_config = config.get('application', {})
    # Use application.name which matches the directory name for CMake source discovery
    app_name = app_config.get('name')

    if not app_name:
        print(f"Warning: No 'name' found in {yaml_path}", file=sys.stderr)
        return None

    # Get all parameter information
    parameters = config.get('parameters', [])
    param_names = [p['name'] for p in parameters]
    constraints = config.get('constraints', [])
    computed_params = get_computed_params(config)

    # Process each experiment set
    all_instances = []
    experiment_sets = config.get('experiment_sets', [])

    for exp_set in experiment_sets:
        if not exp_set.get('generate_tests', False):
            continue

        exp_set_name = exp_set.get('name', 'default')

        # Generate all combinations for this experiment set
        combinations = generate_combinations(exp_set, constraints, computed_params)

        if not combinations:
            print(f"Warning: No valid combinations for {app_name}/{exp_set_name}", file=sys.stderr)
            continue

        # Create instance entries for each combination
        for combo in combinations:
            instance_name = generate_instance_name(app_name, combo, param_names, exp_set_name)
            all_instances.append({
                'name': instance_name,
                'params': combo,
                'experiment_set': exp_set_name
            })

    if not all_instances:
        return None

    # Generate CMakeLists.txt content
    cmake_content = f"""# Auto-generated by generate_experiments.py
# Application: {app_name}
# Generated from: {yaml_path.relative_to(yaml_path.parent.parent.parent)}

include(${{CMAKE_SOURCE_DIR}}/cmake/IaRaApplications.cmake)

"""

    # Group instances by experiment set for clarity
    for exp_set in experiment_sets:
        if not exp_set.get('generate_tests', False):
            continue

        exp_set_name = exp_set.get('name', 'default')
        set_instances = [i for i in all_instances if i['experiment_set'] == exp_set_name]

        if set_instances:
            cmake_content += f"\n# Experiment set: {exp_set_name}\n"

            for instance in set_instances:
                cmake_code = generate_cmake_call(instance, config, exp_set_name, app_name)
                cmake_content += cmake_code

    # Add CTest configuration for sequential execution without parallelization
    cmake_content += """
# ==============================================================================
# CTest Configuration - Sequential Test Execution
# ==============================================================================

# Disable test parallelization to ensure accurate compilation timing
# (parallel builds can interfere with per-test compilation measurements)
set_property(GLOBAL PROPERTY CTEST_LAUNCH_RULE "")

# Set CTest to run tests serially, not in parallel
get_property(test_names DIRECTORY PROPERTY TESTS)
if(test_names)
    set_property(GLOBAL PROPERTY CTEST_PROCESSES 1)
endif()
"""

    # Output path
    output_path = yaml_path.parent / "CMakeLists.txt"

    return (output_path, cmake_content)


def matches_filter(name: str, filter_pattern: str) -> bool:
    """Check if name matches the filter pattern.

    Filter can be:
    - Empty/None: matches everything
    - Substring: matches if contained in name
    - Regex pattern: matches if regex matches
    """
    if not filter_pattern:
        return True

    # Simple substring matching for now
    return filter_pattern in name


def main():
    """Main entry point."""
    # Get IARA_DIR from environment or current directory
    iara_dir = Path(os.environ.get('IARA_DIR', '.'))

    if not iara_dir.exists():
        print(f"ERROR: IARA_DIR not found at {iara_dir}", file=sys.stderr)
        sys.exit(1)

    # Get filter patterns from environment
    experiments_filter = os.environ.get('IARA_EXPERIMENTS_FILTER', '')
    experiment_sets_filter = os.environ.get('IARA_EXPERIMENT_SETS_FILTER', '')

    if experiments_filter:
        print(f"Filtering experiments by: {experiments_filter}")
    if experiment_sets_filter:
        print(f"Filtering experiment sets by: {experiment_sets_filter}")

    # Find all experiments.yaml files
    yaml_files = find_yaml_files(iara_dir)

    if not yaml_files:
        print("WARNING: No experiments.yaml files found", file=sys.stderr)
        return

    print(f"Found {len(yaml_files)} experiments.yaml file(s)")

    generated_count = 0

    # Process each YAML file
    for yaml_path in yaml_files:
        config = parse_yaml_config(yaml_path)
        if not config:
            continue

        app_name = config.get('application', {}).get('name')
        if not app_name:
            continue

        # Check if this application matches the filter
        if not matches_filter(app_name, experiments_filter):
            print(f"Skipping {app_name} (filtered by experiments filter)")
            continue

        print(f"Processing {yaml_path.relative_to(iara_dir)}")

        # Filter experiment sets before generation
        experiment_sets = config.get('experiment_sets', [])
        if experiment_sets_filter:
            filtered_sets = [s for s in experiment_sets if matches_filter(s.get('name', ''), experiment_sets_filter)]
            if not filtered_sets:
                print(f"  -> No experiment sets match filter", file=sys.stderr)
                continue
            config['experiment_sets'] = filtered_sets

        result = generate_cmake_file(yaml_path, config)
        if not result:
            print(f"  -> No tests to generate", file=sys.stderr)
            continue

        output_path, cmake_content = result

        try:
            with open(output_path, 'w') as f:
                f.write(cmake_content)
            print(f"  -> Generated {output_path.relative_to(iara_dir)}")
            generated_count += 1
        except Exception as e:
            print(f"ERROR: Failed to write {output_path}: {e}", file=sys.stderr)
            sys.exit(1)

    print(f"Successfully generated {generated_count} CMakeLists.txt file(s)")


if __name__ == '__main__':
    main()
