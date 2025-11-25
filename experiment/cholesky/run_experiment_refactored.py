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

    def __init__(self):
        """Initialize adapter.

        Note: The experiment CMakeLists.txt directly references source files
        via IARA_DIR environment variable, so we don't need to track paths here.
        """
        pass

    def get_build_env(self, config: ExperimentConfig) -> Dict[str, str]:
        """Return environment variables for building Cholesky config."""
        params = config.params
        # Map experiment scheduler names to build system test schedulers
        # Now using consistent naming:
        # - sequential, omp-for, omp-task, enkits-task: baseline schedulers
        # - vf-omp: Virtual FIFO with OpenMP backend
        # - vf-enkits: Virtual FIFO with EnkiTS backend
        scheduler_map = {
            "sequential": "sequential",
            "omp-for": "omp-for",
            "omp-task": "omp-task",
            "enkits-task": "enkits-task",
            "vf-omp": "vf-omp",
            "vf-enkits": "vf-enkits",
        }
        build_scheduler = scheduler_map.get(params["scheduler"], params["scheduler"])

        return {
            "MATRIX_SIZE": str(params["matrix_size"]),
            "NUM_BLOCKS": str(params["num_blocks"]),
            "SCHEDULER_MODE": build_scheduler,
        }

    def get_cmake_args(self, config: ExperimentConfig) -> List[str]:
        """Return CMake arguments for Cholesky.

        Uses the main IaRa CMakeLists.txt with IARA_BUILD_EXPERIMENTS=ON.
        """
        return [
            f"-DCMAKE_BUILD_TYPE=ExperimentProf",
            "-DIARA_BUILD_TESTS=OFF",  # Don't build tests
            "-DIARA_BUILD_EXPERIMENTS=ON",  # Build experiment instead
        ]

    def get_build_target(self, config: ExperimentConfig) -> str:
        """Return the CMake target to build."""
        # Build the cholesky experiment target
        return "cholesky-experiment"

    def get_executable_path(self, config: ExperimentConfig, build_dir: Path) -> Path:
        """Return path to the Cholesky executable."""
        # The experiment CMakeLists.txt puts the executable in cholesky-{scheduler}/a.out
        scheduler = config.params["scheduler"]
        return build_dir / f"cholesky-{scheduler}" / "a.out"

    def parse_program_output(self, stdout: str, stderr: str) -> Optional[Dict[str, Any]]:
        """Parse Cholesky program output for timing measurements."""
        wall_time = None
        init_time = None
        convert_time = None

        for line in stdout.split('\n'):
            if line.startswith("Wall time:"):
                try:
                    wall_time = float(line.split(":")[-1].strip().split()[0])
                except (ValueError, IndexError):
                    pass
            elif line.startswith("Initialization time:"):
                try:
                    init_time = float(line.split(":")[-1].strip().split()[0])
                except (ValueError, IndexError):
                    pass
            elif line.startswith("Block conversion time:"):
                try:
                    convert_time = float(line.split(":")[-1].strip().split()[0])
                except (ValueError, IndexError):
                    pass

        if wall_time is None:
            return None

        result = {"wall_time": wall_time}
        if init_time is not None:
            result["init_time"] = init_time
        if convert_time is not None:
            result["convert_time"] = convert_time

        return result

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
                       default=["sequential", "omp-task", "vf-omp"],
                       help="Schedulers to test (default: sequential omp-task vf-omp)")

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
    parser.add_argument("--measure-compile-time", action="store_true", default=True,
                       help="Measure compilation time (disables ccache for accurate timing, enabled by default)")
    parser.add_argument("--no-measure-compile-time", dest="measure_compile_time", action="store_false",
                       help="Disable compilation time measurement (enables ccache)")

    args = parser.parse_args()

    # Check environment
    iara_dir = os.environ.get("IARA_DIR")
    if not iara_dir:
        print("ERROR: IARA_DIR environment variable not set", file=sys.stderr)
        print("Please source .env first", file=sys.stderr)
        sys.exit(1)

    project_dir = Path(iara_dir)

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
    # Note: CholeskyAdapter no longer needs sources_dir since the experiment CMakeLists.txt
    # directly references application sources via IARA_DIR env var
    adapter = CholeskyAdapter()

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
