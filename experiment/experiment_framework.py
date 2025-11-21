#!/usr/bin/env python3
"""
Reusable experiment runner framework for benchmarking applications.

This library provides a data-driven, composable framework for running
build-and-benchmark experiments with support for:
- Multiple configurations and repetitions
- Build metrics collection (compile time, binary size)
- Runtime metrics (wall time, memory usage)
- Local or SLURM execution
- Incremental result saving
- Comprehensive logging

To use this framework for a new application:
1. Implement the ApplicationAdapter interface
2. Create configuration factory functions
3. Instantiate ExperimentRunner with your adapter
4. Call run_experiment() with your configs
"""

import os
import subprocess
import csv
import time
import socket
from pathlib import Path
from datetime import datetime
from typing import List, Dict, Any, Optional, Callable, NamedTuple
from dataclasses import dataclass
from abc import ABC, abstractmethod
import json

try:
    from tqdm import tqdm
    HAS_TQDM = True
except ImportError:
    HAS_TQDM = False
    # Fallback when tqdm is not available
    class tqdm:
        """Minimal tqdm fallback that does nothing."""
        def __init__(self, iterable=None, total=None, desc=None, **kwargs):
            self.iterable = iterable
            self.n = 0
            self.total = total

        def __iter__(self):
            return iter(self.iterable)

        def __enter__(self):
            return self

        def __exit__(self, *args):
            pass

        def update(self, n=1):
            self.n += n

        def set_description(self, desc):
            pass

        def close(self):
            pass


# ============================================================================
# Core abstractions
# ============================================================================

@dataclass
class ExperimentConfig:
    """Base configuration for a single experiment instance.

    Each config represents one build configuration to test.

    Attributes:
        name: Unique identifier for this configuration
        params: Dictionary of application-specific parameters
    """
    name: str
    params: Dict[str, Any]

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary for serialization."""
        return {"name": self.name, **self.params}


class ApplicationAdapter(ABC):
    """Abstract interface for application-specific build and run logic.

    Implement this interface to adapt the framework to a new application.
    Each method is called at the appropriate point in the build/run pipeline.
    """

    @abstractmethod
    def get_build_env(self, config: ExperimentConfig) -> Dict[str, str]:
        """Return environment variables needed for building this config.

        These will be merged with os.environ and passed to CMake and make.
        """
        pass

    @abstractmethod
    def get_cmake_args(self, config: ExperimentConfig) -> List[str]:
        """Return CMake arguments for this config.

        Should return a list like ["-DCMAKE_BUILD_TYPE=Release", ...].
        """
        pass

    @abstractmethod
    def get_build_target(self, config: ExperimentConfig) -> str:
        """Return the CMake target name to build.

        This is passed to make as: make -j <target>
        """
        pass

    @abstractmethod
    def get_executable_path(self, config: ExperimentConfig, build_dir: Path) -> Path:
        """Return the path to the executable after building.

        Args:
            config: The configuration that was built
            build_dir: The directory where CMake was run

        Returns:
            Absolute path to the executable
        """
        pass

    @abstractmethod
    def parse_program_output(self, stdout: str, stderr: str) -> Optional[Dict[str, Any]]:
        """Parse program output and return metrics dict, or None if invalid.

        Extract application-specific metrics from the program's output.
        Common metrics might include: wall_time, throughput, accuracy, etc.

        Returns:
            Dict with metric names/values, or None if output is invalid
        """
        pass

    @abstractmethod
    def get_run_command(self, executable: Path) -> List[str]:
        """Return the command to execute (without /usr/bin/time wrapper).

        Args:
            executable: Path to the built executable

        Returns:
            Command as list, e.g., [str(executable), "--arg1", "value"]
        """
        pass


# ============================================================================
# Utility classes
# ============================================================================

class MetricsCollector:
    """Utilities for collecting various runtime metrics."""

    @staticmethod
    def parse_time_output(stderr: str) -> Dict[str, float]:
        """Parse /usr/bin/time output (format: %M %e).

        Returns max_rss_kb (maximum resident set size) and time_elapsed.

        This searches through the output for a line matching the pattern:
        <integer> <float>
        which is the format produced by /usr/bin/time -f "%M %e"
        """
        max_rss_kb = 0
        elapsed_time = 0.0

        # Search through all lines for the time output pattern
        # /usr/bin/time -f "%M %e" produces: <max_rss_kb> <elapsed_seconds>
        lines = stderr.strip().split('\n')
        for line in lines:
            parts = line.strip().split()
            # Look for a line with exactly 2 tokens that are numeric
            if len(parts) == 2:
                try:
                    # Try to parse as integer and float
                    rss = int(parts[0])
                    elapsed = float(parts[1])
                    # Valid parse - update values
                    max_rss_kb = rss
                    elapsed_time = elapsed
                    # Continue searching to get the last matching line
                except (ValueError, IndexError):
                    # Not a valid time output line, continue
                    pass

        return {"max_rss_kb": max_rss_kb, "time_elapsed": elapsed_time}


class PendingJob(NamedTuple):
    """Represents a pending SLURM job."""
    job_id: str
    config: ExperimentConfig
    run_number: int
    is_warmup: bool
    output_file: Path


class CommandExecutor:
    """Executes commands with consistent error handling and timeouts."""

    def __init__(self, logger: Callable[[str], None], default_timeout: int = 300):
        """Initialize executor.

        Args:
            logger: Function to call for logging messages
            default_timeout: Default timeout in seconds for commands
        """
        self.logger = logger
        self.default_timeout = default_timeout

    def run(self, cmd: List[str], cwd: Optional[Path] = None,
            env: Optional[Dict[str, str]] = None,
            timeout: Optional[int] = None) -> tuple[int, str, str]:
        """Run a command and return (exit_code, stdout, stderr)."""
        try:
            result = subprocess.run(
                cmd,
                cwd=cwd,
                env=env,
                capture_output=True,
                text=True,
                timeout=timeout or self.default_timeout
            )
            return result.returncode, result.stdout, result.stderr
        except subprocess.TimeoutExpired:
            self.logger(f"ERROR: Command timed out: {' '.join(cmd)}")
            return -1, "", "Command timed out"
        except Exception as e:
            self.logger(f"ERROR: Command failed: {e}")
            return -1, "", str(e)


class SlurmExecutor:
    """Handles SLURM job submission and monitoring."""

    def __init__(self, logger: Callable[[str], None],
                 slurm_config: Optional[Dict[str, Any]] = None):
        """Initialize SLURM executor.

        Args:
            logger: Function to call for logging messages
            slurm_config: Optional dict with SLURM parameters:
                - nodes: Number of nodes (default: 1)
                - ntasks: Number of tasks (default: 1)
                - cpus_per_task: CPUs per task (default: 48)
                - partition: SLURM partition (default: "cpu")
                - nodelist: Node list (default: "sorgan-cpu[2-3]")
                - time_limit: Time limit (default: "00:10:00")
        """
        self.logger = logger
        self.config = slurm_config or {}

    def is_available(self) -> bool:
        """Check if SLURM commands are available."""
        try:
            result = subprocess.run(["which", "sbatch"], capture_output=True, timeout=5)
            return result.returncode == 0
        except:
            return False

    def create_script(self, script_path: Path, executable: Path,
                     working_dir: Path, output_file: Path,
                     job_name: str) -> None:
        """Create a SLURM batch script.

        Args:
            script_path: Where to write the script
            executable: Path to executable to run
            working_dir: Directory to cd into before running
            output_file: Where to write stdout/stderr
            job_name: Name for the SLURM job
        """
        # Use config or defaults
        nodes = self.config.get("nodes", 1)
        ntasks = self.config.get("ntasks", 1)
        cpus_per_task = self.config.get("cpus_per_task", 48)
        partition = self.config.get("partition", "cpu")
        nodelist = self.config.get("nodelist", "sorgan-cpu[2-3]")
        time_limit = self.config.get("time_limit", "00:10:00")

        script_content = f"""#!/bin/bash
#SBATCH --job-name={job_name}
#SBATCH --output={output_file}
#SBATCH --error={output_file}
#SBATCH --nodes={nodes}
#SBATCH --ntasks={ntasks}
#SBATCH --cpus-per-task={cpus_per_task}
#SBATCH --exclusive
#SBATCH --partition={partition}
#SBATCH --nodelist={nodelist}
#SBATCH --time={time_limit}

echo "=== Job started on $(hostname) at $(date) ==="
echo "SLURM_JOB_ID: $SLURM_JOB_ID"
echo "SLURM_JOB_NODELIST: $SLURM_JOB_NODELIST"
echo ""

cd {working_dir}
/usr/bin/time -f "%M %e" {executable}

exit_code=$?
echo ""
echo "=== Job finished at $(date) with exit code $exit_code ==="
exit $exit_code
"""
        script_path.write_text(script_content)
        script_path.chmod(0o755)

    def submit_job(self, script_path: Path) -> Optional[str]:
        """Submit a SLURM job and return the job ID."""
        try:
            result = subprocess.run(
                ["sbatch", str(script_path)],
                capture_output=True,
                text=True,
                timeout=10
            )

            if result.returncode != 0:
                self.logger(f"ERROR: sbatch failed: {result.stderr}")
                return None

            output = result.stdout.strip()
            if "Submitted batch job" in output:
                return output.split()[-1]
            else:
                self.logger(f"ERROR: Could not parse job ID from: {output}")
                return None
        except Exception as e:
            self.logger(f"ERROR: Failed to submit SLURM job: {e}")
            return None

    def wait_for_job(self, job_id: str, timeout: int = 600) -> bool:
        """Wait for a SLURM job to complete. Returns True if successful."""
        start_time = time.time()

        while time.time() - start_time < timeout:
            try:
                result = subprocess.run(
                    ["squeue", "-j", job_id, "-h", "-o", "%T"],
                    capture_output=True,
                    text=True,
                    timeout=5
                )

                if result.returncode != 0 or not result.stdout.strip():
                    # Check final status with sacct
                    result = subprocess.run(
                        ["sacct", "-j", job_id, "-n", "-o", "State"],
                        capture_output=True,
                        text=True,
                        timeout=5
                    )

                    if result.returncode == 0:
                        state = result.stdout.strip().split('\n')[0].strip()
                        if "COMPLETED" in state:
                            return True
                        else:
                            self.logger(f"Job {job_id} finished with state: {state}")
                            return False
                    return True

                time.sleep(2)
            except Exception as e:
                self.logger(f"ERROR checking job status: {e}")
                time.sleep(2)

        self.logger(f"ERROR: Job {job_id} timed out after {timeout}s")
        return False


# ============================================================================
# Main experiment runner (application-agnostic)
# ============================================================================

class ExperimentRunner:
    """Manages building and running experiments in an application-agnostic way.

    This is the main entry point for the framework. It orchestrates:
    1. Building all configurations
    2. Running measurements with repetitions
    3. Collecting metrics (build time, runtime, memory, binary size)
    4. Saving results incrementally to CSV
    5. Managing SLURM execution if requested

    Example usage:
        adapter = MyApplicationAdapter()
        runner = ExperimentRunner(
            project_dir=Path("/path/to/project"),
            experiment_name="my_experiment",
            app_adapter=adapter,
            num_repetitions=10,
            warmup_runs=2
        )

        configs = [MyConfig(param1=x, param2=y) for x, y in params]
        runner.run_experiment(configs)
    """

    def __init__(self,
                 project_dir: Path,
                 experiment_name: str,
                 app_adapter: ApplicationAdapter,
                 num_repetitions: int = 1,
                 warmup_runs: int = 0,
                 use_slurm: Optional[bool] = None,
                 measure_compile_time: bool = True,
                 slurm_config: Optional[Dict[str, Any]] = None):
        """Initialize the experiment runner.

        Args:
            project_dir: Root directory of the project (usually IARA_DIR)
            experiment_name: Name of this experiment (e.g., "cholesky")
            app_adapter: Application-specific adapter implementation
            num_repetitions: Number of measurement runs per configuration
            warmup_runs: Number of warmup runs before measurements
            use_slurm: Whether to use SLURM (None = auto-detect based on hostname)
            measure_compile_time: Whether to measure compilation time
            slurm_config: Optional SLURM configuration dict
        """
        self.project_dir = Path(project_dir)
        self.experiment_name = experiment_name
        self.app_adapter = app_adapter

        # Set up directory structure
        self.experiment_dir = self.project_dir / "experiment" / experiment_name
        self.instances_dir = self.experiment_dir / "instances"
        self.results_dir = self.experiment_dir / "results"
        self.logs_dir = self.experiment_dir / "logs"
        self.build_metrics_dir = self.experiment_dir / "build_metrics"

        for d in [self.instances_dir, self.results_dir, self.logs_dir, self.build_metrics_dir]:
            d.mkdir(parents=True, exist_ok=True)

        self.num_repetitions = num_repetitions
        self.warmup_runs = warmup_runs
        self.measure_compile_time = measure_compile_time

        # Generate consistent timestamp for this run (will be used for all output files)
        self.run_timestamp = None  # Will be set when run_experiment() is called
        self.output_file = None
        self.build_metrics_file = None
        self.log_file = None
        self.log_handle = None

        # Set up SLURM
        hostname = socket.gethostname()
        if use_slurm is None:
            self.use_slurm = (hostname == "sorgan")
        else:
            self.use_slurm = use_slurm

        self.slurm_executor = SlurmExecutor(self._log, slurm_config)
        if self.use_slurm and not self.slurm_executor.is_available():
            self._log("WARNING: SLURM requested but not available, falling back to local execution")
            self.use_slurm = False

        # Set up command executor and metrics collector
        self.executor = CommandExecutor(self._log)
        self.metrics = MetricsCollector()

    def _log(self, message: str):
        """Write to log file and print to stdout."""
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        log_msg = f"[{timestamp}] {message}"
        print(log_msg)
        if self.log_handle:
            self.log_handle.write(log_msg + "\n")
            self.log_handle.flush()

    def build_configuration(self, config: ExperimentConfig) -> Optional[Dict[str, Any]]:
        """Build a single configuration. Returns build metrics on success, None on failure."""
        instance_dir = self.instances_dir / config.name
        instance_dir.mkdir(parents=True, exist_ok=True)

        self._log(f"Building {config.name}...")

        # Set up build environment
        build_env = os.environ.copy()
        build_env.update(self.app_adapter.get_build_env(config))
        build_env["IARA_DIR"] = str(self.project_dir)
        build_env["USE_CCACHE"] = "0" if self.measure_compile_time else "1"

        # Set iara-opt path if available
        main_iara_opt = self.project_dir / "build" / "bin" / "iara-opt"
        if main_iara_opt.exists():
            build_env["IARA_OPT_PATH"] = str(main_iara_opt)

        # Clean old build artifacts
        build_dir = instance_dir / "build"
        if build_dir.exists():
            for old_file in build_dir.rglob("a.out"):
                old_file.unlink()
            for obj_file in build_dir.rglob("*.o"):
                obj_file.unlink()

        build_dir.mkdir(parents=True, exist_ok=True)

        # Configure CMake
        cmake_args = ["cmake", str(self.project_dir)] + self.app_adapter.get_cmake_args(config)
        exit_code, stdout, stderr = self.executor.run(cmake_args, cwd=build_dir, env=build_env)
        if exit_code != 0:
            self._log(f"ERROR: CMake configure failed for {config.name}")
            self._log(f"  Stderr: {stderr}")
            return None

        # Build
        target = self.app_adapter.get_build_target(config)
        compile_start = time.time()
        build_cmd = ["make", "-j", str(os.cpu_count()), target]
        exit_code, stdout, stderr = self.executor.run(build_cmd, cwd=build_dir, env=build_env)
        compile_time = time.time() - compile_start

        if exit_code != 0:
            self._log(f"ERROR: Build failed for {config.name}")
            self._log(f"  Exit code: {exit_code}")
            self._log(f"  Stderr: {stderr}")
            return None

        # Verify executable exists
        executable = self.app_adapter.get_executable_path(config, build_dir)
        if not executable.exists():
            self._log(f"ERROR: Executable not found for {config.name} at {executable}")
            return None

        # Collect build metrics
        build_metrics = {"config_name": config.name}
        if self.measure_compile_time:
            build_metrics["compile_time"] = compile_time

        self._log(f"✓ Successfully built {config.name}" +
                 (f" (compile time: {compile_time:.2f}s)" if self.measure_compile_time else ""))

        return build_metrics

    def run_single_measurement(self, config: ExperimentConfig, run_number: int) -> Optional[Dict[str, Any]]:
        """Run a single measurement locally. Returns result dict or None on failure."""
        instance_dir = self.instances_dir / config.name
        build_dir = instance_dir / "build"
        executable = self.app_adapter.get_executable_path(config, build_dir)

        if not executable.exists():
            self._log(f"ERROR: Executable not found at {executable}")
            return None

        # Prepare command with time wrapper
        run_cmd = self.app_adapter.get_run_command(executable)
        time_cmd = ["/usr/bin/time", "-f", "%M %e"] + run_cmd

        start_time = time.time()
        exit_code, stdout, stderr = self.executor.run(time_cmd, cwd=executable.parent)
        elapsed_real = time.time() - start_time

        self._log(f"=== Measurement run {run_number} for {config.name} ===")
        self._log(f"[stdout]\n{stdout}")
        self._log(f"[stderr]\n{stderr}")

        if exit_code != 0:
            self._log(f"ERROR: Run {run_number} failed (exit code: {exit_code})")
            return None

        # Parse application-specific output
        app_metrics = self.app_adapter.parse_program_output(stdout, stderr)
        if app_metrics is None:
            self._log(f"ERROR: Failed to parse output for {config.name}")
            return None

        # Parse time metrics
        time_metrics = self.metrics.parse_time_output(stderr)

        # Combine all metrics
        result = {
            **config.to_dict(),
            **app_metrics,
            "real_time": elapsed_real,
            **time_metrics,
            "run_number": run_number,
            "timestamp": datetime.now().isoformat(),
        }

        return result

    def run_single_measurement_slurm(self, config: ExperimentConfig,
                                    run_number: int, is_warmup: bool = False) -> Optional[Dict[str, Any]]:
        """Run a single measurement via SLURM."""
        instance_dir = self.instances_dir / config.name
        build_dir = instance_dir / "build"
        executable = self.app_adapter.get_executable_path(config, build_dir)

        # Set up SLURM job
        slurm_dir = instance_dir / "slurm_jobs"
        slurm_dir.mkdir(exist_ok=True)

        run_type = "warmup" if is_warmup else "run"
        script_path = slurm_dir / f"{run_type}_{run_number}.sh"
        output_file = slurm_dir / f"{run_type}_{run_number}.out"
        job_name = f"{self.experiment_name}_{config.name}_{run_type}{run_number}"

        self.slurm_executor.create_script(
            script_path, executable, executable.parent, output_file, job_name
        )

        # Submit and wait
        job_id = self.slurm_executor.submit_job(script_path)
        if job_id is None:
            return None

        self._log(f"  Submitted SLURM job {job_id} for {config.name} run {run_number}")

        if not self.slurm_executor.wait_for_job(job_id):
            return None

        # Parse output
        if not output_file.exists():
            self._log(f"ERROR: SLURM output file not found: {output_file}")
            return None

        content = output_file.read_text()
        self._log(f"=== SLURM measurement output for {config.name} run {run_number} ===\n{content}")

        # Parse application output
        app_metrics = self.app_adapter.parse_program_output(content, content)
        if app_metrics is None:
            return None

        time_metrics = self.metrics.parse_time_output(content)

        result = {
            **config.to_dict(),
            **app_metrics,
            **time_metrics,
            "run_number": run_number,
            "timestamp": datetime.now().isoformat(),
        }

        return result

    def submit_slurm_jobs_for_config(self, config: ExperimentConfig) -> List[PendingJob]:
        """Submit all SLURM jobs for a configuration and return pending jobs list.

        This allows jobs to run in parallel with subsequent builds.
        """
        pending_jobs = []

        self._log(f"Submitting SLURM jobs for {config.name} ({self.warmup_runs} warmup + {self.num_repetitions} measured runs)...")

        # Submit warmup runs
        for i in range(self.warmup_runs):
            job = self._submit_single_slurm_job(config, -(i+1), is_warmup=True)
            if job:
                pending_jobs.append(job)

        # Submit measured runs
        for i in range(self.num_repetitions):
            job = self._submit_single_slurm_job(config, i, is_warmup=False)
            if job:
                pending_jobs.append(job)

        self._log(f"Submitted {len(pending_jobs)} jobs for {config.name}")
        return pending_jobs

    def _submit_single_slurm_job(self, config: ExperimentConfig,
                                 run_number: int, is_warmup: bool) -> Optional[PendingJob]:
        """Submit a single SLURM job without waiting. Returns PendingJob or None on failure."""
        instance_dir = self.instances_dir / config.name
        build_dir = instance_dir / "build"
        executable = self.app_adapter.get_executable_path(config, build_dir)

        # Set up SLURM job
        slurm_dir = instance_dir / "slurm_jobs"
        slurm_dir.mkdir(exist_ok=True)

        run_type = "warmup" if is_warmup else "run"
        script_path = slurm_dir / f"{run_type}_{run_number}.sh"
        output_file = slurm_dir / f"{run_type}_{run_number}.out"
        job_name = f"{self.experiment_name}_{config.name}_{run_type}{run_number}"

        self.slurm_executor.create_script(
            script_path, executable, executable.parent, output_file, job_name
        )

        # Submit job
        job_id = self.slurm_executor.submit_job(script_path)
        if job_id is None:
            return None

        self._log(f"  Submitted SLURM job {job_id} for {config.name} {run_type} {run_number}")

        return PendingJob(
            job_id=job_id,
            config=config,
            run_number=run_number,
            is_warmup=is_warmup,
            output_file=output_file
        )

    def collect_slurm_results(self, pending_jobs: List[PendingJob]) -> List[Dict[str, Any]]:
        """Wait for and collect results from pending SLURM jobs."""
        results = []

        self._log(f"Collecting results from {len(pending_jobs)} pending SLURM jobs...")

        for job in pending_jobs:
            # Wait for job to complete
            if not self.slurm_executor.wait_for_job(job.job_id):
                self._log(f"WARNING: Job {job.job_id} failed for {job.config.name}")
                continue

            # Parse output
            if not job.output_file.exists():
                self._log(f"ERROR: SLURM output file not found: {job.output_file}")
                continue

            content = job.output_file.read_text()
            self._log(f"=== SLURM measurement output for {job.config.name} run {job.run_number} ===\n{content}")

            # Parse application output
            app_metrics = self.app_adapter.parse_program_output(content, content)
            if app_metrics is None:
                self._log(f"WARNING: Failed to parse output for {job.config.name} run {job.run_number}")
                continue

            time_metrics = self.metrics.parse_time_output(content)

            result = {
                **job.config.to_dict(),
                **app_metrics,
                **time_metrics,
                "run_number": job.run_number,
                "timestamp": datetime.now().isoformat(),
            }

            # Only add to results if not a warmup run
            if not job.is_warmup:
                results.append(result)
                if "wall_time" in result:
                    self._log(f"  {job.config.name} run {job.run_number}: {result['wall_time']:.6f}s")

        return results

    def run_configuration(self, config: ExperimentConfig) -> List[Dict[str, Any]]:
        """Run all repetitions for a configuration."""
        results = []

        self._log(f"Running {config.name} ({self.warmup_runs} warmup + {self.num_repetitions} measured runs)...")

        # Warmup runs
        for i in range(self.warmup_runs):
            if self.use_slurm:
                result = self.run_single_measurement_slurm(config, -(i+1), is_warmup=True)
            else:
                result = self.run_single_measurement(config, -(i+1))

            if result is None:
                self._log(f"WARNING: Warmup run {i+1} failed")

        # Measured runs
        for i in range(self.num_repetitions):
            if self.use_slurm:
                result = self.run_single_measurement_slurm(config, i, is_warmup=False)
            else:
                result = self.run_single_measurement(config, i)

            if result is not None:
                results.append(result)
                # Log primary metric if available
                if "wall_time" in result:
                    self._log(f"  Run {i+1}/{self.num_repetitions}: {result['wall_time']:.6f}s")
            else:
                self._log(f"WARNING: Measurement run {i+1} failed")

        if len(results) < self.num_repetitions * 0.5:
            self._log(f"ERROR: Too many failures for {config.name} ({len(results)}/{self.num_repetitions} succeeded)")

        return results

    def save_results(self, results: List[Dict[str, Any]]):
        """Append results to CSV file."""
        if not results:
            return

        file_exists = self.output_file.exists()
        with open(self.output_file, 'a', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=results[0].keys())
            if not file_exists:
                writer.writeheader()
            writer.writerows(results)

        self._log(f"Saved {len(results)} results to {self.output_file}")

    def save_build_metrics(self, build_metrics: List[Dict[str, Any]]):
        """Save build metrics to CSV file."""
        if not build_metrics:
            return

        file_exists = self.build_metrics_file.exists()
        with open(self.build_metrics_file, 'a', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=build_metrics[0].keys())
            if not file_exists:
                writer.writeheader()
            writer.writerows(build_metrics)

        self._log(f"Saved build metrics to {self.build_metrics_file}")

    def save_metadata(self, configs: List[ExperimentConfig], extra_metadata: Optional[Dict[str, Any]] = None):
        """Save experiment metadata to JSON file."""
        metadata = {
            "timestamp": datetime.now().isoformat(),
            "run_timestamp": self.run_timestamp,  # Add run timestamp for correlation
            "experiment_name": self.experiment_name,
            "project_dir": str(self.project_dir),
            "num_repetitions": self.num_repetitions,
            "warmup_runs": self.warmup_runs,
            "num_configurations": len(configs),
            "configurations": [c.to_dict() for c in configs],
        }

        # Add git commit if available
        try:
            result = subprocess.run(
                ["git", "rev-parse", "HEAD"],
                cwd=self.project_dir,
                capture_output=True,
                text=True,
                timeout=5
            )
            if result.returncode == 0:
                metadata["git_commit"] = result.stdout.strip()
        except:
            pass

        # Add extra metadata
        if extra_metadata:
            metadata.update(extra_metadata)

        metadata_file = self.experiment_dir / "metadata.json"
        with open(metadata_file, 'w') as f:
            json.dump(metadata, f, indent=2)

        self._log(f"Saved metadata to {metadata_file}")

    def run_experiment(self, configs: List[ExperimentConfig],
                      skip_build: bool = False,
                      skip_run: bool = False,
                      extra_metadata: Optional[Dict[str, Any]] = None):
        """Run full experiment for all configurations.

        Args:
            configs: List of configurations to build and run
            skip_build: If True, skip build phase (assume binaries exist)
            skip_run: If True, skip run phase (only build)
            extra_metadata: Optional extra metadata to save to metadata.json
        """
        # Generate timestamp for this run (used for all output files)
        if self.run_timestamp is None:
            self.run_timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            self.output_file = self.results_dir / f"results_{self.run_timestamp}.csv"
            self.build_metrics_file = self.build_metrics_dir / f"build_metrics_{self.run_timestamp}.csv"
            self.log_file = self.logs_dir / f"experiment_{self.run_timestamp}.log"
            self.log_handle = open(self.log_file, 'a')

        # Log initial info
        self._log(f"=== Experiment '{self.experiment_name}' started at {datetime.now()} ===")
        self._log(f"Run timestamp: {self.run_timestamp}")
        hostname = socket.gethostname()
        self._log(f"Hostname: {hostname}")
        self._log(f"SLURM execution: {'enabled' if self.use_slurm else 'disabled'}")
        self._log(f"Compile time measurement: {'enabled' if self.measure_compile_time else 'disabled'}")

        self._log(f"Starting experiment with {len(configs)} configurations")
        self._log(f"Repetitions: {self.num_repetitions}, Warmup: {self.warmup_runs}")

        self.save_metadata(configs, extra_metadata)

        # Build phase (and submit SLURM jobs if using SLURM)
        build_metrics_list = []
        all_pending_jobs = []

        if not skip_build:
            self._log("\n=== BUILD PHASE ===")
            build_failures = []

            # Create progress bar for builds
            with tqdm(total=len(configs), desc="Building", unit="config",
                     disable=not HAS_TQDM, leave=True,
                     bar_format='{l_bar}{bar}| {n_fmt}/{total_fmt} [{elapsed}<{remaining}]') as pbar:

                for i, config in enumerate(configs, 1):
                    pbar.set_description(f"Building {config.name}")
                    self._log(f"\nBuilding {i}/{len(configs)}: {config.name}")

                    build_metrics = self.build_configuration(config)
                    if build_metrics is None:
                        build_failures.append(config)
                        pbar.set_description(f"Building {config.name} (FAILED)")
                    else:
                        build_metrics_list.append(build_metrics)

                        # If using SLURM and not skipping runs, submit jobs immediately
                        if self.use_slurm and not skip_run:
                            pending_jobs = self.submit_slurm_jobs_for_config(config)
                            all_pending_jobs.extend(pending_jobs)

                        pbar.set_description(f"Building {config.name} (done)")

                    pbar.update(1)

            if build_metrics_list:
                self.save_build_metrics(build_metrics_list)

            if build_failures:
                self._log(f"\nWARNING: {len(build_failures)} configurations failed to build")
                configs = [c for c in configs if c not in build_failures]

                if not configs:
                    self._log("ERROR: All builds failed. Aborting experiment.")
                    return
        else:
            self._log("Skipping build phase (--skip-build)")

        # Run phase
        if not skip_run:
            if self.use_slurm:
                # If using SLURM and we built, jobs are already submitted
                if not skip_build:
                    self._log("\n=== COLLECTING SLURM RESULTS ===")
                    self._log(f"Waiting for {len(all_pending_jobs)} SLURM jobs to complete...")

                    # Group jobs by configuration
                    config_jobs = {}
                    for job in all_pending_jobs:
                        if job.config.name not in config_jobs:
                            config_jobs[job.config.name] = []
                        config_jobs[job.config.name].append(job)

                    # Collect results for each configuration with progress bar
                    with tqdm(total=len(config_jobs), desc="Collecting results", unit="config",
                             disable=not HAS_TQDM, leave=True,
                             bar_format='{l_bar}{bar}| {n_fmt}/{total_fmt} [{elapsed}<{remaining}]') as pbar:

                        for config_name, jobs in config_jobs.items():
                            pbar.set_description(f"Collecting {config_name}")
                            self._log(f"\nCollecting results for {config_name}")
                            results = self.collect_slurm_results(jobs)
                            if results:
                                self.save_results(results)
                            pbar.update(1)
                else:
                    # Build was skipped, submit and wait for jobs now
                    self._log("\n=== RUN PHASE (SLURM) ===")

                    with tqdm(total=len(configs), desc="Running", unit="config",
                             disable=not HAS_TQDM, leave=True,
                             bar_format='{l_bar}{bar}| {n_fmt}/{total_fmt} [{elapsed}<{remaining}]') as pbar:

                        for i, config in enumerate(configs, 1):
                            pbar.set_description(f"Running {config.name}")
                            self._log(f"\nSubmitting and running {i}/{len(configs)}: {config.name}")
                            pending_jobs = self.submit_slurm_jobs_for_config(config)
                            results = self.collect_slurm_results(pending_jobs)
                            if results:
                                self.save_results(results)
                            pbar.update(1)
            else:
                # Local execution (not using SLURM)
                self._log("\n=== RUN PHASE ===")

                with tqdm(total=len(configs), desc="Running", unit="config",
                         disable=not HAS_TQDM, leave=True,
                         bar_format='{l_bar}{bar}| {n_fmt}/{total_fmt} [{elapsed}<{remaining}]') as pbar:

                    for i, config in enumerate(configs, 1):
                        pbar.set_description(f"Running {config.name}")
                        self._log(f"\nRunning {i}/{len(configs)}: {config.name}")
                        results = self.run_configuration(config)
                        if results:
                            self.save_results(results)
                        pbar.update(1)
        else:
            self._log("Skipping run phase (--skip-run)")

        self._log("\n=== Experiment completed ===")

    def run_visualization(self, scheduler_order: Optional[List[str]] = None) -> Optional[Path]:
        """Generate visualizations and create a notebook with the results.

        Args:
            scheduler_order: Optional list of schedulers in desired plot order

        Returns:
            Path to images directory, or None if visualization fails
        """
        try:
            import sys
            import shutil

            # Add experiment directory to path to import modules
            experiment_parent = self.project_dir / "experiment"
            if str(experiment_parent) not in sys.path:
                sys.path.insert(0, str(experiment_parent))

            from visualize_experiment import visualize_experiment
            from generate_notebook import create_visualization_notebook

            self._log("\n=== GENERATING VISUALIZATIONS ===")

            # Generate visualizations
            images_dir = visualize_experiment(self.experiment_dir, scheduler_order)

            # Get list of generated images
            image_files = [f.name for f in images_dir.glob("*.png")]

            # Generate notebook with embedded images
            self._log("Generating results notebook...")
            notebook_path = create_visualization_notebook(
                self.experiment_dir,
                self.run_timestamp,
                images_dir,
                image_files
            )
            self._log(f"Generated results notebook: {notebook_path}")

            # Also save backup of visualize_results.ipynb if it exists
            source_notebook = self.experiment_dir / "visualize_results.ipynb"
            if source_notebook.exists():
                notebooks_dir = self.experiment_dir / "notebooks"
                notebooks_dir.mkdir(exist_ok=True)

                backup_path = notebooks_dir / f"visualize_results_{self.run_timestamp}.ipynb"
                shutil.copy(source_notebook, backup_path)
                self._log(f"Saved notebook backup to: {backup_path}")

            self._log(f"Visualizations complete! Images saved to: {images_dir}")
            return images_dir

        except Exception as e:
            self._log(f"WARNING: Visualization failed: {e}")
            import traceback
            self._log(traceback.format_exc())
            return None

    def close(self):
        """Close the log file handle."""
        if hasattr(self, 'log_handle') and self.log_handle and not self.log_handle.closed:
            self.log_handle.close()
