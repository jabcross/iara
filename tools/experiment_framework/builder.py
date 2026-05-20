"""
Build execution module for IaRa Unified Experiment Framework.

This module handles sequential builds with GNU time measurement, retry logic,
and compilation timing capture.
"""

import logging
import os
import re
import subprocess
import time
import json
from datetime import datetime, timezone
from pathlib import Path
from typing import Dict, List, Any, Optional
from dataclasses import dataclass, field

from .common import (log_subprocess_call, run_and_log,
                       parse_time_output, convert_time_to_seconds,
                       convert_memory_to_bytes)
from .config import ConfigError
from .progress import ProgressBar


# Configure logging
logger = logging.getLogger(__name__)


@dataclass
class BuildResult:
    """Result of building a single instance."""
    success: bool
    instance_name: str
    attempt: int
    timestamp: str
    compilation: Dict[str, Any] = field(default_factory=dict)
    binary_size_bytes: int = 0
    errors: List[str] = field(default_factory=list)
    executable_path: Optional[str] = None
    execution: Optional[Dict[str, Any]] = None


@dataclass
class FailedBuildResult:
    """Result of a failed build attempt with detailed error tracking."""
    instance_name: str
    attempts: int  # Number of attempts made
    errors: List[Dict[str, Any]]  # Per-attempt error info
    timestamp: str  # ISO format
    last_error: str  # Most recent error message


@dataclass
class BuildResults:
    """Results of building multiple instances."""
    successful_instances: List[BuildResult] = field(default_factory=list)
    failed_instances: List[FailedBuildResult] = field(default_factory=list)
    timestamp: str = ""

    def __post_init__(self):
        """Calculate aggregate statistics."""
        self.total_instances = len(self.successful_instances) + len(self.failed_instances)
        self.successful_count = len(self.successful_instances)
        self.failed_count = len(self.failed_instances)


def extract_build_errors(build_log: str) -> Dict[str, Any]:
    """
    Parse CMake/compiler output and extract meaningful error messages.

    Algorithm:
    1. Look for error patterns:
       - "error:" (compiler errors)
       - "CMake Error" (CMake errors)
       - "FAILED" (test failures)
       - "fatal:" (fatal errors)
       - "undefined reference" (linker errors)
    2. Extract error message and context (surrounding 5 lines)
    3. Identify phase (configuration, build, etc.) and step (cmake, iara-opt, clang)
    4. Return structured error dict

    Args:
        build_log: Build log output as string

    Returns:
        Dict with keys: phase, step, message, context
        Returns empty dict if no errors found
    """
    if not build_log or not isinstance(build_log, str):
        return {}

    lines = build_log.split('\n')

    # Search for error patterns - order matters! Check most specific first
    for i, line in enumerate(lines):
        full_line = line.lower()

        # Extract context: 2 lines before and 3 lines after
        context_start = max(0, i - 2)
        context_end = min(len(lines), i + 4)
        context = lines[context_start:context_end]
        full_context = '\n'.join(context).lower()

        # Check CMake errors first (very specific)
        if 'cmake error' in full_line:
            return {
                'phase': 'configuration',
                'step': 'cmake',
                'message': line.strip(),
                'context': context
            }

        # Check for undefined reference (linker error, step = clang)
        if 'undefined reference' in full_line:
            return {
                'phase': 'build',
                'step': 'clang',
                'message': line.strip(),
                'context': context
            }

        # Check for IARA errors (before general error check)
        if 'iara' in full_line or 'iara' in full_context:
            # If line contains error or context mentions iara
            if 'error' in full_line or 'failed' in full_line or 'optimization' in full_line:
                return {
                    'phase': 'build',
                    'step': 'iara-opt',
                    'message': line.strip(),
                    'context': context
                }

        # Check for MLIR errors
        if 'mlir' in full_line and ('error' in full_line or 'failed' in full_line):
            return {
                'phase': 'build',
                'step': 'mlir-to-llvm',
                'message': line.strip(),
                'context': context
            }

        # Check for fatal errors
        if 'fatal' in full_line:
            return {
                'phase': 'build',
                'step': 'unknown',
                'message': line.strip(),
                'context': context
            }

        # Check for test failures
        if 'failed' in full_line and ('test' in full_line or 'test' in full_context):
            return {
                'phase': 'build',
                'step': 'unknown',
                'message': line.strip(),
                'context': context
            }

        # Check for general compiler errors
        if 'error:' in full_line or 'error ' in full_line:
            # Try to infer the step from context
            step = 'unknown'
            if 'clang' in full_context or 'gcc' in full_context or 'ld' in full_context:
                step = 'clang'
            elif 'mlir' in full_context or 'llvm' in full_context:
                step = 'mlir-to-llvm'
            elif 'iara' in full_context:
                step = 'iara-opt'

            return {
                'phase': 'build',
                'step': step,
                'message': line.strip(),
                'context': context
            }

    return {}


def get_binary_size(executable: Path) -> int:
    """
    Extract binary size using 'size -A' command.

    Runs 'size -A <executable>' and extracts the total size in bytes.

    Args:
        executable: Path to compiled executable

    Returns:
        Total size in bytes (from the "Total" line)

    Raises:
        ConfigError: If command fails or output cannot be parsed
    """
    if not executable.exists():
        raise ConfigError(f"Executable not found: {executable}")

    logger.debug(f"Getting binary size for {executable}")
    size_cmd = ['size', '-A', str(executable)]
    log_subprocess_call(size_cmd, cwd=Path.cwd())

    try:
        result = subprocess.run(
            size_cmd,
            capture_output=True,
            text=True,
            check=True,
            timeout=30
        )
    except subprocess.CalledProcessError as e:
        logger.error(f"size command failed with return code {e.returncode}")
        logger.error(f"Stderr: {e.stderr}")
        raise ConfigError(f"size command failed for {executable}: {e.stderr}")
    except subprocess.TimeoutExpired:
        logger.error(f"size command timed out after 30s")
        raise ConfigError(f"size command timed out for {executable}")
    except Exception as e:
        logger.error(f"Unexpected error running size command: {e}")
        raise ConfigError(f"Failed to run size command for {executable}: {e}")

    # Parse output to find "Total" line
    for line in result.stdout.split('\n'):
        match = re.match(r'Total\s+(\d+)', line)
        if match:
            total_bytes = int(match.group(1))
            logger.info(f"Binary size for {executable}: {total_bytes} bytes")
            return total_bytes

    logger.error(f"Could not find 'Total' line in size output")
    logger.debug(f"size output: {result.stdout}")
    raise ConfigError(f"Could not find 'Total' line in size output for {executable}")


def _ctest_test_exists(build_dir: Path, test_name: str) -> bool:
    """Check if a named test exists in the generated CTestTestfile.cmake."""
    testfile = build_dir / 'CTestTestfile.cmake'
    if not testfile.exists():
        return False
    return f'add_test([=[{test_name}]=' in testfile.read_text()


def _run_ctest_phase(build_dir: Path, test_name: str, timeout: int) -> tuple:
    """Run a single named CTest test. Writes full output to <build_dir>/<test_name>.log.

    Application tests must run serially (profiling multi-core apps requires
    exclusive CPU access). This function never introduces parallelism.

    Returns:
        (success: bool, log_path: Path) — log_path always points to the full output.
    """
    log_path = build_dir / f'{test_name}.log'
    cmd = [
        'ctest',
        '--test-dir', str(build_dir),
        '-R', f'^{test_name}$',
        '--output-on-failure',
        '-V',
    ]
    try:
        with open(log_path, 'w') as log_file:
            result = subprocess.run(
                cmd,
                stdout=log_file,
                stderr=subprocess.STDOUT,
                timeout=timeout,
                env=os.environ.copy(),
            )
        return result.returncode == 0, log_path
    except subprocess.TimeoutExpired:
        with open(log_path, 'a') as log_file:
            log_file.write(f'\n[TIMEOUT] Phase {test_name} exceeded {timeout}s\n')
        return False, log_path


def build_instance(
    instance_name: str,
    build_dir: Path,
    cmake_source: Path,
    attempt: int = 1,
    app_type: str = 'experiment',
    codegen_timeout: int = 300,
) -> BuildResult:
    """
    Build a single instance with timing measurement.

    Process:
        1. Force clean: rm -rf build_dir/*
        2. Configure: cmake -B build_dir -S cmake_source
           -DCMAKE_BUILD_TYPE=Release -DIARA_EXPERIMENT_INSTANCE=instance_name
        3. Build: cmake --build build_dir --config Release -- -j1
        4. Parse timing files (iara_opt_time.txt, mlir_llvm_time.txt, clang_time.txt)
        5. Extract binary size: size -A <executable>
        6. Convert all units to base SI

    Args:
        instance_name: Name of the instance to build
        build_dir: Build directory path
        cmake_source: Path to CMake source directory
        attempt: Attempt number (for retry tracking)

    Returns:
        BuildResult with compilation timing, binary size, and errors
    """
    timestamp = datetime.now(timezone.utc).isoformat()
    errors = []
    compilation = {}
    binary_size_bytes = 0

    # Log build start
    logger.info("=" * 80)
    logger.info(f"Building instance: {instance_name} (attempt {attempt}/{attempt + 1})")
    logger.info("=" * 80)

    try:
        # Step 1: Force clean build directory
        logger.info(f"Cleaning build directory: {build_dir}")
        try:
            subprocess.run(
                ['rm', '-rf', str(build_dir)],
                check=True,
                capture_output=True,
                timeout=30
            )
        except Exception as e:
            error_msg = f"Failed to clean build directory: {e}"
            logger.error(error_msg)
            errors.append(error_msg)
            return BuildResult(
                success=False,
                instance_name=instance_name,
                attempt=attempt,
                timestamp=timestamp,
                compilation=compilation,
                binary_size_bytes=binary_size_bytes,
                errors=errors
            )

        # Step 2: Configure CMake
        logger.info(f"Configuring CMake for {instance_name}")
        cmake_config_cmd = [
            'cmake',
            '-B', str(build_dir),
            '-S', str(cmake_source),
            '-DCMAKE_BUILD_TYPE=Release',
            f'-DIARA_EXPERIMENT_INSTANCE={instance_name}'
        ]

        # Pass CMAKE_PREFIX_PATH and PKG_CONFIG_PATH from environment to cmake if set
        # These are needed to find packages like casacore
        if 'CMAKE_PREFIX_PATH' in os.environ:
            cmake_config_cmd.append(f'-DCMAKE_PREFIX_PATH={os.environ["CMAKE_PREFIX_PATH"]}')
        if 'PKG_CONFIG_PATH' in os.environ:
            # PKG_CONFIG_PATH needs to be passed as an environment variable, not a CMake define
            # We'll set it when running cmake via subprocess env
            pass  # Handled below in subprocess.run call

        log_subprocess_call(cmake_config_cmd, cwd=Path.cwd())

        # Prepare environment for CMake - preserve existing environment and add PKG_CONFIG_PATH
        cmake_env = os.environ.copy()
        if 'PKG_CONFIG_PATH' in os.environ:
            # PKG_CONFIG_PATH should already be set in the environment
            pass

        result = subprocess.run(
            cmake_config_cmd,
            capture_output=True,
            text=True,
            timeout=120,
            env=cmake_env
        )

        if result.returncode != 0:
            logger.error(f"CMake configuration failed with return code {result.returncode}")
            logger.error(f"Stdout: {result.stdout[:500]}")
            logger.error(f"Stderr: {result.stderr[:500]}")
            if len(result.stderr) > 500:
                logger.debug(f"Full stderr: {result.stderr}")
            error_msg = f"CMake configuration failed: {result.stderr}"
            logger.error(error_msg)
            errors.append(error_msg)
            return BuildResult(
                success=False,
                instance_name=instance_name,
                attempt=attempt,
                timestamp=timestamp,
                compilation=compilation,
                binary_size_bytes=binary_size_bytes,
                errors=errors
            )

        logger.info("CMake configuration successful")
        logger.debug(f"CMake output: {result.stdout[:200]}")

        # Step 3: Run CTest phases individually in order.
        # Application tests run serially (profiling requires exclusive CPU).
        # Regression tests additionally build iara-opt first.
        is_regression = (app_type == 'regression_test')

        phases = [
            # (phase_key,   test_name,                       timeout,          required)
            ('clean',       f'clean-{instance_name}',         30,               True),
            ('iara-build',  f'iara-build-{instance_name}',    300,              is_regression),
            ('setup',       f'setup-{instance_name}',         codegen_timeout,  False),
            ('build',       f'build-{instance_name}',         120,              False),
            ('lower',       f'lower-{instance_name}',         600,              True),
            ('run',         f'run-{instance_name}',           600,              True),
        ]

        build_start_time = time.time()

        for phase_key, test_name, phase_timeout, required in phases:
            if not _ctest_test_exists(build_dir, test_name):
                if required:
                    error_msg = f"Required CTest test '{test_name}' not found in CTestTestfile.cmake"
                    logger.error(error_msg)
                    errors.append(error_msg)
                    return BuildResult(
                        success=False,
                        instance_name=instance_name,
                        attempt=attempt,
                        timestamp=timestamp,
                        compilation=compilation,
                        binary_size_bytes=binary_size_bytes,
                        errors=errors
                    )
                logger.debug(f"Phase '{phase_key}' not present, skipping")
                continue

            logger.info(f"Running phase '{phase_key}' for {instance_name}")
            ok, log_path = _run_ctest_phase(build_dir, test_name, phase_timeout)

            if ok:
                logger.info(f"Phase '{phase_key}': PASS  (log: {log_path})")
            else:
                logger.error(f"Phase '{phase_key}': FAIL  (log: {log_path})")
                error_msg = f"Phase '{phase_key}' failed. Full output: {log_path}"
                logger.error(error_msg)
                errors.append(error_msg)
                return BuildResult(
                    success=False,
                    instance_name=instance_name,
                    attempt=attempt,
                    timestamp=timestamp,
                    compilation=compilation,
                    binary_size_bytes=binary_size_bytes,
                    errors=errors
                )

        build_end_time = time.time()
        build_time_s = build_end_time - build_start_time
        logger.info(f"Build successful (took {build_time_s:.2f}s)")
        logger.debug(f"Build output: {result.stdout[:200]}")

        # Step 4: Parse timing files
        timing_files = {
            'iara_opt_time_s': build_dir / 'iara_opt_time.txt',
            'mlir_llvm_time_s': build_dir / 'mlir_llvm_time.txt',
            'clang_time_s': build_dir / 'clang_time.txt'
        }

        total_time_s = 0.0
        for timing_key, timing_file in timing_files.items():
            if timing_file.exists():
                try:
                    timing_data = parse_time_output(timing_file)
                    if 'wall_time_s' in timing_data:
                        wall_time = timing_data['wall_time_s']
                        compilation[timing_key] = wall_time
                        total_time_s += wall_time
                        logger.debug(f"Parsed {timing_file}: {wall_time}s")
                except ConfigError as e:
                    logger.warning(f"Failed to parse {timing_file}: {e}")
            else:
                logger.debug(f"Timing file not found: {timing_file} (optional)")

        # Use wall-clock build time as fallback or if timing files not available
        if total_time_s == 0.0:
            total_time_s = build_time_s
            logger.debug(f"Using wall-clock build time as total: {build_time_s:.2f}s")
        else:
            logger.debug(f"Using timing files sum: {total_time_s:.2f}s (wall-clock: {build_time_s:.2f}s)")

        compilation['total_time_s'] = total_time_s

        # Step 5: Extract binary size
        # Find the executable - typically named "a.out" after CMake build
        # Common locations: build_dir/, build_dir/bin/
        executable = None

        # Ensure build_dir is absolute to avoid relative path issues during pipeline
        build_dir_abs = build_dir.resolve() if isinstance(build_dir, Path) else Path(build_dir).resolve()

        logger.debug(f"Searching for executable in build_dir: {build_dir}")
        logger.debug(f"Absolute build_dir: {build_dir_abs}")
        logger.debug(f"build_dir exists: {build_dir_abs.exists()}")

        # List directory contents for debugging
        if build_dir_abs.exists():
            logger.debug(f"Contents of {build_dir_abs}:")
            try:
                for item in sorted(build_dir_abs.iterdir()):
                    logger.debug(f"  - {item.name} ({'dir' if item.is_dir() else 'file'})")
            except Exception as e:
                logger.debug(f"  Error listing directory: {e}")

        # Try to find "a.out" first (default CMake executable name)
        for candidate_dir in [build_dir_abs, build_dir_abs / 'bin']:
            logger.debug(f"Checking candidate_dir: {candidate_dir} (exists: {candidate_dir.exists()})")
            if candidate_dir.exists():
                a_out = candidate_dir / 'a.out'
                logger.debug(f"Checking for a.out: {a_out} (exists: {a_out.exists()})")
                if a_out.exists() and a_out.is_file():
                    executable = a_out
                    logger.debug(f"Found executable: {executable}")
                    break

        # If not found, try to find executable named after the instance
        if not executable:
            for candidate_dir in [build_dir_abs / 'bin', build_dir_abs]:
                if candidate_dir.exists():
                    for exe_candidate in candidate_dir.glob(instance_name):
                        if exe_candidate.is_file() and executable is None:
                            executable = exe_candidate
                            break
                    if executable:
                        break

        # If still not found, try a more general search for instance name
        if not executable:
            for exe_candidate in build_dir_abs.rglob(instance_name):
                if exe_candidate.is_file():
                    executable = exe_candidate
                    break

        if executable:
            try:
                binary_size_bytes = get_binary_size(executable)
                logger.debug(f"Binary size: {binary_size_bytes} bytes")
            except ConfigError as e:
                logger.warning(f"Failed to get binary size: {e}")
                # Non-fatal error
        else:
            logger.warning(f"Could not find executable for {instance_name}")

        logger.info("=" * 80)
        logger.info(f"✓ Successfully built instance: {instance_name} on attempt {attempt}")
        logger.info("=" * 80)

        return BuildResult(
            success=True,
            instance_name=instance_name,
            attempt=attempt,
            timestamp=timestamp,
            compilation=compilation,
            binary_size_bytes=binary_size_bytes,
            executable_path=str(executable.resolve()) if executable else None,
            errors=errors
        )

    except subprocess.TimeoutExpired as e:
        error_msg = f"Build timed out: {e}"
        logger.error(error_msg)
        errors.append(error_msg)
        return BuildResult(
            success=False,
            instance_name=instance_name,
            attempt=attempt,
            timestamp=timestamp,
            compilation=compilation,
            binary_size_bytes=binary_size_bytes,
            errors=errors
        )
    except Exception as e:
        error_msg = f"Unexpected error during build: {e}"
        logger.error(error_msg)
        errors.append(error_msg)
        return BuildResult(
            success=False,
            instance_name=instance_name,
            attempt=attempt,
            timestamp=timestamp,
            compilation=compilation,
            binary_size_bytes=binary_size_bytes,
            errors=errors
        )


def build_all_instances(
    instances: List[str],
    base_build_dir: Path,
    cmake_source: Path,
    max_retries: int = 2,
    app_type: str = 'experiment',
    codegen_timeout: int = 300,
) -> BuildResults:
    """
    Build all instances sequentially with retry logic and error tracking.

    Process:
        For each instance:
            attempt = 1
            errors_list = []
            last_error = None

            while attempt <= max_retries:
                result = build_instance(instance, ..., attempt)

                if result.success:
                    successful_instances.append(result)
                    break
                else:
                    # Capture detailed error info per attempt
                    errors_list.append(error_info)
                    last_error = result.errors[0]

                    if attempt < max_retries:
                        delay = 2 ** (attempt - 1)  # Exponential backoff: 1s, 2s, 4s
                        time.sleep(delay)
                        attempt += 1
                    else:
                        break

            if result and not result.success:
                # All retries exhausted, add to failed list
                failed_result = FailedBuildResult(...)
                failed_instances.append(failed_result)
            # Continue processing next instance regardless

    Args:
        instances: List of instance names to build
        base_build_dir: Base build directory
        cmake_source: Path to CMake source directory
        max_retries: Maximum number of retry attempts (default 2)

    Returns:
        BuildResults with successful and failed instances
    """
    successful_instances = []
    failed_instances = []

    logger.info(f"Starting build of {len(instances)} instances with max_retries={max_retries}")

    progress = ProgressBar(len(instances), "Building")
    for instance_name in instances:
        logger.info(f"Building instance: {instance_name}")

        attempt = 1
        result = None
        errors_list = []
        last_error = None

        while attempt <= max_retries:
            build_dir = base_build_dir / instance_name
            logger.debug(f"Attempt {attempt}/{max_retries} for {instance_name}")

            result = build_instance(
                instance_name=instance_name,
                build_dir=build_dir,
                cmake_source=cmake_source,
                attempt=attempt,
                app_type=app_type,
                codegen_timeout=codegen_timeout,
            )

            if result.success:
                logger.info(f"Successfully built {instance_name} on attempt {attempt}")
                successful_instances.append(result)
                break

            # Build failed, capture error info
            logger.warning(f"Build failed for {instance_name} on attempt {attempt}")

            error_info = {
                'attempt': attempt,
                'phase': 'build',
                'step': 'unknown',
                'message': result.errors[0] if result.errors else 'Unknown error',
                'context': [],
                'timestamp': result.timestamp
            }

            # Try to extract more detailed error info from the build output
            if result.errors:
                last_error = result.errors[0]

            errors_list.append(error_info)

            if attempt < max_retries:
                delay = 2 ** (attempt - 1)  # Exponential backoff: 1s, 2s, 4s
                logger.warning(
                    f"Instance {instance_name} attempt {attempt} failed, "
                    f"retrying in {delay}s..."
                )
                time.sleep(delay)
                attempt += 1
            else:
                # Max retries reached, break out of loop
                break

        if result and not result.success:
            # All retries exhausted, add to failed list
            failed_result = FailedBuildResult(
                instance_name=instance_name,
                attempts=attempt,
                errors=errors_list,
                timestamp=datetime.now(timezone.utc).isoformat(),
                last_error=last_error or 'Unknown error'
            )
            failed_instances.append(failed_result)
            logger.error(
                f"Failed to build {instance_name} after {attempt} attempts: {last_error}"
            )
        # Continue processing next instance regardless
        progress.update()

    logger.info(
        f"Build complete: {len(successful_instances)} successful, "
        f"{len(failed_instances)} failed out of {len(instances)} total"
    )

    results = BuildResults(
        successful_instances=successful_instances,
        failed_instances=failed_instances,
        timestamp=datetime.now(timezone.utc).isoformat()
    )

    # Log summary statistics
    if results.total_instances > 0:
        success_rate = (results.successful_count / results.total_instances) * 100
        logger.info(f"Build success rate: {success_rate:.1f}% ({results.successful_count}/{results.total_instances})")

    return results


def get_git_commit() -> str:
    """
    Get current git HEAD commit hash.

    Runs 'git rev-parse HEAD' to get the current commit hash.

    Returns:
        First 8 characters of commit hash (short form) or "unknown" if not in git repo
        or command fails.

    Error handling:
        - If git command fails, return "unknown"
        - If not in a git repository, return "unknown"
    """
    try:
        result = subprocess.run(
            ['git', 'rev-parse', 'HEAD'],
            capture_output=True,
            text=True,
            check=True,
            timeout=10
        )
        # Return first 8 characters (short form)
        commit_hash = result.stdout.strip()
        return commit_hash[:8] if len(commit_hash) >= 8 else commit_hash
    except Exception as e:
        logger.warning(f"Failed to get git commit: {e}")
        return "unknown"


def get_yaml_hash(experiment_dir: Path) -> str:
    """
    Read YAML hash from .experiments.yaml.hash file.

    Algorithm:
        1. Look for .experiments.yaml.hash in experiment_dir
        2. Read the file and return first line (stripped)
        3. If file doesn't exist, return "unknown"

    Args:
        experiment_dir: Path to experiment directory

    Returns:
        64-character hex hash or "unknown" if file doesn't exist
    """
    hash_file = experiment_dir / '.experiments.yaml.hash'

    if not hash_file.exists():
        logger.debug(f"YAML hash file not found: {hash_file}")
        return "unknown"

    try:
        content = hash_file.read_text().strip()
        # Return first line if multiple lines exist
        first_line = content.split('\n')[0].strip()
        if first_line:
            return first_line
        return "unknown"
    except Exception as e:
        logger.warning(f"Failed to read YAML hash from {hash_file}: {e}")
        return "unknown"


def extract_binary_sections(build_result: BuildResult) -> Dict[str, Any]:
    """
    Extract binary sections from build result using 'size -A' command.

    Parses all ELF sections and groups them by base name (e.g. all .ltext.*
    subsections are aggregated under 'ltext').  The result contains
    total_size_bytes (from the filesystem) and a sections dict mapping
    sanitised base names to their cumulative byte sizes.

    Args:
        build_result: BuildResult with binary_size_bytes and executable_path

    Returns:
        Dict with keys:
            total_size_bytes: int  – file size in bytes
            sections: Dict[str, int] – {base_section_name: bytes, ...}
    """
    total_size = build_result.binary_size_bytes
    sections_by_base: Dict[str, int] = {}

    # Try to get section sizes from executable if available
    if build_result.executable_path:
        executable = Path(build_result.executable_path)
        if executable.exists():
            try:
                # Use size -A to get section sizes (simpler and more reliable than readelf)
                result = subprocess.run(
                    ["size", "-A", str(executable)],
                    capture_output=True,
                    text=True,
                    timeout=5
                )

                if result.returncode == 0:
                    # Parse size -A output: .section_name   size   addr
                    for line in result.stdout.split('\n'):
                        line = line.strip()
                        if not line:
                            continue

                        # Skip header / total lines
                        if line.startswith('section') or line.startswith('Total'):
                            continue

                        parts = line.split()
                        if len(parts) >= 2:
                            try:
                                section_name = parts[0]
                                size = int(parts[1])
                                if size == 0:
                                    continue

                                # Group by base name: strip leading dot, take
                                # only the first component so that e.g.
                                # ".ltext", ".ltext.unlikely.", and
                                # ".ltext._ZN8casacore..." all map to "ltext".
                                base = section_name.lstrip('.').split('.')[0]
                                if base:
                                    sections_by_base[base] = sections_by_base.get(base, 0) + size
                            except (ValueError, IndexError):
                                pass

                logger.debug(f"Extracted {len(sections_by_base)} section groups")
            except (subprocess.TimeoutExpired, FileNotFoundError) as e:
                logger.warning(f"Could not extract section sizes: {e}")
        else:
            logger.debug(f"Executable not found at {executable}")

    return {
        "total_size_bytes": total_size,
        "sections": sections_by_base
    }


def _parse_instance_name(instance_name: str) -> Dict[str, Any]:
    """
    Parse instance name to extract parameters.

    Instance names follow the pattern:
    <app>_<scheduler>_<param1>_<value1>_<param2>_<value2>...

    Parameters with underscores (e.g., "matrix_size") are followed by their numeric values.

    Example: "05-cholesky_sequential_matrix_size_10080_num_blocks_4"

    Args:
        instance_name: Full instance name

    Returns:
        Dict with scheduler and parameter key-value pairs
    """
    parts = instance_name.split('_')
    if len(parts) < 3:
        return {}

    params = {}
    # Format: <app>_<set>_<scheduler>_<key1>_<val1>_<key2>_<val2>...
    # Neither keys nor values ever contain underscores, so the structure is a
    # strict sequence of alternating single-token keys and single-token values.
    # parts[0] = app, parts[1] = set name (skip), parts[2] = scheduler
    params['scheduler'] = parts[2]

    remaining = parts[3:]
    for i in range(0, len(remaining) - 1, 2):
        key = remaining[i]
        val_str = remaining[i + 1]
        try:
            params[key] = int(val_str)
        except ValueError:
            try:
                params[key] = float(val_str)
            except ValueError:
                params[key] = val_str

    return params


def write_build_results(
    results: BuildResults,
    output_dir: Path,
    config: Dict[str, Any],
    experiment_set: str,
    experiment_dir: Optional[Path] = None
) -> Path:
    """
    Write BuildResults to JSON file.

    Generates a JSON file following the schema specification with all required fields:
    - schema_version: "1.0.0"
    - experiment: metadata section with application, git commit, yaml hash
    - instances: successful builds with timing and binary data
    - failed_instances: failed builds with error information
    - statistics: aggregated stats

    Args:
        results: BuildResults containing successful and failed instances
        output_dir: Directory to write JSON file to
        config: Configuration dict with 'application' key
        experiment_set: Name of the experiment set (e.g., "regression_test")
        experiment_dir: Optional path to experiment directory for yaml hash

    Returns:
        Path to the created JSON file

    Raises:
        ValueError: If config is missing required fields
    """
    if 'application' not in config:
        raise ValueError("config must contain 'application' key")

    # Create output directory if needed
    output_dir.mkdir(parents=True, exist_ok=True)

    # Get metadata
    git_commit = get_git_commit()
    yaml_hash = "unknown"
    if experiment_dir:
        yaml_hash = get_yaml_hash(experiment_dir)

    # Build successful instances section
    instances_section = []
    for build_result in results.successful_instances:
        binary_sections = extract_binary_sections(build_result)
        parameters = _parse_instance_name(build_result.instance_name)

        instance_obj = {
            "name": build_result.instance_name,
            "parameters": parameters,
            "compilation": build_result.compilation,
            "binary": binary_sections
        }

        # Add executable path if present (needed for Phase 3)
        if build_result.executable_path is not None:
            instance_obj["executable_path"] = build_result.executable_path

        # Add execution data if present (Phase 3)
        if build_result.execution is not None:
            instance_obj["execution"] = build_result.execution

        instances_section.append(instance_obj)

    # Build failed instances section
    failed_instances_section = []
    for failed_result in results.failed_instances:
        parameters = _parse_instance_name(failed_result.instance_name)

        # Transform errors list to match JSON schema
        errors_list = []
        for error_info in failed_result.errors:
            error_obj = {
                "attempt": error_info.get('attempt', 1),
                "phase": error_info.get('phase', 'build'),
                "step": error_info.get('step', 'unknown'),
                "message": error_info.get('message', 'Unknown error'),
                "timestamp": error_info.get('timestamp', failed_result.timestamp)
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
    total_instances = results.total_instances
    successful_count = results.successful_count
    failed_count = results.failed_count
    success_rate = (successful_count / total_instances) if total_instances > 0 else 0.0

    # Build complete JSON structure
    output_json = {
        "schema_version": "1.0.0",
        "experiment": {
            "application": config['application'],
            "experiment_set": experiment_set,
            "timestamp": results.timestamp,
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

    # Generate timestamp-based filename
    # Convert ISO timestamp to filename format (replace colons with hyphens)
    timestamp_str = results.timestamp.replace(':', '-').replace('+', 'Z')
    # Trim to just the date and time without timezone
    timestamp_str = timestamp_str.split('.')[0] + 'Z'

    output_file = output_dir / f"results_{timestamp_str}.json"

    # Write JSON file
    try:
        with open(output_file, 'w') as f:
            json.dump(output_json, f, indent=2, sort_keys=True)
        logger.info(f"Written build results to {output_file}")
        return output_file
    except Exception as e:
        raise ValueError(f"Failed to write JSON file {output_file}: {e}")


def read_build_results(json_file: Path) -> BuildResults:
    """
    Read BuildResults from a JSON file (Phase 2 or Phase 3 format).

    Parses a JSON file containing build results and reconstructs a BuildResults
    object. Handles both Phase 2 format (without execution data) and Phase 3 format
    (with execution data) for backward compatibility.

    Algorithm:
        1. Validate file exists
        2. Load JSON file
        3. Validate schema version
        4. Parse successful instances into BuildResult objects
        5. Parse failed instances into FailedBuildResult objects
        6. Return BuildResults object

    Args:
        json_file: Path to the JSON file containing build results

    Returns:
        BuildResults object with successful_instances, failed_instances, and timestamp

    Raises:
        ConfigError: If file not found, invalid JSON, missing required fields,
                    or unsupported schema version

    Examples:
        Read Phase 2 JSON (without execution):
        >>> results = read_build_results(Path("results_2024-01-15.json"))
        >>> print(len(results.successful_instances))

        Read Phase 3 JSON (with execution):
        >>> results = read_build_results(Path("results_phase3_2024-01-15.json"))
        >>> print(results.successful_instances[0].execution)
    """
    # Validate file exists
    if not json_file.exists():
        raise ConfigError(f"Results file not found: {json_file}")

    # Load JSON
    try:
        content = json_file.read_text()
        data = json.loads(content)
    except json.JSONDecodeError as e:
        raise ConfigError(f"Invalid JSON in {json_file}: {e}")
    except Exception as e:
        raise ConfigError(f"Failed to read {json_file}: {e}")

    # Validate schema version
    schema_version = data.get('schema_version', 'unknown')
    if schema_version != '1.0.0':
        raise ConfigError(f"Unsupported schema version: {schema_version} (expected 1.0.0)")

    # Parse successful instances
    successful_instances = []
    for instance_obj in data.get('instances', []):
        try:
            binary_sections = instance_obj.get('binary', {})
            execution_data = instance_obj.get('execution', None)
            executable_path = instance_obj.get('executable_path', None)

            result = BuildResult(
                success=True,
                instance_name=instance_obj['name'],
                attempt=1,
                timestamp=instance_obj.get('timestamp', data.get('experiment', {}).get('timestamp', '')),
                compilation=instance_obj.get('compilation', {}),
                binary_size_bytes=binary_sections.get('total_size_bytes', 0),
                executable_path=executable_path,
                execution=execution_data
            )
            successful_instances.append(result)
        except KeyError as e:
            raise ConfigError(f"Missing required field in instance object: {e}")

    # Parse failed instances
    failed_instances = []
    for failed_obj in data.get('failed_instances', []):
        try:
            failed = FailedBuildResult(
                instance_name=failed_obj['name'],
                attempts=failed_obj.get('attempts', 1),
                errors=failed_obj.get('errors', []),
                timestamp=failed_obj.get('timestamp', ''),
                last_error=failed_obj.get('last_error', '')
            )
            failed_instances.append(failed)
        except KeyError as e:
            raise ConfigError(f"Missing required field in failed instance object: {e}")

    # Get experiment timestamp
    experiment_timestamp = data.get('experiment', {}).get('timestamp', '')

    return BuildResults(
        successful_instances=successful_instances,
        failed_instances=failed_instances,
        timestamp=experiment_timestamp
    )


def update_instance_execution(
    build_results: BuildResults,
    instance_name: str,
    execution_data: Dict[str, Any]
) -> None:
    """
    Update a specific instance with execution data (in-place modification).

    Finds the instance by name in the BuildResults and updates its execution
    field with the provided execution data. Useful for Phase 3 read-modify-write
    workflow where compilation data is in Phase 2 JSON and execution is added
    from collector.py.

    Algorithm:
        1. Iterate through successful_instances
        2. Find instance matching instance_name
        3. Update its execution field
        4. Return (modifies in-place)

    Args:
        build_results: BuildResults object to update
        instance_name: Name of the instance to update
        execution_data: Execution data dict from collector.execute_instance()

    Raises:
        ValueError: If instance with given name is not found

    Examples:
        Update instance with execution statistics:
        >>> results = read_build_results(Path("phase2.json"))
        >>> exec_data = execute_instance(executable, 5, 30, {}, [])
        >>> update_instance_execution(results, "05-cholesky_vf", exec_data)
        >>> write_build_results(results, output_dir, config, "test")
    """
    for instance in build_results.successful_instances:
        if instance.instance_name == instance_name:
            instance.execution = execution_data
            logger.debug(f"Updated execution data for instance: {instance_name}")
            return

    raise ValueError(f"Instance not found: {instance_name}")
