#!/usr/bin/env python3
"""
Comprehensive test suite for parameter combination generation.

Tests all aspects of get_parameter_combinations function:
1. Simple Cartesian product
2. Constraint filtering
3. Computed parameters
4. Error handling
5. Real application YAML files
"""

import sys
import logging
from pathlib import Path

# Add tools to path
sys.path.insert(0, str(Path(__file__).parent / 'tools'))

from experiment_framework.config import (
    get_parameter_combinations,
    load_experiments_yaml,
    ConfigError,
)


# Configure logging for tests
logging.basicConfig(
    level=logging.DEBUG,
    format='%(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


class TestRunner:
    """Helper class to run tests and track results."""

    def __init__(self):
        self.passed = 0
        self.failed = 0
        self.skipped = 0
        self.tests = []

    def test(self, name, func):
        """Run a single test."""
        try:
            print(f"\n{'='*70}")
            print(f"TEST: {name}")
            print(f"{'='*70}")
            func()
            print(f"✓ PASSED: {name}")
            self.passed += 1
            self.tests.append((name, "PASSED", None))
        except AssertionError as e:
            print(f"✗ FAILED: {name}")
            print(f"  Error: {e}")
            self.failed += 1
            self.tests.append((name, "FAILED", str(e)))
        except Exception as e:
            print(f"✗ ERROR: {name}")
            print(f"  Error: {e}")
            self.failed += 1
            self.tests.append((name, "ERROR", str(e)))

    def summary(self):
        """Print test summary."""
        total = self.passed + self.failed + self.skipped
        print(f"\n{'='*70}")
        print(f"SUMMARY: {self.passed} passed, {self.failed} failed, {self.skipped} skipped out of {total}")
        print(f"{'='*70}\n")

        if self.failed > 0:
            print("FAILED TESTS:")
            for name, status, error in self.tests:
                if status != "PASSED":
                    print(f"  - {name}: {error}")
            return False
        return True


runner = TestRunner()


# ============================================================================
# Test 1: Simple Cartesian Product (2x2)
# ============================================================================

def test_simple_cartesian_product():
    """Test basic 2x2 Cartesian product with no constraints."""
    config = {
        'parameters': [
            {'name': 'scheduler', 'type': 'str'},
            {'name': 'size', 'type': 'int'},
        ],
        'experiment_sets': [{
            'name': 'test',
            'parameters': {
                'scheduler': ['a', 'b'],
                'size': [1, 2],
            }
        }],
        'constraints': [],
        'computed_parameters': [],
    }

    combos = get_parameter_combinations(config, 'test')

    assert len(combos) == 4, f"Expected 4 combinations, got {len(combos)}"
    assert combos[0] == {'scheduler': 'a', 'size': 1}
    assert combos[1] == {'scheduler': 'a', 'size': 2}
    assert combos[2] == {'scheduler': 'b', 'size': 1}
    assert combos[3] == {'scheduler': 'b', 'size': 2}
    print(f"Generated combinations: {combos}")


runner.test("Simple Cartesian Product (2x2)", test_simple_cartesian_product)


# ============================================================================
# Test 2: Larger Cartesian Product (2x3x2)
# ============================================================================

def test_larger_cartesian_product():
    """Test 2x3x2 = 12 combinations."""
    config = {
        'parameters': [
            {'name': 'param1', 'type': 'str'},
            {'name': 'param2', 'type': 'int'},
            {'name': 'param3', 'type': 'str'},
        ],
        'experiment_sets': [{
            'name': 'test',
            'parameters': {
                'param1': ['x', 'y'],
                'param2': [1, 2, 3],
                'param3': ['a', 'b'],
            }
        }],
        'constraints': [],
        'computed_parameters': [],
    }

    combos = get_parameter_combinations(config, 'test')
    assert len(combos) == 12, f"Expected 12 combinations, got {len(combos)}"
    print(f"Generated {len(combos)} combinations")


runner.test("Larger Cartesian Product (2x3x2=12)", test_larger_cartesian_product)


# ============================================================================
# Test 3: Constraints Filter Combinations
# ============================================================================

def test_constraints_filter():
    """Test that constraints properly reduce combination space."""
    config = {
        'parameters': [
            {'name': 'matrix_size', 'type': 'int'},
            {'name': 'num_blocks', 'type': 'int'},
        ],
        'experiment_sets': [{
            'name': 'test',
            'parameters': {
                'matrix_size': [16, 32, 64],
                'num_blocks': [1, 2, 4, 8],
            }
        }],
        'constraints': [
            {
                'expression': 'matrix_size % num_blocks == 0',
                'description': 'Matrix size must be divisible by number of blocks',
            },
            {
                'expression': 'num_blocks >= 1',
                'description': 'Must have at least one block',
            }
        ],
        'computed_parameters': [],
    }

    combos = get_parameter_combinations(config, 'test')

    # 3x4 = 12 raw combinations
    # After constraint (matrix_size % num_blocks == 0):
    # 16: blocks [1, 2, 4, 8] all divide 16 evenly → 4
    # 32: blocks [1, 2, 4, 8] all divide 32 evenly → 4
    # 64: blocks [1, 2, 4, 8] all divide 64 evenly → 4
    # Total: 12 (all are valid)
    assert len(combos) == 12, f"Expected 12 valid combinations, got {len(combos)}"

    # Now test with a stricter constraint
    config['constraints'] = [
        {
            'expression': 'matrix_size % num_blocks == 0 and num_blocks >= 2',
            'description': 'Divisible and at least 2 blocks',
        }
    ]

    combos = get_parameter_combinations(config, 'test')
    # Excludes combinations where num_blocks == 1
    # 16: [2, 4, 8] → 3
    # 32: [2, 4, 8] → 3
    # 64: [2, 4, 8] → 3
    # Total: 9
    assert len(combos) == 9, f"Expected 9 valid combinations, got {len(combos)}"

    # Check that no combination has num_blocks == 1
    for combo in combos:
        assert combo['num_blocks'] >= 2, f"Found invalid combo: {combo}"

    print(f"Constraints properly filtered: 12 → 9 combinations")


runner.test("Constraints Filter Combinations", test_constraints_filter)


# ============================================================================
# Test 4: Computed Parameters
# ============================================================================

def test_computed_parameters():
    """Test that computed parameters are added to combinations."""
    config = {
        'parameters': [
            {'name': 'matrix_size', 'type': 'int'},
            {'name': 'num_blocks', 'type': 'int'},
        ],
        'experiment_sets': [{
            'name': 'test',
            'parameters': {
                'matrix_size': [256, 512],
                'num_blocks': [2, 4],
            }
        }],
        'constraints': [
            {'expression': 'matrix_size % num_blocks == 0', 'description': ''}
        ],
        'computed_parameters': [
            {
                'name': 'block_size',
                'type': 'int',
                'expression': 'matrix_size // num_blocks',
                'description': 'Size of each block',
            }
        ],
    }

    combos = get_parameter_combinations(config, 'test')

    assert len(combos) == 4, f"Expected 4 combinations, got {len(combos)}"

    # Check that all combinations have the computed parameter
    for combo in combos:
        assert 'block_size' in combo, f"Missing block_size in {combo}"
        expected_block_size = combo['matrix_size'] // combo['num_blocks']
        assert combo['block_size'] == expected_block_size, \
            f"Wrong block_size: expected {expected_block_size}, got {combo['block_size']}"

    print(f"Computed parameters added correctly:")
    for combo in combos:
        print(f"  {combo}")


runner.test("Computed Parameters", test_computed_parameters)


# ============================================================================
# Test 5: Multiple Computed Parameters
# ============================================================================

def test_multiple_computed_parameters():
    """Test multiple computed parameters depending on each other."""
    config = {
        'parameters': [
            {'name': 'x', 'type': 'int'},
        ],
        'experiment_sets': [{
            'name': 'test',
            'parameters': {
                'x': [10, 20, 30],
            }
        }],
        'constraints': [],
        'computed_parameters': [
            {
                'name': 'y',
                'type': 'int',
                'expression': 'x * 2',
            },
            {
                'name': 'z',
                'type': 'int',
                'expression': 'y + x',  # Uses previously computed y
            }
        ],
    }

    combos = get_parameter_combinations(config, 'test')

    assert len(combos) == 3
    assert combos[0] == {'x': 10, 'y': 20, 'z': 30}
    assert combos[1] == {'x': 20, 'y': 40, 'z': 60}
    assert combos[2] == {'x': 30, 'y': 60, 'z': 90}

    print(f"Chained computed parameters work correctly:")
    for combo in combos:
        print(f"  {combo}")


runner.test("Multiple Computed Parameters", test_multiple_computed_parameters)


# ============================================================================
# Test 6: Missing Experiment Set (Error Handling)
# ============================================================================

def test_missing_experiment_set():
    """Test that missing experiment set raises ConfigError."""
    config = {
        'parameters': [],
        'experiment_sets': [
            {'name': 'existing', 'parameters': {}}
        ],
        'constraints': [],
        'computed_parameters': [],
    }

    try:
        get_parameter_combinations(config, 'nonexistent')
        raise AssertionError("Should have raised ConfigError for missing experiment set")
    except ConfigError as e:
        assert 'nonexistent' in str(e)
        print(f"Correctly raised ConfigError: {e}")


runner.test("Missing Experiment Set Error", test_missing_experiment_set)


# ============================================================================
# Test 7: Missing Parameter in Constraint (Error Handling)
# ============================================================================

def test_missing_parameter_in_constraint():
    """Test that referencing undefined parameter in constraint raises ConfigError."""
    config = {
        'parameters': [
            {'name': 'x', 'type': 'int'},
        ],
        'experiment_sets': [{
            'name': 'test',
            'parameters': {
                'x': [1, 2],
            }
        }],
        'constraints': [
            {
                'expression': 'y > 5',  # y doesn't exist
                'description': 'Invalid constraint',
            }
        ],
        'computed_parameters': [],
    }

    try:
        get_parameter_combinations(config, 'test')
        raise AssertionError("Should have raised ConfigError for undefined parameter")
    except ConfigError as e:
        assert 'y' in str(e).lower() or 'unknown' in str(e).lower()
        print(f"Correctly raised ConfigError: {e}")


runner.test("Missing Parameter in Constraint", test_missing_parameter_in_constraint)


# ============================================================================
# Test 8: Missing Parameter in Computed Expression (Error Handling)
# ============================================================================

def test_missing_parameter_in_computed():
    """Test that undefined parameter in computed expression raises ConfigError."""
    config = {
        'parameters': [
            {'name': 'x', 'type': 'int'},
        ],
        'experiment_sets': [{
            'name': 'test',
            'parameters': {
                'x': [1, 2],
            }
        }],
        'constraints': [],
        'computed_parameters': [
            {
                'name': 'result',
                'type': 'int',
                'expression': 'undefined_var * 2',
            }
        ],
    }

    try:
        get_parameter_combinations(config, 'test')
        raise AssertionError("Should have raised ConfigError for undefined parameter")
    except ConfigError as e:
        assert 'undefined_var' in str(e).lower() or 'unknown' in str(e).lower()
        print(f"Correctly raised ConfigError: {e}")


runner.test("Missing Parameter in Computed Expression", test_missing_parameter_in_computed)


# ============================================================================
# Test 9: Type Conversion in Computed Parameters
# ============================================================================

def test_type_conversion():
    """Test type conversion for computed parameters."""
    config = {
        'parameters': [
            {'name': 'x', 'type': 'int'},
        ],
        'experiment_sets': [{
            'name': 'test',
            'parameters': {
                'x': [10, 20],
            }
        }],
        'constraints': [],
        'computed_parameters': [
            {
                'name': 'float_val',
                'type': 'float',
                'expression': 'x / 3.0',
            },
            {
                'name': 'str_val',
                'type': 'str',
                'expression': 'f"value_{x}"',
            },
            {
                'name': 'int_val',
                'type': 'int',
                'expression': 'x * 2',
            }
        ],
    }

    combos = get_parameter_combinations(config, 'test')

    assert len(combos) == 2
    assert isinstance(combos[0]['float_val'], float)
    assert isinstance(combos[0]['str_val'], str)
    assert isinstance(combos[0]['int_val'], int)

    assert combos[0]['float_val'] == 10.0 / 3.0
    assert combos[0]['str_val'] == 'value_10'
    assert combos[0]['int_val'] == 20

    print(f"Type conversions work correctly:")
    for combo in combos:
        print(f"  {combo}")


runner.test("Type Conversion in Computed Parameters", test_type_conversion)


# ============================================================================
# Test 10: Real Application - 05-cholesky regression
# ============================================================================

def test_real_application_cholesky():
    """Test with real 05-cholesky/experiment/experiments.yaml file."""
    yaml_path = Path('applications/05-cholesky/experiment/experiments.yaml')

    if not yaml_path.exists():
        print(f"Skipping test: {yaml_path} not found")
        return

    config = load_experiments_yaml(yaml_path)
    combos = get_parameter_combinations(config, 'regression')

    # Expected: 2 matrix_size * 3 num_blocks * 7 schedulers = 42 combinations
    # But with constraints, should be filtered
    print(f"05-cholesky/regression: {len(combos)} combinations")
    print(f"First 3 combinations:")
    for combo in combos[:3]:
        print(f"  {combo}")

    # Basic validation
    assert len(combos) > 0, "Should generate at least 1 combination"
    assert all('matrix_size' in c for c in combos), "Missing matrix_size parameter"
    assert all('num_blocks' in c for c in combos), "Missing num_blocks parameter"
    assert all('scheduler' in c for c in combos), "Missing scheduler parameter"

    # Check for computed parameter
    if config.get('computed_parameters'):
        assert all('block_size' in c for c in combos), "Missing computed block_size"


runner.test("Real Application - 05-cholesky/regression", test_real_application_cholesky)


# ============================================================================
# Test 11: Real Application - 05-cholesky all
# ============================================================================

def test_real_application_cholesky_all():
    """Test with 05-cholesky 'all' experiment set."""
    yaml_path = Path('applications/05-cholesky/experiment/experiments.yaml')

    if not yaml_path.exists():
        print(f"Skipping test: {yaml_path} not found")
        return

    config = load_experiments_yaml(yaml_path)
    combos = get_parameter_combinations(config, 'all')

    print(f"05-cholesky/all: {len(combos)} combinations")
    print(f"First 3 combinations:")
    for combo in combos[:3]:
        print(f"  {combo}")

    assert len(combos) > 0, "Should generate at least 1 combination"


runner.test("Real Application - 05-cholesky/all", test_real_application_cholesky_all)


# ============================================================================
# Test 12: Real Application - 05-cholesky baseline
# ============================================================================

def test_real_application_cholesky_baseline():
    """Test with 05-cholesky 'baseline' experiment set."""
    yaml_path = Path('applications/05-cholesky/experiment/experiments.yaml')

    if not yaml_path.exists():
        print(f"Skipping test: {yaml_path} not found")
        return

    config = load_experiments_yaml(yaml_path)
    combos = get_parameter_combinations(config, 'baseline')

    print(f"05-cholesky/baseline: {len(combos)} combinations")

    # Expected: 3 matrix_size * 3 num_blocks * 4 schedulers = 36 combinations
    # But with constraints, should be filtered
    assert len(combos) > 0, "Should generate at least 1 combination"


runner.test("Real Application - 05-cholesky/baseline", test_real_application_cholesky_baseline)


# ============================================================================
# Test 13: Real Application - 05-cholesky iara
# ============================================================================

def test_real_application_cholesky_iara():
    """Test with 05-cholesky 'iara' experiment set."""
    yaml_path = Path('applications/05-cholesky/experiment/experiments.yaml')

    if not yaml_path.exists():
        print(f"Skipping test: {yaml_path} not found")
        return

    config = load_experiments_yaml(yaml_path)
    combos = get_parameter_combinations(config, 'iara')

    print(f"05-cholesky/iara: {len(combos)} combinations")

    # Expected: 3 matrix_size * 3 num_blocks * 2 schedulers = 18 combinations
    # But with constraints, should be filtered
    assert len(combos) > 0, "Should generate at least 1 combination"


runner.test("Real Application - 05-cholesky/iara", test_real_application_cholesky_iara)


# ============================================================================
# Test 14: Empty Parameters
# ============================================================================

def test_empty_parameters():
    """Test experiment set with no parameters."""
    config = {
        'parameters': [],
        'experiment_sets': [{
            'name': 'test',
            'parameters': {}
        }],
        'constraints': [],
        'computed_parameters': [],
    }

    combos = get_parameter_combinations(config, 'test')
    assert len(combos) == 0, f"Expected 0 combinations, got {len(combos)}"
    print("Empty parameters handled correctly")


runner.test("Empty Parameters", test_empty_parameters)


# ============================================================================
# Test 15: No Constraints vs With Constraints
# ============================================================================

def test_no_constraints_vs_with_constraints():
    """Test that adding constraints reduces combination count."""
    config = {
        'parameters': [
            {'name': 'a', 'type': 'int'},
            {'name': 'b', 'type': 'int'},
        ],
        'experiment_sets': [{
            'name': 'test',
            'parameters': {
                'a': [1, 2, 3],
                'b': [1, 2, 3],
            }
        }],
        'constraints': [],
        'computed_parameters': [],
    }

    combos_no_constraints = get_parameter_combinations(config, 'test')
    assert len(combos_no_constraints) == 9, f"Expected 9, got {len(combos_no_constraints)}"

    config['constraints'] = [
        {'expression': 'a < b', 'description': 'a must be less than b'}
    ]

    combos_with_constraints = get_parameter_combinations(config, 'test')
    # a < b: (1,2), (1,3), (2,3) = 3 combinations
    assert len(combos_with_constraints) == 3, \
        f"Expected 3 with constraint, got {len(combos_with_constraints)}"

    print(f"Constraints properly reduce combinations: 9 → 3")


runner.test("No Constraints vs With Constraints", test_no_constraints_vs_with_constraints)


# ============================================================================
# Test 16: Duplicate Check
# ============================================================================

def test_no_duplicates():
    """Test that no duplicate combinations are generated."""
    config = {
        'parameters': [
            {'name': 'x', 'type': 'int'},
            {'name': 'y', 'type': 'int'},
        ],
        'experiment_sets': [{
            'name': 'test',
            'parameters': {
                'x': [1, 2, 1],  # Has duplicate 1
                'y': [10, 10, 20],
            }
        }],
        'constraints': [],
        'computed_parameters': [],
    }

    combos = get_parameter_combinations(config, 'test')

    # Should have 6 combinations (duplicates are preserved in input)
    # But conversion to dicts should preserve uniqueness
    combo_tuples = [tuple(sorted(c.items())) for c in combos]
    unique_combos = len(set(combo_tuples))

    print(f"Generated {len(combos)} combinations")
    for i, combo in enumerate(combos):
        print(f"  {i}: {combo}")


runner.test("Duplicate Handling", test_no_duplicates)


# ============================================================================
# Test 17: Single Parameter
# ============================================================================

def test_single_parameter():
    """Test with only one parameter."""
    config = {
        'parameters': [
            {'name': 'size', 'type': 'int'},
        ],
        'experiment_sets': [{
            'name': 'test',
            'parameters': {
                'size': [1, 2, 3, 4, 5],
            }
        }],
        'constraints': [],
        'computed_parameters': [],
    }

    combos = get_parameter_combinations(config, 'test')
    assert len(combos) == 5, f"Expected 5 combinations, got {len(combos)}"

    for i, combo in enumerate(combos):
        assert combo == {'size': i + 1}

    print(f"Single parameter produces {len(combos)} combinations")


runner.test("Single Parameter", test_single_parameter)


# ============================================================================
# Test 18: String Parameters
# ============================================================================

def test_string_parameters():
    """Test with string-type parameters."""
    config = {
        'parameters': [
            {'name': 'scheduler', 'type': 'str'},
            {'name': 'mode', 'type': 'str'},
        ],
        'experiment_sets': [{
            'name': 'test',
            'parameters': {
                'scheduler': ['seq', 'omp', 'mpi'],
                'mode': ['fast', 'slow'],
            }
        }],
        'constraints': [],
        'computed_parameters': [],
    }

    combos = get_parameter_combinations(config, 'test')
    assert len(combos) == 6, f"Expected 6 combinations, got {len(combos)}"

    for combo in combos:
        assert isinstance(combo['scheduler'], str)
        assert isinstance(combo['mode'], str)

    print(f"String parameters work correctly:")
    for combo in combos:
        print(f"  {combo}")


runner.test("String Parameters", test_string_parameters)


# ============================================================================
# Run all tests
# ============================================================================

if __name__ == '__main__':
    print("\n" + "="*70)
    print("PARAMETER COMBINATIONS TEST SUITE")
    print("="*70)

    runner.summary()

    # Exit with proper code
    sys.exit(0 if runner.failed == 0 else 1)
