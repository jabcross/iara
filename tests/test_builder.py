"""
Unit tests for the builder module.

Tests BuildResult dataclass, JSON persistence (read/write), and round-trip
workflows for Phase 2 and Phase 3 experiment framework.
"""

import json
import unittest
import tempfile
from pathlib import Path
from datetime import datetime, timezone

from tools.experiment_framework.builder import (
    BuildResult,
    BuildResults,
    FailedBuildResult,
    write_build_results,
    read_build_results,
    update_instance_execution,
)
from tools.experiment_framework.config import ConfigError


class TestBuildResultDataclass(unittest.TestCase):
    """Test cases for BuildResult dataclass with execution field."""

    def test_build_result_with_execution(self):
        """Test creating BuildResult with execution data."""
        execution_data = {
            "instance_name": "test_instance",
            "runs": [
                {"run_number": 1, "measurements": {"compute_time_s": 1.23}},
                {"run_number": 2, "measurements": {"compute_time_s": 1.25}},
            ],
            "failures": [],
            "total_runs": 2,
            "successful_runs": 2,
            "failed_runs": 0,
            "statistics": {
                "compute_time_s": {
                    "mean": 1.24,
                    "std": 0.01,
                    "min": 1.23,
                    "max": 1.25,
                    "count": 2
                }
            }
        }

        result = BuildResult(
            success=True,
            instance_name="test_instance",
            attempt=1,
            timestamp="2024-01-15T10:00:00Z",
            compilation={"total_time_s": 5.0},
            binary_size_bytes=1024000,
            execution=execution_data
        )

        self.assertTrue(result.success)
        self.assertEqual(result.instance_name, "test_instance")
        self.assertIsNotNone(result.execution)
        self.assertEqual(result.execution["successful_runs"], 2)
        self.assertIn("statistics", result.execution)

    def test_build_result_without_execution(self):
        """Test backward compatibility: BuildResult without execution field."""
        result = BuildResult(
            success=True,
            instance_name="test_instance",
            attempt=1,
            timestamp="2024-01-15T10:00:00Z",
            compilation={"total_time_s": 5.0},
            binary_size_bytes=1024000
        )

        self.assertTrue(result.success)
        self.assertEqual(result.instance_name, "test_instance")
        self.assertIsNone(result.execution)

    def test_build_result_execution_none_by_default(self):
        """Test that execution field defaults to None."""
        result = BuildResult(
            success=True,
            instance_name="test",
            attempt=1,
            timestamp="2024-01-15T10:00:00Z"
        )

        self.assertIsNone(result.execution)

    def test_build_result_execution_can_be_set(self):
        """Test that execution field can be set after creation."""
        result = BuildResult(
            success=True,
            instance_name="test",
            attempt=1,
            timestamp="2024-01-15T10:00:00Z"
        )

        execution_data = {"runs": [], "statistics": {}}
        result.execution = execution_data

        self.assertIsNotNone(result.execution)
        self.assertEqual(result.execution, execution_data)


class TestReadBuildResults(unittest.TestCase):
    """Test cases for read_build_results function."""

    def setUp(self):
        """Set up test fixtures."""
        self.test_dir = tempfile.TemporaryDirectory()
        self.test_path = Path(self.test_dir.name)

    def tearDown(self):
        """Clean up test fixtures."""
        self.test_dir.cleanup()

    def test_read_build_results_phase2_json(self):
        """Test reading Phase 2 JSON (without execution field)."""
        # Create a Phase 2 JSON file
        phase2_json = {
            "schema_version": "1.0.0",
            "experiment": {
                "application": "cholesky",
                "experiment_set": "regression_test",
                "timestamp": "2024-01-15T10:00:00Z",
                "git_commit": "abc123",
                "yaml_hash": "def456"
            },
            "instances": [
                {
                    "name": "05-cholesky_vf-omp_matrix-size_2048_num-blocks_2",
                    "parameters": {"scheduler": "vf-omp", "matrix_size": 2048, "num_blocks": 2},
                    "compilation": {"total_time_s": 5.0, "iara_opt_time_s": 2.0},
                    "binary": {"total_size_bytes": 1024000}
                }
            ],
            "failed_instances": [],
            "statistics": {
                "total_instances": 1,
                "successful_count": 1,
                "failed_count": 0,
                "success_rate": 1.0
            }
        }

        json_file = self.test_path / "phase2.json"
        json_file.write_text(json.dumps(phase2_json))

        # Read it back
        results = read_build_results(json_file)

        # Verify structure
        self.assertEqual(len(results.successful_instances), 1)
        self.assertEqual(len(results.failed_instances), 0)
        self.assertEqual(results.successful_instances[0].instance_name,
                        "05-cholesky_vf-omp_matrix-size_2048_num-blocks_2")
        self.assertIsNone(results.successful_instances[0].execution)

    def test_read_build_results_phase3_json(self):
        """Test reading Phase 3 JSON (with execution field)."""
        # Create a Phase 3 JSON file with execution data
        phase3_json = {
            "schema_version": "1.0.0",
            "experiment": {
                "application": "cholesky",
                "experiment_set": "regression_test",
                "timestamp": "2024-01-15T10:00:00Z",
                "git_commit": "abc123",
                "yaml_hash": "def456"
            },
            "instances": [
                {
                    "name": "05-cholesky_vf-omp_matrix-size_2048_num-blocks_2",
                    "parameters": {"scheduler": "vf-omp", "matrix_size": 2048, "num_blocks": 2},
                    "compilation": {"total_time_s": 5.0},
                    "binary": {"total_size_bytes": 1024000},
                    "execution": {
                        "instance_name": "05-cholesky_vf-omp_matrix-size_2048_num-blocks_2",
                        "runs": [
                            {"run_number": 1, "measurements": {"compute_time_s": 1.23}},
                            {"run_number": 2, "measurements": {"compute_time_s": 1.25}},
                        ],
                        "failures": [],
                        "total_runs": 2,
                        "successful_runs": 2,
                        "failed_runs": 0,
                        "statistics": {
                            "compute_time_s": {
                                "mean": 1.24,
                                "std": 0.01,
                                "min": 1.23,
                                "max": 1.25,
                                "count": 2
                            }
                        }
                    }
                }
            ],
            "failed_instances": [],
            "statistics": {
                "total_instances": 1,
                "successful_count": 1,
                "failed_count": 0,
                "success_rate": 1.0
            }
        }

        json_file = self.test_path / "phase3.json"
        json_file.write_text(json.dumps(phase3_json))

        # Read it back
        results = read_build_results(json_file)

        # Verify structure
        self.assertEqual(len(results.successful_instances), 1)
        instance = results.successful_instances[0]
        self.assertIsNotNone(instance.execution)
        self.assertEqual(instance.execution["successful_runs"], 2)
        self.assertIn("statistics", instance.execution)
        self.assertEqual(
            instance.execution["statistics"]["compute_time_s"]["mean"],
            1.24
        )

    def test_read_build_results_missing_file(self):
        """Test error handling when file doesn't exist."""
        nonexistent = self.test_path / "nonexistent.json"

        with self.assertRaises(ConfigError) as context:
            read_build_results(nonexistent)

        self.assertIn("not found", str(context.exception))

    def test_read_build_results_invalid_json(self):
        """Test error handling with invalid JSON."""
        json_file = self.test_path / "invalid.json"
        json_file.write_text("{ invalid json }")

        with self.assertRaises(ConfigError) as context:
            read_build_results(json_file)

        self.assertIn("Invalid JSON", str(context.exception))

    def test_read_build_results_invalid_schema_version(self):
        """Test error handling with unsupported schema version."""
        bad_json = {
            "schema_version": "2.0.0",  # Wrong version
            "experiment": {},
            "instances": [],
            "failed_instances": []
        }

        json_file = self.test_path / "bad_schema.json"
        json_file.write_text(json.dumps(bad_json))

        with self.assertRaises(ConfigError) as context:
            read_build_results(json_file)

        self.assertIn("Unsupported schema version", str(context.exception))

    def test_read_build_results_missing_required_field(self):
        """Test error handling when required fields are missing."""
        incomplete_json = {
            "schema_version": "1.0.0",
            "experiment": {"timestamp": "2024-01-15T10:00:00Z"},
            "instances": [
                {
                    "parameters": {},
                    # Missing 'name' field
                    "compilation": {},
                    "binary": {}
                }
            ],
            "failed_instances": []
        }

        json_file = self.test_path / "incomplete.json"
        json_file.write_text(json.dumps(incomplete_json))

        with self.assertRaises(ConfigError) as context:
            read_build_results(json_file)

        self.assertIn("Missing required field", str(context.exception))

    def test_read_build_results_with_failed_instances(self):
        """Test reading JSON with both successful and failed instances."""
        json_data = {
            "schema_version": "1.0.0",
            "experiment": {"timestamp": "2024-01-15T10:00:00Z"},
            "instances": [
                {
                    "name": "success_instance",
                    "parameters": {},
                    "compilation": {},
                    "binary": {"total_size_bytes": 1000}
                }
            ],
            "failed_instances": [
                {
                    "name": "failed_instance",
                    "parameters": {},
                    "attempts": 2,
                    "errors": [{"attempt": 1, "message": "Error 1"}],
                    "last_error": "Final error"
                }
            ]
        }

        json_file = self.test_path / "mixed.json"
        json_file.write_text(json.dumps(json_data))

        results = read_build_results(json_file)

        self.assertEqual(len(results.successful_instances), 1)
        self.assertEqual(len(results.failed_instances), 1)
        self.assertEqual(results.failed_instances[0].instance_name, "failed_instance")


class TestUpdateInstanceExecution(unittest.TestCase):
    """Test cases for update_instance_execution function."""

    def test_update_instance_execution_success(self):
        """Test successfully updating execution data for an instance."""
        results = BuildResults(
            successful_instances=[
                BuildResult(
                    success=True,
                    instance_name="test_instance",
                    attempt=1,
                    timestamp="2024-01-15T10:00:00Z"
                )
            ],
            failed_instances=[],
            timestamp="2024-01-15T10:00:00Z"
        )

        execution_data = {
            "runs": [{"run_number": 1}],
            "statistics": {"metric": {"mean": 1.0}}
        }

        update_instance_execution(results, "test_instance", execution_data)

        self.assertIsNotNone(results.successful_instances[0].execution)
        self.assertEqual(results.successful_instances[0].execution, execution_data)

    def test_update_instance_execution_not_found(self):
        """Test error when instance not found."""
        results = BuildResults(
            successful_instances=[
                BuildResult(
                    success=True,
                    instance_name="existing_instance",
                    attempt=1,
                    timestamp="2024-01-15T10:00:00Z"
                )
            ],
            failed_instances=[],
            timestamp="2024-01-15T10:00:00Z"
        )

        with self.assertRaises(ValueError) as context:
            update_instance_execution(results, "nonexistent_instance", {})

        self.assertIn("not found", str(context.exception))

    def test_update_instance_execution_multiple_instances(self):
        """Test updating one instance when multiple exist."""
        results = BuildResults(
            successful_instances=[
                BuildResult(
                    success=True,
                    instance_name="instance_1",
                    attempt=1,
                    timestamp="2024-01-15T10:00:00Z"
                ),
                BuildResult(
                    success=True,
                    instance_name="instance_2",
                    attempt=1,
                    timestamp="2024-01-15T10:00:00Z"
                ),
                BuildResult(
                    success=True,
                    instance_name="instance_3",
                    attempt=1,
                    timestamp="2024-01-15T10:00:00Z"
                ),
            ],
            failed_instances=[],
            timestamp="2024-01-15T10:00:00Z"
        )

        execution_data = {"runs": [], "statistics": {}}
        update_instance_execution(results, "instance_2", execution_data)

        # Verify only instance_2 was updated
        self.assertIsNone(results.successful_instances[0].execution)
        self.assertIsNotNone(results.successful_instances[1].execution)
        self.assertIsNone(results.successful_instances[2].execution)


class TestWriteBuildResultsWithExecution(unittest.TestCase):
    """Test cases for write_build_results function with execution data."""

    def setUp(self):
        """Set up test fixtures."""
        self.test_dir = tempfile.TemporaryDirectory()
        self.test_path = Path(self.test_dir.name)

    def tearDown(self):
        """Clean up test fixtures."""
        self.test_dir.cleanup()

    def test_write_build_results_with_execution(self):
        """Test writing Phase 3 JSON with execution data."""
        execution_data = {
            "instance_name": "test_instance",
            "runs": [
                {"run_number": 1, "measurements": {"compute_time_s": 1.23}},
            ],
            "failures": [],
            "total_runs": 1,
            "successful_runs": 1,
            "failed_runs": 0,
            "statistics": {
                "compute_time_s": {
                    "mean": 1.23,
                    "std": 0.0,
                    "min": 1.23,
                    "max": 1.23,
                    "count": 1
                }
            }
        }

        results = BuildResults(
            successful_instances=[
                BuildResult(
                    success=True,
                    instance_name="test_instance",
                    attempt=1,
                    timestamp="2024-01-15T10:00:00Z",
                    compilation={"total_time_s": 5.0},
                    binary_size_bytes=1024000,
                    execution=execution_data
                )
            ],
            failed_instances=[],
            timestamp="2024-01-15T10:00:00Z"
        )

        config = {"application": "test_app"}
        json_file = write_build_results(results, self.test_path, config, "test_set")

        # Read back and verify
        with open(json_file) as f:
            written_data = json.load(f)

        self.assertIn("execution", written_data["instances"][0])
        self.assertEqual(
            written_data["instances"][0]["execution"]["successful_runs"],
            1
        )

    def test_write_build_results_backward_compatible(self):
        """Test that Phase 2 format (without execution) still works."""
        results = BuildResults(
            successful_instances=[
                BuildResult(
                    success=True,
                    instance_name="test_instance",
                    attempt=1,
                    timestamp="2024-01-15T10:00:00Z",
                    compilation={"total_time_s": 5.0},
                    binary_size_bytes=1024000
                    # No execution field
                )
            ],
            failed_instances=[],
            timestamp="2024-01-15T10:00:00Z"
        )

        config = {"application": "test_app"}
        json_file = write_build_results(results, self.test_path, config, "test_set")

        # Read back and verify
        with open(json_file) as f:
            written_data = json.load(f)

        # Execution field should not be present
        self.assertNotIn("execution", written_data["instances"][0])

    def test_write_build_results_mixed_execution_status(self):
        """Test writing when some instances have execution, some don't."""
        results = BuildResults(
            successful_instances=[
                BuildResult(
                    success=True,
                    instance_name="instance_with_exec",
                    attempt=1,
                    timestamp="2024-01-15T10:00:00Z",
                    execution={"runs": [], "statistics": {}}
                ),
                BuildResult(
                    success=True,
                    instance_name="instance_without_exec",
                    attempt=1,
                    timestamp="2024-01-15T10:00:00Z"
                ),
            ],
            failed_instances=[],
            timestamp="2024-01-15T10:00:00Z"
        )

        config = {"application": "test_app"}
        json_file = write_build_results(results, self.test_path, config, "test_set")

        with open(json_file) as f:
            written_data = json.load(f)

        # First instance should have execution
        self.assertIn("execution", written_data["instances"][0])
        # Second instance should not
        self.assertNotIn("execution", written_data["instances"][1])


class TestRoundTripWorkflow(unittest.TestCase):
    """Test cases for round-trip read-write workflows."""

    def setUp(self):
        """Set up test fixtures."""
        self.test_dir = tempfile.TemporaryDirectory()
        self.test_path = Path(self.test_dir.name)

    def tearDown(self):
        """Clean up test fixtures."""
        self.test_dir.cleanup()

    def test_read_write_roundtrip_phase2(self):
        """Test reading Phase 2 JSON, writing it back unchanged."""
        # Create original Phase 2 JSON
        original_json = {
            "schema_version": "1.0.0",
            "experiment": {
                "application": "cholesky",
                "experiment_set": "regression_test",
                "timestamp": "2024-01-15T10:00:00Z",
                "git_commit": "abc123",
                "yaml_hash": "def456"
            },
            "instances": [
                {
                    "name": "05-cholesky_vf-omp_matrix-size_2048",
                    "parameters": {"scheduler": "vf-omp"},
                    "compilation": {"total_time_s": 5.0},
                    "binary": {"total_size_bytes": 1024000}
                }
            ],
            "failed_instances": [],
            "statistics": {
                "total_instances": 1,
                "successful_count": 1,
                "failed_count": 0,
                "success_rate": 1.0
            }
        }

        input_file = self.test_path / "phase2_original.json"
        input_file.write_text(json.dumps(original_json, indent=2))

        # Read it
        results = read_build_results(input_file)

        # Write it back
        config = {"application": "cholesky"}
        output_file = write_build_results(results, self.test_path, config, "regression_test")

        # Read the written file
        with open(output_file) as f:
            written_json = json.load(f)

        # Verify key properties are preserved
        self.assertEqual(written_json["schema_version"], "1.0.0")
        self.assertEqual(len(written_json["instances"]), 1)
        self.assertEqual(written_json["instances"][0]["name"],
                        "05-cholesky_vf-omp_matrix-size_2048")
        self.assertNotIn("execution", written_json["instances"][0])

    def test_read_write_roundtrip_phase3(self):
        """Test reading Phase 3 JSON, writing it back with execution preserved."""
        # Create Phase 3 JSON
        original_json = {
            "schema_version": "1.0.0",
            "experiment": {
                "application": "cholesky",
                "experiment_set": "regression_test",
                "timestamp": "2024-01-15T10:00:00Z",
                "git_commit": "abc123",
                "yaml_hash": "def456"
            },
            "instances": [
                {
                    "name": "05-cholesky_vf-omp_matrix-size_2048",
                    "parameters": {"scheduler": "vf-omp"},
                    "compilation": {"total_time_s": 5.0},
                    "binary": {"total_size_bytes": 1024000},
                    "execution": {
                        "instance_name": "05-cholesky_vf-omp_matrix-size_2048",
                        "runs": [
                            {"run_number": 1, "measurements": {"compute_time_s": 1.23}},
                        ],
                        "failures": [],
                        "total_runs": 1,
                        "successful_runs": 1,
                        "failed_runs": 0,
                        "statistics": {
                            "compute_time_s": {
                                "mean": 1.23,
                                "std": 0.0,
                                "min": 1.23,
                                "max": 1.23,
                                "count": 1
                            }
                        }
                    }
                }
            ],
            "failed_instances": [],
            "statistics": {
                "total_instances": 1,
                "successful_count": 1,
                "failed_count": 0,
                "success_rate": 1.0
            }
        }

        input_file = self.test_path / "phase3_original.json"
        input_file.write_text(json.dumps(original_json, indent=2))

        # Read it
        results = read_build_results(input_file)

        # Write it back
        config = {"application": "cholesky"}
        output_file = write_build_results(results, self.test_path, config, "regression_test")

        # Read the written file
        with open(output_file) as f:
            written_json = json.load(f)

        # Verify execution is preserved
        self.assertIn("execution", written_json["instances"][0])
        self.assertEqual(
            written_json["instances"][0]["execution"]["successful_runs"],
            1
        )
        self.assertIn("statistics", written_json["instances"][0]["execution"])

    def test_read_phase2_add_execution_write_phase3(self):
        """Test Phase 3 workflow: read Phase 2, add execution, write Phase 3."""
        # Create Phase 2 JSON
        phase2_json = {
            "schema_version": "1.0.0",
            "experiment": {
                "application": "cholesky",
                "experiment_set": "regression_test",
                "timestamp": "2024-01-15T10:00:00Z",
                "git_commit": "abc123",
                "yaml_hash": "def456"
            },
            "instances": [
                {
                    "name": "05-cholesky_vf-omp_matrix-size_2048",
                    "parameters": {"scheduler": "vf-omp"},
                    "compilation": {"total_time_s": 5.0},
                    "binary": {"total_size_bytes": 1024000}
                }
            ],
            "failed_instances": [],
            "statistics": {
                "total_instances": 1,
                "successful_count": 1,
                "failed_count": 0,
                "success_rate": 1.0
            }
        }

        input_file = self.test_path / "phase2.json"
        input_file.write_text(json.dumps(phase2_json, indent=2))

        # Step 1: Read Phase 2
        results = read_build_results(input_file)
        self.assertIsNone(results.successful_instances[0].execution)

        # Step 2: Add execution data
        execution_data = {
            "instance_name": "05-cholesky_vf-omp_matrix-size_2048",
            "runs": [
                {"run_number": 1, "measurements": {"compute_time_s": 1.23}},
                {"run_number": 2, "measurements": {"compute_time_s": 1.25}},
            ],
            "failures": [],
            "total_runs": 2,
            "successful_runs": 2,
            "failed_runs": 0,
            "statistics": {
                "compute_time_s": {
                    "mean": 1.24,
                    "std": 0.01,
                    "min": 1.23,
                    "max": 1.25,
                    "count": 2
                }
            }
        }

        update_instance_execution(
            results,
            "05-cholesky_vf-omp_matrix-size_2048",
            execution_data
        )

        # Step 3: Write Phase 3 JSON
        config = {"application": "cholesky"}
        output_file = write_build_results(results, self.test_path, config, "regression_test")

        # Step 4: Verify Phase 3 JSON
        with open(output_file) as f:
            phase3_json = json.load(f)

        # Verify compilation data preserved
        self.assertEqual(phase3_json["instances"][0]["compilation"]["total_time_s"], 5.0)
        self.assertEqual(phase3_json["instances"][0]["binary"]["total_size_bytes"], 1024000)

        # Verify execution data added
        self.assertIn("execution", phase3_json["instances"][0])
        exec_data = phase3_json["instances"][0]["execution"]
        self.assertEqual(exec_data["successful_runs"], 2)
        self.assertEqual(exec_data["statistics"]["compute_time_s"]["mean"], 1.24)


class TestJSONSchemaValidation(unittest.TestCase):
    """Test cases for JSON schema validation."""

    def setUp(self):
        """Set up test fixtures."""
        self.test_dir = tempfile.TemporaryDirectory()
        self.test_path = Path(self.test_dir.name)

    def tearDown(self):
        """Clean up test fixtures."""
        self.test_dir.cleanup()

    def test_write_results_creates_valid_json(self):
        """Test that write_build_results creates valid JSON."""
        results = BuildResults(
            successful_instances=[
                BuildResult(
                    success=True,
                    instance_name="test_instance",
                    attempt=1,
                    timestamp="2024-01-15T10:00:00Z",
                    compilation={"total_time_s": 5.0},
                    binary_size_bytes=1000
                )
            ],
            failed_instances=[],
            timestamp="2024-01-15T10:00:00Z"
        )

        config = {"application": "test_app"}
        json_file = write_build_results(results, self.test_path, config, "test_set")

        # Verify it's valid JSON
        with open(json_file) as f:
            data = json.load(f)

        # Verify schema structure
        self.assertEqual(data["schema_version"], "1.0.0")
        self.assertIn("experiment", data)
        self.assertIn("instances", data)
        self.assertIn("failed_instances", data)
        self.assertIn("statistics", data)

    def test_schema_version_consistent(self):
        """Test that schema version is consistent across read/write."""
        original = {
            "schema_version": "1.0.0",
            "experiment": {"timestamp": "2024-01-15T10:00:00Z"},
            "instances": [],
            "failed_instances": []
        }

        json_file = self.test_path / "version_test.json"
        json_file.write_text(json.dumps(original))

        results = read_build_results(json_file)
        config = {"application": "test"}
        output_file = write_build_results(results, self.test_path, config, "test")

        with open(output_file) as f:
            output = json.load(f)

        self.assertEqual(output["schema_version"], "1.0.0")


if __name__ == '__main__':
    unittest.main()
