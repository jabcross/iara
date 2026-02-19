"""
Unit tests for the collector module.

Tests the execution engine for single runs and multiple repetitions.
"""

import unittest
from unittest.mock import patch, MagicMock, call
from pathlib import Path
import subprocess
import tempfile
import os

from tools.experiment_framework.collector import (
    execute_single_run,
    execute_instance,
    compute_measurement_statistics,
    compute_all_statistics,
    collect_all_measurements,
    parse_regex_measurement,
    parse_json_measurement,
    parse_line_measurement,
    parse_measurement,
    MeasurementError
)


class TestExecuteSingleRun(unittest.TestCase):
    """Test cases for execute_single_run function."""

    def setUp(self):
        """Set up test fixtures."""
        self.test_dir = tempfile.TemporaryDirectory()
        self.executable = Path(self.test_dir.name) / "test_executable"
        self.executable.write_text("#!/bin/bash\necho 'test'")
        self.executable.chmod(0o755)

    def tearDown(self):
        """Clean up test fixtures."""
        self.test_dir.cleanup()

    def test_execute_single_run_success(self):
        """Test successful execution of a single run."""
        with patch('tools.experiment_framework.collector.subprocess.run') as mock_run, \
             patch('tools.experiment_framework.collector.parse_time_output') as mock_parse_time:

            # Mock successful subprocess execution
            mock_result = MagicMock()
            mock_result.returncode = 0
            mock_result.stdout = "Program output"
            mock_result.stderr = "Time output"
            mock_run.return_value = mock_result

            # Mock time parsing
            mock_parse_time.return_value = {
                'user_time_s': 1.0,
                'wall_time_s': 2.0,
                'max_rss_bytes': 1024
            }

            result = execute_single_run(
                executable=self.executable,
                run_number=1,
                timeout=30,
                env_vars={"TEST_VAR": "test_value"}
            )

            # Verify result
            self.assertTrue(result["success"])
            self.assertEqual(result["run_number"], 1)
            self.assertEqual(result["stdout"], "Program output")
            self.assertEqual(result["stderr"], "Time output")
            self.assertEqual(result["returncode"], 0)
            self.assertIsNone(result["error"])
            self.assertIsNotNone(result["gnu_time"])

            # Verify subprocess was called with correct parameters
            mock_run.assert_called_once()
            call_args = mock_run.call_args
            self.assertIn('/usr/bin/time', call_args[0][0])
            self.assertIn('-v', call_args[0][0])
            self.assertEqual(call_args[1]['timeout'], 30)
            self.assertIn('TEST_VAR', call_args[1]['env'])

    def test_execute_single_run_timeout(self):
        """Test timeout handling in execute_single_run."""
        with patch('tools.experiment_framework.collector.subprocess.run') as mock_run:
            # Mock timeout exception
            mock_run.side_effect = subprocess.TimeoutExpired('cmd', timeout=5)

            result = execute_single_run(
                executable=self.executable,
                run_number=1,
                timeout=5,
                env_vars={}
            )

            # Verify timeout was handled
            self.assertFalse(result["success"])
            self.assertEqual(result["run_number"], 1)
            self.assertIn("Timeout", result["error"])
            self.assertEqual(result["returncode"], -1)

    def test_execute_single_run_nonzero_exit(self):
        """Test handling of non-zero exit code."""
        with patch('tools.experiment_framework.collector.subprocess.run') as mock_run, \
             patch('tools.experiment_framework.collector.parse_time_output') as mock_parse_time:

            # Mock failed subprocess execution
            mock_result = MagicMock()
            mock_result.returncode = 1
            mock_result.stdout = ""
            mock_result.stderr = "Error output"
            mock_run.return_value = mock_result

            mock_parse_time.return_value = {}

            result = execute_single_run(
                executable=self.executable,
                run_number=1,
                timeout=30,
                env_vars={}
            )

            # Verify failure
            self.assertFalse(result["success"])
            self.assertEqual(result["returncode"], 1)

    def test_execute_single_run_executable_not_found(self):
        """Test handling when executable doesn't exist."""
        nonexistent = Path("/nonexistent/executable")

        result = execute_single_run(
            executable=nonexistent,
            run_number=1,
            timeout=30,
            env_vars={}
        )

        # Verify error handling
        self.assertFalse(result["success"])
        self.assertIn("not found", result["error"])
        self.assertEqual(result["returncode"], -1)

    def test_execute_single_run_env_vars_merged(self):
        """Test that environment variables are properly merged."""
        with patch('tools.experiment_framework.collector.subprocess.run') as mock_run, \
             patch('tools.experiment_framework.collector.parse_time_output') as mock_parse_time, \
             patch('tools.experiment_framework.collector.os.environ.copy') as mock_env_copy:

            # Mock environment
            original_env = {"ORIGINAL": "value"}
            mock_env_copy.return_value = original_env.copy()

            mock_result = MagicMock()
            mock_result.returncode = 0
            mock_result.stdout = ""
            mock_result.stderr = ""
            mock_run.return_value = mock_result
            mock_parse_time.return_value = {}

            # Call with custom env vars
            execute_single_run(
                executable=self.executable,
                run_number=1,
                timeout=30,
                env_vars={"NEW_VAR": "new_value"}
            )

            # Verify subprocess received merged environment
            call_args = mock_run.call_args
            env_passed = call_args[1]['env']
            self.assertIn("NEW_VAR", env_passed)
            self.assertEqual(env_passed["NEW_VAR"], "new_value")

    def test_execute_single_run_exception_handling(self):
        """Test handling of unexpected exceptions."""
        with patch('tools.experiment_framework.collector.subprocess.run') as mock_run:
            # Mock unexpected exception
            mock_run.side_effect = RuntimeError("Unexpected error")

            result = execute_single_run(
                executable=self.executable,
                run_number=1,
                timeout=30,
                env_vars={}
            )

            # Verify error handling
            self.assertFalse(result["success"])
            self.assertIn("error", result["error"].lower())


class TestExecuteInstance(unittest.TestCase):
    """Test cases for execute_instance function."""

    def setUp(self):
        """Set up test fixtures."""
        self.test_dir = tempfile.TemporaryDirectory()
        self.executable = Path(self.test_dir.name) / "test_executable"
        self.executable.write_text("#!/bin/bash\necho 'test'")
        self.executable.chmod(0o755)

    def tearDown(self):
        """Clean up test fixtures."""
        self.test_dir.cleanup()

    def test_execute_instance_all_success(self):
        """Test instance execution where all repetitions succeed."""
        with patch('tools.experiment_framework.collector.execute_single_run') as mock_run:
            # Mock all successful runs
            def mock_execute(executable, run_number, timeout, env_vars):
                return {
                    "run_number": run_number,
                    "stdout": f"output_{run_number}",
                    "stderr": f"time_{run_number}",
                    "returncode": 0,
                    "gnu_time": {"wall_time_s": 1.0},
                    "success": True,
                    "error": None
                }

            mock_run.side_effect = mock_execute

            result = execute_instance(
                executable=self.executable,
                repetitions=3,
                timeout=30,
                env_vars={},
                measurements=[]
            )

            # Verify result structure
            self.assertEqual(result["instance_name"], "test_executable")
            self.assertEqual(result["total_runs"], 3)
            self.assertEqual(result["successful_runs"], 3)
            self.assertEqual(result["failed_runs"], 0)
            self.assertEqual(len(result["runs"]), 3)
            self.assertEqual(len(result["failures"]), 0)

            # Verify each run has stdout
            for i, run in enumerate(result["runs"], 1):
                self.assertEqual(run["run_number"], i)
                self.assertEqual(run["stdout"], f"output_{i}")
                self.assertEqual(run["returncode"], 0)
                self.assertIn("measurements", run)

    def test_execute_instance_partial_failure(self):
        """Test instance execution with partial failures."""
        with patch('tools.experiment_framework.collector.execute_single_run') as mock_run:
            # Mock some failures, some successes
            def mock_execute(executable, run_number, timeout, env_vars):
                if run_number == 2:  # Second run fails
                    return {
                        "run_number": run_number,
                        "stdout": "",
                        "stderr": "",
                        "returncode": -1,
                        "gnu_time": {},
                        "success": False,
                        "error": "Timeout"
                    }
                else:
                    return {
                        "run_number": run_number,
                        "stdout": f"output_{run_number}",
                        "stderr": f"time_{run_number}",
                        "returncode": 0,
                        "gnu_time": {"wall_time_s": 1.0},
                        "success": True,
                        "error": None
                    }

            mock_run.side_effect = mock_execute

            result = execute_instance(
                executable=self.executable,
                repetitions=3,
                timeout=30,
                env_vars={},
                measurements=[]
            )

            # Verify mixed results
            self.assertEqual(result["total_runs"], 3)
            self.assertEqual(result["successful_runs"], 2)
            self.assertEqual(result["failed_runs"], 1)
            self.assertEqual(len(result["runs"]), 2)
            self.assertEqual(len(result["failures"]), 1)

            # Verify failure tracking
            failure = result["failures"][0]
            self.assertEqual(failure["run_number"], 2)
            self.assertIn("Timeout", failure["error"])

    def test_execute_instance_all_fail(self):
        """Test instance execution where all repetitions fail."""
        with patch('tools.experiment_framework.collector.execute_single_run') as mock_run:
            # Mock all failed runs
            def mock_execute(executable, run_number, timeout, env_vars):
                return {
                    "run_number": run_number,
                    "stdout": "",
                    "stderr": "",
                    "returncode": -1,
                    "gnu_time": {},
                    "success": False,
                    "error": f"Error in run {run_number}"
                }

            mock_run.side_effect = mock_execute

            result = execute_instance(
                executable=self.executable,
                repetitions=3,
                timeout=30,
                env_vars={},
                measurements=[]
            )

            # Verify all failed
            self.assertEqual(result["total_runs"], 3)
            self.assertEqual(result["successful_runs"], 0)
            self.assertEqual(result["failed_runs"], 3)
            self.assertEqual(len(result["runs"]), 0)
            self.assertEqual(len(result["failures"]), 3)

    def test_execute_instance_executable_not_found(self):
        """Test instance execution with non-existent executable."""
        nonexistent = Path("/nonexistent/executable")

        result = execute_instance(
            executable=nonexistent,
            repetitions=2,
            timeout=30,
            env_vars={},
            measurements=[]
        )

        # Verify error handling
        self.assertEqual(result["instance_name"], "executable")
        self.assertEqual(result["total_runs"], 2)
        self.assertEqual(result["successful_runs"], 0)
        self.assertEqual(result["failed_runs"], 2)
        self.assertGreater(len(result["failures"]), 0)
        self.assertIn("not found", result["failures"][0]["error"])

    def test_execute_instance_env_vars_passed(self):
        """Test that environment variables are passed to each run."""
        with patch('tools.experiment_framework.collector.execute_single_run') as mock_run:
            mock_run.return_value = {
                "run_number": 1,
                "stdout": "",
                "stderr": "",
                "returncode": 0,
                "gnu_time": {},
                "success": True,
                "error": None
            }

            env_vars = {"VAR1": "value1", "VAR2": "value2"}

            execute_instance(
                executable=self.executable,
                repetitions=2,
                timeout=30,
                env_vars=env_vars,
                measurements=[]
            )

            # Verify env_vars were passed
            calls = mock_run.call_args_list
            self.assertEqual(len(calls), 2)

            for call_obj in calls:
                # Check that env_vars were passed
                passed_env = call_obj[1]['env_vars']
                self.assertEqual(passed_env, env_vars)

    def test_execute_instance_continuation_on_failure(self):
        """Test that execution continues despite individual run failures."""
        with patch('tools.experiment_framework.collector.execute_single_run') as mock_run:
            # Mock to track call count
            call_count = [0]

            def mock_execute(executable, run_number, timeout, env_vars):
                call_count[0] += 1
                # All runs fail but we should still try all of them
                return {
                    "run_number": run_number,
                    "stdout": "",
                    "stderr": "",
                    "returncode": -1,
                    "gnu_time": {},
                    "success": False,
                    "error": "Failed"
                }

            mock_run.side_effect = mock_execute

            execute_instance(
                executable=self.executable,
                repetitions=5,
                timeout=30,
                env_vars={},
                measurements=[]
            )

            # Verify all runs were attempted despite failures
            self.assertEqual(call_count[0], 5)

    def test_execute_instance_return_structure(self):
        """Test that return structure has all required fields."""
        with patch('tools.experiment_framework.collector.execute_single_run') as mock_run:
            mock_run.return_value = {
                "run_number": 1,
                "stdout": "output",
                "stderr": "time",
                "returncode": 0,
                "gnu_time": {"wall_time_s": 1.5},
                "success": True,
                "error": None
            }

            result = execute_instance(
                executable=self.executable,
                repetitions=1,
                timeout=30,
                env_vars={},
                measurements=[]
            )

            # Verify all required fields exist
            required_fields = [
                "instance_name",
                "runs",
                "failures",
                "total_runs",
                "successful_runs",
                "failed_runs"
            ]

            for field in required_fields:
                self.assertIn(field, result)

            # Verify run structure
            run = result["runs"][0]
            run_required_fields = [
                "run_number",
                "stdout",
                "stderr",
                "returncode",
                "gnu_time",
                "measurements"
            ]

            for field in run_required_fields:
                self.assertIn(field, run)


class TestIntegration(unittest.TestCase):
    """Integration tests for collector module."""

    def setUp(self):
        """Set up test fixtures."""
        self.test_dir = tempfile.TemporaryDirectory()
        # Create a simple executable that succeeds
        self.success_executable = Path(self.test_dir.name) / "success"
        self.success_executable.write_text("#!/bin/bash\nexit 0")
        self.success_executable.chmod(0o755)

        # Create an executable that fails
        self.fail_executable = Path(self.test_dir.name) / "fail"
        self.fail_executable.write_text("#!/bin/bash\nexit 1")
        self.fail_executable.chmod(0o755)

    def tearDown(self):
        """Clean up test fixtures."""
        self.test_dir.cleanup()

    @patch('tools.experiment_framework.collector.parse_time_output')
    def test_single_run_integration(self, mock_parse_time):
        """Test single run with mocked time parsing."""
        mock_parse_time.return_value = {
            'user_time_s': 0.5,
            'system_time_s': 0.1,
            'wall_time_s': 0.6,
            'max_rss_bytes': 2048
        }

        result = execute_single_run(
            executable=self.success_executable,
            run_number=1,
            timeout=5,
            env_vars={}
        )

        self.assertTrue(result["success"])
        self.assertEqual(result["run_number"], 1)
        self.assertGreater(len(result["gnu_time"]), 0)

    @patch('tools.experiment_framework.collector.parse_time_output')
    def test_instance_integration(self, mock_parse_time):
        """Test full instance execution with mocked time parsing."""
        mock_parse_time.return_value = {
            'user_time_s': 0.5,
            'wall_time_s': 0.6,
            'max_rss_bytes': 2048
        }

        result = execute_instance(
            executable=self.success_executable,
            repetitions=2,
            timeout=5,
            env_vars={"TEST": "value"},
            measurements=[]
        )

        self.assertEqual(result["successful_runs"], 2)
        self.assertEqual(result["failed_runs"], 0)
        self.assertEqual(len(result["runs"]), 2)

        for run in result["runs"]:
            self.assertIsNotNone(run["stdout"])
            self.assertIsNotNone(run["gnu_time"])


class TestComputeMeasurementStatistics(unittest.TestCase):
    """Test cases for compute_measurement_statistics function."""

    def test_compute_measurement_statistics_normal(self):
        """Test computing statistics with multiple normal values."""
        runs = [
            {'measurements': {'compute_time_s': 1.23}},
            {'measurements': {'compute_time_s': 1.25}},
            {'measurements': {'compute_time_s': 1.24}},
        ]

        result = compute_measurement_statistics(runs, 'compute_time_s')

        self.assertIsNotNone(result)
        self.assertEqual(result['count'], 3)
        self.assertAlmostEqual(result['mean'], 1.24, places=5)
        self.assertAlmostEqual(result['min'], 1.23, places=5)
        self.assertAlmostEqual(result['max'], 1.25, places=5)
        # std for [1.23, 1.25, 1.24] should be ~0.00816
        self.assertAlmostEqual(result['std'], 0.008164966, places=5)

    def test_compute_measurement_statistics_single_value(self):
        """Test computing statistics with a single value."""
        runs = [
            {'measurements': {'memory_mb': 512}},
        ]

        result = compute_measurement_statistics(runs, 'memory_mb')

        self.assertIsNotNone(result)
        self.assertEqual(result['count'], 1)
        self.assertEqual(result['mean'], 512.0)
        self.assertEqual(result['min'], 512)
        self.assertEqual(result['max'], 512)
        self.assertEqual(result['std'], 0.0)

    def test_compute_measurement_statistics_identical_values(self):
        """Test computing statistics with all identical values."""
        runs = [
            {'measurements': {'latency_ms': 100.0}},
            {'measurements': {'latency_ms': 100.0}},
            {'measurements': {'latency_ms': 100.0}},
        ]

        result = compute_measurement_statistics(runs, 'latency_ms')

        self.assertIsNotNone(result)
        self.assertEqual(result['count'], 3)
        self.assertEqual(result['mean'], 100.0)
        self.assertEqual(result['min'], 100.0)
        self.assertEqual(result['max'], 100.0)
        self.assertEqual(result['std'], 0.0)

    def test_compute_measurement_statistics_empty(self):
        """Test computing statistics with empty runs list."""
        runs = []

        result = compute_measurement_statistics(runs, 'any_measurement')

        self.assertIsNone(result)

    def test_compute_measurement_statistics_missing_measurement(self):
        """Test when measurement is not found in any run."""
        runs = [
            {'measurements': {'other_metric': 10}},
            {'measurements': {'other_metric': 20}},
        ]

        result = compute_measurement_statistics(runs, 'missing_metric')

        self.assertIsNone(result)

    def test_compute_measurement_statistics_missing_in_some_runs(self):
        """Test when measurement is missing in some but not all runs."""
        runs = [
            {'measurements': {'compute_time_s': 1.23, 'memory_mb': 512}},
            {'measurements': {'compute_time_s': 1.25}},  # memory_mb missing
            {'measurements': {'compute_time_s': 1.24, 'memory_mb': 520}},
        ]

        # Test for compute_time_s (all runs have it)
        result = compute_measurement_statistics(runs, 'compute_time_s')
        self.assertEqual(result['count'], 3)

        # Test for memory_mb (only 2 runs have it)
        result = compute_measurement_statistics(runs, 'memory_mb')
        self.assertEqual(result['count'], 2)
        self.assertAlmostEqual(result['mean'], 516.0, places=5)

    def test_compute_measurement_statistics_integer_values(self):
        """Test computing statistics with integer values."""
        runs = [
            {'measurements': {'count': 100}},
            {'measurements': {'count': 200}},
            {'measurements': {'count': 300}},
        ]

        result = compute_measurement_statistics(runs, 'count')

        self.assertIsNotNone(result)
        self.assertEqual(result['count'], 3)
        self.assertEqual(result['mean'], 200.0)
        self.assertEqual(result['min'], 100)
        self.assertEqual(result['max'], 300)
        # std for [100, 200, 300] = sqrt(10000) ~= 81.649
        self.assertAlmostEqual(result['std'], 81.649658, places=5)

    def test_compute_measurement_statistics_mixed_int_float(self):
        """Test computing statistics with mixed int and float values."""
        runs = [
            {'measurements': {'value': 1}},
            {'measurements': {'value': 2.5}},
            {'measurements': {'value': 3}},
        ]

        result = compute_measurement_statistics(runs, 'value')

        self.assertIsNotNone(result)
        self.assertEqual(result['count'], 3)
        self.assertAlmostEqual(result['mean'], 2.1666666, places=5)

    def test_compute_measurement_statistics_non_numeric_error(self):
        """Test error handling with non-numeric values."""
        runs = [
            {'measurements': {'metric': 'not_a_number'}},
        ]

        with self.assertRaises(ValueError) as context:
            compute_measurement_statistics(runs, 'metric')

        self.assertIn('Non-numeric', str(context.exception))

    def test_compute_measurement_statistics_with_none_measurement(self):
        """Test error handling when measurement value is None."""
        runs = [
            {'measurements': {'metric': None}},
        ]

        with self.assertRaises(ValueError):
            compute_measurement_statistics(runs, 'metric')

    def test_compute_measurement_statistics_large_dataset(self):
        """Test with a larger dataset."""
        runs = [
            {'measurements': {'value': float(i)}} for i in range(1, 101)
        ]

        result = compute_measurement_statistics(runs, 'value')

        self.assertIsNotNone(result)
        self.assertEqual(result['count'], 100)
        self.assertAlmostEqual(result['mean'], 50.5, places=5)
        self.assertEqual(result['min'], 1.0)
        self.assertEqual(result['max'], 100.0)
        # std for 1..100: sqrt(8325/100) ~= 28.866
        self.assertAlmostEqual(result['std'], 28.866070, places=5)

    def test_compute_measurement_statistics_no_measurements_dict(self):
        """Test when runs don't have measurements dict."""
        runs = [
            {'run_number': 1},  # No 'measurements' key
            {'run_number': 2},
        ]

        result = compute_measurement_statistics(runs, 'any_metric')

        self.assertIsNone(result)

    def test_compute_measurement_statistics_precision(self):
        """Test floating-point precision in calculations."""
        runs = [
            {'measurements': {'value': 0.1}},
            {'measurements': {'value': 0.2}},
            {'measurements': {'value': 0.3}},
        ]

        result = compute_measurement_statistics(runs, 'value')

        self.assertIsNotNone(result)
        self.assertAlmostEqual(result['mean'], 0.2, places=10)


class TestComputeAllStatistics(unittest.TestCase):
    """Test cases for compute_all_statistics function."""

    def test_compute_all_statistics_multiple_measurements(self):
        """Test computing statistics for multiple measurements."""
        execution_result = {
            'instance_name': 'test_instance',
            'runs': [
                {
                    'run_number': 1,
                    'measurements': {
                        'compute_time_s': 1.23,
                        'max_rss_bytes': 104857600,
                        'init_time_s': 0.15
                    }
                },
                {
                    'run_number': 2,
                    'measurements': {
                        'compute_time_s': 1.25,
                        'max_rss_bytes': 104900000,
                        'init_time_s': 0.16
                    }
                },
                {
                    'run_number': 3,
                    'measurements': {
                        'compute_time_s': 1.24,
                        'max_rss_bytes': 104800000,
                        'init_time_s': 0.15
                    }
                }
            ],
            'failures': []
        }

        result = compute_all_statistics(execution_result)

        # Check structure
        self.assertIsInstance(result, dict)
        self.assertEqual(len(result), 3)

        # Check all measurements are present
        self.assertIn('compute_time_s', result)
        self.assertIn('max_rss_bytes', result)
        self.assertIn('init_time_s', result)

        # Check structure of stats
        for measurement_name, stats in result.items():
            self.assertIn('mean', stats)
            self.assertIn('std', stats)
            self.assertIn('min', stats)
            self.assertIn('max', stats)
            self.assertIn('count', stats)
            self.assertEqual(stats['count'], 3)

    def test_compute_all_statistics_multiple_measurements_partial(self):
        """Test when some measurements are missing in some runs."""
        execution_result = {
            'instance_name': 'test_instance',
            'runs': [
                {
                    'run_number': 1,
                    'measurements': {
                        'compute_time_s': 1.23,
                        'memory_mb': 512,
                        'io_ops': 100
                    }
                },
                {
                    'run_number': 2,
                    'measurements': {
                        'compute_time_s': 1.25,
                        'memory_mb': 520
                        # io_ops missing
                    }
                },
                {
                    'run_number': 3,
                    'measurements': {
                        'compute_time_s': 1.24,
                        'io_ops': 110
                        # memory_mb missing
                    }
                }
            ],
            'failures': []
        }

        result = compute_all_statistics(execution_result)

        # All measurements should be present
        self.assertEqual(len(result), 3)

        # Check counts for each measurement
        self.assertEqual(result['compute_time_s']['count'], 3)
        self.assertEqual(result['memory_mb']['count'], 2)
        self.assertEqual(result['io_ops']['count'], 2)

    def test_compute_all_statistics_no_runs(self):
        """Test with empty execution result."""
        execution_result = {
            'instance_name': 'test_instance',
            'runs': [],
            'failures': []
        }

        result = compute_all_statistics(execution_result)

        self.assertEqual(result, {})

    def test_compute_all_statistics_no_measurements(self):
        """Test when runs exist but have no measurements."""
        execution_result = {
            'instance_name': 'test_instance',
            'runs': [
                {'run_number': 1},
                {'run_number': 2}
            ],
            'failures': []
        }

        result = compute_all_statistics(execution_result)

        self.assertEqual(result, {})

    def test_compute_all_statistics_runs_with_empty_measurements(self):
        """Test when runs have empty measurements dicts."""
        execution_result = {
            'instance_name': 'test_instance',
            'runs': [
                {'run_number': 1, 'measurements': {}},
                {'run_number': 2, 'measurements': {}}
            ],
            'failures': []
        }

        result = compute_all_statistics(execution_result)

        self.assertEqual(result, {})

    def test_compute_all_statistics_one_measurement(self):
        """Test with only one measurement."""
        execution_result = {
            'instance_name': 'test_instance',
            'runs': [
                {'run_number': 1, 'measurements': {'response_time_ms': 42.5}},
                {'run_number': 2, 'measurements': {'response_time_ms': 45.2}},
            ],
            'failures': []
        }

        result = compute_all_statistics(execution_result)

        self.assertEqual(len(result), 1)
        self.assertIn('response_time_ms', result)
        self.assertEqual(result['response_time_ms']['count'], 2)

    def test_compute_all_statistics_different_measurement_availability(self):
        """Test measurements available in different combinations of runs."""
        execution_result = {
            'instance_name': 'test_instance',
            'runs': [
                {
                    'run_number': 1,
                    'measurements': {'metric_a': 10, 'metric_b': 20, 'metric_c': 30}
                },
                {
                    'run_number': 2,
                    'measurements': {'metric_a': 15, 'metric_c': 35}
                },
                {
                    'run_number': 3,
                    'measurements': {'metric_b': 25, 'metric_c': 40}
                }
            ],
            'failures': []
        }

        result = compute_all_statistics(execution_result)

        self.assertEqual(len(result), 3)
        self.assertEqual(result['metric_a']['count'], 2)
        self.assertEqual(result['metric_b']['count'], 2)
        self.assertEqual(result['metric_c']['count'], 3)

    def test_compute_all_statistics_single_run(self):
        """Test with only one run."""
        execution_result = {
            'instance_name': 'test_instance',
            'runs': [
                {
                    'run_number': 1,
                    'measurements': {
                        'time_s': 5.0,
                        'memory_mb': 1024
                    }
                }
            ],
            'failures': []
        }

        result = compute_all_statistics(execution_result)

        self.assertEqual(len(result), 2)
        self.assertEqual(result['time_s']['count'], 1)
        self.assertEqual(result['memory_mb']['count'], 1)
        self.assertEqual(result['time_s']['std'], 0.0)
        self.assertEqual(result['memory_mb']['std'], 0.0)

    def test_compute_all_statistics_consistent_ordering(self):
        """Test that measurement order is consistent (alphabetical)."""
        execution_result = {
            'instance_name': 'test_instance',
            'runs': [
                {
                    'run_number': 1,
                    'measurements': {
                        'zebra_metric': 1,
                        'apple_metric': 2,
                        'banana_metric': 3
                    }
                }
            ],
            'failures': []
        }

        result = compute_all_statistics(execution_result)

        # Keys should be in alphabetical order
        keys = list(result.keys())
        self.assertEqual(keys, sorted(keys))

    def test_compute_all_statistics_mixed_types(self):
        """Test with mixed int and float measurements."""
        execution_result = {
            'instance_name': 'test_instance',
            'runs': [
                {
                    'run_number': 1,
                    'measurements': {
                        'count': 100,  # int
                        'rate': 3.14,  # float
                    }
                },
                {
                    'run_number': 2,
                    'measurements': {
                        'count': 200,  # int
                        'rate': 3.15,  # float
                    }
                }
            ],
            'failures': []
        }

        result = compute_all_statistics(execution_result)

        self.assertEqual(len(result), 2)
        self.assertEqual(result['count']['count'], 2)
        self.assertEqual(result['rate']['count'], 2)

    def test_compute_all_statistics_large_dataset(self):
        """Test with a large number of runs and measurements."""
        # Create 100 runs with 10 measurements each
        runs = []
        for run_num in range(1, 101):
            measurements = {
                f'metric_{i}': float(run_num + i) for i in range(10)
            }
            runs.append({
                'run_number': run_num,
                'measurements': measurements
            })

        execution_result = {
            'instance_name': 'test_instance',
            'runs': runs,
            'failures': []
        }

        result = compute_all_statistics(execution_result)

        self.assertEqual(len(result), 10)
        for i in range(10):
            metric_name = f'metric_{i}'
            self.assertIn(metric_name, result)
            self.assertEqual(result[metric_name]['count'], 100)

    def test_compute_all_statistics_error_propagation(self):
        """Test that errors in measurement computation are propagated."""
        execution_result = {
            'instance_name': 'test_instance',
            'runs': [
                {
                    'run_number': 1,
                    'measurements': {
                        'valid_metric': 10,
                        'invalid_metric': 'not_a_number'
                    }
                }
            ],
            'failures': []
        }

        with self.assertRaises(ValueError):
            compute_all_statistics(execution_result)

    def test_compute_all_statistics_missing_runs_key(self):
        """Test handling when 'runs' key is missing."""
        execution_result = {
            'instance_name': 'test_instance',
            'failures': []
        }

        result = compute_all_statistics(execution_result)

        self.assertEqual(result, {})

    def test_compute_all_statistics_detailed_accuracy(self):
        """Test statistical accuracy with known values."""
        # Create data with known mean (50) and variance (250)
        # std = sqrt(250) ~= 15.811
        execution_result = {
            'instance_name': 'test_instance',
            'runs': [
                {'run_number': i, 'measurements': {'value': float(i * 10)}}
                for i in range(1, 11)  # 10, 20, ..., 100
            ],
            'failures': []
        }

        result = compute_all_statistics(execution_result)

        stats = result['value']
        self.assertEqual(stats['count'], 10)
        self.assertAlmostEqual(stats['mean'], 55.0, places=5)
        self.assertAlmostEqual(stats['min'], 10.0, places=5)
        self.assertAlmostEqual(stats['max'], 100.0, places=5)
        # std should be sqrt(sum((x-55)^2)/10) = sqrt(8250/10) = sqrt(825) ~= 28.722
        self.assertAlmostEqual(stats['std'], 28.722813, places=5)


class TestParseRegexMeasurement(unittest.TestCase):
    """Test cases for parse_regex_measurement function."""

    def test_parse_regex_measurement_pattern_found(self):
        """Test regex extraction when pattern is found."""
        output = "Total time: 42.5 seconds"
        pattern = r"Total time: (\d+\.\d+) seconds"
        value = parse_regex_measurement(output, pattern, 1, 'float')
        self.assertEqual(value, 42.5)

    def test_parse_regex_measurement_multiple_groups(self):
        """Test extracting specific group from pattern."""
        output = "Start: 10:00:00 End: 20:00:00"
        pattern = r"Start: (\d+:\d+:\d+) End: (\d+:\d+:\d+)"
        value = parse_regex_measurement(output, pattern, 2, 'str')
        self.assertEqual(value, "20:00:00")

    def test_parse_regex_measurement_pattern_not_found(self):
        """Test when pattern is not found."""
        output = "No time information"
        pattern = r"Total time: (\d+\.\d+) seconds"
        value = parse_regex_measurement(output, pattern, 1, 'float')
        self.assertIsNone(value)

    def test_parse_regex_measurement_multiline(self):
        """Test regex extraction across multiple lines."""
        output = "Line 1\nTotal time: 123.45 seconds\nLine 3"
        pattern = r"Total time: (\d+\.\d+) seconds"
        value = parse_regex_measurement(output, pattern, 1, 'float')
        self.assertEqual(value, 123.45)

    def test_parse_regex_measurement_int_conversion(self):
        """Test integer conversion."""
        output = "Count: 42"
        pattern = r"Count: (\d+)"
        value = parse_regex_measurement(output, pattern, 1, 'int')
        self.assertEqual(value, 42)
        self.assertIsInstance(value, int)

    def test_parse_regex_measurement_bool_conversion_true(self):
        """Test boolean conversion for true values."""
        output = "Status: passed"
        pattern = r"Status: (\w+)"
        value = parse_regex_measurement(output, pattern, 1, 'bool')
        self.assertTrue(value)

    def test_parse_regex_measurement_bool_conversion_false(self):
        """Test boolean conversion for false values."""
        output = "Status: failed"
        pattern = r"Status: (\w+)"
        value = parse_regex_measurement(output, pattern, 1, 'bool')
        self.assertFalse(value)

    def test_parse_regex_measurement_string_type(self):
        """Test string type preservation."""
        output = "Version: v1.2.3"
        pattern = r"Version: (v[\d.]+)"
        value = parse_regex_measurement(output, pattern, 1, 'str')
        self.assertEqual(value, "v1.2.3")
        self.assertIsInstance(value, str)

    def test_parse_regex_measurement_invalid_regex(self):
        """Test error handling for invalid regex."""
        output = "test"
        pattern = r"(?P<invalid"  # Invalid regex
        with self.assertRaises(ValueError) as context:
            parse_regex_measurement(output, pattern, 1, 'float')
        self.assertIn("Invalid regex", str(context.exception))

    def test_parse_regex_measurement_invalid_group(self):
        """Test error handling for missing group."""
        output = "test"
        pattern = r"(test)"
        with self.assertRaises(ValueError) as context:
            parse_regex_measurement(output, pattern, 5, 'float')  # Group 5 doesn't exist
        self.assertIn("not found", str(context.exception))

    def test_parse_regex_measurement_invalid_conversion(self):
        """Test error handling for invalid type conversion."""
        output = "Value: not_a_number"
        pattern = r"Value: (\w+)"
        with self.assertRaises(ValueError) as context:
            parse_regex_measurement(output, pattern, 1, 'int')
        self.assertIn("Failed to convert", str(context.exception))


class TestParseJsonMeasurement(unittest.TestCase):
    """Test cases for parse_json_measurement function."""

    def test_parse_json_measurement_simple(self):
        """Test simple JSON key extraction."""
        output = '{"timing": 42.5}'
        key_path = ['timing']
        value = parse_json_measurement(output, key_path, 'float')
        self.assertEqual(value, 42.5)

    def test_parse_json_measurement_nested(self):
        """Test nested key path extraction."""
        output = '{"results": {"timing": {"wall_time": 123.45}}}'
        key_path = ['results', 'timing', 'wall_time']
        value = parse_json_measurement(output, key_path, 'float')
        self.assertEqual(value, 123.45)

    def test_parse_json_measurement_int_conversion(self):
        """Test integer conversion from JSON."""
        output = '{"count": 42}'
        key_path = ['count']
        value = parse_json_measurement(output, key_path, 'int')
        self.assertEqual(value, 42)
        self.assertIsInstance(value, int)

    def test_parse_json_measurement_bool_conversion(self):
        """Test boolean conversion from JSON."""
        output = '{"success": true}'
        key_path = ['success']
        value = parse_json_measurement(output, key_path, 'bool')
        self.assertTrue(value)

    def test_parse_json_measurement_string_conversion(self):
        """Test string conversion from JSON."""
        output = '{"name": "test_instance"}'
        key_path = ['name']
        value = parse_json_measurement(output, key_path, 'str')
        self.assertEqual(value, "test_instance")

    def test_parse_json_measurement_invalid_json(self):
        """Test error handling for invalid JSON."""
        output = "not valid json"
        key_path = ['key']
        value = parse_json_measurement(output, key_path, 'float')
        self.assertIsNone(value)

    def test_parse_json_measurement_key_not_found(self):
        """Test error handling when key path not found."""
        output = '{"data": {"value": 10}}'
        key_path = ['data', 'missing_key']
        value = parse_json_measurement(output, key_path, 'float')
        self.assertIsNone(value)

    def test_parse_json_measurement_partial_path_not_found(self):
        """Test error handling when intermediate key is missing."""
        output = '{"data": {"value": 10}}'
        key_path = ['missing', 'value']
        value = parse_json_measurement(output, key_path, 'float')
        self.assertIsNone(value)

    def test_parse_json_measurement_empty_key_path(self):
        """Test with empty key path."""
        output = '42'  # Valid JSON number with no keys
        key_path = []
        # Empty key path will cause the loop to not run, so it returns the parsed JSON
        value = parse_json_measurement(output, key_path, 'float')
        # This should return the original value
        self.assertEqual(value, 42.0)


class TestParseLineMeasurement(unittest.TestCase):
    """Test cases for parse_line_measurement function."""

    def test_parse_line_measurement_first_line(self):
        """Test extraction from first line."""
        output = "42.5\nLine 2\nLine 3"
        value = parse_line_measurement(output, 0, 'float')
        self.assertEqual(value, 42.5)

    def test_parse_line_measurement_middle_line(self):
        """Test extraction from middle line."""
        output = "Line 1\n123.45\nLine 3"
        value = parse_line_measurement(output, 1, 'float')
        self.assertEqual(value, 123.45)

    def test_parse_line_measurement_last_line(self):
        """Test extraction from last line."""
        output = "Line 1\nLine 2\n999.99"
        value = parse_line_measurement(output, 2, 'float')
        self.assertEqual(value, 999.99)

    def test_parse_line_measurement_int_conversion(self):
        """Test integer conversion from line."""
        output = "Value\n42\nOther"
        value = parse_line_measurement(output, 1, 'int')
        self.assertEqual(value, 42)
        self.assertIsInstance(value, int)

    def test_parse_line_measurement_bool_conversion(self):
        """Test boolean conversion from line."""
        output = "Status\npassed\nDone"
        value = parse_line_measurement(output, 1, 'bool')
        self.assertTrue(value)

    def test_parse_line_measurement_string_type(self):
        """Test string type preservation."""
        output = "Status\nrunning\nDone"
        value = parse_line_measurement(output, 1, 'str')
        self.assertEqual(value, "running")
        self.assertIsInstance(value, str)

    def test_parse_line_measurement_line_out_of_bounds(self):
        """Test error handling when line number is out of bounds."""
        output = "Line 1\nLine 2"
        value = parse_line_measurement(output, 5, 'float')
        self.assertIsNone(value)

    def test_parse_line_measurement_negative_index(self):
        """Test error handling for negative line number."""
        output = "Line 1\nLine 2"
        value = parse_line_measurement(output, -1, 'float')
        self.assertIsNone(value)

    def test_parse_line_measurement_empty_line(self):
        """Test handling of empty line."""
        output = "Line 1\n\nLine 3"
        value = parse_line_measurement(output, 1, 'float')
        self.assertIsNone(value)

    def test_parse_line_measurement_whitespace_line(self):
        """Test handling of whitespace-only line."""
        output = "Line 1\n   \nLine 3"
        value = parse_line_measurement(output, 1, 'float')
        self.assertIsNone(value)

    def test_parse_line_measurement_invalid_conversion(self):
        """Test error handling for invalid type conversion."""
        output = "Line 1\nnot_a_number\nLine 3"
        with self.assertRaises(ValueError) as context:
            parse_line_measurement(output, 1, 'int')
        self.assertIn("Failed to convert", str(context.exception))

    def test_parse_line_measurement_with_trailing_newline(self):
        """Test extraction with trailing newline."""
        output = "Line 1\n42.5\n"
        value = parse_line_measurement(output, 1, 'float')
        self.assertEqual(value, 42.5)


class TestParseMeasurement(unittest.TestCase):
    """Test cases for parse_measurement dispatcher function."""

    def test_parse_measurement_regex_required_found(self):
        """Test regex measurement when required and found."""
        output = "Total time: 50.5 seconds"
        spec = {
            "name": "wall_time",
            "type": "regex",
            "parser": {
                "type": "regex",
                "pattern": r"Total time: (\d+\.\d+) seconds",
                "group": 1
            },
            "required": True
        }
        value = parse_measurement(output, spec)
        self.assertEqual(value, "50.5")  # Returns as string since no measurement type specified

    def test_parse_measurement_regex_required_missing(self):
        """Test regex measurement when required but not found."""
        output = "No timing information"
        spec = {
            "name": "wall_time",
            "type": "regex",
            "parser": {
                "type": "regex",
                "pattern": r"Total time: (\d+\.\d+) seconds",
                "group": 1
            },
            "required": True
        }
        with self.assertRaises(MeasurementError) as context:
            parse_measurement(output, spec)
        self.assertIn("Required measurement", str(context.exception))
        self.assertIn("wall_time", str(context.exception))

    def test_parse_measurement_regex_optional_missing(self):
        """Test regex measurement when optional and missing."""
        output = "No timing information"
        spec = {
            "name": "optional_time",
            "type": "regex",
            "parser": {
                "type": "regex",
                "pattern": r"Total time: (\d+\.\d+) seconds",
                "group": 1
            },
            "required": False
        }
        value = parse_measurement(output, spec)
        self.assertIsNone(value)

    def test_parse_measurement_json_required_found(self):
        """Test JSON measurement when required and found."""
        output = '{"results": {"timing": {"wall_time_ms": 50500}}}'
        spec = {
            "name": "wall_time",
            "type": "float",
            "parser": {
                "type": "json",
                "key_path": ["results", "timing", "wall_time_ms"]
            },
            "unit": "ms",
            "required": True
        }
        value = parse_measurement(output, spec)
        # Should be converted from ms to seconds
        self.assertAlmostEqual(value, 50.5, places=5)

    def test_parse_measurement_json_required_missing(self):
        """Test JSON measurement when required but not found."""
        output = '{"data": {"value": 10}}'
        spec = {
            "name": "timing",
            "type": "float",
            "parser": {
                "type": "json",
                "key_path": ["missing", "key"]
            },
            "required": True
        }
        with self.assertRaises(MeasurementError) as context:
            parse_measurement(output, spec)
        self.assertIn("Required measurement", str(context.exception))

    def test_parse_measurement_line_required_found(self):
        """Test line measurement when required and found."""
        output = "Header\n123.45\nFooter"
        spec = {
            "name": "compute_time",
            "type": "float",
            "parser": {
                "type": "line",
                "line_number": 1
            },
            "required": True
        }
        value = parse_measurement(output, spec)
        self.assertEqual(value, 123.45)

    def test_parse_measurement_line_required_missing(self):
        """Test line measurement when required but not found."""
        output = "Line 1\nLine 2"
        spec = {
            "name": "compute_time",
            "type": "float",
            "parser": {
                "type": "line",
                "line_number": 5
            },
            "required": True
        }
        with self.assertRaises(MeasurementError) as context:
            parse_measurement(output, spec)
        self.assertIn("Required measurement", str(context.exception))

    def test_parse_measurement_unit_conversion_time_ms_to_s(self):
        """Test time unit conversion from milliseconds to seconds."""
        output = "Time: 1500 ms"
        spec = {
            "name": "timing",
            "type": "float",
            "parser": {
                "type": "regex",
                "pattern": r"Time: (\d+) ms",
                "group": 1
            },
            "unit": "ms",
            "required": True
        }
        value = parse_measurement(output, spec)
        self.assertEqual(value, 1.5)

    def test_parse_measurement_unit_conversion_memory_kb_to_bytes(self):
        """Test memory unit conversion from KB to bytes."""
        output = "Memory: 512 KB"
        spec = {
            "name": "memory",
            "type": "float",
            "parser": {
                "type": "regex",
                "pattern": r"Memory: (\d+) KB",
                "group": 1
            },
            "unit": "KB",
            "required": True
        }
        value = parse_measurement(output, spec)
        self.assertEqual(value, 512 * 1024)

    def test_parse_measurement_unit_conversion_memory_mb_to_bytes(self):
        """Test memory unit conversion from MB to bytes."""
        output = "Usage: 256 MB"
        spec = {
            "name": "max_memory",
            "type": "float",
            "parser": {
                "type": "regex",
                "pattern": r"Usage: (\d+) MB",
                "group": 1
            },
            "unit": "MB",
            "required": True
        }
        value = parse_measurement(output, spec)
        self.assertEqual(value, 256 * 1024 * 1024)

    def test_parse_measurement_unit_conversion_time_us_to_s(self):
        """Test time unit conversion from microseconds to seconds."""
        output = "Latency: 500000 us"
        spec = {
            "name": "latency",
            "type": "float",
            "parser": {
                "type": "regex",
                "pattern": r"Latency: (\d+) us",
                "group": 1
            },
            "unit": "us",
            "required": True
        }
        value = parse_measurement(output, spec)
        self.assertAlmostEqual(value, 0.5, places=10)

    def test_parse_measurement_unknown_parser_type(self):
        """Test error handling for unknown parser type."""
        output = "test"
        spec = {
            "name": "test",
            "type": "float",
            "parser": {
                "type": "unknown_parser"
            },
            "required": True
        }
        with self.assertRaises(MeasurementError) as context:
            parse_measurement(output, spec)
        self.assertIn("Unknown parser type", str(context.exception))

    def test_parse_measurement_default_required_true(self):
        """Test that required defaults to True."""
        output = "No data"
        spec = {
            "name": "test",
            "type": "float",
            "parser": {
                "type": "regex",
                "pattern": r"Value: (\d+)",
                "group": 1
            }
            # No 'required' key - should default to True
        }
        with self.assertRaises(MeasurementError):
            parse_measurement(output, spec)

    def test_parse_measurement_multiple_unit_conversions(self):
        """Test multiple measurements with different unit conversions."""
        # Test 1: Time conversion
        output1 = "Time: 2000 ms"
        spec1 = {
            "name": "time",
            "type": "float",
            "parser": {
                "type": "regex",
                "pattern": r"Time: (\d+) ms",
                "group": 1
            },
            "unit": "ms"
        }
        value1 = parse_measurement(output1, spec1)
        self.assertEqual(value1, 2.0)

        # Test 2: Memory conversion
        output2 = "Memory: 1 GB"
        spec2 = {
            "name": "memory",
            "type": "float",
            "parser": {
                "type": "regex",
                "pattern": r"Memory: (\d+) GB",
                "group": 1
            },
            "unit": "GB"
        }
        value2 = parse_measurement(output2, spec2)
        self.assertEqual(value2, 1024**3)

    def test_parse_measurement_no_unit_conversion(self):
        """Test measurement with no unit conversion."""
        output = "Count: 42"
        spec = {
            "name": "count",
            "type": "int",
            "parser": {
                "type": "regex",
                "pattern": r"Count: (\d+)",
                "group": 1
            },
            "required": True
            # No 'unit' key
        }
        value = parse_measurement(output, spec)
        self.assertEqual(value, 42)

    def test_parse_measurement_unknown_unit_warning(self):
        """Test handling of unknown unit (should return value as-is)."""
        output = "Value: 42"
        spec = {
            "name": "value",
            "type": "float",
            "parser": {
                "type": "regex",
                "pattern": r"Value: (\d+)",
                "group": 1
            },
            "unit": "unknown_unit"
        }
        # Should return the original value without conversion
        value = parse_measurement(output, spec)
        self.assertEqual(value, 42.0)


class TestCollectAllMeasurements(unittest.TestCase):
    """Integration tests for collect_all_measurements function."""

    def setUp(self):
        """Set up test fixtures."""
        self.test_dir = tempfile.TemporaryDirectory()
        self.test_dir_path = Path(self.test_dir.name)

        # Create test directory structure
        app_dir = self.test_dir_path / "applications" / "test_app" / "build"
        app_dir.mkdir(parents=True)

        # Create test executables
        self.instance1_dir = app_dir / "instance1"
        self.instance1_dir.mkdir(parents=True)
        self.success_executable1 = self.instance1_dir / "instance1"
        self.success_executable1.write_text("#!/bin/bash\nexit 0")
        self.success_executable1.chmod(0o755)

        self.instance2_dir = app_dir / "instance2"
        self.instance2_dir.mkdir(parents=True)
        self.success_executable2 = self.instance2_dir / "instance2"
        self.success_executable2.write_text("#!/bin/bash\nexit 0")
        self.success_executable2.chmod(0o755)

    def tearDown(self):
        """Clean up test fixtures."""
        self.test_dir.cleanup()

    @patch('tools.experiment_framework.collector.execute_instance')
    def test_collect_all_measurements_all_instances_found(self, mock_execute):
        """Test collect_all_measurements when all executables are found."""
        # Mock execute_instance to return successful results
        def execute_side_effect(*args, **kwargs):
            executable = args[0]
            instance_name = executable.stem
            return {
                'instance_name': instance_name,
                'runs': [
                    {'run_number': 1, 'measurements': {'time': 1.0}},
                    {'run_number': 2, 'measurements': {'time': 1.1}},
                ],
                'failures': [],
                'total_runs': 2,
                'successful_runs': 2,
                'failed_runs': 0,
                'statistics': {'time': {'mean': 1.05, 'std': 0.05, 'min': 1.0, 'max': 1.1, 'count': 2}}
            }

        mock_execute.side_effect = execute_side_effect

        build_results = {
            'successful_instances': [
                {'instance_name': 'instance1'},
                {'instance_name': 'instance2'},
            ]
        }

        config = {
            'application': {'name': 'test_app'},
            'execution': {'repetitions': 2, 'timeout': 30, 'environment': {}},
            'measurements': []
        }

        # Patch Path to use our test directory
        with patch('tools.experiment_framework.collector.Path') as mock_path_class:
            def path_constructor(*args, **kwargs):
                # If it's the base path call, use our test directory
                if args and args[0] == 'applications':
                    return self.test_dir_path / 'applications'
                # For other calls, use real Path
                return Path(*args, **kwargs)

            mock_path_class.side_effect = path_constructor

            result = collect_all_measurements(build_results, config)

            # Verify structure
            self.assertEqual(result['summary']['executed'], 2)
            self.assertEqual(result['summary']['skipped'], 0)
            self.assertEqual(result['summary']['errors'], 0)
            self.assertEqual(len(result['execution_results']), 2)
            self.assertIsNotNone(result['timestamp'])

    def test_collect_all_measurements_some_executables_missing(self):
        """Test collect_all_measurements when some executables don't exist."""
        build_results = {
            'successful_instances': [
                {'instance_name': 'missing_instance'},
                {'instance_name': 'instance1'},
            ]
        }

        config = {
            'application': {'name': 'test_app'},
            'execution': {'repetitions': 2, 'timeout': 30, 'environment': {}},
            'measurements': []
        }

        with patch('tools.experiment_framework.collector.execute_instance') as mock_execute:
            def execute_side_effect(*args, **kwargs):
                executable = args[0]
                instance_name = executable.stem
                return {
                    'instance_name': instance_name,
                    'runs': [{'run_number': 1, 'measurements': {}}],
                    'failures': [],
                    'total_runs': 1,
                    'successful_runs': 1,
                    'failed_runs': 0,
                    'statistics': {}
                }

            mock_execute.side_effect = execute_side_effect

            with patch('tools.experiment_framework.collector.Path') as mock_path_class:
                def path_constructor(*args, **kwargs):
                    if args and args[0] == 'applications':
                        return self.test_dir_path / 'applications'
                    return Path(*args, **kwargs)

                mock_path_class.side_effect = path_constructor

                result = collect_all_measurements(build_results, config)

                # Verify one was skipped, one executed
                self.assertEqual(result['summary']['executed'], 1)
                self.assertEqual(result['summary']['skipped'], 1)
                self.assertEqual(result['summary']['errors'], 0)
                self.assertEqual(len(result['skipped_instances']), 1)
                self.assertEqual(result['skipped_instances'][0]['instance_name'], 'missing_instance')

    @patch('tools.experiment_framework.collector.execute_instance')
    def test_collect_all_measurements_execution_failures(self, mock_execute):
        """Test collect_all_measurements when execution fails for some instances."""
        # Create a failing instance directory
        failing_dir = self.test_dir_path / "applications" / "test_app" / "build" / "failing_instance"
        failing_dir.mkdir(parents=True)
        failing_exe = failing_dir / "failing_instance"
        failing_exe.write_text("#!/bin/bash\nexit 0")
        failing_exe.chmod(0o755)

        # Mock execute_instance to raise error for one instance
        def execute_side_effect(*args, **kwargs):
            executable = args[0]
            instance_name = executable.stem
            if instance_name == 'failing_instance':
                raise RuntimeError("Execution failed")
            return {
                'instance_name': instance_name,
                'runs': [{'run_number': 1, 'measurements': {}}],
                'failures': [],
                'total_runs': 1,
                'successful_runs': 1,
                'failed_runs': 0,
                'statistics': {}
            }

        mock_execute.side_effect = execute_side_effect

        build_results = {
            'successful_instances': [
                {'instance_name': 'instance1'},
                {'instance_name': 'failing_instance'},
            ]
        }

        config = {
            'application': {'name': 'test_app'},
            'execution': {'repetitions': 2, 'timeout': 30, 'environment': {}},
            'measurements': []
        }

        with patch('tools.experiment_framework.collector.Path') as mock_path:
            def path_constructor(*args, **kwargs):
                if args and args[0] == 'applications':
                    return self.test_dir_path / 'applications'
                return Path(*args, **kwargs)

            mock_path.side_effect = path_constructor

            result = collect_all_measurements(build_results, config)

            # Verify one executed, one errored
            self.assertEqual(result['summary']['executed'], 1)
            self.assertEqual(result['summary']['errors'], 1)
            self.assertEqual(len(result['errors']), 1)
            self.assertEqual(result['errors'][0]['instance_name'], 'failing_instance')

    @patch('tools.experiment_framework.collector.execute_instance')
    def test_collect_all_measurements_custom_repetitions_timeout(self, mock_execute):
        """Test that CLI args override YAML config."""
        def execute_side_effect(*args, **kwargs):
            executable = args[0]
            instance_name = executable.stem
            return {
                'instance_name': instance_name,
                'runs': [{'run_number': 1, 'measurements': {}}],
                'failures': [],
                'total_runs': 1,
                'successful_runs': 1,
                'failed_runs': 0,
                'statistics': {}
            }

        mock_execute.side_effect = execute_side_effect

        build_results = {
            'successful_instances': [
                {'instance_name': 'instance1'},
            ]
        }

        config = {
            'application': {'name': 'test_app'},
            'execution': {'repetitions': 5, 'timeout': 300, 'environment': {}},
            'measurements': []
        }

        with patch('tools.experiment_framework.collector.Path') as mock_path:
            def path_constructor(*args, **kwargs):
                if args and args[0] == 'applications':
                    return self.test_dir_path / 'applications'
                return Path(*args, **kwargs)

            mock_path.side_effect = path_constructor

            # Override with CLI args
            result = collect_all_measurements(build_results, config, repetitions=10, timeout=60)

            # Verify function executed successfully
            self.assertEqual(result['summary']['executed'], 1)

            # Verify execute_instance was called with CLI args
            mock_execute.assert_called_once()
            call_args = mock_execute.call_args
            self.assertEqual(call_args[0][1], 10)  # repetitions
            self.assertEqual(call_args[0][2], 60)  # timeout


if __name__ == '__main__':
    unittest.main()
