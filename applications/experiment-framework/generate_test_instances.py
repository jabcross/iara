#!/usr/bin/env python3
"""
Generate test instances from experiments.yaml configuration.

This script reads the experiment configuration and generates test directories
with parameters.sh files for all combinations in experiment sets marked with
generate_tests: true.
"""

import sys
import yaml
import itertools
from pathlib import Path
from typing import Dict, List, Any, Tuple


def load_config(yaml_path: Path) -> Dict[str, Any]:
    """Load and parse the experiments.yaml file."""
    with open(yaml_path, 'r') as file:
        return yaml.safe_load(file)


def extract_parameter_info(config: Dict[str, Any]) -> Tuple[List[str], Dict[str, str]]:
    """Extract parameter names and their types from config."""
    param_names = []
    param_types = {}

    for param in config.get('parameters', []):
        param_names.append(param['name'])
        param_types[param['name']] = param.get('type', 'str')

    return param_names, param_types


def get_computed_params(config: Dict[str, Any]) -> Dict[str, str]:
    """Extract computed parameter expressions."""
    computed = {}
    for param in config.get('computed_parameters', []):
        computed[param['name']] = param['expression']
    return computed


def generate_combinations(exp_set: Dict[str, Any], constraints: List[Dict[str, str]], computed_params: Dict[str, str]) -> List[Dict[str, Any]]:
    """Generate all valid parameter combinations for an experiment set."""
    params = exp_set['parameters']
    param_names = list(params.keys())
    param_values = [params[name] for name in param_names]

    # Generate all combinations using itertools.product
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


def validate_combination(params: Dict[str, Any], constraints: List[Dict[str, str]]) -> bool:
    """Check if a parameter combination satisfies all constraints."""
    for constraint in constraints:
        expression = constraint['expression']
        try:
            # Evaluate constraint with parameter values in scope
            if not eval(expression, {}, params):
                return False
        except Exception as e:
            print(f"Error evaluating constraint '{expression}': {e}", file=sys.stderr)
            return False
    return True


def compute_derived_params(combo: Dict[str, Any], computed_params: Dict[str, str]) -> Dict[str, Any]:
    """Compute derived parameters based on expressions."""
    derived = {}
    for param_name, expression in computed_params.items():
        try:
            # Evaluate expression with current parameters in scope
            value = eval(expression, {}, combo)
            derived[param_name] = value
        except Exception as e:
            print(f"Error computing {param_name}: {e}", file=sys.stderr)
    return derived


def create_test_directory(params: Dict[str, Any], test_base_path: Path, config: Dict[str, Any]) -> Path:
    """Create test directory and parameters.sh file."""
    # Create directory name from parameters
    matrix_size = params['matrix_size']
    num_blocks = params['num_blocks']
    scheduler = params['scheduler']

    dir_name = f"05-cholesky-{matrix_size}_{num_blocks}_{scheduler}"
    dir_path = test_base_path / dir_name

    # Create directory
    dir_path.mkdir(parents=True, exist_ok=True)

    # Write parameters.sh file with runtime parameters only
    # Build configuration is now handled by CMakeLists.txt generated from experiments.yaml
    params_file = dir_path / 'parameters.sh'
    with open(params_file, 'w') as f:
        # Write each parameter as an export statement
        for key, value in params.items():
            # Convert parameter names to uppercase for shell variables
            var_name = key.upper()
            f.write(f"export {var_name}={value}\n")

    return dir_path


def main():
    """Main entry point."""
    # Find experiments.yaml in applications/cholesky/experiment/
    yaml_path = Path(__file__).parent.parent / 'cholesky' / 'experiment' / 'experiments.yaml'

    if not yaml_path.exists():
        print(f"Error: {yaml_path} does not exist", file=sys.stderr)
        sys.exit(1)

    # Load configuration
    config = load_config(yaml_path)

    # Extract constraints and computed parameters
    constraints = config.get('constraints', [])
    computed_params = get_computed_params(config)

    # Determine test base directory
    test_base_path = Path(__file__).parent.parent.parent / 'test' / 'Iara'

    print(f"Generating test instances to: {test_base_path}")
    print()

    generated_dirs = []

    # Process each experiment set
    for exp_set in config.get('experiment_sets', []):
        if not exp_set.get('generate_tests', False):
            continue

        exp_name = exp_set['name']
        print(f"Processing experiment set: {exp_name}")

        # Generate valid parameter combinations
        combinations = generate_combinations(exp_set, constraints, computed_params)
        print(f"  Found {len(combinations)} valid combinations")

        # Create test directories for each combination
        for combo in combinations:

            # Create test directory
            test_dir = create_test_directory(combo, test_base_path, config)
            generated_dirs.append(test_dir)

            print(f"  Created: {test_dir.relative_to(Path(__file__).parent.parent.parent)}")

        print()

    print(f"Total directories created: {len(generated_dirs)}")
    if generated_dirs:
        print("\nGenerated test directories:")
        for dir_path in generated_dirs:
            print(f"  {dir_path}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
