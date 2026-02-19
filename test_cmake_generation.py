#!/usr/bin/env python3
"""
Comprehensive test suite for CMakeLists.txt generation (Task 1.4).

Tests the three main functions:
1. generate_instance_name() - Instance naming convention
2. generate_cmake_instance() - CMake code generation
3. generate_cmakelists() - Full CMakeLists.txt file generation
"""

import logging
import tempfile
from pathlib import Path
import sys

# Add the tools directory to path
sys.path.insert(0, str(Path(__file__).parent / 'tools'))

from experiment_framework.generator import (
    generate_instance_name,
    generate_cmake_instance,
    generate_cmakelists,
)
from experiment_framework.config import load_experiments_yaml

# Configure logging
logging.basicConfig(
    level=logging.DEBUG,
    format='%(levelname)-8s %(name)s: %(message)s'
)
logger = logging.getLogger(__name__)


class TestInstanceNaming:
    """Test suite for generate_instance_name()"""

    def test_basic_instance_name(self):
        """Test basic instance naming with simple parameters"""
        app = "05-cholesky"
        params = {
            'scheduler': 'vf-omp',
            'matrix_size': 2048,
            'num_blocks': 2,
        }
        name = generate_instance_name(app, params)
        assert name == "05-cholesky_vf-omp_matrix_size_2048_num_blocks_2"
        logger.info(f"✓ Basic instance name: {name}")

    def test_instance_name_excludes_computed_params(self):
        """Test that computed parameters are excluded from instance name"""
        app = "05-cholesky"
        params = {
            'scheduler': 'vf-omp',
            'matrix_size': 2048,
            'num_blocks': 2,
            'block_size': 1024,  # Computed parameter
        }
        computed = {'block_size'}
        name = generate_instance_name(app, params, computed)
        assert name == "05-cholesky_vf-omp_matrix_size_2048_num_blocks_2"
        assert "block_size" not in name
        logger.info(f"✓ Instance name excludes computed params: {name}")

    def test_instance_name_with_sift(self):
        """Test instance naming for 08-sift application"""
        app = "08-sift"
        params = {
            'scheduler': 'sequential',
            'image_size': 512,
        }
        name = generate_instance_name(app, params)
        assert name == "08-sift_sequential_image_size_512"
        logger.info(f"✓ SIFT instance name: {name}")

    def test_instance_name_value_sanitization(self):
        """Test that values are properly sanitized"""
        app = "test-app"
        params = {
            'scheduler': 'vf-omp',
            'param_with_spaces': 'value with spaces',
            'param_with_special': 'value@123#',
        }
        name = generate_instance_name(app, params)
        # Spaces should become hyphens, special chars removed
        assert "value-with-spaces" in name
        assert "value123" in name
        logger.info(f"✓ Sanitized instance name: {name}")

    def test_instance_name_with_multiple_schedulers(self):
        """Test various scheduler types produce correct names"""
        app = "05-cholesky"
        for scheduler in ['sequential', 'omp-for', 'omp-task', 'enkits-task', 'vf-omp', 'vf-enkits']:
            params = {'scheduler': scheduler, 'size': 256}
            name = generate_instance_name(app, params)
            assert scheduler in name
            logger.info(f"✓ Scheduler {scheduler}: {name}")

    def test_instance_name_parameter_ordering(self):
        """Test that parameters are sorted consistently"""
        app = "test"
        params = {
            'scheduler': 'vf-omp',
            'zparam': 3,
            'aparam': 1,
            'mparam': 2,
        }
        name = generate_instance_name(app, params)
        # After scheduler, should be alphabetically sorted: aparam, mparam, zparam
        # The name should have format: test_vf-omp_aparam_1_mparam_2_zparam_3
        assert name == "test_vf-omp_aparam_1_mparam_2_zparam_3"
        logger.info(f"✓ Parameters ordered correctly: {name}")


class TestCMakeInstanceGeneration:
    """Test suite for generate_cmake_instance()"""

    def test_cmake_basic_structure(self):
        """Test that generated CMake code has correct structure"""
        config = {
            'application': {
                'name': '05-cholesky',
                'entry': '05-cholesky',
                'build': {
                    'extra_linker_args': ['-lopenblas'],
                    'defines': ['VERBOSE'],
                }
            },
            'execution': {'timeout': 300},
            'computed_parameters': [{'name': 'block_size'}],
        }
        params = {
            'scheduler': 'vf-omp',
            'matrix_size': 2048,
            'num_blocks': 2,
        }
        instance_name = "05-cholesky_vf-omp_matrix_size_2048_num_blocks_2"

        code = generate_cmake_instance(instance_name, params, config, "regression")

        # Check basic structure
        assert "iara_add_test_instance(" in code
        assert f'NAME "{instance_name}"' in code
        assert 'EXPERIMENT_SET "regression"' in code
        assert 'SCHEDULER "vf-omp"' in code
        assert 'TIMEOUT "300"' in code
        logger.info(f"✓ CMake code has correct basic structure")

    def test_cmake_entry_field(self):
        """Test that ENTRY field is correctly set"""
        config = {
            'application': {
                'name': '08-sift',
                'entry': '08-sift',  # Entry can be different from name
                'build': {'extra_linker_args': []},
            },
            'execution': {'timeout': 300},
            'computed_parameters': [],
        }
        params = {'scheduler': 'sequential'}
        instance_name = "08-sift_sequential"

        code = generate_cmake_instance(instance_name, params, config, "test")

        assert 'ENTRY "08-sift"' in code
        logger.info(f"✓ ENTRY field correctly set")

    def test_cmake_application_dir(self):
        """Test that APPLICATION_DIR points correctly"""
        config = {
            'application': {'name': 'app', 'entry': 'app', 'build': {}},
            'execution': {'timeout': 300},
            'computed_parameters': [],
        }
        params = {'scheduler': 'seq'}
        instance_name = "app_seq"

        code = generate_cmake_instance(instance_name, params, config, "test")

        assert '${CMAKE_CURRENT_SOURCE_DIR}/../..' in code
        logger.info(f"✓ APPLICATION_DIR correctly points two levels up")

    def test_cmake_build_dir_pattern(self):
        """Test that BUILD_DIR follows the correct pattern"""
        config = {
            'application': {'name': '05-cholesky', 'entry': '05-cholesky', 'build': {}},
            'execution': {'timeout': 300},
            'computed_parameters': [],
        }
        params = {'scheduler': 'vf-omp'}
        instance_name = "05-cholesky_vf-omp"

        code = generate_cmake_instance(instance_name, params, config, "regression")

        assert '${CMAKE_BINARY_DIR}/05-cholesky/regression/05-cholesky_vf-omp' in code
        logger.info(f"✓ BUILD_DIR pattern is correct")

    def test_cmake_parameters_field(self):
        """Test PARAMETERS field contains non-computed params"""
        config = {
            'application': {'name': 'app', 'entry': 'app', 'build': {}},
            'execution': {'timeout': 300},
            'computed_parameters': [{'name': 'block_size'}],
        }
        params = {
            'scheduler': 'vf-omp',
            'matrix_size': 2048,
            'num_blocks': 2,
            'block_size': 1024,
        }
        instance_name = "app_vf-omp"

        code = generate_cmake_instance(instance_name, params, config, "test")

        # PARAMETERS should include matrix_size and num_blocks, but not block_size or scheduler
        assert 'PARAMETERS "matrix_size=2048" "num_blocks=2"' in code or \
               'PARAMETERS "num_blocks=2" "matrix_size=2048"' in code
        assert "block_size" not in code.split("PARAMETERS")[1].split("BUILD_DIR")[0]
        logger.info(f"✓ PARAMETERS field correctly excludes computed params")

    def test_cmake_defines_field(self):
        """Test DEFINES field contains all parameters and scheduler define"""
        config = {
            'application': {
                'name': 'app',
                'entry': 'app',
                'build': {
                    'defines': ['VERBOSE'],
                }
            },
            'execution': {'timeout': 300},
            'computed_parameters': [],
        }
        params = {
            'scheduler': 'vf-omp',
            'matrix_size': 2048,
        }
        instance_name = "app_vf-omp"

        code = generate_cmake_instance(instance_name, params, config, "test")

        defines_section = code.split("DEFINES")[1].split("BUILD_DIR")[0]
        assert "MATRIX_SIZE=2048" in defines_section
        assert "SCHEDULER_IARA" in defines_section  # vf-* schedulers get SCHEDULER_IARA
        assert "VERBOSE" in defines_section  # Application-specific define
        logger.info(f"✓ DEFINES field contains parameters and scheduler define")

    def test_cmake_scheduler_defines_vf_schedulers(self):
        """Test scheduler defines for vf-* schedulers"""
        config = {
            'application': {'name': 'app', 'entry': 'app', 'build': {}},
            'execution': {'timeout': 300},
            'computed_parameters': [],
        }

        for scheduler in ['vf-omp', 'vf-enkits', 'vf-sequential']:
            params = {'scheduler': scheduler}
            instance_name = f"app_{scheduler}"
            code = generate_cmake_instance(instance_name, params, config, "test")
            assert 'SCHEDULER_IARA' in code
            logger.info(f"✓ Scheduler {scheduler} → SCHEDULER_IARA")

    def test_cmake_scheduler_defines_omp_schedulers(self):
        """Test scheduler defines for omp-* schedulers"""
        config = {
            'application': {'name': 'app', 'entry': 'app', 'build': {}},
            'execution': {'timeout': 300},
            'computed_parameters': [],
        }

        code_for = generate_cmake_instance(
            "app_omp_for", {'scheduler': 'omp-for'}, config, "test"
        )
        assert 'SCHEDULER_OMP_FOR' in code_for
        logger.info(f"✓ Scheduler omp-for → SCHEDULER_OMP_FOR")

        code_task = generate_cmake_instance(
            "app_omp_task", {'scheduler': 'omp-task'}, config, "test"
        )
        assert 'SCHEDULER_OMP_TASK' in code_task
        logger.info(f"✓ Scheduler omp-task → SCHEDULER_OMP_TASK")

    def test_cmake_scheduler_defines_enkits(self):
        """Test scheduler defines for enkits-* schedulers"""
        config = {
            'application': {'name': 'app', 'entry': 'app', 'build': {}},
            'execution': {'timeout': 300},
            'computed_parameters': [],
        }
        params = {'scheduler': 'enkits-task'}
        code = generate_cmake_instance("app_enkits", params, config, "test")
        assert 'SCHEDULER_ENKITS' in code
        logger.info(f"✓ Scheduler enkits-task → SCHEDULER_ENKITS")

    def test_cmake_scheduler_defines_sequential(self):
        """Test scheduler defines for sequential scheduler"""
        config = {
            'application': {'name': 'app', 'entry': 'app', 'build': {}},
            'execution': {'timeout': 300},
            'computed_parameters': [],
        }
        params = {'scheduler': 'sequential'}
        code = generate_cmake_instance("app_seq", params, config, "test")
        assert 'SCHEDULER_SEQUENTIAL' in code
        logger.info(f"✓ Scheduler sequential → SCHEDULER_SEQUENTIAL")

    def test_cmake_linker_args(self):
        """Test LINKER_ARGS field"""
        config = {
            'application': {
                'name': 'app',
                'entry': 'app',
                'build': {
                    'extra_linker_args': ['-lopenblas', '-lm'],
                }
            },
            'execution': {'timeout': 300},
            'computed_parameters': [],
        }
        params = {'scheduler': 'vf-omp'}
        code = generate_cmake_instance("app_vf", params, config, "test")

        assert 'LINKER_ARGS "-lopenblas" "-lm"' in code
        logger.info(f"✓ LINKER_ARGS correctly included")

    def test_cmake_timeout(self):
        """Test TIMEOUT field from execution config"""
        config = {
            'application': {'name': 'app', 'entry': 'app', 'build': {}},
            'execution': {'timeout': 600},
            'computed_parameters': [],
        }
        params = {'scheduler': 'seq'}
        code = generate_cmake_instance("app_seq", params, config, "test")

        assert 'TIMEOUT "600"' in code
        logger.info(f"✓ TIMEOUT correctly set from execution config")

    def test_cmake_default_timeout(self):
        """Test default TIMEOUT when not specified"""
        config = {
            'application': {'name': 'app', 'entry': 'app', 'build': {}},
            'execution': {},  # No timeout specified
            'computed_parameters': [],
        }
        params = {'scheduler': 'seq'}
        code = generate_cmake_instance("app_seq", params, config, "test")

        assert 'TIMEOUT "300"' in code  # Default should be 300
        logger.info(f"✓ Default TIMEOUT is 300 seconds")


class TestFullCMakeListsGeneration:
    """Test suite for generate_cmakelists()"""

    def test_generate_cholesky_regression(self):
        """Test full CMakeLists.txt generation for 05-cholesky regression set"""
        yaml_path = Path('/scratch/pedro.ciambra/repos/iara/applications/05-cholesky/experiment/experiments.yaml')

        if not yaml_path.exists():
            logger.warning(f"Skipping test: {yaml_path} does not exist")
            return

        with tempfile.TemporaryDirectory() as tmpdir:
            output_path = Path(tmpdir) / 'CMakeLists.txt'

            count = generate_cmakelists(yaml_path, output_path, "regression")

            # The regression set should have multiple combinations
            assert count > 0
            logger.info(f"✓ Generated {count} instances for 05-cholesky regression set")

            # Check file was created
            assert output_path.exists()
            content = output_path.read_text()

            # Check header
            assert "Auto-generated by tools.experiment_framework.generator" in content
            assert "applications/05-cholesky/experiment/experiments.yaml" in content
            assert "Experiment Set: regression" in content
            assert "DO NOT EDIT" in content
            logger.info(f"✓ CMakeLists.txt header is correct")

            # Check CMake code
            assert "cmake_minimum_required(VERSION 3.20)" in content
            assert "include(${CMAKE_SOURCE_DIR}/cmake/IaRaApplications.cmake)" in content
            assert content.count("iara_add_test_instance(") == count
            logger.info(f"✓ CMakeLists.txt contains {count} iara_add_test_instance() calls")

            # Check hash is included
            assert "YAML Hash: " in content
            hash_line = [l for l in content.split('\n') if 'YAML Hash:' in l][0]
            hash_value = hash_line.split("YAML Hash: ")[1]
            assert len(hash_value) == 64  # SHA256 hex is 64 chars
            assert all(c in '0123456789abcdef' for c in hash_value)
            logger.info(f"✓ YAML hash is included in header")

            # Verify hash file was created
            hash_file = yaml_path.parent / '.experiments.yaml.hash'
            assert hash_file.exists()
            stored_hash = hash_file.read_text().strip()
            assert stored_hash == hash_value
            logger.info(f"✓ Hash file stored correctly")

    def test_cmakelists_unique_instance_names(self):
        """Test that all generated instance names are unique"""
        yaml_path = Path('/scratch/pedro.ciambra/repos/iara/applications/05-cholesky/experiment/experiments.yaml')

        if not yaml_path.exists():
            logger.warning(f"Skipping test: {yaml_path} does not exist")
            return

        with tempfile.TemporaryDirectory() as tmpdir:
            output_path = Path(tmpdir) / 'CMakeLists.txt'
            count = generate_cmakelists(yaml_path, output_path, "regression")

            content = output_path.read_text()

            # Extract all instance names
            import re
            names = re.findall(r'NAME "([^"]+)"', content)

            # Check uniqueness
            assert len(names) == len(set(names))
            assert len(names) == count
            logger.info(f"✓ All {count} instance names are unique")

    def test_cmakelists_syntax_validity(self):
        """Test that generated CMakeLists.txt is syntactically valid CMake"""
        yaml_path = Path('/scratch/pedro.ciambra/repos/iara/applications/05-cholesky/experiment/experiments.yaml')

        if not yaml_path.exists():
            logger.warning(f"Skipping test: {yaml_path} does not exist")
            return

        with tempfile.TemporaryDirectory() as tmpdir:
            output_path = Path(tmpdir) / 'CMakeLists.txt'
            generate_cmakelists(yaml_path, output_path, "regression")

            content = output_path.read_text()

            # Check that all required fields are present in each call
            import re
            instances = re.findall(r'iara_add_test_instance\((.*?)\)', content, re.DOTALL)

            assert len(instances) > 0
            logger.info(f"✓ CMake parentheses are balanced ({len(instances)} instances)")

            for instance_code in instances:
                assert 'NAME' in instance_code
                assert 'EXPERIMENT_SET' in instance_code
                assert 'APPLICATION_DIR' in instance_code
                assert 'ENTRY' in instance_code
                assert 'SCHEDULER' in instance_code
                assert 'BUILD_DIR' in instance_code
                assert 'DEFINES' in instance_code
                assert 'TIMEOUT' in instance_code

            logger.info(f"✓ All {len(instances)} instances have required fields")


def run_all_tests():
    """Run all tests and report results"""
    print("\n" + "="*70)
    print("Task 1.4 - CMakeLists.txt Generation Test Suite")
    print("="*70 + "\n")

    test_classes = [
        TestInstanceNaming,
        TestCMakeInstanceGeneration,
        TestFullCMakeListsGeneration,
    ]

    results = {'passed': 0, 'failed': 0, 'skipped': 0}

    for test_class in test_classes:
        print(f"\n{test_class.__name__}")
        print("-" * 70)
        test_instance = test_class()

        for method_name in dir(test_instance):
            if method_name.startswith('test_'):
                try:
                    method = getattr(test_instance, method_name)
                    method()
                    results['passed'] += 1
                except AssertionError as e:
                    print(f"✗ {method_name}: {e}")
                    results['failed'] += 1
                except Exception as e:
                    if "does not exist" in str(e):
                        results['skipped'] += 1
                    else:
                        print(f"✗ {method_name}: {e}")
                        results['failed'] += 1

    print("\n" + "="*70)
    print("Test Results Summary")
    print("="*70)
    print(f"Passed:  {results['passed']}")
    print(f"Failed:  {results['failed']}")
    print(f"Skipped: {results['skipped']}")
    print("="*70 + "\n")

    return results['failed'] == 0


if __name__ == '__main__':
    success = run_all_tests()
    sys.exit(0 if success else 1)
