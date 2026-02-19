"""
Integration tests for Phase 5: CLI Integration (run command).

Tests the complete pipeline orchestration that chains all phases together.
"""

import unittest
from unittest.mock import patch, MagicMock, call, mock_open
from pathlib import Path
import tempfile
import json
import sys
import subprocess
from io import StringIO

from tools.experiment_framework.cli import (
    main,
    run_generate,
    run_build,
    run_execute,
    run_visualize,
    validate_hash
)


class TestRunHelperFunctions(unittest.TestCase):
    """Test cases for run command helper functions."""

    def setUp(self):
        """Set up test fixtures."""
        self.test_dir = tempfile.TemporaryDirectory()
        self.test_dir_path = Path(self.test_dir.name)

        # Create test directory structure
        self.app_dir = self.test_dir_path / "applications" / "test_app" / "experiment"
        self.app_dir.mkdir(parents=True)

        self.results_dir = self.app_dir / "results"
        self.results_dir.mkdir(parents=True)

        self.yaml_path = self.app_dir / "experiments.yaml"
        self.cmake_path = self.app_dir / "CMakeLists.txt"
        self.hash_file = self.app_dir / ".experiments.yaml.hash"

    def tearDown(self):
        """Clean up test fixtures."""
        self.test_dir.cleanup()

    @patch('tools.experiment_framework.generator.generate_cmakelists')
    def test_run_generate_success(self, mock_generate):
        """Test run_generate helper with successful generation."""
        # Setup
        self.yaml_path.write_text("application:\n  name: test_app\n")
        mock_generate.return_value = 42

        # Execute
        with patch('sys.stdout', new=StringIO()) as mock_stdout:
            result = run_generate("test_app", "regression", self.app_dir, self.yaml_path)

        # Verify
        self.assertEqual(result, 0)
        mock_generate.assert_called_once()
        output = mock_stdout.getvalue()
        self.assertIn("✓ Generated 42 instances", output)

    def test_run_generate_missing_yaml(self):
        """Test run_generate with missing YAML file."""
        # Execute
        with patch('sys.stderr', new=StringIO()) as mock_stderr:
            result = run_generate("test_app", "regression", self.app_dir, self.yaml_path)

        # Verify
        self.assertEqual(result, 2)
        error = mock_stderr.getvalue()
        self.assertIn("ERROR", error)
        self.assertIn("not found", error)

    @patch('tools.experiment_framework.generator.generate_cmakelists')
    def test_run_generate_config_error(self, mock_generate):
        """Test run_generate with configuration error."""
        from tools.experiment_framework.config import ConfigError

        # Setup
        self.yaml_path.write_text("invalid: yaml")
        mock_generate.side_effect = ConfigError("Invalid config")

        # Execute
        with patch('sys.stderr', new=StringIO()) as mock_stderr:
            result = run_generate("test_app", "regression", self.app_dir, self.yaml_path)

        # Verify
        self.assertEqual(result, 2)
        error = mock_stderr.getvalue()
        self.assertIn("Configuration error", error)

    @patch('tools.experiment_framework.builder.write_build_results')
    @patch('tools.experiment_framework.builder.build_all_instances')
    @patch('tools.experiment_framework.config.load_experiments_yaml')
    def test_run_build_success(self, mock_load_yaml, mock_build, mock_write):
        """Test run_build helper with successful build."""
        from tools.experiment_framework.builder import BuildResults, BuildResult

        # Setup
        self.yaml_path.write_text("application:\n  name: test_app\n")
        self.cmake_path.write_text("iara_add_experiment_instance(instance1)\niara_add_experiment_instance(instance2)\n")

        mock_load_yaml.return_value = {'application': {'name': 'test_app'}}

        mock_result = BuildResult(
            success=True,
            instance_name="instance1",
            attempt=1,
            timestamp="2024-01-15T10:00:00+00:00",
            compilation={},
            binary_size_bytes=1024
        )
        mock_build.return_value = BuildResults(
            successful_instances=[mock_result, mock_result],
            failed_instances=[]
        )

        # Execute
        with patch('sys.stdout', new=StringIO()) as mock_stdout:
            result = run_build("test_app", "regression", self.app_dir, self.yaml_path)

        # Verify
        self.assertEqual(result, 0)
        output = mock_stdout.getvalue()
        self.assertIn("✓ Built 2/2 instances", output)

    @patch('tools.experiment_framework.builder.write_build_results')
    @patch('tools.experiment_framework.builder.build_all_instances')
    @patch('tools.experiment_framework.config.load_experiments_yaml')
    def test_run_build_partial_failure(self, mock_load_yaml, mock_build, mock_write):
        """Test run_build with some failed instances."""
        from tools.experiment_framework.builder import BuildResults, BuildResult, FailedBuildResult

        # Setup
        self.yaml_path.write_text("application:\n  name: test_app\n")
        self.cmake_path.write_text("iara_add_experiment_instance(instance1)\niara_add_experiment_instance(instance2)\n")

        mock_load_yaml.return_value = {'application': {'name': 'test_app'}}

        mock_success = BuildResult(
            success=True,
            instance_name="instance1",
            attempt=1,
            timestamp="2024-01-15T10:00:00+00:00",
            compilation={},
            binary_size_bytes=1024
        )
        mock_failure = FailedBuildResult(
            instance_name="instance2",
            attempts=2,
            errors=[],
            timestamp="2024-01-15T10:00:00+00:00",
            last_error="Build failed"
        )
        mock_build.return_value = BuildResults(
            successful_instances=[mock_success],
            failed_instances=[mock_failure]
        )

        # Execute
        with patch('sys.stdout', new=StringIO()) as mock_stdout:
            with patch('sys.stderr', new=StringIO()) as mock_stderr:
                result = run_build("test_app", "regression", self.app_dir, self.yaml_path)

        # Verify
        self.assertEqual(result, 1)
        output = mock_stdout.getvalue()
        self.assertIn("✓ Built 1/2 instances", output)
        error = mock_stderr.getvalue()
        self.assertIn("Warning", error)

    @patch('tools.experiment_framework.config.load_experiments_yaml')
    def test_run_build_missing_cmake(self, mock_load_yaml):
        """Test run_build with missing CMakeLists.txt."""
        # Setup
        self.yaml_path.write_text("application:\n  name: test_app\n")
        mock_load_yaml.return_value = {'application': {'name': 'test_app'}}

        # Execute
        with patch('sys.stderr', new=StringIO()) as mock_stderr:
            result = run_build("test_app", "regression", self.app_dir, self.yaml_path)

        # Verify
        self.assertEqual(result, 2)
        error = mock_stderr.getvalue()
        self.assertIn("CMakeLists.txt not found", error)

    @patch('tools.experiment_framework.builder.write_build_results')
    @patch('tools.experiment_framework.builder.update_instance_execution')
    @patch('tools.experiment_framework.collector.collect_all_measurements')
    @patch('tools.experiment_framework.builder.read_build_results')
    @patch('tools.experiment_framework.config.load_experiments_yaml')
    def test_run_execute_success(self, mock_load_yaml, mock_read_results, mock_collect, mock_update, mock_write):
        """Test run_execute helper with successful execution."""
        from tools.experiment_framework.builder import BuildResults, BuildResult

        # Setup
        self.yaml_path.write_text("application:\n  name: test_app\n")

        results_json = self.results_dir / "results_2024-01-15T100000Z.json"
        results_json.write_text("{}")

        mock_load_yaml.return_value = {'application': {'name': 'test_app'}}

        mock_result = BuildResult(
            success=True,
            instance_name="instance1",
            attempt=1,
            timestamp="2024-01-15T10:00:00+00:00",
            compilation={},
            binary_size_bytes=1024
        )
        mock_build_results = BuildResults(
            successful_instances=[mock_result],
            failed_instances=[]
        )
        mock_read_results.return_value = mock_build_results

        mock_collect.return_value = {
            'execution_results': [
                {
                    'instance_name': 'instance1',
                    'runs': [],
                    'total_runs': 1,
                    'successful_runs': 1,
                    'failed_runs': 0
                }
            ],
            'summary': {
                'executed': 1,
                'skipped': 0,
                'errors': 0
            }
        }

        # Execute
        with patch('sys.stdout', new=StringIO()) as mock_stdout:
            result = run_execute("test_app", "regression", self.app_dir, self.yaml_path)

        # Verify
        self.assertEqual(result, 0)
        output = mock_stdout.getvalue()
        self.assertIn("✓ Executed 1 instances", output)

    def test_run_execute_missing_results(self):
        """Test run_execute with missing results directory."""
        # Setup
        self.yaml_path.write_text("application:\n  name: test_app\n")
        self.results_dir.rmdir()

        # Execute
        with patch('sys.stderr', new=StringIO()) as mock_stderr:
            result = run_execute("test_app", "regression", self.app_dir, self.yaml_path)

        # Verify
        self.assertEqual(result, 2)
        error = mock_stderr.getvalue()
        self.assertIn("Results directory not found", error)

    @patch('tools.experiment_framework.notebook.execute_notebook')
    @patch('tools.experiment_framework.notebook.generate_notebook')
    @patch('tools.experiment_framework.visualizer.generate_vegalite_json')
    @patch('tools.experiment_framework.config.load_experiments_yaml')
    def test_run_visualize_success(self, mock_load_yaml, mock_vegalite, mock_notebook, mock_execute):
        """Test run_visualize helper with successful visualization."""
        # Setup
        self.yaml_path.write_text("application:\n  name: test_app\n")

        results_json = self.results_dir / "results_2024-01-15T100000Z.json"
        results_json.write_text("{}")

        mock_load_yaml.return_value = {
            'application': {'name': 'test_app'},
            'visualization': {'plots': []}
        }
        mock_vegalite.return_value = [Path("plot1.json"), Path("plot2.json")]

        # Execute
        with patch('sys.stdout', new=StringIO()) as mock_stdout:
            result = run_visualize("test_app", "regression", self.app_dir, self.yaml_path)

        # Verify
        self.assertEqual(result, 0)
        output = mock_stdout.getvalue()
        self.assertIn("✓ Generated 2 plot(s)", output)
        self.assertIn("✓ Generated notebook", output)
        self.assertIn("✓ Executed notebook", output)

    def test_run_visualize_missing_results(self):
        """Test run_visualize with missing results."""
        # Setup
        self.yaml_path.write_text("application:\n  name: test_app\n")
        self.results_dir.rmdir()

        # Execute
        with patch('sys.stderr', new=StringIO()) as mock_stderr:
            result = run_visualize("test_app", "regression", self.app_dir, self.yaml_path)

        # Verify
        self.assertEqual(result, 2)
        error = mock_stderr.getvalue()
        self.assertIn("Results directory not found", error)

    @patch('tools.experiment_framework.config.compute_yaml_hash')
    def test_validate_hash_success(self, mock_hash):
        """Test validate_hash with matching hashes."""
        # Setup
        self.yaml_path.write_text("application:\n  name: test_app\n")
        self.hash_file.write_text("abc123")
        mock_hash.return_value = "abc123"

        # Execute
        result = validate_hash(self.yaml_path, self.app_dir)

        # Verify
        self.assertEqual(result, 0)

    @patch('tools.experiment_framework.config.compute_yaml_hash')
    def test_validate_hash_mismatch(self, mock_hash):
        """Test validate_hash with mismatched hashes."""
        # Setup
        self.yaml_path.write_text("application:\n  name: test_app\n")
        self.hash_file.write_text("abc123")
        mock_hash.return_value = "def456"

        # Execute
        with patch('sys.stderr', new=StringIO()) as mock_stderr:
            result = validate_hash(self.yaml_path, self.app_dir)

        # Verify
        self.assertEqual(result, 3)
        error = mock_stderr.getvalue()
        self.assertIn("hash mismatch", error)

    def test_validate_hash_missing_file(self):
        """Test validate_hash with missing hash file."""
        # Setup
        self.yaml_path.write_text("application:\n  name: test_app\n")

        # Execute
        with patch('sys.stderr', new=StringIO()) as mock_stderr:
            result = validate_hash(self.yaml_path, self.app_dir)

        # Verify
        self.assertEqual(result, 2)
        error = mock_stderr.getvalue()
        self.assertIn("Hash file not found", error)


class TestRunCommandIntegration(unittest.TestCase):
    """Integration tests for the run command orchestration."""

    @patch('tools.experiment_framework.cli.run_visualize')
    @patch('tools.experiment_framework.cli.run_execute')
    @patch('tools.experiment_framework.cli.run_build')
    @patch('tools.experiment_framework.cli.run_generate')
    def test_run_full_pipeline(self, mock_gen, mock_build, mock_exec, mock_viz):
        """Test full pipeline run without skip flags."""
        # Setup
        mock_gen.return_value = 0
        mock_build.return_value = 0
        mock_exec.return_value = 0
        mock_viz.return_value = 0

        # Execute
        with patch('sys.argv', ['cli', 'run', '--app', 'test_app', '--set', 'regression']):
            with patch('sys.stdout', new=StringIO()) as mock_stdout:
                result = main()

        # Verify
        self.assertEqual(result, 0)
        mock_gen.assert_called_once()
        mock_build.assert_called_once()
        mock_exec.assert_called_once()
        mock_viz.assert_called_once()

        output = mock_stdout.getvalue()
        self.assertIn("PIPELINE COMPLETE", output)

    @patch('tools.experiment_framework.cli.run_visualize')
    @patch('tools.experiment_framework.cli.run_execute')
    @patch('tools.experiment_framework.cli.run_build')
    @patch('tools.experiment_framework.cli.validate_hash')
    def test_run_with_skip_generate(self, mock_hash, mock_build, mock_exec, mock_viz):
        """Test run command with --skip-generate flag."""
        # Setup
        mock_hash.return_value = 0
        mock_build.return_value = 0
        mock_exec.return_value = 0
        mock_viz.return_value = 0

        # Execute
        with patch('sys.argv', ['cli', 'run', '--app', 'test_app', '--set', 'regression', '--skip-generate']):
            result = main()

        # Verify
        self.assertEqual(result, 0)
        mock_hash.assert_called_once()
        mock_build.assert_called_once()
        mock_exec.assert_called_once()
        mock_viz.assert_called_once()

    @patch('tools.experiment_framework.cli.run_visualize')
    @patch('tools.experiment_framework.cli.run_execute')
    @patch('tools.experiment_framework.cli.run_generate')
    def test_run_with_skip_build(self, mock_gen, mock_exec, mock_viz):
        """Test run command with --skip-build flag."""
        # Setup
        mock_gen.return_value = 0
        mock_exec.return_value = 0
        mock_viz.return_value = 0

        # Execute
        with patch('sys.argv', ['cli', 'run', '--app', 'test_app', '--set', 'regression', '--skip-build']):
            result = main()

        # Verify
        self.assertEqual(result, 0)
        mock_gen.assert_called_once()
        mock_exec.assert_called_once()
        mock_viz.assert_called_once()

    @patch('tools.experiment_framework.cli.run_visualize')
    @patch('tools.experiment_framework.cli.run_build')
    @patch('tools.experiment_framework.cli.run_generate')
    def test_run_with_skip_execute(self, mock_gen, mock_build, mock_viz):
        """Test run command with --skip-execute flag."""
        # Setup
        mock_gen.return_value = 0
        mock_build.return_value = 0
        mock_viz.return_value = 0

        # Execute
        with patch('sys.argv', ['cli', 'run', '--app', 'test_app', '--set', 'regression', '--skip-execute']):
            result = main()

        # Verify
        self.assertEqual(result, 0)
        mock_gen.assert_called_once()
        mock_build.assert_called_once()
        mock_viz.assert_called_once()

    @patch('tools.experiment_framework.cli.run_visualize')
    @patch('tools.experiment_framework.cli.validate_hash')
    def test_run_with_all_skips(self, mock_hash, mock_viz):
        """Test run command with all skip flags (only visualization)."""
        # Setup
        mock_hash.return_value = 0
        mock_viz.return_value = 0

        # Execute
        with patch('sys.argv', ['cli', 'run', '--app', 'test_app', '--set', 'regression',
                                '--skip-generate', '--skip-build', '--skip-execute']):
            result = main()

        # Verify
        self.assertEqual(result, 0)
        mock_hash.assert_called_once()
        mock_viz.assert_called_once()

    @patch('tools.experiment_framework.cli.run_generate')
    def test_run_generate_failure_stops_pipeline(self, mock_gen):
        """Test that generate failure stops the pipeline."""
        # Setup
        mock_gen.return_value = 2

        # Execute
        with patch('sys.argv', ['cli', 'run', '--app', 'test_app', '--set', 'regression']):
            result = main()

        # Verify
        self.assertEqual(result, 2)
        mock_gen.assert_called_once()

    @patch('tools.experiment_framework.cli.run_visualize')
    @patch('tools.experiment_framework.cli.run_execute')
    @patch('tools.experiment_framework.cli.run_build')
    @patch('tools.experiment_framework.cli.run_generate')
    def test_run_partial_failure_continues(self, mock_gen, mock_build, mock_exec, mock_viz):
        """Test that partial failure (exit code 1) continues pipeline."""
        # Setup
        mock_gen.return_value = 0
        mock_build.return_value = 1  # Partial failure
        mock_exec.return_value = 0
        mock_viz.return_value = 0

        # Execute
        with patch('sys.argv', ['cli', 'run', '--app', 'test_app', '--set', 'regression']):
            result = main()

        # Verify
        self.assertEqual(result, 0)
        mock_gen.assert_called_once()
        mock_build.assert_called_once()
        mock_exec.assert_called_once()
        mock_viz.assert_called_once()

    @patch('tools.experiment_framework.cli.run_build')
    @patch('tools.experiment_framework.cli.run_generate')
    def test_run_critical_failure_stops(self, mock_gen, mock_build):
        """Test that critical failure (exit code 2) stops pipeline."""
        # Setup
        mock_gen.return_value = 0
        mock_build.return_value = 2  # Critical failure

        # Execute
        with patch('sys.argv', ['cli', 'run', '--app', 'test_app', '--set', 'regression']):
            result = main()

        # Verify
        self.assertEqual(result, 2)
        mock_gen.assert_called_once()
        mock_build.assert_called_once()

    @patch('tools.experiment_framework.cli.validate_hash')
    def test_run_hash_validation_failure(self, mock_hash):
        """Test that hash validation failure stops pipeline."""
        # Setup
        mock_hash.return_value = 3  # Hash mismatch

        # Execute
        with patch('sys.argv', ['cli', 'run', '--app', 'test_app', '--set', 'regression', '--skip-generate']):
            result = main()

        # Verify
        self.assertEqual(result, 3)
        mock_hash.assert_called_once()


class TestBackwardCompatibilityWrapper(unittest.TestCase):
    """Test cases for the backward compatibility wrapper script."""

    def test_wrapper_exists_and_executable(self):
        """Test that wrapper script exists and is executable."""
        wrapper_path = Path(__file__).parent.parent / "scripts" / "experiment"

        self.assertTrue(wrapper_path.exists(), "Wrapper script should exist")
        self.assertTrue(wrapper_path.stat().st_mode & 0o111, "Wrapper script should be executable")

    def test_wrapper_calls_module(self):
        """Test that wrapper invokes the Python module."""
        wrapper_path = Path(__file__).parent.parent / "scripts" / "experiment"

        # Read wrapper content
        content = wrapper_path.read_text()

        # Verify it calls the module
        self.assertIn("python3 -m tools.experiment_framework", content)
        self.assertIn("exec", content)

    def test_wrapper_shows_deprecation_warning(self):
        """Test that wrapper shows deprecation message."""
        wrapper_path = Path(__file__).parent.parent / "scripts" / "experiment"

        # Read wrapper content
        content = wrapper_path.read_text()

        # Verify deprecation message
        self.assertIn("deprecated", content.lower())
        self.assertIn(">&2", content)  # Message goes to stderr


if __name__ == '__main__':
    unittest.main()
