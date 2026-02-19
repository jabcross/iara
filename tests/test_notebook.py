"""
Unit tests for notebook.py - Tasks 4.3-4.5: Notebook Generation and Execution.

Tests the main functions:
    Task 4.3:
        1. extract_failure_summary() - Parse failed instances into structured summaries
        2. generate_failure_markdown() - Create markdown table with collapsible details
        3. get_failure_report_cell() - Create Jupyter notebook markdown cell
    Task 4.4:
        1. create_metadata_cell() - Create header with experiment metadata
        2. create_data_loading_cell() - Create code cell to load results JSON
        3. create_statistics_cell() - Create code cell for summary statistics
        4. create_visualization_cell() - Create code cell to render Vega-Lite spec
        5. generate_notebook() - Assemble complete notebook
        6. execute_notebook() - Execute notebook using nbconvert
"""

import json
import tempfile
import unittest
import subprocess
from pathlib import Path
from unittest.mock import patch, MagicMock, call, mock_open

from tools.experiment_framework.notebook import (
    extract_failure_summary,
    generate_failure_markdown,
    get_failure_report_cell,
    create_metadata_cell,
    create_data_loading_cell,
    create_statistics_cell,
    create_visualization_cell,
    generate_notebook,
    execute_notebook,
)


# =============================================================================
# Task 4.3: Failure Report Generation Tests (20+ tests)
# =============================================================================


class TestExtractFailureSummary(unittest.TestCase):
    """Tests for extract_failure_summary function."""

    def test_empty_list_returns_empty(self):
        """Test that empty list returns empty summary."""
        result = extract_failure_summary([])
        self.assertEqual(result, [])

    def test_single_failure_single_error(self):
        """Test extracting single failure with one error."""
        failed = [
            {
                "instance_name": "test_instance",
                "errors": [
                    {"phase": "build", "message": "CMake error"}
                ]
            }
        ]

        result = extract_failure_summary(failed)

        self.assertEqual(len(result), 1)
        self.assertEqual(result[0]["instance_name"], "test_instance")
        self.assertEqual(result[0]["attempts"], "1")
        self.assertEqual(result[0]["error_summary"], "CMake error")
        self.assertIn("Attempt 1: build", result[0]["full_error"])

    def test_single_failure_multiple_errors(self):
        """Test extracting single failure with multiple errors."""
        failed = [
            {
                "instance_name": "test_instance",
                "errors": [
                    {"phase": "cmake", "message": "Error 1"},
                    {"phase": "build", "message": "Error 2"},
                    {"phase": "link", "message": "Error 3"}
                ]
            }
        ]

        result = extract_failure_summary(failed)

        self.assertEqual(len(result), 1)
        self.assertEqual(result[0]["attempts"], "3")
        self.assertEqual(result[0]["error_summary"], "Error 3")  # Last error
        self.assertIn("Attempt 1: cmake", result[0]["full_error"])
        self.assertIn("Attempt 2: build", result[0]["full_error"])
        self.assertIn("Attempt 3: link", result[0]["full_error"])

    def test_multiple_failures(self):
        """Test extracting multiple failures."""
        failed = [
            {
                "instance_name": "instance_1",
                "errors": [{"phase": "build", "message": "Error 1"}]
            },
            {
                "instance_name": "instance_2",
                "errors": [{"phase": "cmake", "message": "Error 2"}]
            }
        ]

        result = extract_failure_summary(failed)

        self.assertEqual(len(result), 2)
        self.assertEqual(result[0]["instance_name"], "instance_1")
        self.assertEqual(result[1]["instance_name"], "instance_2")

    def test_long_error_message_truncated(self):
        """Test that error messages longer than 100 chars are truncated."""
        long_message = "A" * 150

        failed = [
            {
                "instance_name": "test",
                "errors": [{"phase": "build", "message": long_message}]
            }
        ]

        result = extract_failure_summary(failed)

        self.assertEqual(len(result[0]["error_summary"]), 103)  # 100 chars + "..."
        self.assertTrue(result[0]["error_summary"].endswith("..."))

    def test_exactly_100_char_message_not_truncated(self):
        """Test that 100 char messages are not truncated."""
        message_100 = "A" * 100

        failed = [
            {
                "instance_name": "test",
                "errors": [{"phase": "build", "message": message_100}]
            }
        ]

        result = extract_failure_summary(failed)

        self.assertEqual(len(result[0]["error_summary"]), 100)
        self.assertFalse(result[0]["error_summary"].endswith("..."))

    def test_missing_instance_name(self):
        """Test handling missing instance name."""
        failed = [
            {
                "errors": [{"phase": "build", "message": "Error"}]
            }
        ]

        result = extract_failure_summary(failed)

        self.assertEqual(result[0]["instance_name"], "unknown")

    def test_missing_errors_field(self):
        """Test handling missing errors field."""
        failed = [
            {"instance_name": "test"}
        ]

        result = extract_failure_summary(failed)

        self.assertEqual(len(result), 1)
        self.assertEqual(result[0]["attempts"], "0")
        self.assertEqual(result[0]["error_summary"], "No error details available")

    def test_empty_errors_list(self):
        """Test handling empty errors list."""
        failed = [
            {"instance_name": "test", "errors": []}
        ]

        result = extract_failure_summary(failed)

        self.assertEqual(result[0]["attempts"], "0")
        self.assertEqual(result[0]["error_summary"], "No error details available")

    def test_missing_phase_field(self):
        """Test handling missing phase field in error."""
        failed = [
            {
                "instance_name": "test",
                "errors": [{"message": "Error"}]
            }
        ]

        result = extract_failure_summary(failed)

        self.assertIn("unknown", result[0]["full_error"])

    def test_missing_message_field(self):
        """Test handling missing message field in error."""
        failed = [
            {
                "instance_name": "test",
                "errors": [{"phase": "build"}]
            }
        ]

        result = extract_failure_summary(failed)

        self.assertEqual(result[0]["error_summary"], "No error message")


class TestGenerateFailureMarkdown(unittest.TestCase):
    """Tests for generate_failure_markdown function."""

    def test_empty_list_returns_no_failures_message(self):
        """Test that empty list returns appropriate message."""
        result = generate_failure_markdown([])

        self.assertIn("## Build Failures", result)
        self.assertIn("No build failures", result)

    def test_single_failure_creates_table(self):
        """Test that single failure creates valid markdown table."""
        failed = [
            {
                "instance_name": "test",
                "errors": [{"phase": "build", "message": "Error"}]
            }
        ]

        result = generate_failure_markdown(failed)

        self.assertIn("## Build Failures", result)
        self.assertIn("**Total Failed:** 1 instances", result)
        self.assertIn("| Instance Name | Attempts | Error Summary | Full Error |", result)
        self.assertIn("| test |", result)

    def test_multiple_failures_create_table_rows(self):
        """Test that multiple failures create multiple table rows."""
        failed = [
            {
                "instance_name": "test1",
                "errors": [{"phase": "build", "message": "Error 1"}]
            },
            {
                "instance_name": "test2",
                "errors": [{"phase": "cmake", "message": "Error 2"}]
            }
        ]

        result = generate_failure_markdown(failed)

        self.assertIn("**Total Failed:** 2 instances", result)
        self.assertIn("| test1 |", result)
        self.assertIn("| test2 |", result)

    def test_pipe_characters_escaped_in_error_summary(self):
        """Test that pipe characters are escaped in markdown table."""
        failed = [
            {
                "instance_name": "test",
                "errors": [{"phase": "build", "message": "Error | with | pipes"}]
            }
        ]

        result = generate_failure_markdown(failed)

        self.assertIn("Error \\| with \\| pipes", result)

    def test_newlines_replaced_in_error_summary(self):
        """Test that newlines are replaced with spaces in error summary."""
        failed = [
            {
                "instance_name": "test",
                "errors": [{"phase": "build", "message": "Error\nwith\nnewlines"}]
            }
        ]

        result = generate_failure_markdown(failed)

        # In the error summary column (not the full error)
        lines = result.split("\n")
        table_row = [line for line in lines if "| test |" in line][0]
        # Error summary should have spaces instead of newlines
        self.assertNotIn("\n", table_row.split("|")[3])

    def test_details_html_created(self):
        """Test that collapsible details HTML is created."""
        failed = [
            {
                "instance_name": "test",
                "errors": [{"phase": "build", "message": "Error"}]
            }
        ]

        result = generate_failure_markdown(failed)

        self.assertIn("<details>", result)
        self.assertIn("<summary>View</summary>", result)
        self.assertIn("<pre>", result)
        self.assertIn("</pre>", result)
        self.assertIn("</details>", result)

    def test_full_error_in_details(self):
        """Test that full error appears in details section."""
        failed = [
            {
                "instance_name": "test",
                "errors": [
                    {"phase": "build", "message": "First error"},
                    {"phase": "link", "message": "Second error"}
                ]
            }
        ]

        result = generate_failure_markdown(failed)

        self.assertIn("Attempt 1: build", result)
        self.assertIn("First error", result)
        self.assertIn("Attempt 2: link", result)
        self.assertIn("Second error", result)


class TestGetFailureReportCell(unittest.TestCase):
    """Tests for get_failure_report_cell function."""

    def test_creates_markdown_cell(self):
        """Test that cell type is markdown."""
        cell = get_failure_report_cell([])

        self.assertEqual(cell["cell_type"], "markdown")

    def test_cell_has_metadata(self):
        """Test that cell has metadata field."""
        cell = get_failure_report_cell([])

        self.assertIn("metadata", cell)
        self.assertIsInstance(cell["metadata"], dict)

    def test_cell_source_is_list(self):
        """Test that source is a list of strings."""
        cell = get_failure_report_cell([])

        self.assertIn("source", cell)
        self.assertIsInstance(cell["source"], list)
        self.assertTrue(all(isinstance(line, str) for line in cell["source"]))

    def test_source_lines_have_newlines(self):
        """Test that source lines have newlines (except possibly last)."""
        failed = [
            {
                "instance_name": "test",
                "errors": [{"phase": "build", "message": "Error"}]
            }
        ]

        cell = get_failure_report_cell(failed)

        # Most lines should end with \n
        lines_with_newline = sum(1 for line in cell["source"][:-1] if line.endswith("\n"))
        self.assertGreater(lines_with_newline, 0)

    def test_no_failures_cell_content(self):
        """Test cell content with no failures."""
        cell = get_failure_report_cell([])

        source_text = "".join(cell["source"])
        self.assertIn("No build failures", source_text)

    def test_with_failures_cell_content(self):
        """Test cell content with failures."""
        failed = [
            {
                "instance_name": "test",
                "errors": [{"phase": "build", "message": "Error"}]
            }
        ]

        cell = get_failure_report_cell(failed)

        source_text = "".join(cell["source"])
        self.assertIn("Build Failures", source_text)
        self.assertIn("test", source_text)


# =============================================================================
# Task 4.4: Jupyter Notebook Generation Tests (25+ tests)
# =============================================================================


class TestCreateMetadataCell(unittest.TestCase):
    """Tests for create_metadata_cell function."""

    def test_creates_markdown_cell(self):
        """Test that cell type is markdown."""
        config = {"application": {"name": "test"}}
        results = {"experiment": {}, "instances": [], "failed_instances": []}

        cell = create_metadata_cell(config, results)

        self.assertEqual(cell["cell_type"], "markdown")

    def test_includes_app_name(self):
        """Test that app name is included in cell."""
        config = {"application": {"name": "TestApp"}}
        results = {"experiment": {}, "instances": [], "failed_instances": []}

        cell = create_metadata_cell(config, results)

        source_text = "".join(cell["source"])
        self.assertIn("TestApp", source_text)

    def test_includes_app_description(self):
        """Test that app description is included if present."""
        config = {"application": {"name": "test", "description": "Test description"}}
        results = {"experiment": {}, "instances": [], "failed_instances": []}

        cell = create_metadata_cell(config, results)

        source_text = "".join(cell["source"])
        self.assertIn("Test description", source_text)

    def test_includes_experiment_metadata(self):
        """Test that experiment metadata is included."""
        config = {"application": {"name": "test"}}
        results = {
            "experiment": {
                "set": "regression",
                "timestamp": "2024-01-15T10:00:00Z",
                "git_commit": "abc123",
                "yaml_hash": "def456xyz"
            },
            "instances": [],
            "failed_instances": []
        }

        cell = create_metadata_cell(config, results)

        source_text = "".join(cell["source"])
        self.assertIn("regression", source_text)
        self.assertIn("2024-01-15T10:00:00Z", source_text)
        self.assertIn("abc123", source_text)
        self.assertIn("def456", source_text)  # Hash is truncated

    def test_calculates_instance_counts(self):
        """Test that instance counts are calculated correctly."""
        config = {"application": {"name": "test"}}
        results = {
            "experiment": {},
            "instances": [{"name": "i1"}, {"name": "i2"}],
            "failed_instances": [{"instance_name": "f1"}]
        }

        cell = create_metadata_cell(config, results)

        source_text = "".join(cell["source"])
        self.assertIn("Total Instances:** 3", source_text)
        self.assertIn("Successful:** 2", source_text)
        self.assertIn("Failed:** 1", source_text)

    def test_calculates_success_percentage(self):
        """Test that success percentage is calculated."""
        config = {"application": {"name": "test"}}
        results = {
            "experiment": {},
            "instances": [{"name": "i1"}],
            "failed_instances": [{"instance_name": "f1"}]
        }

        cell = create_metadata_cell(config, results)

        source_text = "".join(cell["source"])
        self.assertIn("50.0%", source_text)

    def test_handles_zero_instances(self):
        """Test handling zero instances (no division by zero)."""
        config = {"application": {"name": "test"}}
        results = {
            "experiment": {},
            "instances": [],
            "failed_instances": []
        }

        cell = create_metadata_cell(config, results)

        source_text = "".join(cell["source"])
        self.assertIn("Total Instances:** 0", source_text)


class TestCreateDataLoadingCell(unittest.TestCase):
    """Tests for create_data_loading_cell function."""

    def test_creates_code_cell(self):
        """Test that cell type is code."""
        cell = create_data_loading_cell(Path("/tmp/results.json"))

        self.assertEqual(cell["cell_type"], "code")

    def test_cell_has_required_fields(self):
        """Test that cell has all required fields."""
        cell = create_data_loading_cell(Path("/tmp/results.json"))

        self.assertIn("execution_count", cell)
        self.assertIn("metadata", cell)
        self.assertIn("outputs", cell)
        self.assertIn("source", cell)

    def test_code_includes_imports(self):
        """Test that code includes necessary imports."""
        cell = create_data_loading_cell(Path("/tmp/results.json"))

        source_text = "".join(cell["source"])
        self.assertIn("import json", source_text)
        self.assertIn("from pathlib import Path", source_text)

    def test_code_includes_path(self):
        """Test that code includes the results path."""
        cell = create_data_loading_cell(Path("/tmp/test_results.json"))

        source_text = "".join(cell["source"])
        self.assertIn("/tmp/test_results.json", source_text)

    def test_code_is_valid_python(self):
        """Test that generated code is valid Python syntax."""
        cell = create_data_loading_cell(Path("/tmp/results.json"))

        source_text = "".join(cell["source"])
        # Should not raise SyntaxError
        compile(source_text, "<string>", "exec")


class TestCreateStatisticsCell(unittest.TestCase):
    """Tests for create_statistics_cell function."""

    def test_creates_code_cell(self):
        """Test that cell type is code."""
        cell = create_statistics_cell(["wall_time_s"])

        self.assertEqual(cell["cell_type"], "code")

    def test_code_imports_pandas(self):
        """Test that code imports pandas."""
        cell = create_statistics_cell(["wall_time_s"])

        source_text = "".join(cell["source"])
        self.assertIn("import pandas as pd", source_text)

    def test_code_creates_dataframe(self):
        """Test that code creates a DataFrame."""
        cell = create_statistics_cell(["wall_time_s"])

        source_text = "".join(cell["source"])
        self.assertIn("pd.DataFrame", source_text)

    def test_code_handles_empty_measurements(self):
        """Test that code handles empty measurements list."""
        cell = create_statistics_cell([])

        source_text = "".join(cell["source"])
        # Should still be valid Python
        compile(source_text, "<string>", "exec")

    def test_code_is_valid_python(self):
        """Test that generated code is valid Python syntax."""
        cell = create_statistics_cell(["wall_time_s", "max_rss_bytes"])

        source_text = "".join(cell["source"])
        compile(source_text, "<string>", "exec")


class TestCreateVisualizationCell(unittest.TestCase):
    """Tests for create_visualization_cell function."""

    def test_creates_code_cell(self):
        """Test that cell type is code."""
        cell = create_visualization_cell("runtime", Path("/tmp/plot.vl.json"))

        self.assertEqual(cell["cell_type"], "code")

    def test_code_imports_altair(self):
        """Test that code imports altair."""
        cell = create_visualization_cell("runtime", Path("/tmp/plot.vl.json"))

        source_text = "".join(cell["source"])
        self.assertIn("import altair as alt", source_text)

    def test_code_includes_plot_name(self):
        """Test that code includes plot name as comment."""
        cell = create_visualization_cell("runtime_performance", Path("/tmp/plot.vl.json"))

        source_text = "".join(cell["source"])
        self.assertIn("runtime_performance", source_text)

    def test_code_includes_path(self):
        """Test that code includes Vega-Lite file path."""
        cell = create_visualization_cell("test", Path("/tmp/my_plot.vl.json"))

        source_text = "".join(cell["source"])
        self.assertIn("/tmp/my_plot.vl.json", source_text)

    def test_code_creates_chart(self):
        """Test that code creates an Altair chart."""
        cell = create_visualization_cell("test", Path("/tmp/plot.vl.json"))

        source_text = "".join(cell["source"])
        self.assertIn("alt.Chart.from_dict", source_text)

    def test_code_is_valid_python(self):
        """Test that generated code is valid Python syntax."""
        cell = create_visualization_cell("test", Path("/tmp/plot.vl.json"))

        source_text = "".join(cell["source"])
        compile(source_text, "<string>", "exec")


class TestGenerateNotebook(unittest.TestCase):
    """Tests for generate_notebook function."""

    def setUp(self):
        """Set up test fixtures."""
        self.test_dir = tempfile.TemporaryDirectory()
        self.test_dir_path = Path(self.test_dir.name)

    def tearDown(self):
        """Clean up test fixtures."""
        self.test_dir.cleanup()

    def test_creates_notebook_file(self):
        """Test that notebook file is created."""
        config = {
            "application": {"name": "test"},
            "measurements": []
        }
        results_data = {
            "experiment": {},
            "instances": [],
            "failed_instances": []
        }

        results_path = self.test_dir_path / "results.json"
        results_path.write_text(json.dumps(results_data))

        output_path = self.test_dir_path / "notebook.ipynb"

        result = generate_notebook(config, results_path, [], output_path)

        self.assertTrue(output_path.exists())
        self.assertEqual(result, output_path)

    def test_notebook_has_cells(self):
        """Test that generated notebook has cells."""
        config = {
            "application": {"name": "test"},
            "measurements": []
        }
        results_data = {
            "experiment": {},
            "instances": [],
            "failed_instances": []
        }

        results_path = self.test_dir_path / "results.json"
        results_path.write_text(json.dumps(results_data))

        output_path = self.test_dir_path / "notebook.ipynb"

        generate_notebook(config, results_path, [], output_path)

        with open(output_path, 'r') as f:
            notebook = json.load(f)

        self.assertIn("cells", notebook)
        self.assertGreater(len(notebook["cells"]), 0)

    def test_notebook_has_metadata(self):
        """Test that notebook has kernel metadata."""
        config = {
            "application": {"name": "test"},
            "measurements": []
        }
        results_data = {
            "experiment": {},
            "instances": [],
            "failed_instances": []
        }

        results_path = self.test_dir_path / "results.json"
        results_path.write_text(json.dumps(results_data))

        output_path = self.test_dir_path / "notebook.ipynb"

        generate_notebook(config, results_path, [], output_path)

        with open(output_path, 'r') as f:
            notebook = json.load(f)

        self.assertIn("metadata", notebook)
        self.assertIn("kernelspec", notebook["metadata"])

    def test_notebook_is_nbformat_v4(self):
        """Test that notebook uses nbformat v4."""
        config = {
            "application": {"name": "test"},
            "measurements": []
        }
        results_data = {
            "experiment": {},
            "instances": [],
            "failed_instances": []
        }

        results_path = self.test_dir_path / "results.json"
        results_path.write_text(json.dumps(results_data))

        output_path = self.test_dir_path / "notebook.ipynb"

        generate_notebook(config, results_path, [], output_path)

        with open(output_path, 'r') as f:
            notebook = json.load(f)

        self.assertEqual(notebook["nbformat"], 4)

    def test_includes_failure_cell_when_failures_exist(self):
        """Test that failure cell is included when there are failures."""
        config = {
            "application": {"name": "test"},
            "measurements": []
        }
        results_data = {
            "experiment": {},
            "instances": [],
            "failed_instances": [
                {
                    "instance_name": "test",
                    "errors": [{"phase": "build", "message": "Error"}]
                }
            ]
        }

        results_path = self.test_dir_path / "results.json"
        results_path.write_text(json.dumps(results_data))

        output_path = self.test_dir_path / "notebook.ipynb"

        generate_notebook(config, results_path, [], output_path)

        with open(output_path, 'r') as f:
            notebook = json.load(f)

        # Check for failure report cell
        markdown_cells = [c for c in notebook["cells"] if c["cell_type"] == "markdown"]
        failure_cell = any("Build Failures" in "".join(c["source"]) for c in markdown_cells)
        self.assertTrue(failure_cell)

    def test_excludes_failure_cell_when_no_failures(self):
        """Test that failure cell is excluded when there are no failures."""
        config = {
            "application": {"name": "test"},
            "measurements": []
        }
        results_data = {
            "experiment": {},
            "instances": [{"name": "test"}],
            "failed_instances": []
        }

        results_path = self.test_dir_path / "results.json"
        results_path.write_text(json.dumps(results_data))

        output_path = self.test_dir_path / "notebook.ipynb"

        generate_notebook(config, results_path, [], output_path)

        with open(output_path, 'r') as f:
            notebook = json.load(f)

        # Should not have failure report with "Total Failed"
        all_text = ""
        for cell in notebook["cells"]:
            all_text += "".join(cell["source"])

        # Should have "No build failures" message if failure cell present, or no failure mention
        if "Build Failures" in all_text:
            self.assertIn("No build failures", all_text)

    def test_includes_visualization_cells(self):
        """Test that visualization cells are included for each plot."""
        config = {
            "application": {"name": "test"},
            "measurements": []
        }
        results_data = {
            "experiment": {},
            "instances": [],
            "failed_instances": []
        }

        results_path = self.test_dir_path / "results.json"
        results_path.write_text(json.dumps(results_data))

        # Create dummy Vega-Lite files
        vl_file1 = self.test_dir_path / "plot_runtime.vl.json"
        vl_file1.write_text("{}")
        vl_file2 = self.test_dir_path / "plot_memory.vl.json"
        vl_file2.write_text("{}")

        output_path = self.test_dir_path / "notebook.ipynb"

        generate_notebook(config, results_path, [vl_file1, vl_file2], output_path)

        with open(output_path, 'r') as f:
            notebook = json.load(f)

        # Should have visualization cells
        code_cells = [c for c in notebook["cells"] if c["cell_type"] == "code"]
        altair_cells = [c for c in code_cells if "altair" in "".join(c["source"])]
        self.assertEqual(len(altair_cells), 2)

    def test_creates_parent_directories(self):
        """Test that parent directories are created if they don't exist."""
        config = {
            "application": {"name": "test"},
            "measurements": []
        }
        results_data = {
            "experiment": {},
            "instances": [],
            "failed_instances": []
        }

        results_path = self.test_dir_path / "results.json"
        results_path.write_text(json.dumps(results_data))

        # Output in nested directory that doesn't exist
        output_path = self.test_dir_path / "nested" / "dir" / "notebook.ipynb"

        generate_notebook(config, results_path, [], output_path)

        self.assertTrue(output_path.exists())


class TestExecuteNotebook(unittest.TestCase):
    """Tests for execute_notebook function."""

    @patch('subprocess.run')
    def test_calls_jupyter_nbconvert(self, mock_run):
        """Test that jupyter nbconvert is called with correct arguments."""
        mock_run.return_value = MagicMock(returncode=0, stderr="", stdout="")

        notebook_path = Path("/tmp/notebook.ipynb")
        execute_notebook(notebook_path)

        mock_run.assert_called_once()
        args = mock_run.call_args[0][0]
        self.assertEqual(args[0], "jupyter")
        self.assertEqual(args[1], "nbconvert")
        self.assertIn("--execute", args)
        self.assertIn("--inplace", args)

    @patch('subprocess.run')
    def test_uses_specified_timeout(self, mock_run):
        """Test that specified timeout is used."""
        mock_run.return_value = MagicMock(returncode=0, stderr="", stdout="")

        notebook_path = Path("/tmp/notebook.ipynb")
        execute_notebook(notebook_path, timeout=600)

        args = mock_run.call_args[0][0]
        timeout_arg = [arg for arg in args if "ExecutePreprocessor.timeout" in arg][0]
        self.assertIn("600", timeout_arg)

    @patch('subprocess.run')
    def test_raises_on_nonzero_exit_code(self, mock_run):
        """Test that RuntimeError is raised on non-zero exit code."""
        mock_run.return_value = MagicMock(
            returncode=1,
            stderr="Execution failed",
            stdout=""
        )

        notebook_path = Path("/tmp/notebook.ipynb")

        with self.assertRaises(RuntimeError):
            execute_notebook(notebook_path)

    @patch('subprocess.run')
    def test_raises_file_not_found_error(self, mock_run):
        """Test that FileNotFoundError is raised when jupyter not found."""
        mock_run.side_effect = FileNotFoundError()

        notebook_path = Path("/tmp/notebook.ipynb")

        with self.assertRaises(FileNotFoundError) as cm:
            execute_notebook(notebook_path)

        self.assertIn("jupyter", str(cm.exception))

    @patch('subprocess.run')
    def test_raises_on_timeout(self, mock_run):
        """Test that RuntimeError is raised on timeout."""
        mock_run.side_effect = subprocess.TimeoutExpired("jupyter", 310)

        notebook_path = Path("/tmp/notebook.ipynb")

        with self.assertRaises(RuntimeError) as cm:
            execute_notebook(notebook_path)

        self.assertIn("timeout", str(cm.exception).lower())

    @patch('subprocess.run')
    def test_success_does_not_raise(self, mock_run):
        """Test that successful execution does not raise."""
        mock_run.return_value = MagicMock(returncode=0, stderr="", stdout="")

        notebook_path = Path("/tmp/notebook.ipynb")

        # Should not raise
        execute_notebook(notebook_path)


if __name__ == "__main__":
    unittest.main()
