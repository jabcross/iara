#!/usr/bin/env python3
"""
Base experiment runner - generic functions shared across all experiment types.

This module provides all the framework-level functionality for running experiments:
- Configuration loading
- Parameter validation and combination generation
- CMake build orchestration
- Executable discovery and execution
- Measurement parsing and CSV writing

App-specific runners should inherit from this module's classes.
"""

import sys
import os
import glob
import subprocess
import csv
import yaml
import itertools
import argparse
import time
import re
from pathlib import Path
from datetime import datetime
from typing import Dict, List, Any, Tuple, Optional

# Add spinner-fork to sys.path for config loading
spinner_path = Path(__file__).parent.parent.parent.parent / 'spinner-fork'
sys.path.insert(0, str(spinner_path))

try:
    from spinner.config_loader import load_config
except ImportError:
    load_config = None


def load_yaml_config(yaml_path: Path) -> Dict[str, Any]:
    """Load and parse the experiments.yaml file."""
    with open(yaml_path, 'r') as file:
        return yaml.safe_load(file)


def get_computed_params(config: Dict[str, Any]) -> Dict[str, str]:
    """Extract computed parameter expressions."""
    computed = {}
    for param in config.get('computed_parameters', []):
        computed[param['name']] = param['expression']
    return computed


def validate_combination(params: Dict[str, Any], constraints: List[Dict[str, str]]) -> bool:
    """Check if a parameter combination satisfies all constraints."""
    for constraint in constraints:
        expression = constraint['expression']
        try:
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
            value = eval(expression, {}, combo)
            derived[param_name] = value
        except Exception as e:
            print(f"Error computing {param_name}: {e}", file=sys.stderr)
    return derived


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


def setup_cmake_build(app_filter: Optional[str] = None, build_dir='build_experiments') -> bool:
    """Configure and build all test instances."""
    # Always reconfigure CMake when new test directories are present
    # (test directories created by runners may have changed since last configure)

    print(f"Configuring CMake in {build_dir}...")
    try:
        cmd = ['cmake', '-B', build_dir, '-DCMAKE_BUILD_TYPE=Release', '-DIARA_BUILD_TESTS=ON', '.']

        env = os.environ.copy()
        if app_filter:
            env['IARA_TEST_FILTER'] = app_filter

        subprocess.run(cmd, check=True, timeout=600, env=env)

        print("Building test instances...")
        result = subprocess.run(
            ['cmake', '--build', build_dir, '--parallel'],
            capture_output=True,
            text=True,
            timeout=1800
        )

        # If build failed, check if at least the filtered app's tests built
        if result.returncode != 0:
            print(f"WARNING: Build completed with errors")
            if app_filter:
                # Check if any executables for the filtered app were built
                pattern = os.path.join(build_dir, app_filter, '**', 'a.out')
                import glob
                matches = glob.glob(pattern, recursive=True)
                if matches:
                    print(f"  Found {len(matches)} {app_filter} executables - continuing with execution")
                    return True
                else:
                    print(f"  No {app_filter} executables found - build failed")
                    return False
            else:
                # If no filter, require successful build
                return False

        return True
    except subprocess.CalledProcessError as e:
        print(f"ERROR: Build failed: {e}", file=sys.stderr)
        return False
    except Exception as e:
        print(f"ERROR: {e}", file=sys.stderr)
        return False


def find_executable(test_name: str, build_dir: str = 'build_experiments') -> str:
    """Find the compiled executable for a test instance."""
    # Try multiple search patterns for flexibility
    patterns = [
        # New CMake test structure (generate_experiments.py)
        os.path.join(build_dir, '**', test_name, 'a.out'),
        # Old test/Iara structure (legacy)
        os.path.join(build_dir, 'test', 'Iara', f'{test_name}*', 'build-*', 'a.out'),
    ]

    for pattern in patterns:
        matches = glob.glob(pattern, recursive=True)
        if matches:
            return matches[0]

    # Show where we searched
    search_info = '\n  '.join(patterns)
    raise FileNotFoundError(f"Executable for {test_name} not found\n  Searched:\n  {search_info}")


def get_section_sizes(binary_path: str) -> Dict[str, int]:
    """
    Run 'size -A' on binary and parse ELF section sizes.

    Returns:
        Dictionary mapping section names to sizes in bytes
    """
    try:
        result = subprocess.run(
            ['size', '-A', str(binary_path)],
            capture_output=True, text=True, timeout=5
        )
        if result.returncode != 0:
            return {}

        section_sizes = {}
        for line in result.stdout.splitlines():
            m = re.match(r'\s*(\S+)\s+(\d+)', line)
            if m:
                section = m.group(1)
                size = int(m.group(2))
                # Skip header and total line
                if section not in ['section', 'Total']:
                    section_sizes[section] = size
        return section_sizes
    except Exception:
        return {}


def run_executable(exe_path: str, env_vars: Dict[str, str], timeout: int) -> Tuple[bool, str]:
    """Run an executable with time -v and capture output."""
    try:
        env = os.environ.copy()
        env.update(env_vars)

        result = subprocess.run(
            ['/usr/bin/time', '-v', exe_path],
            capture_output=True,
            text=True,
            timeout=timeout,
            env=env
        )

        return result.returncode == 0, result.stdout + result.stderr

    except subprocess.TimeoutExpired:
        print(f"ERROR: {exe_path} timed out after {timeout}s", file=sys.stderr)
        return False, ""
    except Exception as e:
        print(f"ERROR running {exe_path}: {e}", file=sys.stderr)
        return False, str(e)


def get_measurement_unit(config: Dict[str, Any], meas_name: str) -> str:
    """Extract unit for a measurement from config."""
    for meas in config.get('measurements', []):
        if meas.get('name') == meas_name:
            return meas.get('unit', '')
    return ""


def parse_measurement_value(text: str, measurement: Dict[str, Any]) -> Optional[Any]:
    """
    Parse a measurement value from output text using configuration.

    Supports:
    - regex patterns with group extraction
    - type conversion (int, float, bool)
    - custom converter functions
    """
    parser = measurement.get('parser', {})

    if parser.get('type') == 'regex':
        pattern = parser.get('pattern', '')
        group = parser.get('group', 1)

        match = re.search(pattern, text)
        if not match:
            return None

        try:
            value = match.group(int(group))

            # Apply custom converter if specified
            converter = parser.get('converter')
            if converter:
                # For now, eval converters (should be lambda functions)
                value = eval(converter)(value)

            # Convert to specified type
            meas_type = measurement.get('type', 'float')
            if meas_type == 'float':
                return float(value)
            elif meas_type == 'int':
                return int(value)
            elif meas_type == 'bool':
                return value.lower() in ['true', 'yes', '1']
            else:
                return value
        except Exception as e:
            print(f"Error parsing measurement '{pattern}': {e}", file=sys.stderr)
            return None

    return None


def write_csv_results(results: List[Dict[str, Any]], csv_path: Path) -> None:
    """Write experiment results to CSV file."""
    if not results:
        print("WARNING: No results to write", file=sys.stderr)
        return

    # Get all unique keys across all results
    all_keys = set()
    for result in results:
        all_keys.update(result.keys())

    fieldnames = sorted(list(all_keys))

    csv_path.parent.mkdir(parents=True, exist_ok=True)

    with open(csv_path, 'w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(results)

    print(f"Results written to: {csv_path}")


def create_cli_parser() -> argparse.ArgumentParser:
    """Create and return the standard CLI argument parser for experiment runners."""
    parser = argparse.ArgumentParser(
        description='Run experiments from a specified experiment set',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
  python3 run_experiments.py --experiment-set dev
  python3 run_experiments.py --experiment-set full
  python3 run_experiments.py -e baseline
  python3 run_experiments.py --list  # List available experiment sets
        '''
    )
    parser.add_argument('-e', '--experiment-set',
                        dest='experiment_set',
                        help='Name of the experiment set to run')
    parser.add_argument('--list', action='store_true',
                        help='List available experiment sets and exit')

    return parser
