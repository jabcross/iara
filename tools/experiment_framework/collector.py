"""
Measurement collection module for IaRa Unified Experiment Framework.

This module handles execution of instances and collection of runtime measurements.
"""

import json
import logging
import math
import os
import re
import subprocess
import tempfile
from datetime import datetime, timezone
from pathlib import Path
from typing import Dict, List, Any, Optional, Union

# Support both relative and absolute imports
try:
    from .builder import parse_time_output, convert_time_to_seconds, convert_memory_to_bytes
    from .progress import ProgressBar
except ImportError:
    from builder import parse_time_output, convert_time_to_seconds, convert_memory_to_bytes
    from progress import ProgressBar


# Configure logging
logger = logging.getLogger(__name__)


# Standard measurements always collected for every experiment.
# These are injected into stdout by execute_single_run() from GNU time output,
# so no per-app yaml configuration is required.
STANDARD_MEASUREMENTS = [
    {
        "name": "wall_time",
        "type": "float",
        "parser": {
            "type": "regex",
            "pattern": r"GNU Wall time:\s+(\d+\.?\d*)",
            "group": 1
        },
        "unit": "s",
        "required": True,
        "description": "Total wall clock execution time (from GNU time)"
    },
    # NOTE: no "unit" key — converter already yields MB;
    # adding "unit": "MB" would trigger a second bytes conversion on top.
    {
        "name": "max_rss_mb",
        "type": "float",
        "parser": {
            "type": "regex",
            "pattern": r"Maximum resident set size.*:\s+(\d+)",
            "group": 1,
            "converter": "lambda x: float(x) / 1024"
        },
        "required": False,
        "description": "Peak resident set size in MB"
    }
]


def log_subprocess_call(
    cmd: List[str],
    cwd: Optional[Path] = None,
    env: Optional[Dict[str, str]] = None,
) -> None:
    """
    Log subprocess call details for debugging.

    Args:
        cmd: Command and arguments as list
        cwd: Working directory for the subprocess
        env: Environment variables (custom vars only, logs if non-empty)
    """
    logger.info(f"Executing subprocess: {' '.join(cmd)}")
    if cwd:
        logger.debug(f"  Working directory: {cwd}")
    if env:
        logger.debug(f"  Environment variables: {json.dumps(env, indent=2)}")


class MeasurementError(Exception):
    """Raised when required measurement cannot be parsed."""
    pass


def execute_single_run(
    executable: Path,
    run_number: int,
    timeout: int,
    env_vars: Dict[str, str]
) -> Dict[str, Any]:
    """
    Execute binary once with GNU time wrapper and capture all output.

    Process:
        1. Construct GNU time command: /usr/bin/time -v <executable>
        2. Create temporary file for GNU time output (stderr)
        3. Merge environment: copy os.environ and update with env_vars
        4. Run subprocess with timeout
        5. Parse GNU time output using parse_time_output()
        6. Return structured result

    Args:
        executable: Path to the compiled executable
        run_number: Run number (1-indexed) for tracking
        timeout: Timeout in seconds
        env_vars: Environment variables to set for execution

    Returns:
        Dict with keys:
            - run_number: int
            - stdout: str (program output)
            - stderr: str (time output)
            - returncode: int
            - gnu_time: Dict[str, Any] (parsed time data)
            - success: bool (returncode == 0 and no timeout)
            - error: Optional[str] (error message if failed)

    Error Handling:
        - subprocess.TimeoutExpired: success=False, error="Timeout after {timeout}s"
        - FileNotFoundError: success=False, error="Executable not found"
        - Other exceptions: success=False with error message
    """
    if not executable.exists():
        return {
            "run_number": run_number,
            "stdout": "",
            "stderr": "",
            "returncode": -1,
            "gnu_time": {},
            "success": False,
            "error": f"Executable not found: {executable}"
        }

    # Merge environment variables
    env_dict = os.environ.copy()
    env_dict.update(env_vars)

    try:
        # Create temporary file for GNU time output
        with tempfile.NamedTemporaryFile(mode='w+', suffix='.txt', delete=False) as time_file:
            time_file_path = Path(time_file.name)

        try:
            # Construct command: /usr/bin/time -v -o <file> <executable>
            # Using -o writes GNU time output to a separate file, keeping it out of
            # the program's stderr. This mirrors the old run_instance.sh approach.
            cmd = ['/usr/bin/time', '-v', '-o', str(time_file_path), str(executable)]

            logger.info(f"Executing run {run_number}/{timeout}s timeout")
            log_subprocess_call(cmd, cwd=Path.cwd(), env=env_vars if env_vars else None)

            # Run subprocess
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=timeout,
                env=env_dict,
                cwd=executable.parent
            )

            # stdout/stderr are now clean program output; GNU time goes to time_file_path
            stdout = result.stdout
            stderr = result.stderr

            # Parse GNU time output from the -o file
            gnu_time = parse_time_output(time_file_path)

            logger.info(f"Run {run_number} completed with return code {result.returncode}")
            logger.debug(f"Stdout length: {len(stdout)} bytes")
            logger.debug(f"GNU time data: {json.dumps(gnu_time, indent=2)}")

            # Inject structured metrics into stdout so regex parsers can find them.
            # GNU time output goes to a separate -o file, not combined_output.
            if 'wall_time_s' in gnu_time:
                stdout = f"GNU Wall time: {gnu_time['wall_time_s']:.6f}\n" + stdout
            if 'max_rss_bytes' in gnu_time:
                kbytes = gnu_time['max_rss_bytes'] // 1024
                stdout = f"Maximum resident set size (kbytes): {kbytes}\n" + stdout

            return {
                "run_number": run_number,
                "stdout": stdout,
                "stderr": stderr,
                "returncode": result.returncode,
                "gnu_time": gnu_time,
                "success": result.returncode == 0,
                "error": None
            }

        finally:
            # Clean up temporary file
            if time_file_path.exists():
                time_file_path.unlink()

    except subprocess.TimeoutExpired:
        error_msg = f"Timeout after {timeout}s"
        logger.error(f"Run {run_number}: {error_msg}")
        logger.error(f"Command: {' '.join(cmd)}")
        logger.error(f"Working directory: {Path.cwd()}")
        return {
            "run_number": run_number,
            "stdout": "",
            "stderr": "",
            "returncode": -1,
            "gnu_time": {},
            "success": False,
            "error": error_msg
        }

    except FileNotFoundError:
        error_msg = f"Executable not found: {executable}"
        logger.error(f"Run {run_number}: {error_msg}")
        return {
            "run_number": run_number,
            "stdout": "",
            "stderr": "",
            "returncode": -1,
            "gnu_time": {},
            "success": False,
            "error": error_msg
        }

    except Exception as e:
        error_msg = f"Execution error: {str(e)}"
        logger.error(f"Run {run_number}: {error_msg}", exc_info=True)
        logger.error(f"Executable: {executable}")
        logger.error(f"Timeout: {timeout}s")
        return {
            "run_number": run_number,
            "stdout": "",
            "stderr": "",
            "returncode": -1,
            "gnu_time": {},
            "success": False,
            "error": error_msg
        }


def execute_instance(
    executable: Path,
    repetitions: int,
    timeout: int,
    env_vars: Dict[str, str],
    measurements: List[Dict[str, Any]],
    instance_name: str = None
) -> Dict[str, Any]:
    """
    Execute instance N times and collect all measurements.

    Process:
        1. Validate executable exists and is executable
        2. Initialize: runs = [], failures = []
        3. For each repetition i from 1 to N:
           - Call execute_single_run(executable, i, timeout, env_vars)
           - If success: append run data with raw stdout
           - If failure: append to failures list
        4. Return ExecutionResult with runs and failures

    Args:
        executable: Path to the compiled executable
        repetitions: Number of times to execute
        timeout: Timeout in seconds
        env_vars: Environment variables to set
        measurements: List of measurement specifications from YAML
                      (reserved for future Task 3.2 measurement parsing)
        instance_name: Optional instance name (defaults to executable stem if not provided)

    Returns:
        ExecutionResult dict with:
            - instance_name: str
            - runs: List[Dict] with run_number, stdout, measurements
            - failures: List[Dict] with run_number, error
            - total_runs: int
            - successful_runs: int
            - failed_runs: int

    Note:
        Measurement parsing deferred to Task 3.2. Currently stores raw stdout
        in runs for later processing.
    """
    # Use provided instance_name or derive from executable
    if instance_name is None:
        instance_name = executable.stem

    # Validate executable
    if not executable.exists():
        logger.error(f"Executable not found: {executable}")
        return {
            "instance_name": instance_name,
            "runs": [],
            "failures": [{
                "run_number": 1,
                "error": f"Executable not found: {executable}"
            }],
            "total_runs": repetitions,
            "successful_runs": 0,
            "failed_runs": repetitions
        }

    if not os.access(executable, os.X_OK):
        logger.warning(f"Executable not executable: {executable}")

    runs = []
    failures = []

    logger.info(f"Executing {executable.stem} with {repetitions} repetitions (timeout={timeout}s)")

    # Execute each repetition
    for run_number in range(1, repetitions + 1):
        logger.debug(f"Running repetition {run_number}/{repetitions}")

        result = execute_single_run(
            executable=executable,
            run_number=run_number,
            timeout=timeout,
            env_vars=env_vars
        )

        if result["success"]:
            # Parse measurements from stdout and stderr
            combined_output = result["stdout"] + "\n" + result["stderr"]
            parsed_measurements = {}

            for measurement_spec in measurements:
                measurement_name = measurement_spec.get('name', 'unknown')
                try:
                    value = parse_measurement(combined_output, measurement_spec)
                    if value is not None:
                        parsed_measurements[measurement_name] = value
                except Exception as e:
                    is_required = measurement_spec.get('required', True)
                    if is_required:
                        logger.warning(f"Failed to parse required measurement '{measurement_name}': {e}")
                    else:
                        logger.debug(f"Failed to parse optional measurement '{measurement_name}': {e}")

            # Successful run - store with parsed measurements
            run_data = {
                "run_number": run_number,
                "returncode": result["returncode"],
                "gnu_time": result["gnu_time"],
                "measurements": parsed_measurements
            }
            runs.append(run_data)
            logger.debug(f"Run {run_number} succeeded with {len(parsed_measurements)} measurements")
        else:
            # Failed run - track error
            failure_data = {
                "run_number": run_number,
                "error": result["error"]
            }
            failures.append(failure_data)
            logger.warning(f"Run {run_number} failed: {result['error']}")

    successful_runs = len(runs)
    failed_runs = len(failures)

    logger.info(
        f"Execution complete for {instance_name}: "
        f"{successful_runs} successful, {failed_runs} failed out of {repetitions} total"
    )

    # Compute statistics for all measurements
    execution_result = {
        "instance_name": instance_name,
        "runs": runs,
        "failures": failures,
        "total_runs": repetitions,
        "successful_runs": successful_runs,
        "failed_runs": failed_runs
    }

    statistics = compute_all_statistics(execution_result)
    execution_result["statistics"] = statistics

    return execution_result


def parse_regex_measurement(
    output: str,
    pattern: str,
    group: int,
    measurement_type: str
) -> Optional[Union[float, int, str]]:
    """
    Extract measurement using regex pattern.

    Algorithm:
        1. Compile and search: match = re.search(pattern, output, re.MULTILINE)
        2. If no match: return None
        3. Extract group: value_str = match.group(group)
        4. Convert based on measurement_type:
           - 'float': float(value_str)
           - 'int': int(value_str)
           - 'bool': value_str.lower() in ('true', '1', 'yes', 'passed')
           - 'str': Return as-is
        5. Return converted value

    Args:
        output: Program output string
        pattern: Regex pattern to match
        group: Group number to extract (1-indexed)
        measurement_type: Type to convert value to ('float', 'int', 'bool', 'str')

    Returns:
        Converted value or None if no match found

    Raises:
        ValueError: If regex compilation fails or conversion fails
    """
    try:
        match = re.search(pattern, output, re.MULTILINE)
    except re.error as e:
        raise ValueError(f"Invalid regex pattern '{pattern}': {e}")

    if not match:
        return None

    try:
        value_str = match.group(group)
    except IndexError as e:
        raise ValueError(f"Regex group {group} not found in pattern '{pattern}': {e}")

    # Convert to appropriate type
    try:
        if measurement_type == 'float':
            return float(value_str)
        elif measurement_type == 'int':
            return int(value_str)
        elif measurement_type == 'bool':
            return value_str.lower() in ('true', '1', 'yes', 'passed')
        elif measurement_type == 'str':
            return value_str
        else:
            # Default to string if unknown type
            return value_str
    except (ValueError, TypeError) as e:
        raise ValueError(
            f"Failed to convert '{value_str}' to {measurement_type}: {e}"
        )


def parse_json_measurement(
    output: str,
    key_path: List[str],
    measurement_type: str
) -> Optional[Union[float, int, str]]:
    """
    Extract measurement from JSON output using key path.

    Algorithm:
        1. Parse JSON: data = json.loads(output)
        2. Navigate path: for key in key_path: data = data[key]
        3. Convert to type (same logic as regex)
        4. Return value

    Args:
        output: Program output string (should be valid JSON)
        key_path: List of keys to navigate nested JSON structure
        measurement_type: Type to convert value to ('float', 'int', 'bool', 'str')

    Returns:
        Converted value or None if JSON invalid or key not found

    Raises:
        ValueError: If type conversion fails
    """
    try:
        data = json.loads(output)
    except json.JSONDecodeError as e:
        logger.warning(f"Failed to parse JSON output: {e}")
        return None

    # Navigate the key path
    try:
        for key in key_path:
            data = data[key]
    except (KeyError, TypeError) as e:
        logger.warning(f"Key path {key_path} not found in JSON: {e}")
        return None

    # Convert to appropriate type
    try:
        if measurement_type == 'float':
            return float(data)
        elif measurement_type == 'int':
            return int(data)
        elif measurement_type == 'bool':
            if isinstance(data, bool):
                return data
            if isinstance(data, str):
                return data.lower() in ('true', '1', 'yes', 'passed')
            return bool(data)
        elif measurement_type == 'str':
            return str(data)
        else:
            # Default to string if unknown type
            return str(data)
    except (ValueError, TypeError) as e:
        raise ValueError(
            f"Failed to convert JSON value {data} to {measurement_type}: {e}"
        )


def parse_line_measurement(
    output: str,
    line_number: int,
    measurement_type: str
) -> Optional[Union[float, int, str]]:
    """
    Extract measurement from specific line number.

    Algorithm:
        1. Split output: lines = output.split('\\n')
        2. Check bounds: if line_number >= len(lines) or line_number < 0: return None
        3. Get and strip: line = lines[line_number].strip()
        4. If empty: return None
        5. Convert to type
        6. Return value

    Args:
        output: Program output string
        line_number: Line number (0-indexed)
        measurement_type: Type to convert value to ('float', 'int', 'bool', 'str')

    Returns:
        Converted value or None if out of bounds or empty line

    Raises:
        ValueError: If type conversion fails
    """
    lines = output.split('\n')

    # Check bounds
    if line_number < 0 or line_number >= len(lines):
        return None

    # Get and strip the line
    line = lines[line_number].strip()

    # If empty, return None
    if not line:
        return None

    # Convert to appropriate type
    try:
        if measurement_type == 'float':
            return float(line)
        elif measurement_type == 'int':
            return int(line)
        elif measurement_type == 'bool':
            return line.lower() in ('true', '1', 'yes', 'passed')
        elif measurement_type == 'str':
            return line
        else:
            # Default to string if unknown type
            return line
    except (ValueError, TypeError) as e:
        raise ValueError(
            f"Failed to convert line '{line}' to {measurement_type}: {e}"
        )


def parse_measurement(
    output: str,
    spec: Dict[str, Any]
) -> Optional[Union[float, int, str]]:
    """
    Extract single measurement using parser from YAML.

    Parser Types:
        - regex: Apply pattern, extract group, convert to type
        - json: Parse JSON, extract key path
        - line: Extract specific line number

    Unit Conversion:
        Apply conversion from spec unit to base SI:
        - ms → s (/ 1000.0)
        - KB → bytes (* 1024)
        - MB → bytes (* 1024 * 1024)
        - GB → bytes (* 1024 * 1024 * 1024)

    Args:
        output: Program output string
        spec: Measurement specification from YAML with keys:
            - name: Measurement name
            - type: Value type ('float', 'int', 'bool', 'str')
            - parser: Dict with type, pattern/key_path/line_number
            - unit: Optional unit string for conversion
            - required: Whether measurement is required (default True)

    Returns:
        Converted value or None if not found/required=false

    Raises:
        MeasurementError: If required measurement missing
        ValueError: If parser type unknown or conversion fails
    """
    parser = spec.get('parser', {})
    measurement_name = spec.get('name', 'unknown')
    spec_type = spec.get('type', 'str')
    is_required = spec.get('required', True)

    # Check if this is a composite measurement (computed from other measurements)
    # Composite measurements should not be parsed from output
    if 'composite' in spec:
        # Composite measurements can't be parsed from output, only computed
        # Return None so they can be skipped at this stage
        return None

    parser_type = parser.get('type', 'regex')
    # Call appropriate parser
    value = None

    try:
        if parser_type == 'regex':
            pattern = parser.get('pattern', '')
            group = parser.get('group', 1)
            value = parse_regex_measurement(output, pattern, group, spec_type)

        elif parser_type == 'json':
            key_path = parser.get('key_path', [])
            value = parse_json_measurement(output, key_path, spec_type)

        elif parser_type == 'line':
            line_number = parser.get('line_number', 0)
            value = parse_line_measurement(output, line_number, spec_type)

        else:
            raise ValueError(f"Unknown parser type: {parser_type}")

    except ValueError as e:
        raise MeasurementError(f"Error parsing measurement '{measurement_name}': {e}")

    # Check if required measurement is missing
    if value is None:
        if is_required:
            raise MeasurementError(
                f"Required measurement '{measurement_name}' not found in output"
            )
        else:
            # Optional measurement, return None
            return None

    # Apply custom converter function if specified in parser
    if 'converter' in parser and value is not None:
        converter_str = parser['converter']
        try:
            # Evaluate lambda expression for conversion
            converter_func = eval(converter_str)
            value = converter_func(value)
            logger.debug(
                f"Applied converter to {measurement_name}: {converter_str}"
            )
        except Exception as e:
            logger.warning(
                f"Failed to apply converter for measurement '{measurement_name}': {e}"
            )
            # Continue without conversion if it fails

    # Apply unit conversion if specified
    if 'unit' in spec and value is not None:
        unit = spec['unit']

        try:
            # Check if it's a time unit
            if unit in ['us', 'μs', 'ms', 's', 'sec', 'min', 'h', 'hour']:
                # Convert time to seconds
                converted_value = convert_time_to_seconds(float(value), unit)
                logger.debug(
                    f"Converted {measurement_name} from {unit} to s: "
                    f"{value} -> {converted_value}"
                )
                return converted_value

            # Check if it's a memory unit
            elif unit in ['B', 'byte', 'KB', 'KiB', 'MB', 'MiB', 'GB', 'GiB', 'TB', 'TiB']:
                # Convert memory to bytes
                converted_value = convert_memory_to_bytes(float(value), unit)
                logger.debug(
                    f"Converted {measurement_name} from {unit} to bytes: "
                    f"{value} -> {converted_value}"
                )
                return converted_value

            else:
                # Unknown unit, return as-is
                logger.warning(f"Unknown unit '{unit}' for measurement '{measurement_name}'")
                return value

        except (ValueError, TypeError) as e:
            raise MeasurementError(
                f"Failed to convert unit for measurement '{measurement_name}': {e}"
            )

    return value


def compute_measurement_statistics(
    runs: List[Dict[str, Any]],
    measurement_name: str
) -> Optional[Dict[str, Union[float, int]]]:
    """
    Compute statistics for a single measurement across all runs.

    Calculates mean, standard deviation, min, max, and count for a single
    measurement by extracting values from all successful runs.

    Algorithm:
        1. Extract values for this measurement from all runs
        2. If no values available: return None
        3. Compute statistics:
           - mean: arithmetic mean of values
           - std: population standard deviation (sqrt of variance)
           - min: minimum value
           - max: maximum value
           - count: number of values

    Args:
        runs: List of run dictionaries from ExecutionResult
              Each run should have 'measurements' dict with measurement values
        measurement_name: Name of the measurement to compute statistics for

    Returns:
        Dictionary with statistics if values found:
            {
                "mean": float,
                "std": float,
                "min": float or int,
                "max": float or int,
                "count": int
            }
        Returns None if measurement not found in any run or if runs list is empty

    Raises:
        ValueError: If measurement values are non-numeric or invalid types

    Examples:
        >>> runs = [
        ...     {'measurements': {'compute_time_s': 1.23}},
        ...     {'measurements': {'compute_time_s': 1.25}},
        ...     {'measurements': {'compute_time_s': 1.24}}
        ... ]
        >>> stats = compute_measurement_statistics(runs, 'compute_time_s')
        >>> stats['count']
        3
        >>> abs(stats['mean'] - 1.24) < 0.01
        True

        >>> runs = [{'measurements': {'mem_mb': 512}}]
        >>> stats = compute_measurement_statistics(runs, 'mem_mb')
        >>> stats['std']
        0.0
    """
    # Extract values for this measurement
    values = []
    for run in runs:
        if 'measurements' in run and measurement_name in run['measurements']:
            value = run['measurements'][measurement_name]
            values.append(value)

    # If no values found, return None
    if not values:
        logger.debug(f"No values found for measurement '{measurement_name}'")
        return None

    # Validate all values are numeric
    try:
        # Convert all values to float for consistent computation
        float_values = []
        for value in values:
            if isinstance(value, (int, float)):
                float_values.append(float(value))
            else:
                raise ValueError(
                    f"Non-numeric value for measurement '{measurement_name}': {value} (type: {type(value).__name__})"
                )
    except (ValueError, TypeError) as e:
        raise ValueError(str(e))

    # Compute statistics
    n = len(float_values)
    mean_val = sum(float_values) / n

    # Population standard deviation
    variance = sum((x - mean_val) ** 2 for x in float_values) / n
    std_val = math.sqrt(variance)

    min_val = min(values)  # Keep original type
    max_val = max(values)  # Keep original type

    logger.debug(
        f"Computed statistics for '{measurement_name}': "
        f"mean={mean_val:.6f}, std={std_val:.6f}, min={min_val}, max={max_val}, count={n}"
    )

    return {
        "mean": mean_val,
        "std": std_val,
        "min": min_val,
        "max": max_val,
        "count": n
    }


def compute_all_statistics(
    execution_result: Dict[str, Any]
) -> Dict[str, Dict[str, Union[float, int]]]:
    """
    Compute statistics for all measurements in an execution result.

    Iterates through all runs and identifies all unique measurement names,
    then computes statistics for each measurement across all runs.

    Algorithm:
        1. Extract runs from execution_result
        2. If no runs: return empty dict
        3. Collect all unique measurement names from all runs
        4. For each measurement name:
           - Call compute_measurement_statistics()
           - Add to results if not None
        5. Return complete statistics dictionary

    Args:
        execution_result: ExecutionResult dict with structure:
            {
                "instance_name": str,
                "runs": List[Dict with 'measurements' dict],
                "failures": List[Dict],
                "total_runs": int,
                "successful_runs": int,
                "failed_runs": int
            }

    Returns:
        Dictionary mapping measurement names to their statistics:
            {
                "measurement1": {
                    "mean": float,
                    "std": float,
                    "min": float or int,
                    "max": float or int,
                    "count": int
                },
                "measurement2": {...},
                ...
            }
        Returns empty dict if no runs or no measurements found

    Raises:
        ValueError: If any measurement has invalid values

    Examples:
        >>> result = {
        ...     "instance_name": "test",
        ...     "runs": [
        ...         {
        ...             "run_number": 1,
        ...             "measurements": {
        ...                 "compute_time_s": 1.23,
        ...                 "memory_mb": 512
        ...             }
        ...         },
        ...         {
        ...             "run_number": 2,
        ...             "measurements": {
        ...                 "compute_time_s": 1.25,
        ...                 "memory_mb": 520
        ...             }
        ...         }
        ...     ],
        ...     "failures": []
        ... }
        >>> stats = compute_all_statistics(result)
        >>> sorted(stats.keys())
        ['compute_time_s', 'memory_mb']
        >>> stats['memory_mb']['count']
        2
    """
    # Get runs from execution result
    runs = execution_result.get('runs', [])

    # If no runs, return empty dict
    if not runs:
        logger.debug("No runs found in execution result")
        return {}

    # Collect all unique measurement names from all runs
    measurement_names = set()
    for run in runs:
        if 'measurements' in run:
            measurement_names.update(run['measurements'].keys())

    # If no measurements found, return empty dict
    if not measurement_names:
        logger.debug("No measurements found in any runs")
        return {}

    # Compute statistics for each measurement
    statistics = {}
    for measurement_name in sorted(measurement_names):  # Sort for consistent output
        try:
            stats = compute_measurement_statistics(runs, measurement_name)
            if stats is not None:
                statistics[measurement_name] = stats
        except ValueError as e:
            logger.warning(f"Failed to compute statistics for '{measurement_name}': {e}")
            raise

    logger.info(
        f"Computed statistics for {len(statistics)} measurements from {len(runs)} runs"
    )

    return statistics


def collect_all_measurements(
    build_results: Dict[str, Any],
    config: Dict[str, Any],
    repetitions: Optional[int] = None,
    timeout: Optional[int] = None
) -> Dict[str, Any]:
    """
    Execute all successful instances and collect measurements.

    Process:
        For each successful instance:
            1. Get executable path from build result
            2. Extract environment variables from config
            3. Execute with specified repetitions
            4. Collect all measurements from runs
            5. Track execution failures

    Args:
        build_results: Results from build phase (dict with 'successful_instances' key)
        config: Complete configuration dictionary
        repetitions: Number of executions per instance (overrides YAML config)
        timeout: Timeout in seconds per execution (overrides YAML config)

    Returns:
        CollectionResults dict with:
            - execution_results: List[ExecutionResult]
            - skipped_instances: List[Dict] with skipped instance info
            - errors: List[Dict] with error instance info
            - timestamp: str (ISO format)
            - summary: Dict with counts

    Raises:
        KeyError: If required config keys are missing
        ValueError: If build_results is missing required fields
    """
    # Parse execution configuration
    exec_config = config.get('execution', {})
    final_repetitions = repetitions or exec_config.get('repetitions', 5)
    final_timeout = timeout or exec_config.get('timeout', 300)
    env_vars = exec_config.get('environment', {})
    # Standard measurements always precede app-specific ones.
    # Apps must not redefine wall_time or max_rss_mb in their yaml.
    app_measurements = [
        m for m in config.get('measurements', [])
        if m.get('name') not in {'wall_time', 'max_rss_mb'}
    ]
    measurements = STANDARD_MEASUREMENTS + app_measurements

    logger.info(f"Execution config: repetitions={final_repetitions}, timeout={final_timeout}s")

    # Extract application info
    app_name = config['application']['name']
    base_build_dir = Path('applications') / app_name / 'build'

    # Initialize collections
    execution_results = []
    skipped_instances = []
    errors = []

    # Process each successful instance
    successful_instances = build_results.get('successful_instances', [])
    logger.info(f"Processing {len(successful_instances)} successful instances")

    progress = ProgressBar(len(successful_instances), "Executing")
    for instance in successful_instances:
        instance_name = instance.get('instance_name') or instance.get('name')
        if not instance_name:
            logger.warning("Instance missing name field, skipping")
            progress.update()
            continue

        # Get executable path from build result (stored during Phase 2)
        executable_path = instance.get('executable_path')
        if not executable_path:
            logger.warning(f"Skipping {instance_name}: no executable path in build result")
            skipped_instances.append({
                "instance_name": instance_name,
                "reason": "no executable path in build result"
            })
            progress.update()
            continue

        executable = Path(executable_path)

        if not executable.exists():
            logger.warning(f"Skipping {instance_name}: executable not found at {executable}")
            skipped_instances.append({
                "instance_name": instance_name,
                "reason": "executable not found"
            })
            progress.update()
            continue

        if not os.access(executable, os.X_OK):
            logger.warning(f"Skipping {instance_name}: executable not executable")
            skipped_instances.append({
                "instance_name": instance_name,
                "reason": "executable not executable"
            })
            progress.update()
            continue

        try:
            # Execute instance with all repetitions
            exec_result = execute_instance(
                executable,
                final_repetitions,
                final_timeout,
                env_vars,
                measurements,
                instance_name=instance_name
            )

            execution_results.append(exec_result)
            logger.info(f"Executed {instance_name}: {len(exec_result['runs'])} successful runs")

        except Exception as e:
            logger.error(f"Execution failed for {instance_name}: {e}", exc_info=True)
            errors.append({
                "instance_name": instance_name,
                "error": str(e)
            })
        finally:
            progress.update()

    # Return collection results
    return {
        "execution_results": execution_results,
        "skipped_instances": skipped_instances,
        "errors": errors,
        "timestamp": datetime.now(timezone.utc).isoformat(),
        "summary": {
            "executed": len(execution_results),
            "skipped": len(skipped_instances),
            "errors": len(errors),
            "total_attempted": len(execution_results) + len(skipped_instances)
        }
    }
