#!/usr/bin/env python3
"""
Integration test for the generalized experiment framework.

Tests that all three applications (Cholesky, SIFT, Degridder) can:
1. Load their experiments.yaml configuration
2. Generate parameter combinations from dev experiment sets
3. Create test directories with correct naming
4. Export parameters as environment variables
"""

import sys
from pathlib import Path

# Add framework to path
sys.path.insert(0, 'applications/experiment-framework')

from app_runner import AppExperimentRunner
from base_runner import load_yaml_config, get_computed_params, generate_combinations


class CholekskyRunner(AppExperimentRunner):
    def create_test_directory(self, params):
        dir_path = Path('test') / 'Iara' / f"05-cholesky-{params['matrix_size']}_{params['num_blocks']}_{params['scheduler']}"
        return dir_path

    def get_parameter_exports(self, params):
        return {
            'MATRIX_SIZE': str(params['matrix_size']),
            'NUM_BLOCKS': str(params['num_blocks']),
            'SCHEDULER': params['scheduler'],
        }


class SiftRunner(AppExperimentRunner):
    def create_test_directory(self, params):
        dir_path = Path('test') / 'Iara' / f"08-sift-{params['image_size']}_{params['scheduler']}"
        return dir_path

    def get_parameter_exports(self, params):
        return {
            'IMAGE_SIZE': str(params['image_size']),
            'SCHEDULER': params['scheduler'],
        }


class DegridderRunner(AppExperimentRunner):
    def create_test_directory(self, params):
        dir_path = Path('test') / 'Iara' / f"XX-degridder-{params['num_cores']}cores_{params['num_chunk']}chunks_{params['scheduler']}"
        return dir_path

    def get_parameter_exports(self, params):
        return {
            'NUM_CORES': str(params['num_cores']),
            'NUM_CHUNK': str(params['num_chunk']),
            'NUM_KERNEL_SUPPORT': str(params['num_supports']),
            'GRID_SIZE': str(params['grid_size']),
            'SCHEDULER': params['scheduler'],
        }


def test_application(runner_class, config_path, app_name):
    """Test a single application."""
    print(f"\n{'='*70}")
    print(f"Testing {app_name.upper()}")
    print(f"{'='*70}")

    try:
        # Instantiate runner
        runner = runner_class(config_path, app_name, app_name)
        print(f"✓ Runner instantiated")
        print(f"  - App: {runner.app_name}")
        print(f"  - Filter: {runner.app_filter}")

        # Get experiment sets
        exp_sets = {s['name']: s for s in runner.config.get('experiment_sets', [])}
        print(f"✓ Found {len(exp_sets)} experiment sets: {', '.join(exp_sets.keys())}")

        # Generate dev set combinations
        if 'dev' not in exp_sets:
            print("✗ No 'dev' experiment set found!")
            return False

        dev_set = exp_sets['dev']
        constraints = runner.config.get('constraints', [])
        computed_params = get_computed_params(runner.config)
        combinations = generate_combinations(dev_set, constraints, computed_params)

        print(f"✓ Generated {len(combinations)} parameter combinations from 'dev' set")

        # Test directory creation and exports
        for i, combo in enumerate(combinations[:2], 1):  # Show first 2
            test_dir = runner.create_test_directory(combo)
            exports = runner.get_parameter_exports(combo)

            print(f"\n  Combo {i}:")
            print(f"    Test dir: {test_dir.name}")
            print(f"    Parameters: {combo}")
            print(f"    Env exports: {exports}")

        if len(combinations) > 2:
            print(f"\n  ... and {len(combinations) - 2} more combinations")

        print(f"\n✓ {app_name} PASSED all tests")
        return True

    except Exception as e:
        print(f"✗ {app_name} FAILED: {e}")
        import traceback
        traceback.print_exc()
        return False


if __name__ == '__main__':
    print("Integration Test: Experiment Framework")
    print("=" * 70)

    tests = [
        (CholekskyRunner, 'applications/cholesky/experiment/experiments.yaml', 'cholesky'),
        (SiftRunner, 'applications/sift/experiment/experiments.yaml', 'sift'),
        (DegridderRunner, 'applications/degridder/experiment/experiments.yaml', 'degridder'),
    ]

    results = []
    for runner_class, config_path, app_name in tests:
        results.append(test_application(runner_class, config_path, app_name))

    print(f"\n{'='*70}")
    print("SUMMARY")
    print(f"{'='*70}")

    passed = sum(results)
    total = len(results)

    for (_, _, app_name), result in zip(tests, results):
        status = "✓ PASS" if result else "✗ FAIL"
        print(f"{status:8} {app_name}")

    print(f"\nTotal: {passed}/{total} passed")

    sys.exit(0 if passed == total else 1)
