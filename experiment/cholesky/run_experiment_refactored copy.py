#!/usr/bin/env python3
"""
Cholesky decomposition experiment runner.

Uses the experiment_framework library with Cholesky-specific configuration.
"""

import os
import sys
from pathlib import Path
from typing import List, Dict, Any, Optional
import argparse

# Add parent directory to path to import experiment_framework
sys.path.insert(0, str(Path(__file__).parent.parent))

from experiment_framework import (
    ExperimentConfig,
    ApplicationAdapter,
    ExperimentRunner
)


# ============================================================================
# Cholesky-specific adapter
# ============================================================================

class CholeskyAdapter(ApplicationAdapter):
    """Adapter for Cholesky decomposition experiments."""

    def __init__(self, sources_dir: Path):
        """Initialize adapter.

        Args:
            sources_dir: Path to the Cholesky test sources directory
        """
        self.sources_dir = sources_dir

    def get_build_env(self, config: ExperimentConfig) -> Dict[str, str]:
        """Return environment variables for building Cholesky config."""
        params = config.params
        return {
            "MATRIX_SIZE": str(params["matrix_size"]),
            "NUM_BLOCKS": str(params["num_blocks"]),
            "SCHEDULER_MODE": params["scheduler"],
            "SCHEDULER_MODES": params["scheduler"],
            "PATH_TO_TEST_BUILD_DIR": str(Path(".")),  # Will be set to instance_dir
            "PATH_TO_TEST_SOURCES": str(self.sources_dir),
            "CMAKE_BUILD_TYPE": "ExperimentSize",
        }

    def get_cmake_args(self, config: ExperimentConfig) -> List[str]:
        """Return CMake arguments for Cholesky."""
        params = config.params
        return [
            f"-DCMAKE_BUILD_TYPE=ExperimentSize",
            f"-DSCHEDULER_MODES={params['scheduler']}",
            f"-DPATH_TO_TEST_SOURCES={self.sources_dir}",
        ]

    def get_build_target(self, config: ExperimentConfig) -> str:
        """Return the CMake target to build."""
        return f"run-05-cholesky-{config.params['scheduler']}"

    def get_executable_path(self, config: ExperimentConfig, build_dir: Path) -> Path:
        """Return path to the Cholesky executable."""
        scheduler = config.params['scheduler']
        return build_dir / "test" / "Iara" / "05-cholesky" / f"build-{scheduler}" / "a.out"

    def parse_program_output(self, stdout: str, stderr: str) -> Optional[Dict[str, Any]]:
        """Parse Cholesky program output for wall time."""
        wall_time = None
        for line in stdout.split('\n'):
            if line.startswith("Wall time:"):
                try:
                    wall_time = float(line.split(":")[-1].strip().split()[0])
                except (ValueError, IndexError):
                    return None

        if wall_time is None:
            return None

        return {"wall_time": wall_time}

    def get_run_command(self, executable: Path) -> List[str]:
        """Return command to run Cholesky (no arguments needed)."""
        return [str(executable)]


# ============================================================================
# Configuration factory
# ============================================================================

def create_cholesky_config(matrix_size: int, num_blocks: int, scheduler: str) -> ExperimentConfig:
    """Factory function for creating Cholesky configurations.

    Args:
        matrix_size: Size of the matrix (e.g., 2048)
        num_blocks: Number of blocks to divide the matrix into
        scheduler: Scheduler type (e.g., "sequential", "omp-task", "virtual-fifo")

    Returns:
        ExperimentConfig with Cholesky-specific parameters
    """
    name = f"{matrix_size}_{num_blocks}_{scheduler}"
    params = {
        "matrix_size": matrix_size,
        "num_blocks": num_blocks,
        "scheduler": scheduler,
        "block_size": matrix_size // num_blocks,
        "block_memory": (matrix_size // num_blocks) ** 2 * 8,  # 8 bytes per double
    }
    return ExperimentConfig(name=name, params=params)


# ============================================================================
# CLI
# ============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="Run Cholesky decomposition experiments",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Run with default settings
  %(prog)s

  # Run specific configurations
  %(prog)s --matrix-sizes 2048 4096 --num-blocks 1 2 4 --schedulers sequential omp-task

  # Run with many repetitions and warmup
  %(prog)s --repetitions 10 --warmup 2

  # Only build, don't run
  %(prog)s --skip-run

  # Use SLURM for execution
  %(prog)s --slurm yes
        """
    )

    # Configuration parameters
    parser.add_argument("--matrix-sizes", type=int, nargs='+', default=[2048],
                       help="Matrix sizes to test (default: 2048)")
    parser.add_argument("--num-blocks", type=int, nargs='+', default=[1],
                       help="Number of blocks to test (default: 1)")
    parser.add_argument("--schedulers", type=str, nargs='+',
                       default=["sequential", "omp-task", "virtual-fifo"],
                       help="Schedulers to test (default: sequential omp-task virtual-fifo)")

    # Experiment parameters
    parser.add_argument("--repetitions", type=int, default=1,
                       help="Number of measurement repetitions per configuration (default: 1)")
    parser.add_argument("--warmup", type=int, default=0,
                       help="Number of warmup runs per configuration (default: 0)")

    # Control flow
    parser.add_argument("--skip-build", action="store_true",
                       help="Skip build phase (assume binaries exist)")
    parser.add_argument("--skip-run", action="store_true",
                       help="Skip execution phase (only build binaries)")

    # Execution mode
    parser.add_argument("--slurm", type=str, choices=["auto", "yes", "no"], default="auto",
                       help="Use SLURM for running experiments (default: auto, uses SLURM if on sorgan)")
    parser.add_argument("--measure-compile-time", action="store_true",
                       help="Measure compilation time (disables ccache for accurate timing)")

    args = parser.parse_args()

    # Check environment
    iara_dir = os.environ.get("IARA_DIR")
    if not iara_dir:
        print("ERROR: IARA_DIR environment variable not set", file=sys.stderr)
        print("Please source .env first", file=sys.stderr)
        sys.exit(1)

    project_dir = Path(iara_dir)
    sources_dir = project_dir / "test" / "Iara" / "05-cholesky"

    # Validate sources directory
    if not sources_dir.exists():
        print(f"ERROR: Sources directory not found: {sources_dir}", file=sys.stderr)
        sys.exit(1)

    # Generate configurations
    configs = [
        create_cholesky_config(matrix_size, num_blocks, scheduler)
        for matrix_size in args.matrix_sizes
        for num_blocks in args.num_blocks
        for scheduler in args.schedulers
    ]

    print(f"Generated {len(configs)} configurations:")
    for config in configs:
        print(f"  - {config.name}")

    # Set up adapter and runner
    adapter = CholeskyAdapter(sources_dir)

    use_slurm = None if args.slurm == "auto" else (args.slurm == "yes")

    runner = ExperimentRunner(
        project_dir=project_dir,
        experiment_name="cholesky",
        app_adapter=adapter,
        num_repetitions=args.repetitions,
        warmup_runs=args.warmup,
        use_slurm=use_slurm,
        measure_compile_time=args.measure_compile_time
    )

    # Run experiment
    runner.run_experiment(
        configs,
        skip_build=args.skip_build,
        skip_run=args.skip_run
    )

    # Generate visualizations automatically after experiment completes
    if not args.skip_run:
        runner.run_visualization(scheduler_order=args.schedulers)

    # Close the log file
    runner.close()


if __name__ == "__main__":
    main()
