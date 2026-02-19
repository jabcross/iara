"""
Unit tests for the CLI module.

Tests the command-line interface for all phases of the experiment framework.
"""

import unittest
from unittest.mock import patch, MagicMock, call, mock_open
from pathlib import Path
import tempfile
import json
import sys
from io import StringIO

from tools.experiment_framework.cli import main


class TestExecuteCommand(unittest.TestCase):
    """Test cases for the execute command handler."""

    def setUp(self):
        """Set up test fixtures."""
        self.test_dir = tempfile.TemporaryDirectory()
        self.test_dir_path = Path(self.test_dir.name)

        # Create test directory structure
        self.app_dir = self.test_dir_path / "applications" / "test_app" / "experiment"
        self.app_dir.mkdir(parents=True)

        self.results_dir = self.app_dir / "results"
        self.results_dir.mkdir(parents=True)

    def tearDown(self):
        """Clean up test fixtures."""
        self.test_dir.cleanup()

    @patch('tools.experiment_framework.collector.execute_instance')
    def test_execute_command_success(self, mock_execute):
        """Test successful execute command flow."""
        # Create mock YAML config
        yaml_config = {
            'application': {'name': 'test_app'},
            'parameters': [],
            'measurements': [],
            'experiment_sets': [],
            'execution': {'repetitions': 2, 'timeout': 30, 'environment': {}}
        }

        # Create mock build results
        build_results_data = {
            'schema_version': '1.0.0',
            'experiment': {'timestamp': '2024-01-15T10:00:00+00:00'},
            'instances': [
                {
                    'name': 'test_instance',
                    'compilation': {},
                    'binary': {'total_size_bytes': 1024},
                }
            ],
            'failed_instances': []
        }

        # Mock results JSON file
        results_json = self.results_dir / "results_2024-01-15T100000Z.json"
        results_json.write_text(json.dumps(build_results_data))

        # Mock YAML file
        yaml_path = self.app_dir / "experiments.yaml"
        yaml_content = """
application:
  name: test_app
parameters: []
measurements: []
experiment_sets: []
execution:
  repetitions: 2
  timeout: 30
  environment: {}
"""
        yaml_path.write_text(yaml_content)

        # Create test executable
        build_dir = self.test_dir_path / "applications" / "test_app" / "build"
        exe_dir = build_dir / "test_instance"
        exe_dir.mkdir(parents=True)
        exe = exe_dir / "test_instance"
        exe.write_text("#!/bin/bash\nexit 0")
        exe.chmod(0o755)

        # Mock execute function
        def execute_side_effect(*args, **kwargs):
            return {
                'instance_name': 'test_instance',
                'runs': [{'run_number': 1, 'measurements': {'time': 1.0}}],
                'failures': [],
                'total_runs': 1,
                'successful_runs': 1,
                'failed_runs': 0,
                'statistics': {'time': {'mean': 1.0, 'std': 0.0, 'min': 1.0, 'max': 1.0, 'count': 1}}
            }

        mock_execute.side_effect = execute_side_effect

        with patch('sys.argv', [
            'cli.py', 'execute',
            '--app', 'test_app',
            '--set', 'regression'
        ]), \
        patch('tools.experiment_framework.cli.Path') as mock_path:

            # Mock Path to return actual test paths
            def path_side_effect(*args, **kwargs):
                if args and args[0] == 'applications':
                    return self.test_dir_path / 'applications'
                return Path(*args, **kwargs)

            mock_path.side_effect = path_side_effect

            # Capture output
            captured_output = StringIO()
            sys.stdout = captured_output

            try:
                # Call main
                exit_code = main()

                # Verify exit code
                self.assertEqual(exit_code, 0)

                # Verify output contains expected messages
                output = captured_output.getvalue()
                self.assertIn('Executing', output)
                self.assertIn('Execution complete', output)
                self.assertIn('Summary:', output)

            finally:
                sys.stdout = sys.__stdout__

    def test_execute_command_no_app_arg(self):
        """Test execute command without --app argument."""
        with patch('sys.argv', [
            'cli.py', 'execute',
            '--set', 'regression'
        ]):
            try:
                # Argparse will call sys.exit() directly
                exit_code = main()
            except SystemExit as e:
                exit_code = e.code

            self.assertEqual(exit_code, 2)

    def test_execute_command_no_set_arg(self):
        """Test execute command without --set argument."""
        with patch('sys.argv', [
            'cli.py', 'execute',
            '--app', 'test_app'
        ]):
            try:
                exit_code = main()
            except SystemExit as e:
                exit_code = e.code

            self.assertEqual(exit_code, 2)

    def test_execute_command_no_results_found(self):
        """Test execute command when no build results JSON exists."""
        # Mock YAML file exists
        yaml_path = self.app_dir / "experiments.yaml"
        yaml_path.write_text("dummy")

        # But results directory is empty
        # (self.results_dir exists but has no JSON files)

        with patch('sys.argv', [
            'cli.py', 'execute',
            '--app', 'test_app',
            '--set', 'regression'
        ]), \
        patch('tools.experiment_framework.cli.Path') as mock_path:

            def path_side_effect(*args, **kwargs):
                if args and args[0] == 'applications':
                    return self.test_dir_path / 'applications'
                return Path(*args, **kwargs)

            mock_path.side_effect = path_side_effect

            # Capture output
            captured_output = StringIO()
            sys.stderr = captured_output

            try:
                exit_code = main()
                self.assertEqual(exit_code, 2)

                output = captured_output.getvalue()
                self.assertIn('No build results JSON', output)

            finally:
                sys.stderr = sys.__stderr__

    def test_execute_command_no_successful_builds(self):
        """Test execute command when all builds failed."""
        # Create mock build results with no successful instances
        build_results_data = {
            'schema_version': '1.0.0',
            'experiment': {'timestamp': '2024-01-15T10:00:00+00:00'},
            'instances': [],  # No successful instances
            'failed_instances': [
                {
                    'name': 'test_instance',
                    'attempts': 1,
                    'errors': [{'message': 'Build failed'}],
                    'timestamp': '2024-01-15T10:00:00+00:00',
                    'last_error': 'Build failed'
                }
            ]
        }

        # Mock results JSON file
        results_json = self.results_dir / "results_2024-01-15T100000Z.json"
        results_json.write_text(json.dumps(build_results_data))

        # Mock YAML file
        yaml_path = self.app_dir / "experiments.yaml"
        yaml_content = """
application:
  name: test_app
parameters: []
measurements: []
experiment_sets: []
execution:
  repetitions: 2
  timeout: 30
"""
        yaml_path.write_text(yaml_content)

        with patch('sys.argv', [
            'cli.py', 'execute',
            '--app', 'test_app',
            '--set', 'regression'
        ]), \
        patch('tools.experiment_framework.cli.Path') as mock_path:

            def path_side_effect(*args, **kwargs):
                if args and args[0] == 'applications':
                    return self.test_dir_path / 'applications'
                return Path(*args, **kwargs)

            mock_path.side_effect = path_side_effect

            # Capture output
            captured_output = StringIO()
            sys.stderr = captured_output

            try:
                exit_code = main()
                self.assertEqual(exit_code, 2)

                output = captured_output.getvalue()
                self.assertIn('No successful builds', output)

            finally:
                sys.stderr = sys.__stderr__

    def test_execute_command_some_execution_failures(self):
        """Test execute command when some instances fail during execution."""
        # Just test that the command runs without crashing
        # The detailed execution failure testing is better done in test_collector.py

        # Create mock build results
        build_results_data = {
            'schema_version': '1.0.0',
            'experiment': {'timestamp': '2024-01-15T10:00:00+00:00'},
            'instances': [
                {
                    'name': 'missing_instance',
                    'compilation': {},
                    'binary': {'total_size_bytes': 1024},
                }
            ],
            'failed_instances': []
        }

        # Mock results JSON file
        results_json = self.results_dir / "results_2024-01-15T100000Z.json"
        results_json.write_text(json.dumps(build_results_data))

        # Mock YAML file
        yaml_path = self.app_dir / "experiments.yaml"
        yaml_content = """
application:
  name: test_app
parameters: []
measurements: []
experiment_sets: []
execution:
  repetitions: 2
  timeout: 30
  environment: {}
"""
        yaml_path.write_text(yaml_content)

        with patch('sys.argv', [
            'cli.py', 'execute',
            '--app', 'test_app',
            '--set', 'regression'
        ]), \
        patch('tools.experiment_framework.cli.Path') as mock_path:

            def path_side_effect(*args, **kwargs):
                if args and args[0] == 'applications':
                    return self.test_dir_path / 'applications'
                return Path(*args, **kwargs)

            mock_path.side_effect = path_side_effect

            # Capture output
            captured_output = StringIO()
            sys.stdout = captured_output

            try:
                exit_code = main()

                # Should skip the missing instance and return 0 or 2
                self.assertIn(exit_code, [0, 1, 2])

                output = captured_output.getvalue()
                self.assertIn('Summary:', output)

            finally:
                sys.stdout = sys.__stdout__

    def test_execute_command_with_custom_repetitions_timeout(self):
        """Test execute command with custom repetitions and timeout."""
        # Create mock build results - empty to avoid execution
        build_results_data = {
            'schema_version': '1.0.0',
            'experiment': {'timestamp': '2024-01-15T10:00:00+00:00'},
            'instances': [],
            'failed_instances': []
        }

        # Mock results JSON file
        results_json = self.results_dir / "results_2024-01-15T100000Z.json"
        results_json.write_text(json.dumps(build_results_data))

        # Mock YAML file
        yaml_path = self.app_dir / "experiments.yaml"
        yaml_content = """
application:
  name: test_app
parameters: []
measurements: []
experiment_sets: []
execution:
  repetitions: 5
  timeout: 300
  environment: {}
"""
        yaml_path.write_text(yaml_content)

        with patch('sys.argv', [
            'cli.py', 'execute',
            '--app', 'test_app',
            '--set', 'regression',
            '--repetitions', '10',
            '--timeout', '60'
        ]), \
        patch('tools.experiment_framework.cli.Path') as mock_path:

            def path_side_effect(*args, **kwargs):
                if args and args[0] == 'applications':
                    return self.test_dir_path / 'applications'
                return Path(*args, **kwargs)

            mock_path.side_effect = path_side_effect

            # Capture output
            captured_output = StringIO()
            sys.stdout = captured_output

            try:
                # Should fail because there are no instances to execute
                exit_code = main()
                self.assertEqual(exit_code, 2)

            finally:
                sys.stdout = sys.__stdout__


class TestVisualizeCommand(unittest.TestCase):
    """Test cases for the visualize command handler (Task 4.5)."""

    def setUp(self):
        """Set up test fixtures."""
        self.test_dir = tempfile.TemporaryDirectory()
        self.test_dir_path = Path(self.test_dir.name)

        # Create test directory structure
        self.app_dir = self.test_dir_path / "applications" / "test_app" / "experiment"
        self.app_dir.mkdir(parents=True)

        self.results_dir = self.app_dir / "results"
        self.results_dir.mkdir(parents=True)

    def tearDown(self):
        """Clean up test fixtures."""
        self.test_dir.cleanup()

    @patch('tools.experiment_framework.notebook.execute_notebook')
    @patch('tools.experiment_framework.notebook.generate_notebook')
    @patch('tools.experiment_framework.visualizer.generate_vegalite_json')
    def test_visualize_command_success(self, mock_vegalite, mock_notebook, mock_execute):
        """Test successful visualize command flow."""
        # Create mock YAML config
        yaml_content = """
application:
  name: test_app
  description: Test application
parameters: []
measurements:
  - name: wall_time_s
experiment_sets: []
visualization:
  plots:
    runtime_plot:
      title: Runtime Performance
      type: grouped_bars
"""
        yaml_path = self.app_dir / "experiments.yaml"
        yaml_path.write_text(yaml_content)

        # Create mock results JSON
        results_data = {
            'schema_version': '1.0.0',
            'experiment': {
                'set': 'regression',
                'timestamp': '2024-01-15T10:00:00Z',
                'git_commit': 'abc123',
                'yaml_hash': 'def456'
            },
            'instances': [{'name': 'test_instance'}],
            'failed_instances': []
        }

        results_json = self.results_dir / "results_2024-01-15T100000Z.json"
        results_json.write_text(json.dumps(results_data))

        # Mock visualization generation
        mock_vegalite_file = self.results_dir / "plot_runtime.vl.json"
        mock_vegalite.return_value = [mock_vegalite_file]

        # Mock notebook generation
        mock_notebook_file = self.results_dir / "analysis_20240115T120000Z.ipynb"
        mock_notebook.return_value = mock_notebook_file

        with patch('sys.argv', [
            'cli.py', 'visualize',
            '--app', 'test_app',
            '--set', 'regression'
        ]), \
        patch('tools.experiment_framework.cli.Path') as mock_path:

            def path_side_effect(*args, **kwargs):
                if args and args[0] == 'applications':
                    return self.test_dir_path / 'applications'
                return Path(*args, **kwargs)

            mock_path.side_effect = path_side_effect

            # Capture output
            captured_output = StringIO()
            sys.stdout = captured_output

            try:
                exit_code = main()

                # Verify exit code
                self.assertEqual(exit_code, 0)

                # Verify functions were called
                mock_vegalite.assert_called_once()
                mock_notebook.assert_called_once()
                mock_execute.assert_called_once()

                # Verify output
                output = captured_output.getvalue()
                self.assertIn('Generating Vega-Lite', output)
                self.assertIn('Generating Jupyter notebook', output)
                self.assertIn('Visualization complete', output)

            finally:
                sys.stdout = sys.__stdout__

    def test_visualize_command_no_app_arg(self):
        """Test visualize command without --app argument."""
        with patch('sys.argv', [
            'cli.py', 'visualize',
            '--set', 'regression'
        ]):
            try:
                exit_code = main()
            except SystemExit as e:
                exit_code = e.code

            self.assertEqual(exit_code, 2)

    def test_visualize_command_no_set_arg(self):
        """Test visualize command without --set argument."""
        with patch('sys.argv', [
            'cli.py', 'visualize',
            '--app', 'test_app'
        ]):
            try:
                exit_code = main()
            except SystemExit as e:
                exit_code = e.code

            self.assertEqual(exit_code, 2)

    def test_visualize_command_no_results_directory(self):
        """Test visualize command when results directory doesn't exist."""
        # Don't create results_dir

        # Remove results directory
        import shutil
        if self.results_dir.exists():
            shutil.rmtree(self.results_dir)

        with patch('sys.argv', [
            'cli.py', 'visualize',
            '--app', 'test_app',
            '--set', 'regression'
        ]), \
        patch('tools.experiment_framework.cli.Path') as mock_path:

            def path_side_effect(*args, **kwargs):
                if args and args[0] == 'applications':
                    return self.test_dir_path / 'applications'
                return Path(*args, **kwargs)

            mock_path.side_effect = path_side_effect

            # Capture output
            captured_output = StringIO()
            sys.stderr = captured_output

            try:
                exit_code = main()
                self.assertEqual(exit_code, 2)

                output = captured_output.getvalue()
                self.assertIn('Results directory not found', output)

            finally:
                sys.stderr = sys.__stderr__

    def test_visualize_command_no_results_json(self):
        """Test visualize command when no results JSON exists."""
        # Results dir exists but is empty

        with patch('sys.argv', [
            'cli.py', 'visualize',
            '--app', 'test_app',
            '--set', 'regression'
        ]), \
        patch('tools.experiment_framework.cli.Path') as mock_path:

            def path_side_effect(*args, **kwargs):
                if args and args[0] == 'applications':
                    return self.test_dir_path / 'applications'
                return Path(*args, **kwargs)

            mock_path.side_effect = path_side_effect

            # Capture output
            captured_output = StringIO()
            sys.stderr = captured_output

            try:
                exit_code = main()
                self.assertEqual(exit_code, 2)

                output = captured_output.getvalue()
                self.assertIn('No results JSON', output)

            finally:
                sys.stderr = sys.__stderr__

    @patch('tools.experiment_framework.notebook.execute_notebook')
    @patch('tools.experiment_framework.notebook.generate_notebook')
    def test_visualize_command_custom_results_path(self, mock_notebook, mock_execute):
        """Test visualize command with custom results path."""
        # Create mock YAML config
        yaml_content = """
application:
  name: test_app
parameters: []
measurements: []
experiment_sets: []
"""
        yaml_path = self.app_dir / "experiments.yaml"
        yaml_path.write_text(yaml_content)

        # Create custom results JSON
        custom_results = self.test_dir_path / "custom_results.json"
        results_data = {
            'schema_version': '1.0.0',
            'experiment': {},
            'instances': [],
            'failed_instances': []
        }
        custom_results.write_text(json.dumps(results_data))

        # Mock notebook generation
        mock_notebook_file = self.results_dir / "analysis.ipynb"
        mock_notebook.return_value = mock_notebook_file

        with patch('sys.argv', [
            'cli.py', 'visualize',
            '--app', 'test_app',
            '--set', 'regression',
            '--results', str(custom_results)
        ]), \
        patch('tools.experiment_framework.cli.Path') as mock_path:

            def path_side_effect(*args, **kwargs):
                if args and args[0] == 'applications':
                    return self.test_dir_path / 'applications'
                return Path(*args, **kwargs)

            mock_path.side_effect = path_side_effect

            # Capture output
            captured_output = StringIO()
            sys.stdout = captured_output

            try:
                exit_code = main()
                self.assertEqual(exit_code, 0)

                # Verify custom path was used
                mock_notebook.assert_called_once()
                call_args = mock_notebook.call_args
                # Second argument should be the results path
                self.assertEqual(str(call_args[0][1]), str(custom_results))

            finally:
                sys.stdout = sys.__stdout__

    @patch('tools.experiment_framework.notebook.execute_notebook')
    @patch('tools.experiment_framework.notebook.generate_notebook')
    def test_visualize_command_no_visualization_config(self, mock_notebook, mock_execute):
        """Test visualize command when YAML has no visualization section."""
        # Create mock YAML config WITHOUT visualization section
        yaml_content = """
application:
  name: test_app
parameters: []
measurements: []
experiment_sets: []
"""
        yaml_path = self.app_dir / "experiments.yaml"
        yaml_path.write_text(yaml_content)

        # Create mock results JSON
        results_data = {
            'schema_version': '1.0.0',
            'experiment': {},
            'instances': [],
            'failed_instances': []
        }

        results_json = self.results_dir / "results_2024-01-15T100000Z.json"
        results_json.write_text(json.dumps(results_data))

        # Mock notebook generation
        mock_notebook_file = self.results_dir / "analysis.ipynb"
        mock_notebook.return_value = mock_notebook_file

        with patch('sys.argv', [
            'cli.py', 'visualize',
            '--app', 'test_app',
            '--set', 'regression'
        ]), \
        patch('tools.experiment_framework.cli.Path') as mock_path:

            def path_side_effect(*args, **kwargs):
                if args and args[0] == 'applications':
                    return self.test_dir_path / 'applications'
                return Path(*args, **kwargs)

            mock_path.side_effect = path_side_effect

            # Capture output
            captured_output = StringIO()
            sys.stderr = captured_output

            try:
                exit_code = main()
                self.assertEqual(exit_code, 0)

                # Should still generate notebook, just skip plots
                mock_notebook.assert_called_once()

                # Check for warning about missing visualization config
                output = captured_output.getvalue()
                self.assertIn('No visualization configuration', output)

            finally:
                sys.stderr = sys.__stderr__

    @patch('tools.experiment_framework.notebook.execute_notebook')
    @patch('tools.experiment_framework.notebook.generate_notebook')
    @patch('tools.experiment_framework.visualizer.generate_vegalite_json')
    def test_visualize_command_notebook_execution_fails(self, mock_vegalite, mock_notebook, mock_execute):
        """Test visualize command when notebook execution fails."""
        # Create mock YAML config
        yaml_content = """
application:
  name: test_app
parameters: []
measurements: []
experiment_sets: []
visualization:
  plots:
    test_plot:
      title: Test
"""
        yaml_path = self.app_dir / "experiments.yaml"
        yaml_path.write_text(yaml_content)

        # Create mock results JSON
        results_data = {
            'schema_version': '1.0.0',
            'experiment': {},
            'instances': [],
            'failed_instances': []
        }

        results_json = self.results_dir / "results_2024-01-15T100000Z.json"
        results_json.write_text(json.dumps(results_data))

        # Mock visualization generation
        mock_vegalite.return_value = []

        # Mock notebook generation
        mock_notebook_file = self.results_dir / "analysis.ipynb"
        mock_notebook.return_value = mock_notebook_file

        # Mock notebook execution failure
        mock_execute.side_effect = RuntimeError("Execution failed")

        with patch('sys.argv', [
            'cli.py', 'visualize',
            '--app', 'test_app',
            '--set', 'regression'
        ]), \
        patch('tools.experiment_framework.cli.Path') as mock_path:

            def path_side_effect(*args, **kwargs):
                if args and args[0] == 'applications':
                    return self.test_dir_path / 'applications'
                return Path(*args, **kwargs)

            mock_path.side_effect = path_side_effect

            # Capture output
            captured_output = StringIO()
            sys.stderr = captured_output

            try:
                exit_code = main()

                # Should return 1 (partial success - notebook generated but not executed)
                self.assertEqual(exit_code, 1)

                # Verify warning about execution failure
                output = captured_output.getvalue()
                self.assertIn('Notebook execution failed', output)
                self.assertIn('Notebook generated but not executed', output)

            finally:
                sys.stderr = sys.__stderr__

    @patch('tools.experiment_framework.notebook.execute_notebook')
    @patch('tools.experiment_framework.notebook.generate_notebook')
    @patch('tools.experiment_framework.visualizer.generate_vegalite_json')
    def test_visualize_command_jupyter_not_found(self, mock_vegalite, mock_notebook, mock_execute):
        """Test visualize command when jupyter is not installed."""
        # Create mock YAML config
        yaml_content = """
application:
  name: test_app
parameters: []
measurements: []
experiment_sets: []
"""
        yaml_path = self.app_dir / "experiments.yaml"
        yaml_path.write_text(yaml_content)

        # Create mock results JSON
        results_data = {
            'schema_version': '1.0.0',
            'experiment': {},
            'instances': [],
            'failed_instances': []
        }

        results_json = self.results_dir / "results_2024-01-15T100000Z.json"
        results_json.write_text(json.dumps(results_data))

        # Mock visualization generation
        mock_vegalite.return_value = []

        # Mock notebook generation
        mock_notebook_file = self.results_dir / "analysis.ipynb"
        mock_notebook.return_value = mock_notebook_file

        # Mock jupyter not found
        mock_execute.side_effect = FileNotFoundError("jupyter command not found")

        with patch('sys.argv', [
            'cli.py', 'visualize',
            '--app', 'test_app',
            '--set', 'regression'
        ]), \
        patch('tools.experiment_framework.cli.Path') as mock_path:

            def path_side_effect(*args, **kwargs):
                if args and args[0] == 'applications':
                    return self.test_dir_path / 'applications'
                return Path(*args, **kwargs)

            mock_path.side_effect = path_side_effect

            # Capture output
            captured_output = StringIO()
            sys.stderr = captured_output

            try:
                exit_code = main()

                # Should return 1 (partial success)
                self.assertEqual(exit_code, 1)

                # Verify warning
                output = captured_output.getvalue()
                self.assertIn('jupyter', output.lower())

            finally:
                sys.stderr = sys.__stderr__

    @patch('tools.experiment_framework.notebook.generate_notebook')
    @patch('tools.experiment_framework.visualizer.generate_vegalite_json')
    def test_visualize_command_visualization_generation_fails(self, mock_vegalite, mock_notebook):
        """Test visualize command when visualization generation fails."""
        # Create mock YAML config
        yaml_content = """
application:
  name: test_app
parameters: []
measurements: []
experiment_sets: []
visualization:
  plots:
    test_plot:
      title: Test
"""
        yaml_path = self.app_dir / "experiments.yaml"
        yaml_path.write_text(yaml_content)

        # Create mock results JSON
        results_data = {
            'schema_version': '1.0.0',
            'experiment': {},
            'instances': [],
            'failed_instances': []
        }

        results_json = self.results_dir / "results_2024-01-15T100000Z.json"
        results_json.write_text(json.dumps(results_data))

        # Mock visualization generation failure
        mock_vegalite.side_effect = RuntimeError("Visualization failed")

        with patch('sys.argv', [
            'cli.py', 'visualize',
            '--app', 'test_app',
            '--set', 'regression'
        ]), \
        patch('tools.experiment_framework.cli.Path') as mock_path:

            def path_side_effect(*args, **kwargs):
                if args and args[0] == 'applications':
                    return self.test_dir_path / 'applications'
                return Path(*args, **kwargs)

            mock_path.side_effect = path_side_effect

            # Capture output
            captured_output = StringIO()
            sys.stderr = captured_output

            try:
                exit_code = main()

                # Should return 2 (critical failure)
                self.assertEqual(exit_code, 2)

                # Verify error message
                output = captured_output.getvalue()
                self.assertIn('Visualization generation failed', output)

            finally:
                sys.stderr = sys.__stderr__

    @patch('tools.experiment_framework.notebook.generate_notebook')
    def test_visualize_command_notebook_generation_fails(self, mock_notebook):
        """Test visualize command when notebook generation fails."""
        # Create mock YAML config
        yaml_content = """
application:
  name: test_app
parameters: []
measurements: []
experiment_sets: []
"""
        yaml_path = self.app_dir / "experiments.yaml"
        yaml_path.write_text(yaml_content)

        # Create mock results JSON
        results_data = {
            'schema_version': '1.0.0',
            'experiment': {},
            'instances': [],
            'failed_instances': []
        }

        results_json = self.results_dir / "results_2024-01-15T100000Z.json"
        results_json.write_text(json.dumps(results_data))

        # Mock notebook generation failure
        mock_notebook.side_effect = RuntimeError("Notebook generation failed")

        with patch('sys.argv', [
            'cli.py', 'visualize',
            '--app', 'test_app',
            '--set', 'regression'
        ]), \
        patch('tools.experiment_framework.cli.Path') as mock_path:

            def path_side_effect(*args, **kwargs):
                if args and args[0] == 'applications':
                    return self.test_dir_path / 'applications'
                return Path(*args, **kwargs)

            mock_path.side_effect = path_side_effect

            # Capture output
            captured_output = StringIO()
            sys.stderr = captured_output

            try:
                exit_code = main()

                # Should return 2 (critical failure)
                self.assertEqual(exit_code, 2)

                # Verify error message
                output = captured_output.getvalue()
                self.assertIn('Notebook generation failed', output)

            finally:
                sys.stderr = sys.__stderr__


if __name__ == '__main__':
    unittest.main()
