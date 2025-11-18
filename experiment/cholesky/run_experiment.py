#!/usr/bin/env python3
"""
Robust experiment runner for Cholesky decomposition benchmarks.

This script:
- Builds all test configurations
- Runs experiments with multiple repetitions
- Validates outputs and handles errors
- Saves results incrementally with metadata
- Provides progress tracking and error reporting
- Supports SLURM execution on compute nodes (when running on sorgan)
"""

import os
import sys
import subprocess
import json
import csv
import time
import socket
from pathlib import Path
from datetime import datetime
from typing import List, Dict, Any, Optional
import argparse


class ExperimentConfig:
    """Configuration for a single experiment instance."""
    
    def __init__(self, matrix_size: int, num_blocks: int, scheduler: str):
        self.matrix_size = matrix_size
        self.num_blocks = num_blocks
        self.scheduler = scheduler
        self.block_size = matrix_size // num_blocks
        self.block_memory = self.block_size * self.block_size * 8  # 8 bytes per double
    
    @property
    def name(self) -> str:
        """Unique name for this configuration."""
        return f"{self.matrix_size}_{self.num_blocks}_{self.scheduler}"
    
    def __repr__(self) -> str:
        return f"Config({self.name})"


class ExperimentRunner:
    """Manages building and running experiments."""
    
    def __init__(self,
                 iara_dir: str,
                 num_repetitions: int = 1,
                 warmup_runs: int = 0,
                 use_slurm: bool = None,
                 measure_compile_time: bool = True):
        self.iara_dir = Path(iara_dir)
        self.experiment_dir = self.iara_dir / "experiment" / "cholesky"
        self.instances_dir = self.experiment_dir / "instances"
        # Create the standard experiment subfolders under experiment/cholesky
        # so we can store results, logs and build metrics in tidy locations.
        self.results_dir = self.experiment_dir / "results"
        self.logs_dir = self.experiment_dir / "logs"
        self.build_metrics_dir = self.experiment_dir / "build_metrics"
        # Ensure the directories exist
        self.results_dir.mkdir(parents=True, exist_ok=True)
        self.logs_dir.mkdir(parents=True, exist_ok=True)
        self.build_metrics_dir.mkdir(parents=True, exist_ok=True)
        self.sources_dir = self.iara_dir / "test" / "Iara" / "05-cholesky"

        self.num_repetitions = num_repetitions
        self.warmup_runs = warmup_runs
        # Add datetime suffix to output CSVs
        dt_str = datetime.now().strftime("%Y%m%d_%H%M%S")
        self.output_file = self.results_dir / f"results_{dt_str}.csv"
        self.build_metrics_file = self.build_metrics_dir / f"build_metrics_{dt_str}.csv"
        self.log_file = self.logs_dir / f"experiment_{dt_str}.log"

        # Always measure compile time
        self.measure_compile_time = True

        # Determine if we should use SLURM
        hostname = socket.gethostname()
        if use_slurm is None:
            # Auto-detect: use SLURM if we're on sorgan
            self.use_slurm = (hostname == "sorgan")
        else:
            self.use_slurm = use_slurm

        if self.use_slurm:
            # Verify SLURM is available
            if not self._check_slurm_available():
                print("WARNING: SLURM requested but not available, falling back to local execution")
                self.use_slurm = False

        # Create instances directory
        self.instances_dir.mkdir(parents=True, exist_ok=True)

        # Initialize log file
        self.log_handle = open(self.log_file, 'a')
        self._log(f"=== Experiment started at {datetime.now()} ===")
        self._log(f"Hostname: {hostname}")
        self._log(f"SLURM execution: {'enabled' if self.use_slurm else 'disabled'}")
        self._log(f"Compile time measurement: enabled")
    
    def _log(self, message: str):
        """Write to log file and print to stdout."""
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        log_msg = f"[{timestamp}] {message}"
        print(log_msg)
        self.log_handle.write(log_msg + "\n")
        self.log_handle.flush()
    
    def _check_slurm_available(self) -> bool:
        """Check if SLURM commands are available."""
        try:
            result = subprocess.run(["which", "sbatch"], capture_output=True, timeout=5)
            return result.returncode == 0
        except:
            return False
    
    def _run_command(self, cmd: List[str], cwd: Optional[Path] = None, 
                     env: Optional[Dict[str, str]] = None) -> tuple[int, str, str]:
        """Run a command and return (exit_code, stdout, stderr)."""
        try:
            result = subprocess.run(
                cmd,
                cwd=cwd,
                env=env,
                capture_output=True,
                text=True,
                timeout=300  # 5 minute timeout
            )
            return result.returncode, result.stdout, result.stderr
        except subprocess.TimeoutExpired:
            self._log(f"ERROR: Command timed out: {' '.join(cmd)}")
            return -1, "", "Command timed out"
        except Exception as e:
            self._log(f"ERROR: Command failed: {e}")
            return -1, "", str(e)
    
    def build_configuration(self, config: ExperimentConfig) -> Optional[Dict[str, Any]]:
        """Build a single configuration. Returns build metrics on success, None on failure."""
        instance_dir = self.instances_dir / config.name
        instance_dir.mkdir(parents=True, exist_ok=True)
        

        self._log(f"Building {config.name}...")

        # Set up environment variables for the build
        build_env = os.environ.copy()
        build_env.update({
            "MATRIX_SIZE": str(config.matrix_size),
            "NUM_BLOCKS": str(config.num_blocks),
            "SCHEDULER_MODE": config.scheduler,
            "SCHEDULER_MODES": config.scheduler,
            "PATH_TO_TEST_BUILD_DIR": str(instance_dir),
            "PATH_TO_TEST_SOURCES": str(self.sources_dir),
            "CMAKE_BUILD_TYPE": "ExperimentSize",  # Use ExperimentSize for minimal binary size
        })
        # Inform CMake about the IARA directory and optionally iara-opt path
        build_env["IARA_DIR"] = str(self.iara_dir)
        main_iara_opt = self.iara_dir / "build" / "bin" / "iara-opt"
        if main_iara_opt.exists():
            build_env["IARA_OPT_PATH"] = str(main_iara_opt)

        # Enable ccache if not measuring compile time
        # if not self.measure_compile_time:
        build_env["USE_CCACHE"] = "0"

        # Remove old binary and object files before building
        scheduler_build_dir = instance_dir / f"build"
        if scheduler_build_dir.exists():
            old_binary = scheduler_build_dir / "a.out"
            if old_binary.exists():
                self._log(f"Removing old binary: {old_binary}")
                old_binary.unlink()
            # Remove all object files (*.o) in the build directory
            for obj_file in scheduler_build_dir.glob("*.o"):
                self._log(f"Removing object file: {obj_file}")
                obj_file.unlink()

        # Build using CMake in the instance directory
        # This allows each experiment instance to have its own build
        scheduler_build_dir = instance_dir / f"build"
        scheduler_build_dir.mkdir(parents=True, exist_ok=True)

        # Configure CMake in the scheduler build directory
        # Pass PATH_TO_TEST_BUILD_DIR to control where outputs go
        # Include the scheduler as a CMake variable so only the requested
        # scheduler's targets are generated in this build directory.
        cmake_cmd = ["cmake", str(self.iara_dir),
                     f"-DCMAKE_BUILD_TYPE={build_env['CMAKE_BUILD_TYPE']}",
                     f"-DSCHEDULER_MODES={config.scheduler}",
                     f"-DPATH_TO_TEST_BUILD_DIR={build_env['PATH_TO_TEST_BUILD_DIR']}",
                     f"-DPATH_TO_TEST_SOURCES={build_env['PATH_TO_TEST_SOURCES']}"]
        exit_code, stdout, stderr = self._run_command(cmake_cmd, cwd=scheduler_build_dir, env=build_env)
        if exit_code != 0:
            self._log(f"ERROR: CMake configure failed for {config.name}")
            self._log(f"  Stderr: {stderr}")
            return None

        # Build the specific target
        target_name = f"run-05-cholesky-{config.scheduler}"
        compile_start = time.time() if self.measure_compile_time else None
        build_cmd = ["make", "-j", str(os.cpu_count()), target_name]
        exit_code, stdout, stderr = self._run_command(build_cmd, cwd=scheduler_build_dir, env=build_env)
        compile_time = (time.time() - compile_start) if self.measure_compile_time else 0.0

        if exit_code != 0:
            self._log(f"ERROR: Build failed for {config.name}")
            self._log(f"  Exit code: {exit_code}")
            self._log(f"  Stderr: {stderr}")
            return None

        # Verify that executable was created
        # CMake builds it in the test subdirectory within our build directory
        executable = scheduler_build_dir / "test" / "Iara" / "05-cholesky" / f"build-{config.scheduler}" / "a.out"
        if not executable.exists():
            self._log(f"ERROR: Executable not found for {config.name} at {executable}")
            return None
        
        # Get binary size information using size command
        size_metrics = self._get_binary_size_info(executable)
        
        # Run smoke test (no arguments needed - scheduler is compiled in)
        smoke_cmd = [str(executable)]
        smoke_run_dir = executable.parent
        exit_code, stdout, stderr = self._run_command(smoke_cmd, cwd=smoke_run_dir)
        
        if exit_code != 0:
            self._log(f"ERROR: Smoke test failed for {config.name}")
            self._log(f"  Stderr: {stderr[:200]}")
            return None
        
        # Verify output contains expected fields
        if "Wall time:" not in stdout:
            self._log(f"ERROR: Invalid output from {config.name} - missing wall time")
            self._log(f"  Output: {stdout[:200]}")
            return None
        
        # Compile build metrics (only include compile_time if measured)
        build_metrics = {
            "config_name": config.name,
            **size_metrics
        }
        if self.measure_compile_time:
            build_metrics["compile_time"] = compile_time
        
        if self.measure_compile_time:
            self._log(f"✓ Successfully built {config.name} (compile time: {compile_time:.2f}s)")
        else:
            self._log(f"✓ Successfully built {config.name} (using ccache)")
        return build_metrics
    
    def _get_binary_size_info(self, executable: Path) -> Dict[str, int]:
        """Get binary section sizes using size command."""
        try:
            # Use size -A for detailed section information
            result = subprocess.run(
                ["size", "-A", str(executable)],
                capture_output=True,
                text=True,
                timeout=5
            )

            # Log full output of size command
            self._log(f"=== size -A output for {executable} ===\n[stdout]\n{result.stdout}\n[stderr]\n{result.stderr}")

            if result.returncode != 0:
                return {"text_size": 0, "data_size": 0, "bss_size": 0, "total_size": 0}

            # Parse output
            text_size = 0
            data_size = 0
            bss_size = 0

            for line in result.stdout.split('\n'):
                parts = line.split()
                if len(parts) >= 2:
                    section = parts[0]
                    try:
                        size = int(parts[1])
                        if section == '.text':
                            text_size = size
                        elif section == '.data':
                            data_size = size
                        elif section == '.bss':
                            bss_size = size
                    except (ValueError, IndexError):
                        continue

            # Also get total file size
            total_size = executable.stat().st_size

            return {
                "text_size": text_size,
                "data_size": data_size,
                "bss_size": bss_size,
                "total_size": total_size
            }
        except Exception as e:
            self._log(f"WARNING: Could not get binary size info: {e}")
            return {"text_size": 0, "data_size": 0, "bss_size": 0, "total_size": 0}
    
    def run_single_measurement(self, config: ExperimentConfig,
                               run_number: int) -> Optional[Dict[str, Any]]:
        """Run a single measurement. Returns result dict or None on failure."""
        instance_dir = self.instances_dir / config.name
        scheduler_build_dir = instance_dir / f"build"
        executable = scheduler_build_dir / "test" / "Iara" / "05-cholesky" / f"build-{config.scheduler}" / "a.out"

        if not executable.exists():
            self._log(f"ERROR: Executable not found for {config.name} at {executable}")
            return None
        
        # Use /usr/bin/time to measure max RSS
        # Format: %M = max RSS in KB, %e = elapsed real time
        time_cmd = ["/usr/bin/time", "-f", "%M %e", str(executable)]

        start_time = time.time()
        run_dir = executable.parent
        exit_code, stdout, stderr = self._run_command(time_cmd, cwd=run_dir)
        elapsed_real = time.time() - start_time

        # Log full output of measurement tool
        self._log(f"=== Measurement run {run_number} for {config.name} ===")
        self._log(f"[stdout]\n{stdout}")
        self._log(f"[stderr]\n{stderr}")

        if exit_code != 0:
            self._log(f"ERROR: Run {run_number} failed for {config.name} (exit code: {exit_code})")
            return None

        # Parse wall time from stdout (program output)
        wall_time = None
        for line in stdout.split('\n'):
            if line.startswith("Wall time:"):
                try:
                    wall_time = float(line.split(":")[-1].strip().split()[0])
                except (ValueError, IndexError) as e:
                    self._log(f"ERROR: Could not parse wall time from: {line}")
                    return None

        if wall_time is None:
            self._log(f"ERROR: No wall time found in output for {config.name}")
            return None
        
        # Parse max RSS from stderr (time output)
        max_rss_kb = 0
        time_elapsed = 0.0
        stderr_lines = stderr.strip().split('\n')
        if stderr_lines:
            last_line = stderr_lines[-1]
            parts = last_line.split()
            if len(parts) >= 2:
                try:
                    max_rss_kb = int(parts[0])
                    time_elapsed = float(parts[1])
                except (ValueError, IndexError):
                    self._log(f"WARNING: Could not parse time output: {last_line}")
        
        return {
            "matrix_size": config.matrix_size,
            "num_blocks": config.num_blocks,
            "block_size": config.block_size,
            "block_memory": config.block_memory,
            "scheduler": config.scheduler,
            "wall_time": wall_time,
            "real_time": elapsed_real,
            "max_rss_kb": max_rss_kb,
            "run_number": run_number,
            "timestamp": datetime.now().isoformat(),
        }
    
    def _create_slurm_script(self, config: ExperimentConfig, run_number: int, 
                            is_warmup: bool = False) -> Path:
        """Create a SLURM batch script for a single measurement run."""
        instance_dir = self.instances_dir / config.name
        scheduler_build_dir = instance_dir / f"build/test/Iara/05-cholesky/build-{config.scheduler}"
        executable = scheduler_build_dir / "a.out"
        
        # Create SLURM scripts directory
        slurm_dir = instance_dir / "slurm_jobs"
        slurm_dir.mkdir(exist_ok=True)
        
        run_type = "warmup" if is_warmup else "run"
        script_name = f"{run_type}_{run_number}.sh"
        script_path = slurm_dir / script_name
        output_file = slurm_dir / f"{run_type}_{run_number}.out"
        
        # SLURM script content
        script_content = f"""#!/bin/bash
#SBATCH --job-name=chol_{config.name}_{run_type}{run_number}
#SBATCH --output={output_file}
#SBATCH --error={output_file}
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=48
#SBATCH --exclusive
#SBATCH --partition=cpu
#SBATCH --nodelist=sorgan-cpu[2-3]
#SBATCH --time=00:10:00

# Log node information
echo "=== Job started on $(hostname) at $(date) ==="
echo "SLURM_JOB_ID: $SLURM_JOB_ID"
echo "SLURM_JOB_NODELIST: $SLURM_JOB_NODELIST"
echo "CPUs allocated: $SLURM_CPUS_PER_TASK"
echo ""

# Change to the build directory
cd {scheduler_build_dir}

# Run the executable with time measurement
/usr/bin/time -f "%M %e" {executable}

exit_code=$?

echo ""
echo "=== Job finished at $(date) with exit code $exit_code ==="
exit $exit_code
"""
        
        # Write script
        with open(script_path, 'w') as f:
            f.write(script_content)
        
        # Make executable
        script_path.chmod(0o755)
        
        return script_path
    
    def _submit_slurm_job(self, script_path: Path) -> Optional[str]:
        """Submit a SLURM job and return the job ID."""
        try:
            result = subprocess.run(
                ["sbatch", str(script_path)],
                capture_output=True,
                text=True,
                timeout=10
            )
            
            if result.returncode != 0:
                self._log(f"ERROR: sbatch failed: {result.stderr}")
                return None
            
            # Parse job ID from output like "Submitted batch job 12345"
            output = result.stdout.strip()
            if "Submitted batch job" in output:
                job_id = output.split()[-1]
                return job_id
            else:
                self._log(f"ERROR: Could not parse job ID from: {output}")
                return None
        except Exception as e:
            self._log(f"ERROR: Failed to submit SLURM job: {e}")
            return None
    
    def _wait_for_slurm_job(self, job_id: str, timeout: int = 600) -> bool:
        """Wait for a SLURM job to complete. Returns True if successful."""
        start_time = time.time()
        
        while time.time() - start_time < timeout:
            try:
                # Check job status with squeue
                result = subprocess.run(
                    ["squeue", "-j", job_id, "-h", "-o", "%T"],
                    capture_output=True,
                    text=True,
                    timeout=5
                )
                
                if result.returncode != 0 or not result.stdout.strip():
                    # Job not in queue anymore - it completed or failed
                    # Check with sacct to get final status
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
                            self._log(f"Job {job_id} finished with state: {state}")
                            return False
                    else:
                        # Job completed (no longer in sacct either means it finished a while ago)
                        return True
                
                # Job still running, wait a bit
                time.sleep(2)
                
            except Exception as e:
                self._log(f"ERROR checking job status: {e}")
                time.sleep(2)
        
        self._log(f"ERROR: Job {job_id} timed out after {timeout}s")
        return False
    
    def _parse_slurm_output(self, output_file: Path, config: ExperimentConfig, 
                           run_number: int) -> Optional[Dict[str, Any]]:
        """Parse the output file from a SLURM job."""
        if not output_file.exists():
            self._log(f"ERROR: SLURM output file not found: {output_file}")
            return None
        
        try:
            with open(output_file, 'r') as f:
                content = f.read()

            # Log full SLURM output file
            self._log(f"=== SLURM measurement output for {config.name} run {run_number} ===\n{content}")

            # Parse wall time from program output
            wall_time = None
            for line in content.split('\n'):
                if line.startswith("Wall time:"):
                    try:
                        wall_time = float(line.split(":")[-1].strip().split()[0])
                    except (ValueError, IndexError):
                        pass

            if wall_time is None:
                self._log(f"ERROR: Could not find wall time in {output_file}")
                return None

            # Parse time command output (last line with two numbers)
            max_rss_kb = 0
            real_time = 0.0
            lines = content.strip().split('\n')
            for line in reversed(lines):
                parts = line.split()
                if len(parts) >= 2:
                    try:
                        max_rss_kb = int(parts[0])
                        real_time = float(parts[1])
                        break
                    except (ValueError, IndexError):
                        continue

            return {
                "matrix_size": config.matrix_size,
                "num_blocks": config.num_blocks,
                "block_size": config.block_size,
                "block_memory": config.block_memory,
                "scheduler": config.scheduler,
                "wall_time": wall_time,
                "real_time": real_time,
                "max_rss_kb": max_rss_kb,
                "run_number": run_number,
                "timestamp": datetime.now().isoformat(),
            }
        except Exception as e:
            self._log(f"ERROR parsing SLURM output: {e}")
            return None
    
    def run_single_measurement_slurm(self, config: ExperimentConfig, 
                                    run_number: int, is_warmup: bool = False) -> Optional[Dict[str, Any]]:
        """Run a single measurement via SLURM. Returns result dict or None on failure."""
        # Create SLURM script
        script_path = self._create_slurm_script(config, run_number, is_warmup)
        
        # Submit job
        job_id = self._submit_slurm_job(script_path)
        if job_id is None:
            return None
        
        self._log(f"  Submitted SLURM job {job_id} for {config.name} run {run_number}")
        
        # Wait for completion
        if not self._wait_for_slurm_job(job_id):
            return None
        
        # Parse output
        instance_dir = self.instances_dir / config.name
        slurm_dir = instance_dir / "slurm_jobs"
        run_type = "warmup" if is_warmup else "run"
        output_file = slurm_dir / f"{run_type}_{run_number}.out"
        
        return self._parse_slurm_output(output_file, config, run_number)
    
    def run_configuration(self, config: ExperimentConfig) -> List[Dict[str, Any]]:
        """Run all repetitions for a configuration. Returns list of results."""
        results = []
        
        self._log(f"Running {config.name} ({self.warmup_runs} warmup + {self.num_repetitions} measured runs)...")
        
        if self.use_slurm:
            self._log(f"  Using SLURM execution on compute nodes")
        
        # Warmup runs
        for i in range(self.warmup_runs):
            if self.use_slurm:
                result = self.run_single_measurement_slurm(config, run_number=-(i+1), is_warmup=True)
            else:
                result = self.run_single_measurement(config, run_number=-(i+1))
            
            if result is None:
                self._log(f"WARNING: Warmup run {i+1} failed for {config.name}")
        
        # Measured runs
        for i in range(self.num_repetitions):
            if self.use_slurm:
                result = self.run_single_measurement_slurm(config, run_number=i, is_warmup=False)
            else:
                result = self.run_single_measurement(config, run_number=i)
            
            if result is not None:
                results.append(result)
                self._log(f"  Run {i+1}/{self.num_repetitions}: {result['wall_time']:.6f}s (max RSS: {result['max_rss_kb']/1024:.1f} MB)")
            else:
                self._log(f"WARNING: Measurement run {i+1} failed for {config.name}")
        
        if len(results) < self.num_repetitions * 0.5:  # Less than 50% succeeded
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
        """Save build metrics to separate CSV file."""
        if not build_metrics:
            return

        file_exists = self.build_metrics_file.exists()

        with open(self.build_metrics_file, 'a', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=build_metrics[0].keys())
            if not file_exists:
                writer.writeheader()
            writer.writerows(build_metrics)

        self._log(f"Saved build metrics to {self.build_metrics_file}")
    
    def save_metadata(self, configs: List[ExperimentConfig]):
        """Save experiment metadata."""
        metadata = {
            "timestamp": datetime.now().isoformat(),
            "iara_dir": str(self.iara_dir),
            "num_repetitions": self.num_repetitions,
            "warmup_runs": self.warmup_runs,
            "num_configurations": len(configs),
            "configurations": [
                {
                    "matrix_size": c.matrix_size,
                    "num_blocks": c.num_blocks,
                    "scheduler": c.scheduler,
                }
                for c in configs
            ],
        }
        
        # Try to get git commit
        try:
            result = subprocess.run(
                ["git", "rev-parse", "HEAD"],
                cwd=self.iara_dir,
                capture_output=True,
                text=True,
                timeout=5
            )
            if result.returncode == 0:
                metadata["git_commit"] = result.stdout.strip()
        except:
            pass
        
        metadata_file = self.experiment_dir / "metadata.json"
        with open(metadata_file, 'w') as f:
            json.dump(metadata, f, indent=2)
        
        self._log(f"Saved metadata to {metadata_file}")
    
    def run_experiment(self, configs: List[ExperimentConfig],
                      skip_build: bool = False,
                      skip_run: bool = False):
        """Run full experiment for all configurations."""
        self._log(f"Starting experiment with {len(configs)} configurations")
        self._log(f"Repetitions: {self.num_repetitions}, Warmup: {self.warmup_runs}")

        self.save_metadata(configs)

        # Build phase
        build_metrics_list = []
        if not skip_build:
            self._log("\n=== BUILD PHASE ===")
            build_failures = []
            for i, config in enumerate(configs, 1):
                self._log(f"\nBuilding {i}/{len(configs)}: {config.name}")
                build_metrics = self.build_configuration(config)
                if build_metrics is None:
                    build_failures.append(config)
                else:
                    build_metrics_list.append(build_metrics)

            # Save build metrics
            if build_metrics_list:
                self.save_build_metrics(build_metrics_list)

            if build_failures:
                self._log(f"\nWARNING: {len(build_failures)} configurations failed to build:")
                for config in build_failures:
                    self._log(f"  - {config.name}")

                # Remove failed configs from run list
                configs = [c for c in configs if c not in build_failures]

                if not configs:
                    self._log("ERROR: All builds failed. Aborting experiment.")
                    return
        else:
            self._log("Skipping build phase (--skip-build)")

        # Run phase
        if not skip_run:
            self._log("\n=== RUN PHASE ===")
            for i, config in enumerate(configs, 1):
                self._log(f"\nRunning {i}/{len(configs)}: {config.name}")
                results = self.run_configuration(config)

                # Save incrementally (resilient to crashes)
                if results:
                    self.save_results(results)
        else:
            self._log("Skipping run phase (--skip-run)")

        self._log("\n=== Experiment completed ===")
        self.log_handle.close()


def main():
    parser = argparse.ArgumentParser(description="Run Cholesky decomposition experiments")
    parser.add_argument("--matrix-sizes", type=int, nargs='+', 
                       default=[2048],
                       help="Matrix sizes to test")
    parser.add_argument("--num-blocks", type=int, nargs='+',
                       default=[1],
                       help="Number of blocks to test")
    parser.add_argument("--schedulers", type=str, nargs='+',
                       default=["sequential", "omp-task", "virtual-fifo"],
                       help="Schedulers to test")
    parser.add_argument("--repetitions", type=int, default=1,
                       help="Number of measurement repetitions per configuration")
    parser.add_argument("--warmup", type=int, default=0,
                       help="Number of warmup runs per configuration")
    parser.add_argument("--output", type=str, default="results.csv",
                       help="Output CSV file")
    parser.add_argument("--skip-build", action="store_true",
                       help="Skip build phase (assume binaries exist)")
    parser.add_argument("--skip-run", action="store_true",
                       help="Skip execution phase (only build binaries)")
    parser.add_argument("--subset", type=str, choices=["small", "medium", "large"],
                       help="Run predefined subset of tests")
    parser.add_argument("--slurm", type=str, choices=["auto", "yes", "no"], default="auto",
                       help="Use SLURM for running experiments (auto: use if on sorgan)")
    parser.add_argument("--measure-compile-time", action="store_true",
                       help="Measure compilation time (disables ccache for accurate timing)")
    
    args = parser.parse_args()
    
    # Check environment
    iara_dir = os.environ.get("IARA_DIR")
    if not iara_dir:
        print("ERROR: IARA_DIR environment variable not set", file=sys.stderr)
        print("Please source .env first", file=sys.stderr)
        sys.exit(1)
    
    # Override with subset if specified
    if args.subset == "small":
        args.matrix_sizes = [256, 1024]
        args.num_blocks = [1, 4]
        args.repetitions = 5
    elif args.subset == "medium":
        args.matrix_sizes = [2048, 4096, 8192]
        args.num_blocks = [1, 2, 4, 8]
        args.repetitions = 10
    elif args.subset == "large":
        # Use defaults
        pass
    
    # Determine SLURM usage
    use_slurm = None
    if args.slurm == "yes":
        use_slurm = True
    elif args.slurm == "no":
        use_slurm = False
    # else auto (None)
    
    # Generate all configurations
    configs = []
    for matrix_size in args.matrix_sizes:
        for num_blocks in args.num_blocks:
            for scheduler in args.schedulers:
                configs.append(ExperimentConfig(matrix_size, num_blocks, scheduler))
    
    print(f"Generated {len(configs)} configurations")
    
    # Run experiment
    runner = ExperimentRunner(
        iara_dir=iara_dir,
        num_repetitions=args.repetitions,
        warmup_runs=args.warmup,
        use_slurm=use_slurm,
        measure_compile_time=args.measure_compile_time
    )
    
    runner.run_experiment(configs, skip_build=args.skip_build, skip_run=args.skip_run)


if __name__ == "__main__":
    main()
