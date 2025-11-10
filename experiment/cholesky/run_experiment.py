#!/usr/bin/env python3
"""
Robust experiment runner for Cholesky decomposition benchmarks.

This script:
- Builds all test configurations
- Runs experiments with multiple repetitions
- Validates outputs and handles errors
- Saves results incrementally with metadata
- Provides progress tracking and error reporting
"""

import os
import sys
import subprocess
import json
import csv
import time
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
                 num_repetitions: int = 10,
                 warmup_runs: int = 2,
                 output_file: str = "results.csv",
                 log_file: str = "experiment.log"):
        self.iara_dir = Path(iara_dir)
        self.experiment_dir = self.iara_dir / "experiment" / "cholesky"
        self.instances_dir = self.experiment_dir / "instances"
        self.sources_dir = self.iara_dir / "test" / "Iara" / "05-cholesky"
        
        self.num_repetitions = num_repetitions
        self.warmup_runs = warmup_runs
        self.output_file = self.experiment_dir / output_file
        self.log_file = self.experiment_dir / log_file
        
        # Create instances directory
        self.instances_dir.mkdir(parents=True, exist_ok=True)
        
        # Initialize log file
        self.log_handle = open(self.log_file, 'a')
        self._log(f"=== Experiment started at {datetime.now()} ===")
    
    def _log(self, message: str):
        """Write to log file and print to stdout."""
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        log_msg = f"[{timestamp}] {message}"
        print(log_msg)
        self.log_handle.write(log_msg + "\n")
        self.log_handle.flush()
    
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
    
    def build_configuration(self, config: ExperimentConfig) -> bool:
        """Build a single configuration. Returns True on success."""
        instance_dir = self.instances_dir / config.name
        instance_dir.mkdir(parents=True, exist_ok=True)
        
        self._log(f"Building {config.name}...")
        
        # Set up environment variables for the build
        build_env = os.environ.copy()
        build_env.update({
            "MATRIX_SIZE": str(config.matrix_size),
            "NUM_BLOCKS": str(config.num_blocks),
            "SCHEDULER_MODE": config.scheduler,
            "PATH_TO_TEST_BUILD_DIR": str(instance_dir),
            "PATH_TO_TEST_SOURCES": str(self.sources_dir),
        })
        
        # Run build script
        build_script = self.iara_dir / "scripts" / "build-single-test.sh"
        cmd = [str(build_script), str(self.sources_dir), config.scheduler]
        
        exit_code, stdout, stderr = self._run_command(cmd, cwd=self.sources_dir, env=build_env)
        
        if exit_code != 0:
            self._log(f"ERROR: Build failed for {config.name}")
            self._log(f"  Exit code: {exit_code}")
            self._log(f"  Stderr: {stderr[:500]}")  # First 500 chars
            return False
        
        # Verify that executable was created
        # The build script creates build-{scheduler}/a.out under PATH_TO_TEST_BUILD_DIR
        scheduler_build_dir = instance_dir / f"build-{config.scheduler}"
        executable = scheduler_build_dir / "a.out"
        if not executable.exists():
            self._log(f"ERROR: Executable not found for {config.name} at {executable}")
            return False
        
        # Run smoke test
        smoke_cmd = [str(executable), f"--scheduler={config.scheduler}"]
        exit_code, stdout, stderr = self._run_command(smoke_cmd, cwd=scheduler_build_dir)
        
        if exit_code != 0:
            self._log(f"ERROR: Smoke test failed for {config.name}")
            self._log(f"  Stderr: {stderr[:200]}")
            return False
        
        # Verify output contains expected fields
        if "Wall time:" not in stdout:
            self._log(f"ERROR: Invalid output from {config.name} - missing wall time")
            self._log(f"  Output: {stdout[:200]}")
            return False
        
        self._log(f"✓ Successfully built {config.name}")
        return True
    
    def run_single_measurement(self, config: ExperimentConfig, 
                               run_number: int) -> Optional[Dict[str, Any]]:
        """Run a single measurement. Returns result dict or None on failure."""
        instance_dir = self.instances_dir / config.name
        scheduler_build_dir = instance_dir / f"build-{config.scheduler}"
        executable = scheduler_build_dir / "a.out"
        
        if not executable.exists():
            self._log(f"ERROR: Executable not found for {config.name} at {executable}")
            return None
        
        cmd = [str(executable), f"--scheduler={config.scheduler}"]
        
        start_time = time.time()
        exit_code, stdout, stderr = self._run_command(cmd, cwd=scheduler_build_dir)
        elapsed_real = time.time() - start_time
        
        if exit_code != 0:
            self._log(f"ERROR: Run {run_number} failed for {config.name} (exit code: {exit_code})")
            return None
        
        # Parse wall time from output
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
        
        return {
            "matrix_size": config.matrix_size,
            "num_blocks": config.num_blocks,
            "block_size": config.block_size,
            "block_memory": config.block_memory,
            "scheduler": config.scheduler,
            "wall_time": wall_time,
            "real_time": elapsed_real,
            "run_number": run_number,
            "timestamp": datetime.now().isoformat(),
        }
    
    def run_configuration(self, config: ExperimentConfig) -> List[Dict[str, Any]]:
        """Run all repetitions for a configuration. Returns list of results."""
        results = []
        
        self._log(f"Running {config.name} ({self.warmup_runs} warmup + {self.num_repetitions} measured runs)...")
        
        # Warmup runs
        for i in range(self.warmup_runs):
            result = self.run_single_measurement(config, run_number=-(i+1))
            if result is None:
                self._log(f"WARNING: Warmup run {i+1} failed for {config.name}")
        
        # Measured runs
        for i in range(self.num_repetitions):
            result = self.run_single_measurement(config, run_number=i)
            if result is not None:
                results.append(result)
                self._log(f"  Run {i+1}/{self.num_repetitions}: {result['wall_time']:.6f}s")
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
                      skip_build: bool = False):
        """Run full experiment for all configurations."""
        self._log(f"Starting experiment with {len(configs)} configurations")
        self._log(f"Repetitions: {self.num_repetitions}, Warmup: {self.warmup_runs}")
        
        self.save_metadata(configs)
        
        # Build phase
        if not skip_build:
            self._log("\n=== BUILD PHASE ===")
            build_failures = []
            for i, config in enumerate(configs, 1):
                self._log(f"\nBuilding {i}/{len(configs)}: {config.name}")
                if not self.build_configuration(config):
                    build_failures.append(config)
            
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
        self._log("\n=== RUN PHASE ===")
        for i, config in enumerate(configs, 1):
            self._log(f"\nRunning {i}/{len(configs)}: {config.name}")
            results = self.run_configuration(config)
            
            # Save incrementally (resilient to crashes)
            if results:
                self.save_results(results)
        
        self._log("\n=== Experiment completed ===")
        self.log_handle.close()


def main():
    parser = argparse.ArgumentParser(description="Run Cholesky decomposition experiments")
    parser.add_argument("--matrix-sizes", type=int, nargs='+', 
                       default=[2048, 4096, 6144, 8192, 10240, 12288, 14336, 16384],
                       help="Matrix sizes to test")
    parser.add_argument("--num-blocks", type=int, nargs='+',
                       default=[1, 2, 4, 8, 16, 32],
                       help="Number of blocks to test")
    parser.add_argument("--schedulers", type=str, nargs='+',
                       default=["sequential", "omp-task", "virtual-fifo"],
                       help="Schedulers to test")
    parser.add_argument("--repetitions", type=int, default=10,
                       help="Number of measurement repetitions per configuration")
    parser.add_argument("--warmup", type=int, default=2,
                       help="Number of warmup runs per configuration")
    parser.add_argument("--output", type=str, default="results.csv",
                       help="Output CSV file")
    parser.add_argument("--skip-build", action="store_true",
                       help="Skip build phase (assume binaries exist)")
    parser.add_argument("--subset", type=str, choices=["small", "medium", "large"],
                       help="Run predefined subset of tests")
    
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
        output_file=args.output
    )
    
    runner.run_experiment(configs, skip_build=args.skip_build)


if __name__ == "__main__":
    main()
