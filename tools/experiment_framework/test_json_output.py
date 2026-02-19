#!/usr/bin/env python3
"""
Test script for JSON results output functionality.

Tests the following:
1. get_git_commit() - git commit hash extraction
2. get_yaml_hash() - yaml hash file reading
3. write_build_results() - JSON file generation and schema validation
4. Parameter parsing from instance names
"""

import json
import tempfile
from pathlib import Path
from datetime import datetime, timezone

from builder import (
    BuildResult,
    BuildResults,
    FailedBuildResult,
    get_git_commit,
    get_yaml_hash,
    write_build_results,
    extract_binary_sections,
    _parse_instance_name
)


def test_get_git_commit():
    """Test git commit extraction."""
    print("\n=== Test: get_git_commit() ===")
    commit = get_git_commit()
    print(f"Git commit: {commit}")
    assert isinstance(commit, str), "Should return string"
    assert len(commit) == 8 or commit == "unknown", f"Should be 8 chars or 'unknown', got {len(commit)}"
    print("PASS: get_git_commit returns valid format")


def test_get_yaml_hash():
    """Test YAML hash file reading."""
    print("\n=== Test: get_yaml_hash() ===")

    # Test with real hash file
    hash_file = Path("/scratch/pedro.ciambra/repos/iara/applications/05-cholesky/experiment")
    if hash_file.exists():
        hash_val = get_yaml_hash(hash_file)
        print(f"YAML hash: {hash_val}")
        assert len(hash_val) == 64 or hash_val == "unknown", f"Expected 64 chars or 'unknown', got {len(hash_val)}"
        print("PASS: get_yaml_hash reads valid hash")

    # Test with non-existent directory
    hash_val = get_yaml_hash(Path("/nonexistent"))
    print(f"Non-existent dir returns: {hash_val}")
    assert hash_val == "unknown", "Should return 'unknown' for non-existent dir"
    print("PASS: get_yaml_hash handles missing file")


def test_parse_instance_name():
    """Test instance name parameter parsing."""
    print("\n=== Test: _parse_instance_name() ===")

    test_cases = [
        ("05-cholesky_vf-omp_matrix-size_2048_num-blocks_2",
         {"scheduler": "vf-omp", "matrix_size": 2048, "num_blocks": 2}),
        ("08-sift_sequential_image-size_512",
         {"scheduler": "sequential", "image_size": 512}),
        ("simple_enkits",
         {"scheduler": "enkits"}),
    ]

    for instance_name, expected_params in test_cases:
        parsed = _parse_instance_name(instance_name)
        print(f"  {instance_name}")
        print(f"    -> {parsed}")
        assert parsed == expected_params, f"Expected {expected_params}, got {parsed}"

    print("PASS: _parse_instance_name works correctly")


def test_extract_binary_sections():
    """Test binary section extraction."""
    print("\n=== Test: extract_binary_sections() ===")

    result = BuildResult(
        success=True,
        instance_name="test_app",
        attempt=1,
        timestamp=datetime.now(timezone.utc).isoformat(),
        binary_size_bytes=1234567
    )

    sections = extract_binary_sections(result)
    print(f"Binary sections: {sections}")

    assert sections["total_size_bytes"] == 1234567, "Should have correct total size"
    assert "text_section_bytes" in sections, "Should have text section"
    assert "data_section_bytes" in sections, "Should have data section"
    assert "bss_section_bytes" in sections, "Should have bss section"

    print("PASS: extract_binary_sections returns correct structure")


def test_write_build_results():
    """Test JSON file generation."""
    print("\n=== Test: write_build_results() ===")

    with tempfile.TemporaryDirectory() as tmpdir:
        output_dir = Path(tmpdir)

        # Create sample build results
        timestamp = "2025-01-14T15:00:00Z"
        successful = [
            BuildResult(
                success=True,
                instance_name="05-cholesky_vf-omp_matrix-size_2048_num-blocks_2",
                attempt=1,
                timestamp=timestamp,
                compilation={
                    "iara_opt_time_s": 1.234,
                    "mlir_llvm_time_s": 0.567,
                    "clang_time_s": 0.890,
                    "total_time_s": 2.691
                },
                binary_size_bytes=1234567
            ),
            BuildResult(
                success=True,
                instance_name="05-cholesky_vf-omp_matrix-size_2048_num-blocks_4",
                attempt=1,
                timestamp=timestamp,
                compilation={
                    "total_time_s": 3.5
                },
                binary_size_bytes=2345678
            )
        ]

        failed = [
            FailedBuildResult(
                instance_name="05-cholesky_sequential_matrix-size_2048_num-blocks_4",
                attempts=2,
                errors=[
                    {
                        "attempt": 1,
                        "phase": "build",
                        "step": "iara-opt",
                        "message": "Optimization failed",
                        "timestamp": timestamp
                    },
                    {
                        "attempt": 2,
                        "phase": "build",
                        "step": "clang",
                        "message": "Linker error",
                        "timestamp": timestamp
                    }
                ],
                timestamp=timestamp,
                last_error="Linker error"
            )
        ]

        results = BuildResults(
            successful_instances=successful,
            failed_instances=failed,
            timestamp=timestamp
        )

        # Write results
        config = {"application": "05-cholesky"}
        json_path = write_build_results(
            results=results,
            output_dir=output_dir,
            config=config,
            experiment_set="regression_test",
            experiment_dir=Path("/scratch/pedro.ciambra/repos/iara/applications/05-cholesky/experiment")
        )

        print(f"Output file: {json_path}")
        assert json_path.exists(), "JSON file should be created"

        # Validate JSON structure
        with open(json_path) as f:
            data = json.load(f)

        print("\nJSON Schema Validation:")
        print(f"  schema_version: {data.get('schema_version')}")
        assert data["schema_version"] == "1.0.0", "Should have correct schema version"

        print(f"  experiment.application: {data['experiment']['application']}")
        assert data["experiment"]["application"] == "05-cholesky"

        print(f"  experiment.experiment_set: {data['experiment']['experiment_set']}")
        assert data["experiment"]["experiment_set"] == "regression_test"

        print(f"  experiment.git_commit: {data['experiment']['git_commit']}")
        assert len(data["experiment"]["git_commit"]) == 8 or data["experiment"]["git_commit"] == "unknown"

        print(f"  experiment.yaml_hash: {data['experiment']['yaml_hash']}")
        assert len(data["experiment"]["yaml_hash"]) == 64 or data["experiment"]["yaml_hash"] == "unknown"

        print(f"  instances: {len(data['instances'])} successful")
        assert len(data["instances"]) == 2, "Should have 2 successful instances"

        # Validate first instance structure
        inst = data["instances"][0]
        print(f"\n  Instance[0] structure:")
        print(f"    name: {inst['name']}")
        assert "name" in inst, "Instance should have name"
        assert "parameters" in inst, "Instance should have parameters"
        assert "compilation" in inst, "Instance should have compilation"
        assert "binary" in inst, "Instance should have binary"

        print(f"    parameters: {inst['parameters']}")
        assert "scheduler" in inst["parameters"], "Parameters should have scheduler"
        assert inst["parameters"]["scheduler"] == "vf-omp"
        assert inst["parameters"]["matrix_size"] == 2048

        print(f"    compilation.total_time_s: {inst['compilation'].get('total_time_s')}")
        assert "total_time_s" in inst["compilation"]

        print(f"    binary.total_size_bytes: {inst['binary']['total_size_bytes']}")
        assert inst["binary"]["total_size_bytes"] == 1234567

        print(f"  failed_instances: {len(data['failed_instances'])} failed")
        assert len(data["failed_instances"]) == 1, "Should have 1 failed instance"

        # Validate failed instance structure
        failed = data["failed_instances"][0]
        print(f"\n  Failed instance[0] structure:")
        print(f"    name: {failed['name']}")
        print(f"    attempts: {failed['attempts']}")
        assert failed["attempts"] == 2
        assert len(failed["errors"]) == 2, "Should have 2 error entries"

        print(f"    errors[0].step: {failed['errors'][0]['step']}")
        assert failed["errors"][0]["step"] == "iara-opt"
        assert failed["errors"][1]["step"] == "clang"

        print(f"\n  statistics:")
        stats = data["statistics"]
        print(f"    total_instances: {stats['total_instances']}")
        print(f"    successful_count: {stats['successful_count']}")
        print(f"    failed_count: {stats['failed_count']}")
        print(f"    success_rate: {stats['success_rate']}")

        assert stats["total_instances"] == 3
        assert stats["successful_count"] == 2
        assert stats["failed_count"] == 1
        assert abs(stats["success_rate"] - 0.667) < 0.001, f"Expected ~0.667, got {stats['success_rate']}"

        print("\nPASS: write_build_results generates valid JSON")

        # Print sample output
        print("\n=== Sample JSON Output ===")
        print(json.dumps(data, indent=2)[:1500] + "\n...")


def test_json_round_trip():
    """Test that generated JSON can be loaded and validated."""
    print("\n=== Test: JSON Round-trip ===")

    with tempfile.TemporaryDirectory() as tmpdir:
        output_dir = Path(tmpdir)

        results = BuildResults(
            successful_instances=[
                BuildResult(
                    success=True,
                    instance_name="app_scheduler_param_100",
                    attempt=1,
                    timestamp="2025-01-14T15:00:00Z",
                    compilation={"total_time_s": 1.5},
                    binary_size_bytes=512
                )
            ],
            failed_instances=[],
            timestamp="2025-01-14T15:00:00Z"
        )

        config = {"application": "test_app"}
        json_path = write_build_results(results, output_dir, config, "test_set")

        # Load and validate
        with open(json_path) as f:
            data = json.load(f)

        # Perform comprehensive checks
        required_top_level_keys = ["schema_version", "experiment", "instances", "failed_instances", "statistics"]
        for key in required_top_level_keys:
            assert key in data, f"Missing required key: {key}"

        assert "application" in data["experiment"]
        assert "git_commit" in data["experiment"]
        assert "yaml_hash" in data["experiment"]

        print("PASS: JSON round-trip validation successful")


if __name__ == "__main__":
    print("=" * 60)
    print("Testing JSON Results Output Implementation")
    print("=" * 60)

    try:
        test_get_git_commit()
        test_get_yaml_hash()
        test_parse_instance_name()
        test_extract_binary_sections()
        test_write_build_results()
        test_json_round_trip()

        print("\n" + "=" * 60)
        print("All tests PASSED!")
        print("=" * 60)
    except AssertionError as e:
        print(f"\nTest FAILED: {e}")
        exit(1)
    except Exception as e:
        print(f"\nUnexpected error: {e}")
        import traceback
        traceback.print_exc()
        exit(1)
