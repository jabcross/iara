"""
Command-line interface for IaRa Unified Experiment Framework.

This module provides the argparse scaffolding for all CLI commands:
- generate: Generate CMakeLists.txt from experiments.yaml
- build: Build instances with timing measurement
- execute: Execute instances and collect runtime measurements
- visualize: Generate Vega-Lite visualizations and notebooks
- run: Orchestrate full pipeline

Command logic is deferred to Phase 5.
"""

# Allow direct invocation (python3 cli.py ...) in addition to module invocation
# (python3 -m tools.experiment_framework ...) by fixing up __package__ so that
# relative imports inside the lazy-imported helper functions resolve correctly.
if __name__ == "__main__" and __package__ is None:
    import sys as _sys
    from pathlib import Path as _Path
    _pkg_root = str(_Path(__file__).resolve().parent.parent.parent)
    if _pkg_root not in _sys.path:
        _sys.path.insert(0, _pkg_root)
    __package__ = "tools.experiment_framework"

try:
    import argcomplete
    _ARGCOMPLETE = True
except ImportError:
    _ARGCOMPLETE = False
import argparse
import logging
import subprocess
import sys
from pathlib import Path
from typing import Optional
from datetime import datetime, timezone


def setup_logging(
    verbose: int, quiet: bool, log_file: Optional[str], use_default_log: bool = True
) -> Path:
    """
    Configure comprehensive logging to both stderr and file.

    Logs to stderr at a level determined by verbosity flags, and optionally
    logs to a file at DEBUG level for comprehensive debugging.

    Args:
        verbose: Verbosity level (0=normal/WARNING, 1=INFO, 2+=DEBUG)
        quiet: If True, suppress non-error output (level=ERROR)
        log_file: Optional explicit log file path. If None and use_default_log=True,
                  creates a default log file in logs/ directory
        use_default_log: If True and log_file is None, create default log file

    Returns:
        Path to the log file that was created/used, or Path("/dev/null") if no logging

    Examples:
        # Normal execution with default log file
        log_path = setup_logging(verbose=0, quiet=False, log_file=None)

        # Verbose with custom log file
        log_path = setup_logging(verbose=1, quiet=False, log_file="/tmp/my.log")

        # Quiet mode with default log file
        log_path = setup_logging(verbose=0, quiet=True, log_file=None)

        # No log file (only console)
        log_path = setup_logging(verbose=0, quiet=False, log_file=None, use_default_log=False)
    """
    # Determine console log level based on verbosity
    if quiet:
        console_level = logging.ERROR
    elif verbose == 0:
        console_level = logging.WARNING
    elif verbose == 1:
        console_level = logging.INFO
    else:  # verbose >= 2
        console_level = logging.DEBUG

    # Determine log file path
    log_file_path: Optional[Path] = None
    if log_file:
        log_file_path = Path(log_file)
    elif use_default_log:
        # Create default log file in logs/ directory
        log_dir = Path("logs")
        log_dir.mkdir(parents=True, exist_ok=True)
        timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
        log_file_path = log_dir / f"framework_{timestamp}.log"

    # Clear any existing handlers to avoid duplicates
    root_logger = logging.getLogger()
    for handler in root_logger.handlers[:]:
        root_logger.removeHandler(handler)

    # Set root logger to DEBUG to capture everything (handlers will filter)
    root_logger.setLevel(logging.DEBUG)

    # Create formatters
    console_format = logging.Formatter(
        "[%(levelname)s] %(name)s - %(message)s"
    )
    file_format = logging.Formatter(
        "%(asctime)s [%(levelname)8s] %(name)s - %(message)s",
        datefmt="%Y-%m-%d %H:%M:%S"
    )

    # Add stderr handler
    stderr_handler = logging.StreamHandler(sys.stderr)
    stderr_handler.setLevel(console_level)
    stderr_handler.setFormatter(console_format)
    root_logger.addHandler(stderr_handler)

    # Add file handler if log file specified
    if log_file_path:
        file_handler = logging.FileHandler(log_file_path, mode='w')
        file_handler.setLevel(logging.DEBUG)  # File always gets DEBUG level
        file_handler.setFormatter(file_format)
        root_logger.addHandler(file_handler)

        # Log startup info to file
        logger = logging.getLogger(__name__)
        logger.info("=" * 80)
        logger.info("IaRa Unified Experiment Framework - Logging Started")
        logger.info("=" * 80)
        logger.info(f"Console log level: {logging.getLevelName(console_level)}")
        logger.info(f"File log level: DEBUG")
        logger.info(f"Log file: {log_file_path.absolute()}")
        logger.info("=" * 80)

        return log_file_path

    return Path("/dev/null")


def run_generate(app: str, exp_set: str, app_dir: Path, yaml_path: Path) -> int:
    """
    Run Phase 1: Generate CMakeLists.txt.

    Args:
        app: Application name
        exp_set: Experiment set name
        app_dir: Application experiment directory
        yaml_path: Path to experiments.yaml

    Returns:
        0 on success, 2 on error
    """
    logger = logging.getLogger(__name__)

    from .generator import generate_cmakelists
    from .config import ConfigError

    output_path = app_dir / "CMakeLists.txt"

    # Validate YAML file exists
    if not yaml_path.exists():
        print(f"ERROR: YAML file not found: {yaml_path}", file=sys.stderr)
        logger.error(f"YAML file not found: {yaml_path}")
        return 2

    try:
        count = generate_cmakelists(yaml_path, output_path, exp_set)
        print(f"  ✓ Generated {count} instances")
        logger.info(f"Successfully generated {count} instances")
        return 0
    except ConfigError as e:
        print(f"ERROR: Configuration error: {e}", file=sys.stderr)
        logger.error(f"Configuration error: {e}")
        return 2
    except Exception as e:
        print(f"ERROR: Generation failed: {e}", file=sys.stderr)
        logger.error(f"Generation error: {e}", exc_info=True)
        return 2


def run_build(app: str, exp_set: str, app_dir: Path, yaml_path: Path) -> int:
    """
    Run Phase 2: Build all instances.

    Args:
        app: Application name
        exp_set: Experiment set name
        app_dir: Application experiment directory
        yaml_path: Path to experiments.yaml

    Returns:
        0 on success, 1 on partial failure, 2 on critical error
    """
    logger = logging.getLogger(__name__)

    from .builder import build_all_instances, write_build_results
    from .config import load_experiments_yaml, ConfigError

    # Load configuration
    try:
        config = load_experiments_yaml(yaml_path)
    except ConfigError as e:
        print(f"ERROR: Configuration error: {e}", file=sys.stderr)
        logger.error(f"Configuration error: {e}")
        return 2
    except Exception as e:
        print(f"ERROR: Failed to load configuration: {e}", file=sys.stderr)
        logger.error(f"Configuration load error: {e}", exc_info=True)
        return 2

    # Extract instance names from CMakeLists.txt
    cmake_path = app_dir / "CMakeLists.txt"
    if not cmake_path.exists():
        print(f"ERROR: CMakeLists.txt not found at {cmake_path}", file=sys.stderr)
        print("       Run generate command first", file=sys.stderr)
        logger.error(f"CMakeLists.txt not found: {cmake_path}")
        return 2

    try:
        # Parse instance names from CMakeLists.txt
        # Look for iara_add_test_instance calls with NAME parameter
        import re
        instances = []
        with open(cmake_path, 'r') as f:
            content = f.read()
            # Match NAME "instance_name" from iara_add_test_instance calls
            pattern = r'NAME\s+"([^"]+)"'
            instances = re.findall(pattern, content)

        if not instances:
            print("ERROR: No instances found in CMakeLists.txt", file=sys.stderr)
            logger.error("No instances found in CMakeLists.txt")
            return 2

        logger.info(f"Found {len(instances)} instances to build")

    except Exception as e:
        print(f"ERROR: Failed to parse CMakeLists.txt: {e}", file=sys.stderr)
        logger.error(f"CMakeLists.txt parse error: {e}", exc_info=True)
        return 2

    # Setup build directory
    build_dir = Path("build_experiments") / app
    cmake_source = app_dir

    # Build all instances
    try:
        build_results = build_all_instances(instances, build_dir, cmake_source)
    except Exception as e:
        print(f"ERROR: Build failed: {e}", file=sys.stderr)
        logger.error(f"Build error: {e}", exc_info=True)
        return 2

    # Write build results
    results_dir = app_dir / "results"
    try:
        write_build_results(build_results, results_dir, config, exp_set, app_dir)
    except Exception as e:
        print(f"ERROR: Failed to write build results: {e}", file=sys.stderr)
        logger.error(f"Write error: {e}", exc_info=True)
        return 2

    # Print summary
    total = len(build_results.successful_instances) + len(build_results.failed_instances)
    successful = len(build_results.successful_instances)
    print(f"  ✓ Built {successful}/{total} instances")

    if build_results.failed_instances:
        print(f"  ⚠ Warning: {len(build_results.failed_instances)} instance(s) failed to build", file=sys.stderr)
        logger.warning(f"{len(build_results.failed_instances)} instances failed")
        return 1

    return 0


def run_execute(app: str, exp_set: str, app_dir: Path, yaml_path: Path,
               repetitions: Optional[int] = None, timeout: Optional[int] = None) -> int:
    """
    Run Phase 3: Execute instances and collect measurements.

    Args:
        app: Application name
        exp_set: Experiment set name
        app_dir: Application experiment directory
        yaml_path: Path to experiments.yaml
        repetitions: Override YAML repetitions count (optional)
        timeout: Override YAML timeout in seconds (optional)

    Returns:
        0 on success, 1 on partial failure, 2 on critical error
    """
    logger = logging.getLogger(__name__)

    from .collector import collect_all_measurements
    from .builder import read_build_results, update_instance_execution, write_build_results
    from .config import load_experiments_yaml, ConfigError

    results_dir = app_dir / "results"

    # Find most recent results JSON
    if not results_dir.exists():
        print(f"ERROR: Results directory not found: {results_dir}", file=sys.stderr)
        print("       Run build command first", file=sys.stderr)
        logger.error(f"Results directory not found: {results_dir}")
        return 2

    json_files = sorted(results_dir.glob("results_*.json"))
    if not json_files:
        print(f"ERROR: No build results found in {results_dir}", file=sys.stderr)
        print("       Run build command first", file=sys.stderr)
        logger.error("No results JSON files found")
        return 2

    latest_json = json_files[-1]
    logger.info(f"Using results file: {latest_json}")

    # Load configuration and build results
    try:
        config = load_experiments_yaml(yaml_path)
        build_results_obj = read_build_results(latest_json)

        # Convert to dict for collect_all_measurements
        build_results_dict = {
            'successful_instances': [
                {
                    'instance_name': inst.instance_name,
                    'compilation': inst.compilation,
                    'binary_size_bytes': inst.binary_size_bytes,
                    'execution': inst.execution,
                    'executable_path': inst.executable_path
                }
                for inst in build_results_obj.successful_instances
            ],
            'failed_instances': [
                {
                    'instance_name': inst.instance_name,
                    'errors': inst.errors
                }
                for inst in build_results_obj.failed_instances
            ]
        }
    except Exception as e:
        print(f"ERROR: Failed to load configuration/results: {e}", file=sys.stderr)
        logger.error(f"Load error: {e}", exc_info=True)
        return 2

    # Check for successful builds
    if not build_results_obj.successful_instances:
        print("ERROR: No successful builds to execute", file=sys.stderr)
        logger.error("No successful instances to execute")
        return 2

    # Execute all instances
    try:
        collection_results = collect_all_measurements(
            build_results_dict, config,
            repetitions=repetitions,
            timeout=timeout
        )
    except Exception as e:
        print(f"ERROR: Execution failed: {e}", file=sys.stderr)
        logger.error(f"Execution error: {e}", exc_info=True)
        return 2

    # Update build results with execution data
    updated_count = 0
    for exec_result in collection_results['execution_results']:
        instance_name = exec_result['instance_name']
        try:
            update_instance_execution(build_results_obj, instance_name, exec_result)
            updated_count += 1
        except ValueError as e:
            logger.warning(f"Could not update instance {instance_name}: {e}")

    # Write updated results
    try:
        write_build_results(build_results_obj, results_dir, config, exp_set, app_dir)
    except Exception as e:
        print(f"ERROR: Failed to write results: {e}", file=sys.stderr)
        logger.error(f"Write error: {e}", exc_info=True)
        return 2

    # Print summary
    summary = collection_results['summary']
    print(f"  ✓ Executed {summary['executed']} instances")

    if summary['skipped'] > 0:
        print(f"  ⚠ Warning: Skipped {summary['skipped']} instance(s)", file=sys.stderr)
        logger.warning(f"Skipped {summary['skipped']} instances")

    if summary['errors'] > 0:
        print(f"  ⚠ Warning: {summary['errors']} instance(s) had errors", file=sys.stderr)
        logger.warning(f"{summary['errors']} instances had errors")
        return 1

    return 0


def run_visualize(app: str, exp_set: str, app_dir: Path, yaml_path: Path,
                  results_json_path: Optional[Path] = None) -> int:
    """
    Run Phase 4: Generate visualizations and notebook.

    Args:
        app: Application name
        exp_set: Experiment set name
        app_dir: Application experiment directory
        yaml_path: Path to experiments.yaml
        results_json_path: Explicit results JSON path (optional; defaults to latest in results/)

    Returns:
        0 on success, 2 on critical error
    """
    logger = logging.getLogger(__name__)

    from .visualizer import generate_vegalite_json
    from .notebook import generate_notebook, execute_notebook
    from .config import load_experiments_yaml, ConfigError
    from datetime import datetime, timezone

    results_dir = app_dir / "results"

    # Resolve results JSON path
    if results_json_path is not None:
        if not results_json_path.exists():
            print(f"ERROR: Specified results file not found: {results_json_path}", file=sys.stderr)
            logger.error(f"Results file not found: {results_json_path}")
            return 2
        results_json = results_json_path
    else:
        if not results_dir.exists():
            print(f"ERROR: Results directory not found: {results_dir}", file=sys.stderr)
            logger.error(f"Results directory not found: {results_dir}")
            return 2
        json_files = sorted(results_dir.glob("results_*.json"))
        if not json_files:
            print(f"ERROR: No results found in {results_dir}", file=sys.stderr)
            logger.error("No results JSON files found")
            return 2
        results_json = json_files[-1]

    logger.info(f"Using results file: {results_json}")

    # Load configuration
    try:
        config = load_experiments_yaml(yaml_path)
    except Exception as e:
        print(f"ERROR: Failed to load configuration: {e}", file=sys.stderr)
        logger.error(f"Configuration load error: {e}", exc_info=True)
        return 2

    # Generate Vega-Lite plots
    vegalite_files = []
    if "visualization" in config and "plots" in config.get("visualization", {}):
        try:
            vegalite_files = generate_vegalite_json(config, results_json, results_dir)
            print(f"  ✓ Generated {len(vegalite_files)} plot(s)")
            logger.info(f"Generated {len(vegalite_files)} Vega-Lite files")
        except Exception as e:
            print(f"ERROR: Visualization generation failed: {e}", file=sys.stderr)
            logger.error(f"Visualization error: {e}", exc_info=True)
            return 2

    # Generate Jupyter notebook
    try:
        timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
        notebook_path = results_dir / f"analysis_{timestamp}.ipynb"

        # Pass absolute paths for notebook to use
        generate_notebook(config, results_json, vegalite_files, notebook_path)

        print(f"  ✓ Generated notebook: {notebook_path.name}")
        logger.info(f"Generated notebook: {notebook_path}")
    except Exception as e:
        print(f"ERROR: Notebook generation failed: {e}", file=sys.stderr)
        logger.error(f"Notebook generation error: {e}", exc_info=True)
        return 2

    # Execute notebook (non-critical)
    try:
        execute_notebook(notebook_path)
        print(f"  ✓ Executed notebook")
        logger.info("Notebook execution completed")
    except FileNotFoundError as e:
        logger.warning(f"Jupyter not available: {e}")
    except Exception as e:
        logger.warning(f"Notebook execution failed: {e}")

    return 0


def validate_hash(yaml_path: Path, app_dir: Path) -> int:
    """
    Validate that YAML hash matches stored hash.

    Args:
        yaml_path: Path to experiments.yaml
        app_dir: Application experiment directory

    Returns:
        0 if match, 3 if mismatch, 2 if hash file missing
    """
    logger = logging.getLogger(__name__)

    from .config import compute_yaml_hash

    hash_file = app_dir / ".experiments.yaml.hash"

    if not hash_file.exists():
        print(f"ERROR: Hash file not found: {hash_file}", file=sys.stderr)
        print("       Run generate command first (without --skip-generate)", file=sys.stderr)
        logger.error(f"Hash file not found: {hash_file}")
        return 2

    try:
        # Read stored hash
        with open(hash_file, 'r') as f:
            stored_hash = f.read().strip()

        # Compute current hash
        current_hash = compute_yaml_hash(yaml_path)

        if stored_hash != current_hash:
            print(f"ERROR: YAML hash mismatch - experiments.yaml has changed", file=sys.stderr)
            print(f"       Stored:  {stored_hash}", file=sys.stderr)
            print(f"       Current: {current_hash}", file=sys.stderr)
            print("       Run without --skip-generate to regenerate CMakeLists.txt", file=sys.stderr)
            logger.error("YAML hash mismatch")
            return 3

        logger.info("YAML hash validation passed")
        return 0

    except Exception as e:
        print(f"ERROR: Hash validation failed: {e}", file=sys.stderr)
        logger.error(f"Hash validation error: {e}", exc_info=True)
        return 2


def _iter_app_sets(app_type_filter: str, set_filter: Optional[str] = None):
    """Yield (app_name, set_name, exp_dir, yaml_path) for all apps matching app_type_filter."""
    from .config import load_experiments_yaml, ConfigError
    apps_dir = Path("applications")
    if not apps_dir.exists():
        return
    for app_dir in sorted(apps_dir.iterdir()):
        if not app_dir.is_dir():
            continue
        yaml_path = app_dir / "experiment" / "experiments.yaml"
        if not yaml_path.exists():
            continue
        try:
            config = load_experiments_yaml(yaml_path)
        except ConfigError as e:
            print(f"WARNING: Skipping {app_dir.name}: {e}", file=sys.stderr)
            continue
        if config.get("application", {}).get("app_type", "") != app_type_filter:
            continue
        exp_dir = app_dir / "experiment"
        for exp_set in config.get("experiment_sets", []):
            set_name = exp_set["name"]
            if set_filter and set_name != set_filter:
                continue
            yield app_dir.name, set_name, exp_dir, yaml_path


def _batch_summary(results: list) -> int:
    """Print pass/fail summary. Returns 0 if all passed, 1 if any failed."""
    if not results:
        print("No matching apps/sets found.", file=sys.stderr)
        return 1
    passed = sum(1 for _, _, rc in results if rc == 0)
    failed = sum(1 for _, _, rc in results if rc != 0)
    print("\n=== Summary ===")
    for app_name, set_name, rc in results:
        status = "PASS" if rc == 0 else "FAIL"
        print(f"  [{status}] {app_name} / {set_name}")
    print(f"\n{passed} passed, {failed} failed")
    return 0 if failed == 0 else 1


def _run_pipeline(app: str, exp_set: str, app_dir: Path, yaml_path: Path, args) -> int:
    """Run the full pipeline (generate→build→execute→visualize) for one (app, set)."""
    onetime_script = Path("applications") / app / "onetime.sh"
    if onetime_script.exists():
        try:
            subprocess.run(["sh", str(onetime_script)], check=True)
        except subprocess.CalledProcessError as e:
            logging.getLogger(__name__).error(f"One-time setup script failed: {e}")
            return 2

    if not args.skip_generate:
        rc = run_generate(app, exp_set, app_dir, yaml_path)
        if rc != 0:
            return rc
    else:
        rc = validate_hash(yaml_path, app_dir)
        if rc != 0:
            return rc

    if not args.skip_build:
        rc = run_build(app, exp_set, app_dir, yaml_path)
        if rc == 2:
            return 2

    if not args.skip_execute:
        rc = run_execute(app, exp_set, app_dir, yaml_path)
        if rc == 2:
            return 2

    return run_visualize(app, exp_set, app_dir, yaml_path)


def _list_available_apps() -> list:
    """Return sorted list of application names that have experiments.yaml."""
    apps_dir = Path("applications")
    if not apps_dir.exists():
        return []
    return sorted(
        d.name for d in apps_dir.iterdir()
        if d.is_dir() and (d / "experiment" / "experiments.yaml").exists()
    )


def _list_available_sets(yaml_path: Path) -> list:
    """Return list of experiment set names from a YAML config file."""
    try:
        from .config import load_experiments_yaml
        config = load_experiments_yaml(yaml_path)
        return [s["name"] for s in config.get("experiment_sets", [])]
    except Exception:
        return []


def main() -> int:
    """
    Main entry point for the CLI.

    Returns:
        Exit code (0=success, 1=partial failure, 2=critical failure)
    """
    parser = argparse.ArgumentParser(
        prog="python3 -m tools.experiment_framework",
        description="IaRa Unified Experiment Framework - Declarative experiment management",
    )

    # Global options
    parser.add_argument(
        "-v", "--verbose",
        action="count",
        default=0,
        help="Increase output verbosity (-v for verbose, -vv for very verbose)",
    )

    parser.add_argument(
        "-q", "--quiet",
        action="store_true",
        help="Suppress non-error output",
    )

    parser.add_argument(
        "--log",
        type=str,
        metavar="FILE",
        help="Write detailed log to specific file (default: logs/framework_{timestamp}.log)",
    )

    parser.add_argument(
        "--no-log-file",
        action="store_true",
        help="Disable default log file creation (only log to stderr)",
    )

    # Subcommands
    subparsers = parser.add_subparsers(dest="command", help="Available commands")

    # ============================================================================
    # generate command
    # ============================================================================
    generate_parser = subparsers.add_parser(
        "generate",
        help="Generate CMakeLists.txt from experiments.yaml",
    )

    generate_parser.add_argument(
        "--app",
        type=str,
        nargs="?",
        metavar="NAME",
        help="Application name (e.g., '05-cholesky')",
    )

    generate_parser.add_argument(
        "--set",
        type=str,
        nargs="?",
        metavar="NAME",
        help="Experiment set name (e.g., 'regression')",
    )

    generate_parser.add_argument(
        "--app_type",
        type=str,
        metavar="TYPE",
        help="Run for all apps of this type (e.g., 'regression_test', 'experiment')",
    )

    # ============================================================================
    # build command
    # ============================================================================
    build_parser = subparsers.add_parser(
        "build",
        help="Build instances with timing measurement",
    )

    build_parser.add_argument(
        "--app",
        type=str,
        nargs="?",
        metavar="NAME",
        help="Application name (e.g., '05-cholesky')",
    )

    build_parser.add_argument(
        "--set",
        type=str,
        nargs="?",
        metavar="NAME",
        help="Experiment set name (e.g., 'regression')",
    )

    build_parser.add_argument(
        "--app_type",
        type=str,
        metavar="TYPE",
        help="Run for all apps of this type (e.g., 'regression_test', 'experiment')",
    )

    build_parser.add_argument(
        "--clean",
        type=bool,
        default=True,
        metavar="BOOL",
        help="Force clean build (default: true)",
    )

    # ============================================================================
    # execute command
    # ============================================================================
    execute_parser = subparsers.add_parser(
        "execute",
        help="Execute instances and collect runtime measurements",
    )

    execute_parser.add_argument(
        "--app",
        type=str,
        nargs="?",
        metavar="NAME",
        help="Application name (e.g., '05-cholesky')",
    )

    execute_parser.add_argument(
        "--set",
        type=str,
        nargs="?",
        metavar="NAME",
        help="Experiment set name (e.g., 'regression')",
    )

    execute_parser.add_argument(
        "--app_type",
        type=str,
        metavar="TYPE",
        help="Run for all apps of this type (e.g., 'regression_test', 'experiment')",
    )

    execute_parser.add_argument(
        "--repetitions",
        type=int,
        metavar="N",
        help="Number of execution repetitions (default: from YAML)",
    )

    execute_parser.add_argument(
        "--timeout",
        type=int,
        metavar="SECONDS",
        help="Execution timeout in seconds (default: from YAML)",
    )

    # ============================================================================
    # visualize command
    # ============================================================================
    visualize_parser = subparsers.add_parser(
        "visualize",
        help="Generate visualizations and Jupyter notebook",
    )

    visualize_parser.add_argument(
        "--app",
        type=str,
        nargs="?",
        metavar="NAME",
        help="Application name (e.g., '05-cholesky')",
    )

    visualize_parser.add_argument(
        "--set",
        type=str,
        nargs="?",
        metavar="NAME",
        help="Experiment set name (e.g., 'regression')",
    )

    visualize_parser.add_argument(
        "--app_type",
        type=str,
        metavar="TYPE",
        help="Run for all apps of this type (e.g., 'regression_test', 'experiment')",
    )

    visualize_parser.add_argument(
        "--results",
        type=str,
        metavar="PATH",
        help="Path to results JSON file (default: latest in results/)",
    )

    # ============================================================================
    # clean command
    # ============================================================================
    clean_parser = subparsers.add_parser(
        "clean",
        help="Delete all built instances (build_experiments/)",
    )

    clean_parser.add_argument(
        "--app",
        type=str,
        nargs="?",
        metavar="NAME",
        help="Limit to a specific application (default: all apps)",
    )

    # ============================================================================
    # run command (orchestrator)
    # ============================================================================
    run_parser = subparsers.add_parser(
        "run",
        help="Orchestrate full pipeline: generate → build → execute → visualize",
    )

    run_parser.add_argument(
        "--app",
        type=str,
        nargs="?",
        metavar="NAME",
        help="Application name (e.g., '05-cholesky')",
    )

    run_parser.add_argument(
        "--set",
        type=str,
        nargs="?",
        metavar="NAME",
        help="Experiment set name (e.g., 'regression')",
    )

    run_parser.add_argument(
        "--app_type",
        type=str,
        metavar="TYPE",
        help="Run for all apps of this type (e.g., 'regression_test', 'experiment')",
    )

    run_parser.add_argument(
        "--skip-generate",
        action="store_true",
        help="Skip generation if CMakeLists.txt exists",
    )

    run_parser.add_argument(
        "--skip-build",
        action="store_true",
        help="Use existing binaries (skip build phase)",
    )

    run_parser.add_argument(
        "--skip-execute",
        action="store_true",
        help="Use existing measurements (skip execution)",
    )

    # Register completers and activate argcomplete (no-op if not installed)
    if _ARGCOMPLETE:
        def _app_completer(prefix, **kw):
            return _list_available_apps()

        def _set_completer(prefix, parsed_args, **kw):
            app = getattr(parsed_args, 'app', None)
            if not app:
                return []
            return _list_available_sets(
                Path("applications") / app / "experiment" / "experiments.yaml")

        for _sp in [generate_parser, build_parser, execute_parser, visualize_parser, run_parser, clean_parser]:
            for _action in _sp._actions:
                if _action.dest == 'app':
                    _action.completer = _app_completer
                elif _action.dest == 'set':
                    _action.completer = _set_completer

        argcomplete.autocomplete(parser)

    # Parse arguments
    args = parser.parse_args()

    # Setup logging with default log file creation
    use_default_log = not getattr(args, 'no_log_file', False)
    log_file_path = setup_logging(args.verbose, args.quiet, args.log, use_default_log=use_default_log)

    logger = logging.getLogger(__name__)

    # Notify user about log file location
    if log_file_path != Path("/dev/null"):
        print(f"Log file: {log_file_path}", file=sys.stderr)

    # Handle no command
    if args.command is None:
        parser.print_help()
        return 0

    logger.info(f"Command: {args.command}")

    # ============================================================================
    # Validate --app and --set for commands that require them
    # ============================================================================
    if args.command in ("generate", "build", "execute", "visualize", "run"):
        app_type = getattr(args, 'app_type', None)

        if args.app and app_type:
            print("ERROR: --app and --app_type are mutually exclusive.", file=sys.stderr)
            return 2

        if not app_type:
            # Single app/set mode — validate app and set
            if not args.app:
                available = _list_available_apps()
                print("Available applications:", file=sys.stderr)
                for a in available:
                    print(f"  {a}", file=sys.stderr)
                return 2

            _yaml_path = Path("applications") / args.app / "experiment" / "experiments.yaml"
            if not _yaml_path.exists():
                print(f"ERROR: Application '{args.app}' not found.", file=sys.stderr)
                available = _list_available_apps()
                if available:
                    print("Available applications:", file=sys.stderr)
                    for a in available:
                        print(f"  {a}", file=sys.stderr)
                return 2

            available_sets = _list_available_sets(_yaml_path)
            if not args.set:
                print(f"Available sets for '{args.app}':", file=sys.stderr)
                for s in available_sets:
                    print(f"  {s}", file=sys.stderr)
                return 2

            if args.set not in available_sets:
                print(f"ERROR: Experiment set '{args.set}' not found in '{args.app}'.", file=sys.stderr)
                if available_sets:
                    print("Available sets:", file=sys.stderr)
                    for s in available_sets:
                        print(f"  {s}", file=sys.stderr)
                return 2

    # ============================================================================
    # generate command
    # ============================================================================
    if args.command == "generate":
        if getattr(args, 'app_type', None):
            results = []
            for app_name, set_name, exp_dir, yaml_path in _iter_app_sets(args.app_type, args.set):
                print(f"\n=== {app_name} / {set_name} ===")
                rc = run_generate(app_name, set_name, exp_dir, yaml_path)
                results.append((app_name, set_name, rc))
            return _batch_summary(results)

        logger.info(f"Application: {args.app}")
        logger.info(f"Experiment set: {args.set}")

        from .generator import generate_cmakelists
        from .config import ConfigError

        app_dir = Path("applications") / args.app / "experiment"
        yaml_path = app_dir / "experiments.yaml"
        output_path = app_dir / "CMakeLists.txt"

        try:
            count = generate_cmakelists(yaml_path, output_path, args.set)
            print(f"Generated {count} instances in {output_path}")
            logger.info(f"Successfully generated {count} instances")
            return 0
        except ConfigError as e:
            print(f"ERROR: {e}", file=sys.stderr)
            logger.error(f"Configuration error: {e}")
            return 2
        except Exception as e:
            print(f"ERROR: Unexpected error during generation: {e}", file=sys.stderr)
            logger.error(f"Unexpected error: {e}", exc_info=True)
            return 2

    # ============================================================================
    # build command
    # ============================================================================
    if args.command == "build":
        if getattr(args, 'app_type', None):
            results = []
            for app_name, set_name, exp_dir, yaml_path in _iter_app_sets(args.app_type, args.set):
                print(f"\n=== {app_name} / {set_name} ===")
                rc = run_build(app_name, set_name, exp_dir, yaml_path)
                results.append((app_name, set_name, rc))
            return _batch_summary(results)

        logger.info(f"Phase 2: Build - App: {args.app}, Set: {args.set}")
        app_dir = Path("applications") / args.app / "experiment"
        yaml_path = app_dir / "experiments.yaml"
        return run_build(args.app, args.set, app_dir, yaml_path)

    # ============================================================================
    # execute command
    # ============================================================================
    if args.command == "execute":
        if getattr(args, 'app_type', None):
            results = []
            for app_name, set_name, exp_dir, yaml_path in _iter_app_sets(args.app_type, args.set):
                print(f"\n=== {app_name} / {set_name} ===")
                rc = run_execute(app_name, set_name, exp_dir, yaml_path,
                                 repetitions=getattr(args, 'repetitions', None),
                                 timeout=getattr(args, 'timeout', None))
                results.append((app_name, set_name, rc))
            return _batch_summary(results)

        logger.info(f"Phase 3: Execute - App: {args.app}, Set: {args.set}")
        app_dir = Path("applications") / args.app / "experiment"
        yaml_path = app_dir / "experiments.yaml"
        return run_execute(args.app, args.set, app_dir, yaml_path,
                           repetitions=getattr(args, 'repetitions', None),
                           timeout=getattr(args, 'timeout', None))

    # ============================================================================
    # visualize command
    # ============================================================================
    if args.command == "visualize":
        if getattr(args, 'app_type', None):
            results = []
            for app_name, set_name, exp_dir, yaml_path in _iter_app_sets(args.app_type, args.set):
                print(f"\n=== {app_name} / {set_name} ===")
                rc = run_visualize(app_name, set_name, exp_dir, yaml_path)
                results.append((app_name, set_name, rc))
            return _batch_summary(results)

        logger.info(f"Phase 4: Visualize - App: {args.app}, Set: {args.set}")
        app_dir = Path("applications").resolve() / args.app / "experiment"
        yaml_path = app_dir / "experiments.yaml"
        results_path = Path(args.results) if getattr(args, 'results', None) else None
        return run_visualize(args.app, args.set, app_dir, yaml_path, results_json_path=results_path)

    # ============================================================================
    # run command (orchestrator)
    # ============================================================================
    if args.command == "run":
        if getattr(args, 'app_type', None):
            results = []
            for app_name, set_name, exp_dir, yaml_path in _iter_app_sets(args.app_type, args.set):
                print(f"\n=== {app_name} / {set_name} ===")
                rc = _run_pipeline(app_name, set_name, exp_dir, yaml_path, args)
                results.append((app_name, set_name, rc))
            return _batch_summary(results)

        logger.info(f"Running full pipeline for {args.app}/{args.set}")
        app_dir = Path("applications") / args.app / "experiment"
        yaml_path = app_dir / "experiments.yaml"
        rc = _run_pipeline(args.app, args.set, app_dir, yaml_path, args)
        if rc == 0:
            print("\n" + "="*60)
            print("PIPELINE COMPLETE")
            print("="*60)
            print(f"Application: {args.app}")
            print(f"Experiment Set: {args.set}")
            print(f"Results: {app_dir / 'results'}")
        return rc

    # ============================================================================
    # clean command
    # ============================================================================
    if args.command == "clean":
        import shutil
        build_root = Path("build_experiments")
        if not build_root.exists():
            print("Nothing to clean (build_experiments/ does not exist).")
            return 0

        app = getattr(args, 'app', None)
        if app:
            target = build_root / app
            if not target.exists():
                print(f"Nothing to clean for '{app}' ({target} does not exist).")
                return 0
        else:
            target = build_root

        try:
            answer = input(f"Remove {target}? [y/N] ")
        except (EOFError, KeyboardInterrupt):
            print()
            return 1
        if answer.strip().lower() != "y":
            print("Aborted.")
            return 1
        shutil.rmtree(target)
        print(f"Removed {target}")
        return 0

    logger.warning(f"Command '{args.command}' handler not yet implemented")
    return 0


if __name__ == "__main__":
    sys.exit(main())
