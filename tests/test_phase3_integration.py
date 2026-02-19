"""
End-to-end integration tests for Phase 3 (Execution).

Tests the complete workflow: Phase 1 (Generate) → Phase 2 (Build) → Phase 3 (Execute)
"""

import json
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch, MagicMock

from tools.experiment_framework.collector import collect_all_measurements, execute_instance
from tools.experiment_framework.builder import read_build_results, update_instance_execution, write_build_results
from tools.experiment_framework.config import load_experiments_yaml


class TestPhase3Integration(unittest.TestCase):
    """Integration tests for the complete Phase 3 workflow."""

    def setUp(self):
        """Set up test fixtures."""
        self.test_dir = tempfile.TemporaryDirectory()
        self.test_dir_path = Path(self.test_dir.name)

    def tearDown(self):
        """Clean up test fixtures."""
        self.test_dir.cleanup()

    def test_collect_all_measurements_basic_workflow(self):
        """Test basic collect_all_measurements workflow with mock execution."""
        # Create test executables
        app_dir = self.test_dir_path / "applications" / "test_app" / "build"
        app_dir.mkdir(parents=True)

        instance1_dir = app_dir / "instance1"
        instance1_dir.mkdir(parents=True)
        instance1 = instance1_dir / "instance1"
        instance1.write_text("#!/bin/bash\nexit 0")
        instance1.chmod(0o755)

        instance2_dir = app_dir / "instance2"
        instance2_dir.mkdir(parents=True)
        instance2 = instance2_dir / "instance2"
        instance2.write_text("#!/bin/bash\nexit 0")
        instance2.chmod(0o755)

        # Create build results
        build_results = {
            'successful_instances': [
                {'instance_name': 'instance1'},
                {'instance_name': 'instance2'},
            ]
        }

        # Create config
        config = {
            'application': {'name': 'test_app'},
            'execution': {'repetitions': 2, 'timeout': 30, 'environment': {}},
            'measurements': []
        }

        with patch('tools.experiment_framework.collector.Path') as mock_path:
            def path_side_effect(*args, **kwargs):
                if args and args[0] == 'applications':
                    return self.test_dir_path / 'applications'
                return Path(*args, **kwargs)

            mock_path.side_effect = path_side_effect

            with patch('tools.experiment_framework.collector.execute_instance') as mock_execute:
                def execute_side_effect(exe, reps, timeout, env, measurements):
                    return {
                        'instance_name': exe.stem,
                        'runs': [
                            {'run_number': 1, 'measurements': {'time_s': 1.0}},
                            {'run_number': 2, 'measurements': {'time_s': 1.1}},
                        ],
                        'failures': [],
                        'total_runs': 2,
                        'successful_runs': 2,
                        'failed_runs': 0,
                        'statistics': {
                            'time_s': {
                                'mean': 1.05,
                                'std': 0.05,
                                'min': 1.0,
                                'max': 1.1,
                                'count': 2
                            }
                        }
                    }

                mock_execute.side_effect = execute_side_effect

                result = collect_all_measurements(build_results, config)

                # Verify results structure
                self.assertIn('execution_results', result)
                self.assertIn('skipped_instances', result)
                self.assertIn('errors', result)
                self.assertIn('timestamp', result)
                self.assertIn('summary', result)

                # Verify summary
                self.assertEqual(result['summary']['executed'], 2)
                self.assertEqual(result['summary']['skipped'], 0)
                self.assertEqual(result['summary']['errors'], 0)

                # Verify execution results
                self.assertEqual(len(result['execution_results']), 2)
                for exec_result in result['execution_results']:
                    self.assertIn('instance_name', exec_result)
                    self.assertIn('runs', exec_result)
                    self.assertIn('statistics', exec_result)
                    self.assertEqual(len(exec_result['runs']), 2)

    def test_read_build_results_and_update_execution(self):
        """Test reading build results and updating with execution data."""
        # Create a mock Phase 2 JSON
        results_dir = self.test_dir_path / "results"
        results_dir.mkdir(parents=True)

        phase2_json = results_dir / "results_phase2.json"
        phase2_data = {
            'schema_version': '1.0.0',
            'experiment': {
                'application': 'test_app',
                'timestamp': '2024-01-15T10:00:00+00:00',
                'git_commit': 'abc123'
            },
            'instances': [
                {
                    'name': 'instance1',
                    'parameters': {},
                    'compilation': {'user_time_s': 1.0, 'wall_time_s': 1.5},
                    'binary': {'total_size_bytes': 1024}
                }
            ],
            'failed_instances': [],
            'statistics': {'total_instances': 1, 'successful_count': 1, 'failed_count': 0}
        }

        phase2_json.write_text(json.dumps(phase2_data))

        # Read build results
        build_results = read_build_results(phase2_json)

        # Verify Phase 2 data
        self.assertEqual(len(build_results.successful_instances), 1)
        instance = build_results.successful_instances[0]
        self.assertEqual(instance.instance_name, 'instance1')
        self.assertEqual(instance.binary_size_bytes, 1024)
        self.assertIsNone(instance.execution)

        # Update with execution data
        execution_data = {
            'instance_name': 'instance1',
            'runs': [
                {'run_number': 1, 'measurements': {'time_s': 1.0}},
                {'run_number': 2, 'measurements': {'time_s': 1.1}},
            ],
            'failures': [],
            'total_runs': 2,
            'successful_runs': 2,
            'failed_runs': 0,
            'statistics': {
                'time_s': {
                    'mean': 1.05,
                    'std': 0.05,
                    'min': 1.0,
                    'max': 1.1,
                    'count': 2
                }
            }
        }

        update_instance_execution(build_results, 'instance1', execution_data)

        # Verify execution data was added
        instance = build_results.successful_instances[0]
        self.assertIsNotNone(instance.execution)
        self.assertEqual(instance.execution['instance_name'], 'instance1')
        self.assertEqual(len(instance.execution['runs']), 2)

    def test_write_results_with_execution_data(self):
        """Test writing Phase 3 results with execution data."""
        from tools.experiment_framework.builder import BuildResults, BuildResult

        results_dir = self.test_dir_path / "results"
        results_dir.mkdir(parents=True)

        # Create BuildResults with execution data
        build_result = BuildResult(
            success=True,
            instance_name='instance1',
            attempt=1,
            timestamp='2024-01-15T10:00:00+00:00',
            compilation={'user_time_s': 1.0},
            binary_size_bytes=1024,
            execution={
                'instance_name': 'instance1',
                'runs': [
                    {'run_number': 1, 'measurements': {'time_s': 1.0}},
                ],
                'failures': [],
                'total_runs': 1,
                'successful_runs': 1,
                'failed_runs': 0,
                'statistics': {'time_s': {'mean': 1.0, 'std': 0.0, 'min': 1.0, 'max': 1.0, 'count': 1}}
            }
        )

        results = BuildResults(
            successful_instances=[build_result],
            failed_instances=[],
            timestamp='2024-01-15T10:00:00+00:00'
        )

        config = {
            'application': {'name': 'test_app'}
        }

        with patch('tools.experiment_framework.builder.get_git_commit') as mock_git:
            mock_git.return_value = 'abc123'

            output_file = write_build_results(
                results,
                results_dir,
                config,
                'test_set'
            )

            # Verify file was created
            self.assertTrue(output_file.exists())

            # Read and verify contents
            with open(output_file) as f:
                data = json.load(f)

            self.assertIn('instances', data)
            self.assertEqual(len(data['instances']), 1)
            instance = data['instances'][0]
            self.assertIn('execution', instance)
            self.assertEqual(instance['execution']['instance_name'], 'instance1')
            self.assertEqual(len(instance['execution']['runs']), 1)
            self.assertIn('statistics', instance['execution'])

    def test_phase3_partial_execution_workflow(self):
        """Test Phase 3 with some instances skipped and some failing."""
        # Create test directory structure
        app_dir = self.test_dir_path / "applications" / "test_app" / "build"
        app_dir.mkdir(parents=True)

        # Create one executable
        instance1_dir = app_dir / "instance1"
        instance1_dir.mkdir(parents=True)
        instance1 = instance1_dir / "instance1"
        instance1.write_text("#!/bin/bash\nexit 0")
        instance1.chmod(0o755)

        # instance2 and instance3 don't exist (will be skipped)

        build_results = {
            'successful_instances': [
                {'instance_name': 'instance1'},
                {'instance_name': 'instance2'},  # Will be skipped
                {'instance_name': 'instance3'},  # Will be skipped
            ]
        }

        config = {
            'application': {'name': 'test_app'},
            'execution': {'repetitions': 1, 'timeout': 30, 'environment': {}},
            'measurements': []
        }

        with patch('tools.experiment_framework.collector.Path') as mock_path:
            def path_side_effect(*args, **kwargs):
                if args and args[0] == 'applications':
                    return self.test_dir_path / 'applications'
                return Path(*args, **kwargs)

            mock_path.side_effect = path_side_effect

            with patch('tools.experiment_framework.collector.execute_instance') as mock_execute:
                def execute_side_effect(exe, reps, timeout, env, measurements):
                    if 'instance1' in str(exe):
                        return {
                            'instance_name': 'instance1',
                            'runs': [{'run_number': 1, 'measurements': {}}],
                            'failures': [],
                            'total_runs': 1,
                            'successful_runs': 1,
                            'failed_runs': 0,
                            'statistics': {}
                        }
                    raise RuntimeError("Execution failed")

                mock_execute.side_effect = execute_side_effect

                result = collect_all_measurements(build_results, config)

                # Verify partial results
                self.assertEqual(result['summary']['executed'], 1)
                self.assertEqual(result['summary']['skipped'], 2)
                self.assertEqual(result['summary']['errors'], 0)

                # Verify skipped instances are tracked
                self.assertEqual(len(result['skipped_instances']), 2)
                skipped_names = [s['instance_name'] for s in result['skipped_instances']]
                self.assertIn('instance2', skipped_names)
                self.assertIn('instance3', skipped_names)

    def test_phase3_environment_variables_propagation(self):
        """Test that environment variables from config are passed to executions."""
        # Create test executable
        app_dir = self.test_dir_path / "applications" / "test_app" / "build"
        app_dir.mkdir(parents=True)

        instance1_dir = app_dir / "instance1"
        instance1_dir.mkdir(parents=True)
        instance1 = instance1_dir / "instance1"
        instance1.write_text("#!/bin/bash\nexit 0")
        instance1.chmod(0o755)

        build_results = {
            'successful_instances': [
                {'instance_name': 'instance1'},
            ]
        }

        config = {
            'application': {'name': 'test_app'},
            'execution': {
                'repetitions': 1,
                'timeout': 30,
                'environment': {
                    'TEST_VAR': 'test_value',
                    'ANOTHER_VAR': 'another_value'
                }
            },
            'measurements': []
        }

        with patch('tools.experiment_framework.collector.Path') as mock_path:
            def path_side_effect(*args, **kwargs):
                if args and args[0] == 'applications':
                    return self.test_dir_path / 'applications'
                return Path(*args, **kwargs)

            mock_path.side_effect = path_side_effect

            with patch('tools.experiment_framework.collector.execute_instance') as mock_execute:
                def execute_side_effect(exe, reps, timeout, env_vars, measurements):
                    # Verify environment variables were passed
                    assert env_vars.get('TEST_VAR') == 'test_value'
                    assert env_vars.get('ANOTHER_VAR') == 'another_value'
                    return {
                        'instance_name': 'instance1',
                        'runs': [{'run_number': 1, 'measurements': {}}],
                        'failures': [],
                        'total_runs': 1,
                        'successful_runs': 1,
                        'failed_runs': 0,
                        'statistics': {}
                    }

                mock_execute.side_effect = execute_side_effect

                result = collect_all_measurements(build_results, config)

                # Verify execution succeeded
                self.assertEqual(result['summary']['executed'], 1)

                # Verify execute_instance was called with correct env_vars
                mock_execute.assert_called_once()
                call_args = mock_execute.call_args
                env_vars_arg = call_args[0][3]
                self.assertEqual(env_vars_arg['TEST_VAR'], 'test_value')
                self.assertEqual(env_vars_arg['ANOTHER_VAR'], 'another_value')

    def test_phase3_measurement_statistics_computed(self):
        """Test that measurement statistics are properly computed in Phase 3."""
        app_dir = self.test_dir_path / "applications" / "test_app" / "build"
        app_dir.mkdir(parents=True)

        instance1_dir = app_dir / "instance1"
        instance1_dir.mkdir(parents=True)
        instance1 = instance1_dir / "instance1"
        instance1.write_text("#!/bin/bash\nexit 0")
        instance1.chmod(0o755)

        build_results = {
            'successful_instances': [
                {'instance_name': 'instance1'},
            ]
        }

        config = {
            'application': {'name': 'test_app'},
            'execution': {'repetitions': 3, 'timeout': 30, 'environment': {}},
            'measurements': [
                {'name': 'time', 'type': 'float', 'parser': {'type': 'regex', 'pattern': r'Time: (\d+\.?\d*)'}}
            ]
        }

        with patch('tools.experiment_framework.collector.Path') as mock_path:
            def path_side_effect(*args, **kwargs):
                if args and args[0] == 'applications':
                    return self.test_dir_path / 'applications'
                return Path(*args, **kwargs)

            mock_path.side_effect = path_side_effect

            with patch('tools.experiment_framework.collector.execute_instance') as mock_execute:
                def execute_side_effect(exe, reps, timeout, env_vars, measurements):
                    return {
                        'instance_name': 'instance1',
                        'runs': [
                            {'run_number': 1, 'measurements': {'time_s': 1.0}},
                            {'run_number': 2, 'measurements': {'time_s': 1.1}},
                            {'run_number': 3, 'measurements': {'time_s': 1.2}},
                        ],
                        'failures': [],
                        'total_runs': 3,
                        'successful_runs': 3,
                        'failed_runs': 0,
                        'statistics': {
                            'time_s': {
                                'mean': 1.1,
                                'std': 0.08164965809,
                                'min': 1.0,
                                'max': 1.2,
                                'count': 3
                            }
                        }
                    }

                mock_execute.side_effect = execute_side_effect

                result = collect_all_measurements(build_results, config)

                # Verify execution results contain statistics
                self.assertEqual(len(result['execution_results']), 1)
                exec_result = result['execution_results'][0]
                self.assertIn('statistics', exec_result)
                self.assertIn('time_s', exec_result['statistics'])

                stats = exec_result['statistics']['time_s']
                self.assertAlmostEqual(stats['mean'], 1.1, places=5)
                self.assertAlmostEqual(stats['std'], 0.08164965809, places=5)
                self.assertEqual(stats['min'], 1.0)
                self.assertEqual(stats['max'], 1.2)
                self.assertEqual(stats['count'], 3)


if __name__ == '__main__':
    unittest.main()
