#!/usr/bin/env python3
"""
Standalone JSON schema validation script.

This validates the JSON output structure against the specification.
"""

import json
import tempfile
from pathlib import Path
from datetime import datetime, timezone


def validate_json_schema(json_data):
    """Validate JSON structure against schema specification."""
    errors = []

    # Check schema version
    if json_data.get("schema_version") != "1.0.0":
        errors.append("schema_version must be '1.0.0'")

    # Check experiment section
    exp = json_data.get("experiment", {})
    required_exp_fields = ["application", "experiment_set", "timestamp", "git_commit", "yaml_hash"]
    for field in required_exp_fields:
        if field not in exp:
            errors.append(f"experiment.{field} is required")

    # Check git_commit format
    git_commit = exp.get("git_commit", "")
    if git_commit != "unknown" and len(git_commit) != 8:
        errors.append(f"git_commit should be 8 chars or 'unknown', got '{git_commit}'")

    # Check yaml_hash format
    yaml_hash = exp.get("yaml_hash", "")
    if yaml_hash != "unknown" and len(yaml_hash) != 64:
        errors.append(f"yaml_hash should be 64 chars or 'unknown', got {len(yaml_hash)}")

    # Check instances array
    instances = json_data.get("instances", [])
    if not isinstance(instances, list):
        errors.append("instances must be an array")
    else:
        for i, inst in enumerate(instances):
            required_inst_fields = ["name", "parameters", "compilation", "binary"]
            for field in required_inst_fields:
                if field not in inst:
                    errors.append(f"instances[{i}].{field} is required")

            # Check binary structure
            binary = inst.get("binary", {})
            binary_fields = ["total_size_bytes", "text_section_bytes", "data_section_bytes", "bss_section_bytes"]
            for field in binary_fields:
                if field not in binary:
                    errors.append(f"instances[{i}].binary.{field} is required")

            # Check compilation has timing data
            compilation = inst.get("compilation", {})
            if "total_time_s" not in compilation and not compilation:
                errors.append(f"instances[{i}].compilation should have timing data")

    # Check failed_instances array
    failed = json_data.get("failed_instances", [])
    if not isinstance(failed, list):
        errors.append("failed_instances must be an array")
    else:
        for i, failed_inst in enumerate(failed):
            required_fail_fields = ["name", "parameters", "attempts", "errors"]
            for field in required_fail_fields:
                if field not in failed_inst:
                    errors.append(f"failed_instances[{i}].{field} is required")

            # Check errors structure
            error_list = failed_inst.get("errors", [])
            if not isinstance(error_list, list):
                errors.append(f"failed_instances[{i}].errors must be an array")
            else:
                for j, err in enumerate(error_list):
                    error_fields = ["attempt", "phase", "step", "message", "timestamp"]
                    for field in error_fields:
                        if field not in err:
                            errors.append(f"failed_instances[{i}].errors[{j}].{field} is required")

    # Check statistics section
    stats = json_data.get("statistics", {})
    required_stats = ["total_instances", "successful_count", "failed_count", "success_rate"]
    for field in required_stats:
        if field not in stats:
            errors.append(f"statistics.{field} is required")

    # Validate statistics consistency
    total = stats.get("total_instances", 0)
    success = stats.get("successful_count", 0)
    failed = stats.get("failed_count", 0)

    if success + failed != total:
        errors.append(f"statistics: successful ({success}) + failed ({failed}) != total ({total})")

    if total > 0:
        expected_rate = success / total
        actual_rate = stats.get("success_rate", 0)
        if abs(expected_rate - actual_rate) > 0.001:
            errors.append(f"statistics.success_rate mismatch: expected {expected_rate:.3f}, got {actual_rate}")

    return errors


def create_sample_json():
    """Create a sample JSON file for validation."""
    data = {
        "schema_version": "1.0.0",
        "experiment": {
            "application": "05-cholesky",
            "experiment_set": "regression_test",
            "timestamp": "2025-01-14T15:00:00Z",
            "git_commit": "3b27d17e",
            "yaml_hash": "2178138e46af1df4b543c625cdd1b252a20915b1603bcf47e6a4502f87b7cc30"
        },
        "instances": [
            {
                "name": "05-cholesky_vf-omp_matrix-size_2048_num-blocks_2",
                "parameters": {
                    "scheduler": "vf-omp",
                    "matrix_size": 2048,
                    "num_blocks": 2
                },
                "compilation": {
                    "iara_opt_time_s": 1.234,
                    "mlir_llvm_time_s": 0.567,
                    "clang_time_s": 0.890,
                    "total_time_s": 2.691
                },
                "binary": {
                    "total_size_bytes": 1234567,
                    "text_section_bytes": 0,
                    "data_section_bytes": 0,
                    "bss_section_bytes": 0
                }
            }
        ],
        "failed_instances": [
            {
                "name": "05-cholesky_sequential_matrix-size_2048_num-blocks_4",
                "parameters": {
                    "scheduler": "sequential",
                    "matrix_size": 2048,
                    "num_blocks": 4
                },
                "attempts": 2,
                "errors": [
                    {
                        "attempt": 1,
                        "phase": "build",
                        "step": "iara-opt",
                        "message": "error message here",
                        "timestamp": "2025-01-14T15:00:10Z"
                    }
                ]
            }
        ],
        "statistics": {
            "total_instances": 2,
            "successful_count": 1,
            "failed_count": 1,
            "success_rate": 0.5
        }
    }
    return data


if __name__ == "__main__":
    print("=" * 70)
    print("JSON Schema Validation Test")
    print("=" * 70)

    # Create and validate sample JSON
    print("\nGenerating sample JSON based on specification...")
    sample_json = create_sample_json()

    # Write to file for inspection
    with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
        json.dump(sample_json, f, indent=2, sort_keys=True)
        temp_path = f.name

    print(f"Sample JSON written to: {temp_path}")

    # Validate
    print("\nValidating JSON schema...")
    errors = validate_json_schema(sample_json)

    if errors:
        print("VALIDATION FAILED:")
        for error in errors:
            print(f"  ✗ {error}")
        exit(1)
    else:
        print("✓ All schema requirements validated successfully!")

    # Print statistics
    print("\nSample JSON Statistics:")
    stats = sample_json["statistics"]
    print(f"  Total instances: {stats['total_instances']}")
    print(f"  Successful: {stats['successful_count']}")
    print(f"  Failed: {stats['failed_count']}")
    print(f"  Success rate: {stats['success_rate']:.1%}")

    # Show structure
    print("\nJSON Structure:")
    print(f"  Keys: {sorted(sample_json.keys())}")
    print(f"  Successful instances: {len(sample_json['instances'])}")
    print(f"  Failed instances: {len(sample_json['failed_instances'])}")

    # Show sample instance
    if sample_json["instances"]:
        inst = sample_json["instances"][0]
        print(f"\nSample successful instance:")
        print(f"  Name: {inst['name']}")
        print(f"  Parameters: {inst['parameters']}")
        print(f"  Total time: {inst['compilation']['total_time_s']}s")
        print(f"  Binary size: {inst['binary']['total_size_bytes']} bytes")

    # Show sample failed instance
    if sample_json["failed_instances"]:
        failed = sample_json["failed_instances"][0]
        print(f"\nSample failed instance:")
        print(f"  Name: {failed['name']}")
        print(f"  Attempts: {failed['attempts']}")
        print(f"  Errors: {len(failed['errors'])}")
        if failed["errors"]:
            err = failed["errors"][0]
            print(f"    Error[0]: phase={err['phase']}, step={err['step']}")

    print("\n" + "=" * 70)
    print("Schema validation PASSED!")
    print("=" * 70)
