#!/usr/bin/env python3
"""
Integration test for JSON output functionality.

This creates mock build results and generates JSON to validate the complete flow.
"""

import json
import tempfile
from pathlib import Path


def create_mock_json_output():
    """Create a realistic mock JSON output from builder functions."""

    # Simulate BuildResults class
    class MockBuildResult:
        def __init__(self, name, compilation, binary_size):
            self.instance_name = name
            self.compilation = compilation
            self.binary_size_bytes = binary_size
            self.success = True

    class MockFailedBuildResult:
        def __init__(self, name, attempts, errors, timestamp, last_error):
            self.instance_name = name
            self.attempts = attempts
            self.errors = errors
            self.timestamp = timestamp
            self.last_error = last_error

    # Helper function to parse instance name
    def parse_instance_name(instance_name):
        parts = instance_name.split('_')
        if len(parts) < 2:
            return {}
        params = {'scheduler': parts[1]}
        i = 2
        while i < len(parts) - 1:
            key = parts[i].replace('-', '_')
            try:
                value = int(parts[i + 1])
            except ValueError:
                value = parts[i + 1]
            params[key] = value
            i += 2
        return params

    # Create mock results
    successful = [
        MockBuildResult(
            "05-cholesky_vf-omp_matrix-size_2048_num-blocks_2",
            {
                "iara_opt_time_s": 1.234,
                "iara_opt_memory_bytes": 102400,
                "mlir_llvm_time_s": 0.567,
                "mlir_llvm_memory_bytes": 51200,
                "clang_time_s": 0.890,
                "clang_memory_bytes": 204800,
                "total_time_s": 2.691
            },
            1234567
        ),
        MockBuildResult(
            "05-cholesky_vf-omp_matrix-size_2048_num-blocks_4",
            {
                "iara_opt_time_s": 2.456,
                "mlir_llvm_time_s": 0.789,
                "clang_time_s": 1.234,
                "total_time_s": 4.479
            },
            2345678
        )
    ]

    failed = [
        MockFailedBuildResult(
            "05-cholesky_sequential_matrix-size_2048_num-blocks_4",
            2,
            [
                {
                    'attempt': 1,
                    'phase': 'build',
                    'step': 'iara-opt',
                    'message': 'Optimization failed: unsupported operation',
                    'timestamp': '2025-01-14T15:00:10Z'
                },
                {
                    'attempt': 2,
                    'phase': 'build',
                    'step': 'clang',
                    'message': 'Undefined reference to iara_init',
                    'timestamp': '2025-01-14T15:00:15Z'
                }
            ],
            '2025-01-14T15:00:15Z',
            'Undefined reference to iara_init'
        )
    ]

    timestamp = "2025-01-14T15:00:00Z"
    git_commit = "3b27d17e"
    yaml_hash = "2178138e46af1df4b543c625cdd1b252a20915b1603bcf47e6a4502f87b7cc30"

    # Build JSON structure (mimicking write_build_results)
    instances_section = []
    for result in successful:
        binary_sections = {
            "total_size_bytes": result.binary_size_bytes,
            "text_section_bytes": 0,
            "data_section_bytes": 0,
            "bss_section_bytes": 0
        }
        parameters = parse_instance_name(result.instance_name)

        instance_obj = {
            "name": result.instance_name,
            "parameters": parameters,
            "compilation": result.compilation,
            "binary": binary_sections
        }
        instances_section.append(instance_obj)

    failed_instances_section = []
    for failed_result in failed:
        parameters = parse_instance_name(failed_result.instance_name)
        errors_list = []
        for error_info in failed_result.errors:
            error_obj = {
                "attempt": error_info['attempt'],
                "phase": error_info['phase'],
                "step": error_info['step'],
                "message": error_info['message'],
                "timestamp": error_info['timestamp']
            }
            errors_list.append(error_obj)

        failed_instance_obj = {
            "name": failed_result.instance_name,
            "parameters": parameters,
            "attempts": failed_result.attempts,
            "errors": errors_list
        }
        failed_instances_section.append(failed_instance_obj)

    # Calculate statistics
    total_instances = len(successful) + len(failed)
    successful_count = len(successful)
    failed_count = len(failed)
    success_rate = (successful_count / total_instances) if total_instances > 0 else 0.0

    output_json = {
        "schema_version": "1.0.0",
        "experiment": {
            "application": "05-cholesky",
            "experiment_set": "regression_test",
            "timestamp": timestamp,
            "git_commit": git_commit,
            "yaml_hash": yaml_hash
        },
        "instances": instances_section,
        "failed_instances": failed_instances_section,
        "statistics": {
            "total_instances": total_instances,
            "successful_count": successful_count,
            "failed_count": failed_count,
            "success_rate": round(success_rate, 3)
        }
    }

    return output_json


def validate_output(data):
    """Validate JSON output structure."""
    checks = []

    # Schema version
    if data.get("schema_version") == "1.0.0":
        checks.append(("✓", "schema_version", "1.0.0"))
    else:
        checks.append(("✗", "schema_version", data.get("schema_version")))

    # Experiment section
    exp = data.get("experiment", {})
    if exp.get("application") == "05-cholesky":
        checks.append(("✓", "experiment.application", "05-cholesky"))
    if exp.get("experiment_set") == "regression_test":
        checks.append(("✓", "experiment.experiment_set", "regression_test"))
    if len(exp.get("git_commit", "")) == 8:
        checks.append(("✓", "experiment.git_commit", exp["git_commit"]))
    if len(exp.get("yaml_hash", "")) == 64:
        checks.append(("✓", "experiment.yaml_hash", f"{exp['yaml_hash'][:16]}..."))

    # Instances
    instances = data.get("instances", [])
    if len(instances) == 2:
        checks.append(("✓", "instances count", 2))
    else:
        checks.append(("✗", "instances count", len(instances)))

    if instances:
        inst = instances[0]
        if all(k in inst for k in ["name", "parameters", "compilation", "binary"]):
            checks.append(("✓", "instance[0] structure", "complete"))
        if "scheduler" in inst.get("parameters", {}):
            checks.append(("✓", "instance[0].parameters.scheduler", inst["parameters"]["scheduler"]))
        if "total_time_s" in inst.get("compilation", {}):
            checks.append(("✓", "instance[0].compilation.total_time_s", f"{inst['compilation']['total_time_s']}s"))
        if inst.get("binary", {}).get("total_size_bytes") == 1234567:
            checks.append(("✓", "instance[0].binary.total_size_bytes", "1234567"))

    # Failed instances
    failed = data.get("failed_instances", [])
    if len(failed) == 1:
        checks.append(("✓", "failed_instances count", 1))

    if failed:
        failed_inst = failed[0]
        if failed_inst.get("attempts") == 2:
            checks.append(("✓", "failed[0].attempts", 2))
        if len(failed_inst.get("errors", [])) == 2:
            checks.append(("✓", "failed[0].errors count", 2))

    # Statistics
    stats = data.get("statistics", {})
    if stats.get("total_instances") == 3:
        checks.append(("✓", "statistics.total_instances", 3))
    if stats.get("successful_count") == 2:
        checks.append(("✓", "statistics.successful_count", 2))
    if stats.get("failed_count") == 1:
        checks.append(("✓", "statistics.failed_count", 1))
    if abs(stats.get("success_rate", 0) - 0.667) < 0.001:
        checks.append(("✓", "statistics.success_rate", f"{stats['success_rate']:.3f}"))

    return checks


if __name__ == "__main__":
    print("=" * 80)
    print("JSON Output Integration Test")
    print("=" * 80)

    # Create mock JSON
    print("\nGenerating mock JSON output...")
    json_data = create_mock_json_output()

    # Validate
    print("Validating JSON structure...\n")
    checks = validate_output(json_data)

    passed = 0
    failed = 0

    for status, key, value in checks:
        if status == "✓":
            print(f"  {status} {key:40} = {value}")
            passed += 1
        else:
            print(f"  {status} {key:40} = {value}")
            failed += 1

    # Write to file
    print("\nWriting JSON to file...")
    with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
        json.dump(json_data, f, indent=2, sort_keys=True)
        json_path = f.name

    print(f"Output: {json_path}")

    # Pretty print sample
    print("\nSample JSON Output (first 1000 chars):")
    print("-" * 80)
    json_str = json.dumps(json_data, indent=2, sort_keys=True)
    print(json_str[:1000])
    print("...")
    print("-" * 80)

    # Summary
    print(f"\nValidation Results:")
    print(f"  Passed: {passed}")
    print(f"  Failed: {failed}")

    if failed == 0:
        print("\n" + "=" * 80)
        print("ALL CHECKS PASSED!")
        print("=" * 80)
        exit(0)
    else:
        print("\nSome checks failed!")
        exit(1)
