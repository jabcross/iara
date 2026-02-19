"""
Unit tests for collector.py - Testing measurement parsing functions.
"""

import unittest
import json
from unittest.mock import patch, MagicMock

from . import collector
from .collector import (
    parse_regex_measurement,
    parse_json_measurement,
    parse_line_measurement,
    parse_measurement,
    MeasurementError,
)


class TestParseRegexMeasurement(unittest.TestCase):
    """Tests for parse_regex_measurement function."""

    def test_parse_regex_measurement_float(self):
        """Test extraction of floating point values."""
        output = "Wall time: 1.234 s"
        pattern = r'Wall time:\s+(\d+\.?\d*)'
        result = parse_regex_measurement(output, pattern, 1, 'float')
        self.assertEqual(result, 1.234)
        self.assertIsInstance(result, float)

    def test_parse_regex_measurement_int(self):
        """Test extraction of integer values."""
        output = "Maximum resident set size (kbytes): 512"
        pattern = r'Maximum resident set size.*:\s+(\d+)'
        result = parse_regex_measurement(output, pattern, 1, 'int')
        self.assertEqual(result, 512)
        self.assertIsInstance(result, int)

    def test_parse_regex_measurement_bool_true(self):
        """Test extraction of boolean values (true)."""
        output = "Verification: PASSED"
        pattern = r'Verification:\s+(PASSED|FAILED)'
        result = parse_regex_measurement(output, pattern, 1, 'bool')
        self.assertIs(result, True)

    def test_parse_regex_measurement_bool_false(self):
        """Test extraction of boolean values (false)."""
        output = "Verification: FAILED"
        pattern = r'Verification:\s+(PASSED|FAILED)'
        result = parse_regex_measurement(output, pattern, 1, 'bool')
        self.assertIs(result, False)

    def test_parse_regex_measurement_string(self):
        """Test extraction of string values."""
        output = "Scheduler: omp-task"
        pattern = r'Scheduler:\s+(\w+-\w+)'
        result = parse_regex_measurement(output, pattern, 1, 'str')
        self.assertEqual(result, "omp-task")
        self.assertIsInstance(result, str)

    def test_parse_regex_measurement_missing(self):
        """Test handling of missing pattern match."""
        output = "Some output without the pattern"
        pattern = r'Wall time:\s+(\d+\.?\d*)'
        result = parse_regex_measurement(output, pattern, 1, 'float')
        self.assertIsNone(result)

    def test_parse_regex_measurement_multiline(self):
        """Test regex matching across multiple lines."""
        output = """
        Some header
        Wall time: 2.567 s
        More output
        """
        pattern = r'Wall time:\s+(\d+\.?\d*)'
        result = parse_regex_measurement(output, pattern, 1, 'float')
        self.assertEqual(result, 2.567)

    def test_parse_regex_measurement_invalid_pattern(self):
        """Test handling of invalid regex pattern."""
        output = "Some output"
        pattern = r'(?P<invalid'  # Invalid regex
        with self.assertRaises(ValueError):
            parse_regex_measurement(output, pattern, 1, 'float')

    def test_parse_regex_measurement_invalid_group(self):
        """Test handling of invalid group number."""
        output = "Wall time: 1.234 s"
        pattern = r'Wall time:\s+(\d+\.?\d*)'
        with self.assertRaises(ValueError):
            parse_regex_measurement(output, pattern, 5, 'float')

    def test_parse_regex_measurement_conversion_error(self):
        """Test handling of conversion errors."""
        output = "Value: not_a_number"
        pattern = r'Value:\s+(\S+)'
        with self.assertRaises(ValueError):
            parse_regex_measurement(output, pattern, 1, 'float')


class TestParseJsonMeasurement(unittest.TestCase):
    """Tests for parse_json_measurement function."""

    def test_parse_json_measurement_flat(self):
        """Test extraction from flat JSON."""
        output = json.dumps({"time": 1.234})
        result = parse_json_measurement(output, ["time"], "float")
        self.assertEqual(result, 1.234)

    def test_parse_json_measurement_nested(self):
        """Test extraction from nested JSON."""
        output = json.dumps({"results": {"timing": {"wall_time": 2.567}}})
        result = parse_json_measurement(output, ["results", "timing", "wall_time"], "float")
        self.assertEqual(result, 2.567)

    def test_parse_json_measurement_int(self):
        """Test extraction of integer from JSON."""
        output = json.dumps({"memory_bytes": 1048576})
        result = parse_json_measurement(output, ["memory_bytes"], "int")
        self.assertEqual(result, 1048576)
        self.assertIsInstance(result, int)

    def test_parse_json_measurement_bool(self):
        """Test extraction of boolean from JSON."""
        output = json.dumps({"passed": True})
        result = parse_json_measurement(output, ["passed"], "bool")
        self.assertIs(result, True)

    def test_parse_json_measurement_string(self):
        """Test extraction of string from JSON."""
        output = json.dumps({"scheduler": "omp-task"})
        result = parse_json_measurement(output, ["scheduler"], "str")
        self.assertEqual(result, "omp-task")

    def test_parse_json_measurement_invalid_json(self):
        """Test handling of invalid JSON."""
        output = "{ invalid json"
        result = parse_json_measurement(output, ["time"], "float")
        self.assertIsNone(result)

    def test_parse_json_measurement_missing_key(self):
        """Test handling of missing key path."""
        output = json.dumps({"time": 1.234})
        result = parse_json_measurement(output, ["nonexistent"], "float")
        self.assertIsNone(result)

    def test_parse_json_measurement_missing_nested_key(self):
        """Test handling of missing key in nested path."""
        output = json.dumps({"results": {"timing": {"wall_time": 2.567}}})
        result = parse_json_measurement(output, ["results", "missing", "wall_time"], "float")
        self.assertIsNone(result)

    def test_parse_json_measurement_conversion_error(self):
        """Test handling of conversion errors."""
        output = json.dumps({"value": "not_a_number"})
        with self.assertRaises(ValueError):
            parse_json_measurement(output, ["value"], "float")

    def test_parse_json_measurement_array_access(self):
        """Test that array access fails gracefully."""
        output = json.dumps({"items": [1, 2, 3]})
        # Trying to access by string key when it's an array should fail
        result = parse_json_measurement(output, ["items", "invalid"], "int")
        self.assertIsNone(result)


class TestParseLineMeasurement(unittest.TestCase):
    """Tests for parse_line_measurement function."""

    def test_parse_line_measurement_float(self):
        """Test extraction of floating point from specific line."""
        output = "Header line\n1.234\nFooter line"
        result = parse_line_measurement(output, 1, 'float')
        self.assertEqual(result, 1.234)

    def test_parse_line_measurement_int(self):
        """Test extraction of integer from specific line."""
        output = "Line 0\n512\nLine 2"
        result = parse_line_measurement(output, 1, 'int')
        self.assertEqual(result, 512)

    def test_parse_line_measurement_string(self):
        """Test extraction of string from specific line."""
        output = "Line 0\nomp-task\nLine 2"
        result = parse_line_measurement(output, 1, 'str')
        self.assertEqual(result, "omp-task")

    def test_parse_line_measurement_bool(self):
        """Test extraction of boolean from specific line."""
        output = "Line 0\nPASSED\nLine 2"
        result = parse_line_measurement(output, 1, 'bool')
        self.assertIs(result, True)

    def test_parse_line_measurement_out_of_bounds_positive(self):
        """Test handling of out of bounds line number (too large)."""
        output = "Line 0\nLine 1"
        result = parse_line_measurement(output, 5, 'float')
        self.assertIsNone(result)

    def test_parse_line_measurement_out_of_bounds_negative(self):
        """Test handling of negative line number."""
        output = "Line 0\nLine 1"
        result = parse_line_measurement(output, -1, 'float')
        self.assertIsNone(result)

    def test_parse_line_measurement_empty_line(self):
        """Test handling of empty line."""
        output = "Line 0\n\nLine 2"
        result = parse_line_measurement(output, 1, 'float')
        self.assertIsNone(result)

    def test_parse_line_measurement_whitespace_line(self):
        """Test handling of line with only whitespace."""
        output = "Line 0\n   \nLine 2"
        result = parse_line_measurement(output, 1, 'float')
        self.assertIsNone(result)

    def test_parse_line_measurement_first_line(self):
        """Test extraction from first line."""
        output = "5.678\nLine 1\nLine 2"
        result = parse_line_measurement(output, 0, 'float')
        self.assertEqual(result, 5.678)

    def test_parse_line_measurement_conversion_error(self):
        """Test handling of conversion errors."""
        output = "Line 0\nnot_a_number\nLine 2"
        with self.assertRaises(ValueError):
            parse_line_measurement(output, 1, 'float')


class TestParseMeasurementDispatcher(unittest.TestCase):
    """Tests for parse_measurement main dispatcher function."""

    def test_parse_measurement_regex_float(self):
        """Test dispatcher with regex parser and float type."""
        output = "Wall time: 1.234 s"
        spec = {
            'name': 'compute_time',
            'type': 'float',
            'parser': {
                'type': 'regex',
                'pattern': r'Wall time:\s+(\d+\.?\d*)',
                'group': 1
            },
            'required': True
        }
        result = parse_measurement(output, spec)
        self.assertEqual(result, 1.234)

    def test_parse_measurement_json_nested(self):
        """Test dispatcher with JSON parser and nested keys."""
        output = json.dumps({"results": {"timing": {"wall_time": 2.567}}})
        spec = {
            'name': 'wall_time',
            'type': 'float',
            'parser': {
                'type': 'json',
                'key_path': ["results", "timing", "wall_time"]
            },
            'required': True
        }
        result = parse_measurement(output, spec)
        self.assertEqual(result, 2.567)

    def test_parse_measurement_line_extraction(self):
        """Test dispatcher with line parser."""
        output = "Header\n3.141\nFooter"
        spec = {
            'name': 'pi_value',
            'type': 'float',
            'parser': {
                'type': 'line',
                'line_number': 1
            },
            'required': True
        }
        result = parse_measurement(output, spec)
        self.assertEqual(result, 3.141)

    def test_parse_measurement_required_missing(self):
        """Test error when required measurement not found."""
        output = "Some output without pattern"
        spec = {
            'name': 'compute_time',
            'type': 'float',
            'parser': {
                'type': 'regex',
                'pattern': r'Wall time:\s+(\d+\.?\d*)',
                'group': 1
            },
            'required': True
        }
        with self.assertRaises(MeasurementError):
            parse_measurement(output, spec)

    def test_parse_measurement_optional_missing(self):
        """Test that optional missing measurement returns None."""
        output = "Some output without pattern"
        spec = {
            'name': 'optional_time',
            'type': 'float',
            'parser': {
                'type': 'regex',
                'pattern': r'Optional time:\s+(\d+\.?\d*)',
                'group': 1
            },
            'required': False
        }
        result = parse_measurement(output, spec)
        self.assertIsNone(result)

    def test_parse_measurement_unit_conversion_ms_to_s(self):
        """Test unit conversion from milliseconds to seconds."""
        output = "Computation time: 1500"
        spec = {
            'name': 'compute_time',
            'type': 'float',
            'parser': {
                'type': 'regex',
                'pattern': r'Computation time:\s+(\d+\.?\d*)',
                'group': 1
            },
            'unit': 'ms',
            'required': True
        }
        result = parse_measurement(output, spec)
        self.assertEqual(result, 1.5)  # 1500 ms = 1.5 s

    def test_parse_measurement_unit_conversion_kb_to_bytes(self):
        """Test unit conversion from kilobytes to bytes."""
        output = "Memory used: 1024"
        spec = {
            'name': 'memory',
            'type': 'float',
            'parser': {
                'type': 'regex',
                'pattern': r'Memory used:\s+(\d+)',
                'group': 1
            },
            'unit': 'KB',
            'required': True
        }
        result = parse_measurement(output, spec)
        self.assertEqual(result, 1024 * 1024)  # 1024 KB = 1 MB in bytes

    def test_parse_measurement_unit_conversion_mb_to_bytes(self):
        """Test unit conversion from megabytes to bytes."""
        output = "Memory: 512"
        spec = {
            'name': 'memory',
            'type': 'float',
            'parser': {
                'type': 'regex',
                'pattern': r'Memory:\s+(\d+)',
                'group': 1
            },
            'unit': 'MB',
            'required': True
        }
        result = parse_measurement(output, spec)
        self.assertEqual(result, 512 * 1024 * 1024)

    def test_parse_measurement_unit_conversion_gb_to_bytes(self):
        """Test unit conversion from gigabytes to bytes."""
        output = "Disk: 2"
        spec = {
            'name': 'disk_size',
            'type': 'float',
            'parser': {
                'type': 'regex',
                'pattern': r'Disk:\s+(\d+)',
                'group': 1
            },
            'unit': 'GB',
            'required': True
        }
        result = parse_measurement(output, spec)
        self.assertEqual(result, 2 * (1024 ** 3))

    def test_parse_measurement_unit_conversion_us_to_s(self):
        """Test unit conversion from microseconds to seconds."""
        output = "Time: 1000"
        spec = {
            'name': 'time_us',
            'type': 'float',
            'parser': {
                'type': 'regex',
                'pattern': r'Time:\s+(\d+)',
                'group': 1
            },
            'unit': 'us',
            'required': True
        }
        result = parse_measurement(output, spec)
        self.assertAlmostEqual(result, 1000 * 1e-6, places=9)  # 1000 us = 1 ms

    def test_parse_measurement_unit_conversion_min_to_s(self):
        """Test unit conversion from minutes to seconds."""
        output = "Duration: 2"
        spec = {
            'name': 'duration',
            'type': 'float',
            'parser': {
                'type': 'regex',
                'pattern': r'Duration:\s+(\d+)',
                'group': 1
            },
            'unit': 'min',
            'required': True
        }
        result = parse_measurement(output, spec)
        self.assertEqual(result, 2 * 60)  # 2 min = 120 s

    def test_parse_measurement_unknown_parser_type(self):
        """Test error for unknown parser type."""
        output = "Some output"
        spec = {
            'name': 'measurement',
            'type': 'float',
            'parser': {
                'type': 'unknown_parser',
            },
            'required': True
        }
        with self.assertRaises(MeasurementError):
            parse_measurement(output, spec)

    def test_parse_measurement_default_parser_type(self):
        """Test that default parser type is regex."""
        output = "Wall time: 1.5 s"
        spec = {
            'name': 'compute_time',
            'type': 'float',
            'parser': {
                'pattern': r'Wall time:\s+(\d+\.?\d*)',
                'group': 1
                # 'type' is missing, should default to 'regex'
            },
            'required': True
        }
        result = parse_measurement(output, spec)
        self.assertEqual(result, 1.5)

    def test_parse_measurement_unit_without_conversion(self):
        """Test that unknown unit is handled gracefully."""
        output = "Value: 100"
        spec = {
            'name': 'measurement',
            'type': 'float',
            'parser': {
                'type': 'regex',
                'pattern': r'Value:\s+(\d+)',
                'group': 1
            },
            'unit': 'unknown_unit',
            'required': True
        }
        # Should return value as-is without conversion
        result = parse_measurement(output, spec)
        self.assertEqual(result, 100.0)

    def test_parse_measurement_without_unit(self):
        """Test measurement without unit specification."""
        output = "Value: 42"
        spec = {
            'name': 'count',
            'type': 'int',
            'parser': {
                'type': 'regex',
                'pattern': r'Value:\s+(\d+)',
                'group': 1
            },
            'required': True
        }
        result = parse_measurement(output, spec)
        self.assertEqual(result, 42)

    def test_parse_measurement_bool_true_variants(self):
        """Test various representations of boolean true."""
        output_variants = [
            ("Status: PASSED", "Status:\\s+(\\w+)"),
            ("Status: true", "Status:\\s+(\\w+)"),
            ("Status: 1", "Status:\\s+(\\d+)"),
            ("Status: yes", "Status:\\s+(\\w+)"),
        ]

        for output, pattern in output_variants:
            spec = {
                'name': 'status',
                'type': 'bool',
                'parser': {
                    'type': 'regex',
                    'pattern': pattern,
                    'group': 1
                },
                'required': True
            }
            result = parse_measurement(output, spec)
            self.assertIs(result, True, f"Expected True for output: {output}")

    def test_parse_measurement_bool_false_variants(self):
        """Test various representations of boolean false."""
        output_variants = [
            ("Status: FAILED", "Status:\\s+(\\w+)"),
            ("Status: false", "Status:\\s+(\\w+)"),
            ("Status: 0", "Status:\\s+(\\d+)"),
            ("Status: no", "Status:\\s+(\\w+)"),
        ]

        for output, pattern in output_variants:
            spec = {
                'name': 'status',
                'type': 'bool',
                'parser': {
                    'type': 'regex',
                    'pattern': pattern,
                    'group': 1
                },
                'required': True
            }
            result = parse_measurement(output, spec)
            self.assertIs(result, False, f"Expected False for output: {output}")

    def test_parse_measurement_regex_with_scientific_notation(self):
        """Test regex extraction with scientific notation."""
        output = "Scientific value: 1.23e-4"
        spec = {
            'name': 'sci_value',
            'type': 'float',
            'parser': {
                'type': 'regex',
                'pattern': r'Scientific value:\s+([\d.e-]+)',
                'group': 1
            },
            'required': True
        }
        result = parse_measurement(output, spec)
        self.assertEqual(result, 1.23e-4)

    def test_parse_measurement_json_string_bool_conversion(self):
        """Test JSON parser converting string 'true' to boolean."""
        output = json.dumps({"passed": "true"})
        spec = {
            'name': 'passed',
            'type': 'bool',
            'parser': {
                'type': 'json',
                'key_path': ["passed"]
            },
            'required': True
        }
        result = parse_measurement(output, spec)
        self.assertIs(result, True)

    def test_parse_measurement_regex_multiple_occurrences(self):
        """Test that regex extracts first match."""
        output = "Time: 1.0\nAnother time: 2.0"
        spec = {
            'name': 'first_time',
            'type': 'float',
            'parser': {
                'type': 'regex',
                'pattern': r'Time:\s+(\d+\.?\d*)',
                'group': 1
            },
            'required': True
        }
        result = parse_measurement(output, spec)
        # Should extract the first match
        self.assertEqual(result, 1.0)


class TestMeasurementIntegration(unittest.TestCase):
    """Integration tests for measurement parsing with realistic scenarios."""

    def test_cholesky_example_compute_time(self):
        """Test with realistic Cholesky benchmark output."""
        output = """
        Matrix size: 4096
        Number of blocks: 4
        Initialization time: 0.234 s
        Block conversion time: 0.123 s
        Wall time: 5.678 s
        Maximum resident set size (kbytes): 262144
        Verification: PASSED
        """

        # Test compute_time extraction
        spec = {
            'name': 'compute_time',
            'type': 'float',
            'parser': {
                'type': 'regex',
                'pattern': r'Wall time:\s+(\d+\.?\d*)',
                'group': 1
            },
            'unit': 's',
            'required': True
        }
        result = parse_measurement(output, spec)
        self.assertEqual(result, 5.678)

    def test_cholesky_example_memory(self):
        """Test memory extraction from Cholesky output."""
        output = """
        Maximum resident set size (kbytes): 262144
        """

        spec = {
            'name': 'max_rss_mb',
            'type': 'float',
            'parser': {
                'type': 'regex',
                'pattern': r'Maximum resident set size.*:\s+(\d+)',
                'group': 1
            },
            'unit': 'KB',
            'required': True
        }
        result = parse_measurement(output, spec)
        self.assertEqual(result, 262144 * 1024)  # Convert KB to bytes

    def test_json_benchmark_results(self):
        """Test JSON extraction from structured benchmark output."""
        output = json.dumps({
            "benchmark": {
                "name": "matrix_multiply",
                "timing": {
                    "setup_ms": 234,
                    "compute_ms": 5678,
                    "total_ms": 5912
                },
                "verification": {
                    "passed": True,
                    "checksum": "abc123def456"
                }
            }
        })

        # Test compute time extraction
        spec = {
            'name': 'compute_time',
            'type': 'float',
            'parser': {
                'type': 'json',
                'key_path': ["benchmark", "timing", "compute_ms"]
            },
            'unit': 'ms',
            'required': True
        }
        result = parse_measurement(output, spec)
        self.assertEqual(result, 5.678)  # 5678 ms = 5.678 s

    def test_multiple_measurements_same_output(self):
        """Test extracting multiple measurements from same output."""
        output = """
        Init time: 234 ms
        Compute time: 5678 ms
        Total time: 5912 ms
        Verification: PASSED
        """

        measurements = [
            {
                'name': 'init_time',
                'type': 'float',
                'parser': {
                    'type': 'regex',
                    'pattern': r'Init time:\s+(\d+)',
                    'group': 1
                },
                'unit': 'ms',
                'required': False
            },
            {
                'name': 'compute_time',
                'type': 'float',
                'parser': {
                    'type': 'regex',
                    'pattern': r'Compute time:\s+(\d+)',
                    'group': 1
                },
                'unit': 'ms',
                'required': True
            },
            {
                'name': 'verified',
                'type': 'bool',
                'parser': {
                    'type': 'regex',
                    'pattern': r'Verification:\s+(\w+)',
                    'group': 1
                },
                'required': False
            }
        ]

        results = {}
        for spec in measurements:
            results[spec['name']] = parse_measurement(output, spec)

        self.assertEqual(results['init_time'], 0.234)
        self.assertEqual(results['compute_time'], 5.678)
        self.assertIs(results['verified'], True)


if __name__ == '__main__':
    unittest.main()
