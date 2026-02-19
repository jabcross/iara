#!/usr/bin/env python3
"""
App-specific experiment runner base class.

Inheriting classes must implement:
- create_test_directory(): Build test directory name and create parameters.sh/CMakeLists.txt
- get_parameter_exports(): Map parameters to environment variables

The framework handles:
- Parameter validation and combination generation
- CMake build orchestration
- Executable discovery and execution
- Measurement parsing
- CSV result writing
"""

import os
import subprocess
import time
from pathlib import Path
from typing import Dict, List, Any, Optional
from datetime import datetime

from base_runner import (
    load_yaml_config, get_computed_params, generate_combinations,
    setup_cmake_build, find_executable, run_executable, get_section_sizes,
    parse_measurement_value, write_csv_results, create_cli_parser
)


class AppExperimentRunner:
    """Base class for application-specific experiment runners."""

    def __init__(self, yaml_config_path: str, app_name: str, app_filter: str):
        """
        Initialize experiment runner.

        Args:
            yaml_config_path: Path to experiments.yaml (relative to runner location or repo root)
            app_name: Name of application (e.g., 'cholesky', 'sift')
            app_filter: CMake filter for test selection (e.g., 'cholesky', 'sift')
        """
        # Store original script directory before changing cwd
        original_script_dir = Path(__file__).parent.parent.parent  # framework -> ... -> app dir

        # Find repo root by looking for CMakeLists.txt
        self.repo_root = self._find_repo_root()

        # Change to repo root for all operations
        os.chdir(self.repo_root)

        self.yaml_config_path = Path(yaml_config_path)
        self.app_name = app_name
        self.app_filter = app_filter

        # Load YAML configuration
        if not self.yaml_config_path.exists():
            # Try relative to original script location (the runner script)
            yaml_path = original_script_dir / yaml_config_path
            if yaml_path.exists():
                self.yaml_config_path = yaml_path

        if not self.yaml_config_path.exists():
            raise FileNotFoundError(f"Config file not found: {self.yaml_config_path}")

        self.config = load_yaml_config(self.yaml_config_path)
        self.build_dir = 'build_experiments'
        self.results = []

    def _find_repo_root(self) -> Path:
        """Find the repository root by looking for CMakeLists.txt or IARA_DIR env var."""
        # First try IARA_DIR environment variable (set in .env)
        iara_dir = os.environ.get('IARA_DIR')
        if iara_dir and Path(iara_dir).is_dir():
            if (Path(iara_dir) / 'CMakeLists.txt').exists():
                return Path(iara_dir)

        # Fall back to searching from current directory
        current = Path.cwd()
        while current != current.parent:
            if (current / 'CMakeLists.txt').exists():
                return current
            current = current.parent
        # Fallback to current directory if not found
        return Path.cwd()

    def create_test_directory(self, params: Dict[str, Any]) -> Path:
        """
        Create test directory and associated configuration files.

        Subclasses must implement this to:
        1. Construct test directory name from parameters
        2. Create the directory
        3. Write parameters.sh with parameter exports
        4. Write CMakeLists.txt with build configuration

        Args:
            params: Parameter combination

        Returns:
            Path to created test directory
        """
        raise NotImplementedError(f"{self.__class__.__name__} must implement create_test_directory()")

    def get_parameter_exports(self, params: Dict[str, Any]) -> Dict[str, str]:
        """
        Map parameters to environment variables for execution.

        Subclasses must implement this to specify which parameters
        become which environment variables.

        Args:
            params: Parameter combination

        Returns:
            Dictionary mapping environment variable names to values
        """
        raise NotImplementedError(f"{self.__class__.__name__} must implement get_parameter_exports()")

    def get_test_name_from_params(self, params: Dict[str, Any]) -> str:
        """
        Get the test name prefix for finding executables.

        Default: extract from test directory name created by create_test_directory().
        Override if your test naming differs from directory naming.
        """
        test_dir = self.create_test_directory(params)
        return test_dir.name

    def run(self, experiment_set_name: Optional[str] = None) -> None:
        """
        Run a complete experiment set.

        Args:
            experiment_set_name: Name of experiment set to run
        """
        # List experiment sets if no name provided
        if not experiment_set_name:
            exp_sets = {s['name']: s for s in self.config.get('experiment_sets', [])}
            print("Available experiment sets:")
            for name, exp_set in exp_sets.items():
                desc = exp_set.get('description', 'No description')
                print(f"  {name:15s} - {desc}")
            return

        # Find specified experiment set
        exp_sets = {s['name']: s for s in self.config.get('experiment_sets', [])}
        exp_set = exp_sets.get(experiment_set_name)

        if not exp_set:
            print(f"ERROR: Unknown experiment set '{experiment_set_name}'")
            print(f"Available sets: {', '.join(sorted(exp_sets.keys()))}")
            return

        print(f"Running experiment set: {experiment_set_name}")
        print(f"Description: {exp_set.get('description', 'No description')}")
        print()

        # Generate parameter combinations
        constraints = self.config.get('constraints', [])
        computed_params = get_computed_params(self.config)
        combinations = generate_combinations(exp_set, constraints, computed_params)

        if not combinations:
            print("ERROR: No valid parameter combinations for this experiment set")
            return

        print(f"Generated {len(combinations)} parameter combinations\n")

        # Create test directories
        print("Creating test directories...")
        for combo_idx, combo in enumerate(combinations, 1):
            self.create_test_directory(combo)
            if combo_idx % 10 == 0 or combo_idx == len(combinations):
                print(f"  Created {combo_idx}/{len(combinations)} test directories")

        print()

        # Setup CMake build
        print(f"Setting up CMake build ({self.app_name} only)...")
        build_start_time = time.time()

        if not setup_cmake_build(app_filter=self.app_filter, build_dir=self.build_dir):
            print("ERROR: CMake build setup failed")
            return

        build_time = time.time() - build_start_time
        print(f"Build completed in {build_time:.1f}s\n")

        # Run tests
        print("Running tests...")
        execution_config = self.config.get('execution', {})
        num_runs = execution_config.get('repetitions', 5)
        timeout = execution_config.get('timeout', 300)

        for combo_idx, combo in enumerate(combinations, 1):
            test_name = self.get_test_name_from_params(combo)
            print(f"[{combo_idx}/{len(combinations)}] Running {test_name}...")

            # Collect build metrics for first combination only
            if combo_idx == 1:
                try:
                    exe_path = find_executable(test_name, self.build_dir)
                    section_sizes = get_section_sizes(exe_path)

                    # Store binary size data
                    for section, size in section_sizes.items():
                        self.results.append({
                            'test_name': test_name,
                            'scheduler': combo.get('scheduler', 'unknown'),
                            'run_num': 0,
                            'measurement': 'section',
                            'section': section,
                            'value': size,
                            'unit': 'bytes'
                        })
                except Exception as e:
                    print(f"  Warning: Could not analyze binary: {e}")

            # Run multiple times
            for run_num in range(1, num_runs + 1):
                try:
                    exe_path = find_executable(test_name, self.build_dir)
                    env_vars = self.get_parameter_exports(combo)

                    success, output = run_executable(exe_path, env_vars, timeout)

                    if not success:
                        print(f"  Run {run_num}/{num_runs} FAILED")
                        continue

                    # Parse measurements
                    for measurement in self.config.get('measurements', []):
                        meas_name = measurement.get('name')
                        value = parse_measurement_value(output, measurement)

                        if value is not None:
                            result = {
                                'test_name': test_name,
                            }
                            # Add all parameters to result
                            result.update(combo)
                            result.update({
                                'run_num': run_num,
                                'measurement': meas_name,
                                'value': value,
                                'unit': measurement.get('unit', '')
                            })
                            self.results.append(result)

                    print(f"  Run {run_num}/{num_runs} completed")

                except FileNotFoundError as e:
                    print(f"  Run {run_num}/{num_runs} FAILED: {e}")
                except Exception as e:
                    print(f"  Run {run_num}/{num_runs} ERROR: {e}")

            print()

        # Write results
        if self.results:
            timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
            # Always use derived path from app name - no hardcoded output configs
            results_dir = Path(f'applications/{self.app_name}/experiment/results')
            csv_file = results_dir / f"results_{timestamp}.csv"

            write_csv_results(self.results, csv_file)
            print(f"Results written to: {csv_file}")
            print(f"Total measurements: {len(self.results)}")
        else:
            print("WARNING: No results collected")
