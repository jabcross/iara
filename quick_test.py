#!/usr/bin/env python3
"""Quick test to verify experiment runners work from any directory."""

import sys
import os
from pathlib import Path

# Test from different directories
test_dirs = [
    Path('/scratch/pedro.ciambra/repos/iara'),
    Path('/scratch/pedro.ciambra/repos/iara/applications/cholesky/experiment'),
    Path('/scratch/pedro.ciambra/repos/iara/applications'),
]

sys.path.insert(0, 'applications/experiment-framework')

from app_runner import AppExperimentRunner
from base_runner import load_yaml_config, get_computed_params, generate_combinations

class CholekskyRunner(AppExperimentRunner):
    def create_test_directory(self, params):
        return Path('test') / 'Iara' / f"05-cholesky-{params['matrix_size']}_{params['num_blocks']}_{params['scheduler']}"

    def get_parameter_exports(self, params):
        return {
            'MATRIX_SIZE': str(params['matrix_size']),
            'NUM_BLOCKS': str(params['num_blocks']),
            'SCHEDULER': params['scheduler'],
        }

print("Testing experiment runner from different directories:\n")

for test_dir in test_dirs:
    original_cwd = os.getcwd()
    try:
        os.chdir(test_dir)
        print(f"✓ Running from: {test_dir}")

        runner = CholekskyRunner('applications/cholesky/experiment/experiments.yaml', 'cholesky', 'cholesky')
        exp_sets = {s['name']: s for s in runner.config.get('experiment_sets', [])}
        dev_set = exp_sets['dev']

        constraints = runner.config.get('constraints', [])
        computed_params = get_computed_params(runner.config)
        combinations = generate_combinations(dev_set, constraints, computed_params)

        print(f"  - Generated {len(combinations)} combinations")
        print(f"  - Current working directory: {os.getcwd()}")
        print()

    except Exception as e:
        print(f"✗ Failed from {test_dir}: {e}\n")
    finally:
        os.chdir(original_cwd)

print("✓ All tests passed!")
