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


def run_visualize(app: str, exp_set: str, app_dir: Path, yaml_path: Path) -> int:
    """
    Run Phase 4: Generate visualizations and notebook.

    Args:
        app: Application name
        exp_set: Experiment set name
        app_dir: Application experiment directory
        yaml_path: Path to experiments.yaml

    Returns:
        0 on success, 2 on critical error
    """
    logger = logging.getLogger(__name__)

    from .visualizer import generate_vegalite_json
    from .notebook import generate_notebook, execute_notebook
    from .config import load_experiments_yaml, ConfigError
    from datetime import datetime, timezone

    results_dir = app_dir / "results"

    # Find most recent results JSON
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
        required=True,
        metavar="NAME",
        help="Application name (e.g., '05-cholesky')",
    )

    generate_parser.add_argument(
        "--set",
        type=str,
        required=True,
        metavar="NAME",
        help="Experiment set name (e.g., 'regression')",
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
        required=True,
        metavar="NAME",
        help="Application name (e.g., '05-cholesky')",
    )

    build_parser.add_argument(
        "--set",
        type=str,
        required=True,
        metavar="NAME",
        help="Experiment set name (e.g., 'regression')",
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
        required=True,
        metavar="NAME",
        help="Application name (e.g., '05-cholesky')",
    )

    execute_parser.add_argument(
        "--set",
        type=str,
        required=True,
        metavar="NAME",
        help="Experiment set name (e.g., 'regression')",
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
        required=True,
        metavar="NAME",
        help="Application name (e.g., '05-cholesky')",
    )

    visualize_parser.add_argument(
        "--set",
        type=str,
        required=True,
        metavar="NAME",
        help="Experiment set name (e.g., 'regression')",
    )

    visualize_parser.add_argument(
        "--results",
        type=str,
        metavar="PATH",
        help="Path to results JSON file (default: latest in results/)",
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
        required=True,
        metavar="NAME",
        help="Application name (e.g., '05-cholesky')",
    )

    run_parser.add_argument(
        "--set",
        type=str,
        required=True,
        metavar="NAME",
        help="Experiment set name (e.g., 'regression')",
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
    # generate command
    # ============================================================================
    if args.command == "generate":
        logger.info(f"Application: {args.app}")
        logger.info(f"Experiment set: {args.set}")

        from .generator import generate_cmakelists
        from .config import ConfigError

        # Construct paths
        app_dir = Path("applications") / args.app / "experiment"
        yaml_path = app_dir / "experiments.yaml"
        output_path = app_dir / "CMakeLists.txt"

        # Validate YAML file exists
        if not yaml_path.exists():
            print(f"ERROR: Application '{args.app}' not found at applications/{args.app}/experiment/experiments.yaml", file=sys.stderr)
            logger.error(f"YAML file not found: {yaml_path}")
            return 2

        # Generate CMakeLists.txt
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
        # Validate required arguments
        if not args.app or not args.set:
            print("ERROR: --app and --set are required for build command", file=sys.stderr)
            return 2

        logger.info(f"Phase 2: Build - App: {args.app}, Set: {args.set}")

        # Construct paths
        app_dir = Path("applications") / args.app / "experiment"
        yaml_path = app_dir / "experiments.yaml"

        # Call run_build which handles all build logic
        exit_code = run_build(args.app, args.set, app_dir, yaml_path)
        return exit_code

    # ============================================================================
    # execute command
    # ============================================================================
    if args.command == "execute":
        # Validate required arguments
        if not args.app or not args.set:
            print("ERROR: --app and --set are required for execute command", file=sys.stderr)
            return 2

        logger.info(f"Phase 3: Execute - App: {args.app}, Set: {args.set}")

        # Import required Phase 3 functions
        from .collector import collect_all_measurements
        from .builder import read_build_results, update_instance_execution, write_build_results
        from .config import load_experiments_yaml, ConfigError
        from datetime import datetime, timezone

        # Construct paths
        app_dir = Path("applications") / args.app / "experiment"
        yaml_path = app_dir / "experiments.yaml"
        results_dir = app_dir / "results"

        # Verify build results exist
        if not results_dir.exists():
            print(f"ERROR: Build results not found at {results_dir}", file=sys.stderr)
            print(f"       Run 'python3 -m tools.experiment_framework build --app {args.app} --set {args.set}' first", file=sys.stderr)
            logger.error(f"Results directory {results_dir} does not exist")
            return 2

        # Find most recent results JSON
        json_files = sorted(results_dir.glob("results_*.json"))
        if not json_files:
            print(f"ERROR: No build results JSON found in {results_dir}", file=sys.stderr)
            print("       Run the build command first", file=sys.stderr)
            logger.error("No results JSON files found")
            return 2

        latest_json = json_files[-1]
        logger.info(f"Using results file: {latest_json}")

        # Load configuration and build results
        try:
            config = load_experiments_yaml(yaml_path)
            build_results_obj = read_build_results(latest_json)

            # Convert BuildResults object to dict for collect_all_measurements
            build_results_dict = {
                'successful_instances': [
                    {
                        'instance_name': inst.instance_name,
                        'compilation': inst.compilation,
                        'binary_size_bytes': inst.binary_size_bytes,
                        'executable_path': inst.executable_path,
                        'execution': inst.execution
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

            # Debug: Log the structure being passed to collector
            logger.debug(f"build_results_dict structure: {len(build_results_dict['successful_instances'])} successful instances")
            for inst_dict in build_results_dict['successful_instances']:
                logger.debug(f"  Instance '{inst_dict['instance_name']}': executable_path={inst_dict.get('executable_path')}")

        except ConfigError as e:
            print(f"ERROR: Configuration/results loading failed: {e}", file=sys.stderr)
            logger.error(f"Failed to load config/results: {e}", exc_info=True)
            return 2
        except Exception as e:
            print(f"ERROR: Unexpected error loading config/results: {e}", file=sys.stderr)
            logger.error(f"Unexpected error: {e}", exc_info=True)
            return 2

        # Check for successful builds to execute
        if not build_results_obj.successful_instances:
            print("ERROR: No successful builds found - nothing to execute", file=sys.stderr)
            logger.warning("No successful instances to execute")
            return 2

        print(f"Executing {len(build_results_obj.successful_instances)} instances...")
        logger.info(f"Starting Phase 3 execution of {len(build_results_obj.successful_instances)} instances")

        # Execute all instances
        try:
            # Use CLI args if provided, otherwise from YAML config
            repetitions = args.repetitions if hasattr(args, 'repetitions') and args.repetitions else None
            timeout = args.timeout if hasattr(args, 'timeout') and args.timeout else None

            collection_results = collect_all_measurements(
                build_results_dict,
                config,
                repetitions=repetitions,
                timeout=timeout
            )

        except KeyboardInterrupt:
            print("\nExecution interrupted by user", file=sys.stderr)
            logger.warning("Execution interrupted")
            return 1
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

        logger.info(f"Updated {updated_count} instances with execution data")

        # Write updated results back to JSON
        try:
            output_file = write_build_results(
                build_results_obj,
                results_dir,
                config,
                args.set,
                app_dir
            )

            print(f"\nExecution complete!")
            print(f"  Results updated in: {output_file}")
            logger.info(f"Results written to {output_file}")

            # Print summary statistics
            summary = collection_results['summary']
            print(f"\nSummary:")
            print(f"  Executed:  {summary['executed']} instances")
            if summary['skipped'] > 0:
                print(f"  Skipped:   {summary['skipped']} instances")
            if summary['errors'] > 0:
                print(f"  Errors:    {summary['errors']} instances")

            # Show measurement counts
            if collection_results['execution_results']:
                first_result = collection_results['execution_results'][0]
                meas_count = len(first_result['runs'][0].get('measurements', {})) if first_result['runs'] else 0
                print(f"  Measurements per run: {meas_count}")

            return 0 if summary['errors'] == 0 else 1

        except Exception as e:
            print(f"ERROR: Failed to write results: {e}", file=sys.stderr)
            logger.error(f"Write error: {e}", exc_info=True)
            return 2

    # ============================================================================
    # visualize command
    # ============================================================================
    if args.command == "visualize":
        # Validate required arguments
        if not args.app or not args.set:
            print("ERROR: --app and --set are required for visualize command", file=sys.stderr)
            return 2

        logger.info(f"Phase 4: Visualize - App: {args.app}, Set: {args.set}")

        # Import required Phase 4 functions
        from .visualizer import generate_vegalite_json
        from .notebook import generate_notebook, execute_notebook
        from .config import load_experiments_yaml, ConfigError
        from datetime import datetime, timezone

        # Construct paths (use absolute paths for notebook generation)
        app_dir = Path("applications").resolve() / args.app / "experiment"
        yaml_path = app_dir / "experiments.yaml"
        results_dir = app_dir / "results"

        # Verify results directory exists
        if not results_dir.exists():
            print(f"ERROR: Results directory not found at {results_dir}", file=sys.stderr)
            print(f"       Run 'python3 -m tools.experiment_framework execute --app {args.app} --set {args.set}' first", file=sys.stderr)
            logger.error(f"Results directory {results_dir} does not exist")
            return 2

        # Find most recent results JSON or use specified path
        if args.results:
            results_json = Path(args.results)
            if not results_json.exists():
                print(f"ERROR: Specified results file not found: {args.results}", file=sys.stderr)
                return 2
        else:
            json_files = sorted(results_dir.glob("results_*.json"))
            if not json_files:
                print(f"ERROR: No results JSON found in {results_dir}", file=sys.stderr)
                print("       Run the execute command first", file=sys.stderr)
                logger.error("No results JSON files found")
                return 2
            results_json = json_files[-1]

        logger.info(f"Using results file: {results_json}")

        # Load configuration
        try:
            config = load_experiments_yaml(yaml_path)
        except ConfigError as e:
            print(f"ERROR: Configuration loading failed: {e}", file=sys.stderr)
            logger.error(f"Failed to load config: {e}", exc_info=True)
            return 2
        except Exception as e:
            print(f"ERROR: Unexpected error loading config: {e}", file=sys.stderr)
            logger.error(f"Unexpected error: {e}", exc_info=True)
            return 2

        # Check if visualization config exists
        if "visualization" not in config or "plots" not in config.get("visualization", {}):
            print("WARNING: No visualization configuration found in YAML", file=sys.stderr)
            print("         Skipping plot generation", file=sys.stderr)
            logger.warning("No visualization.plots section in config")
            vegalite_files = []
        else:
            # Generate Vega-Lite plots
            print("Generating Vega-Lite visualizations...")
            try:
                vegalite_files = generate_vegalite_json(config, results_json, results_dir)
                print(f"  Generated {len(vegalite_files)} plot(s)")
                logger.info(f"Generated {len(vegalite_files)} Vega-Lite files")
            except Exception as e:
                print(f"ERROR: Visualization generation failed: {e}", file=sys.stderr)
                logger.error(f"Visualization error: {e}", exc_info=True)
                return 2

        # Generate Jupyter notebook
        print("Generating Jupyter notebook...")
        try:
            timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
            notebook_path = results_dir / f"analysis_{timestamp}.ipynb"

            # Pass absolute paths for notebook to use
            generate_notebook(config, results_json, vegalite_files, notebook_path)
            print(f"  Notebook created: {notebook_path}")
            logger.info(f"Generated notebook: {notebook_path}")
        except Exception as e:
            print(f"ERROR: Notebook generation failed: {e}", file=sys.stderr)
            logger.error(f"Notebook generation error: {e}", exc_info=True)
            return 2

        # Execute notebook (unless skipped)
        skip_execution = hasattr(args, 'skip_notebook_execution') and args.skip_notebook_execution
        if not skip_execution:
            print("Executing notebook...")
            try:
                execute_notebook(notebook_path)
                print(f"  Notebook executed successfully")
                logger.info("Notebook execution completed")
            except FileNotFoundError as e:
                print(f"WARNING: {e}", file=sys.stderr)
                print(f"         Notebook generated but not executed: {notebook_path}", file=sys.stderr)
                logger.warning(f"Jupyter not available: {e}")
                return 1
            except Exception as e:
                print(f"WARNING: Notebook execution failed: {e}", file=sys.stderr)
                print(f"         Notebook generated but not executed: {notebook_path}", file=sys.stderr)
                logger.error(f"Notebook execution error: {e}", exc_info=True)
                return 1

        print("\nVisualization complete!")
        print(f"  Results: {results_json}")
        print(f"  Notebook: {notebook_path}")
        if vegalite_files:
            print(f"  Plots: {len(vegalite_files)} Vega-Lite JSON files in {results_dir}")

        return 0

    # ============================================================================
    # run command (orchestrator)
    # ============================================================================
    if args.command == "run":
        logger.info(f"Running full pipeline for {args.app}/{args.set}")
        app_dir = Path("applications") / args.app / "experiment"
        yaml_path = app_dir / "experiments.yaml"

        # Run one-time setup script if it exists
        onetime_script = Path("applications") / args.app / "onetime.sh"
        if onetime_script.exists():
            logger.info(f"Executing one-time setup script: {onetime_script}")
            try:
                subprocess.run(["sh", str(onetime_script)], check=True)
            except subprocess.CalledProcessError as e:
                logger.error(f"One-time setup script failed: {e}")
                return 2
        
        # Phase 1: Generate (conditional)
        if not args.skip_generate:
            logger.info("Phase 1: Generating CMakeLists.txt...")
            exit_code = run_generate(args.app, args.set, app_dir, yaml_path)
            if exit_code != 0:
                return exit_code
        else:
            logger.info("Phase 1: Skipped (--skip-generate)")
            # Validate hash
            exit_code = validate_hash(yaml_path, app_dir)
            if exit_code != 0:
                return exit_code

        # Phase 2: Build (conditional)
        if not args.skip_build:
            logger.info("Phase 2: Building instances...")
            exit_code = run_build(args.app, args.set, app_dir, yaml_path)
            if exit_code == 2:
                return 2

        # Phase 3: Execute (conditional)
        if not args.skip_execute:
            logger.info("Phase 3: Executing instances...")
            exit_code = run_execute(args.app, args.set, app_dir, yaml_path)
            if exit_code == 2:
                return 2

        # Phase 4: Visualize (always)
        logger.info("Phase 4: Generating visualizations...")
        exit_code = run_visualize(args.app, args.set, app_dir, yaml_path)
        if exit_code == 2:
            return 2

        # Success
        print("\n" + "="*60)
        print("PIPELINE COMPLETE")
        print("="*60)
        print(f"Application: {args.app}")
        print(f"Experiment Set: {args.set}")
        print(f"Results: {app_dir / 'results'}")

        return 0

    # ============================================================================
    # Other commands (not yet implemented)
    # ============================================================================
    logger.warning(f"Command '{args.command}' handler not yet implemented")
    return 0


if __name__ == "__main__":
    sys.exit(main())
