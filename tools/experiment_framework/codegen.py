"""
Code generation module for the IaRa experiment framework.

Replaces per-app codegen.sh and preesm-codegen.sh shell scripts.
Called from CMake's setup-* CTest targets.

Preesm mode: generates architecture (.slam) and scenario (.scenario) files
via Preesm's generatorCli, runs the Preesm workflow to produce C code, copies
output to the instance build directory, and optionally injects wall-clock timing.

Usage:
  python -m experiment_framework.codegen --mode preesm \\
      --output-dir /path/to/build \\
      --preesm-dist /path/to/eclipse \\
      --preesm-project /path/to/SIFT \\
      --preesm-name SIFTapp \\
      --workflow Codegen.workflow \\
      --num-cores 4 \\
      --timing-patch
"""

import argparse
import logging
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

logger = logging.getLogger(__name__)


def _run_eclipse(eclipsec, workspace, application, *args, timeout=600):
    """Run an eclipsec headless application in a workspace."""
    cmd = [
        str(eclipsec),
        "--launcher.suppressErrors",
        "-nosplash",
        "-consolelog",
        "-data",
        str(workspace),
        "-application",
        application,
        *args,
    ]
    logger.info("Running: %s", " ".join(cmd))
    result = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
    if result.returncode != 0:
        tail = result.stderr[-3000:] if len(result.stderr) > 3000 else result.stderr
        raise RuntimeError(
            f"Eclipse application {application} failed (rc={result.returncode}):\n{tail}"
        )
    return result


def _inject_timing(main_c_path):
    """Idempotent: inject clock_gettime around communicationInit() + return 0."""
    text = main_c_path.read_text()
    if "clock_gettime" in text:
        return

    text = text.replace(
        "#include <stdio.h>",
        "#include <stdio.h>\n#include <time.h>",
    )
    text = text.replace(
        "  communicationInit();\n",
        "  struct timespec _t0, _t1;\n"
        "  clock_gettime(CLOCK_MONOTONIC, &_t0);\n"
        "  communicationInit();\n",
    )
    idx = text.rfind("  return 0;\n}")
    if idx >= 0:
        tail = (
            "  clock_gettime(CLOCK_MONOTONIC, &_t1);\n"
            "  double _wt = (double)(_t1.tv_sec - _t0.tv_sec)"
            " + (double)(_t1.tv_nsec - _t0.tv_nsec) * 1e-9;\n"
            '  printf("Wall time: %lf s\\n", _wt);\n'
            "  return 0;\n}"
        )
        text = text[:idx] + tail + text[idx + len("  return 0;\n}"):]
    main_c_path.write_text(text)
    logger.info("Timing injected into %s", main_c_path)


def codegen_preesm(output_dir, preesm_dist, preesm_project, preesm_name,
                   workflow, num_cores, timing_patch, pi_basename=None,
                   extra_setup=None):
    """Generate architecture + scenarios, run Preesm workflow, copy output."""
    eclipsec = Path(preesm_dist) / "eclipse"
    if not eclipsec.exists():
        # Try eclipsec.exe (headless variant)
        eclipsec = Path(preesm_dist) / "eclipsec"
    if not eclipsec.exists():
        raise FileNotFoundError(
            f"Eclipse binary not found at {preesm_dist}/eclipse or {preesm_dist}/eclipsec"
        )

    dest = Path(output_dir) / "generated"
    dest.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(suffix="_preesm-workspace") as ws:
        ws_path = Path(ws)
        logger.info("Workspace: %s", ws_path)

        # 1. Import project into temp workspace
        _run_eclipse(
            eclipsec, ws_path,
            "org.eclipse.cdt.managedbuilder.core.headlessbuild",
            "-import", str(preesm_project),
        )

        # 2. Generate architecture + scenarios
        _run_eclipse(
            eclipsec, ws_path,
            "org.preesm.cli.generatorCli",
            preesm_name, "-x", str(num_cores), "-s",
        )

        # 3. Determine scenario name
        # ScenariosGenerator names: <pi_basename>_<archi_name>.scenario
        archi_name = f"{num_cores}CoresX86"
        if pi_basename:
            pi_name = pi_basename
        else:
            algo_dir = Path(preesm_project) / "Algo"
            pi_files = sorted(algo_dir.glob("*.pi"))
            if not pi_files:
                raise FileNotFoundError(f"No .pi files found in {algo_dir}")
            pi_name = pi_files[0].stem
        scenario_name = f"{pi_name}_{archi_name}.scenario"
        logger.info("Scenario: %s", scenario_name)

        # 4. Run Preesm workflow
        _run_eclipse(
            eclipsec, ws_path,
            "org.preesm.cli.workflowCli",
            preesm_name, "-w", workflow, "-s", scenario_name,
        )

    # 5. Copy generated C code to build dir
    generated = Path(preesm_project) / "Code" / "generated"
    for pattern in ["*.c", "*.h"]:
        for f in generated.glob(pattern):
            shutil.copy2(f, dest)
            logger.debug("Copied %s -> %s", f.name, dest)

    # 6. Inject wall-clock timing (app-agnostic, idempotent)
    main_c = dest / "main.c"
    if timing_patch:
        if main_c.exists():
            _inject_timing(main_c)
        else:
            logger.warning("main.c not found at %s, skipping timing patch", main_c)

    # 7. Run extra setup commands (app-specific post-processing)
    if extra_setup:
        for cmd in extra_setup:
            logger.info("Extra setup: %s", cmd)
            subprocess.run(cmd, shell=True, check=True,
                           cwd=str(Path(output_dir).parent))

    logger.info("Preesm codegen complete")


def main():
    parser = argparse.ArgumentParser(
        description="Code generation for IaRa experiment framework")
    parser.add_argument("--mode", required=True, choices=["preesm"],
                        help="Codegen mode")

    # Preesm mode arguments (accepted only when --mode preesm)
    parser.add_argument("--output-dir", help="Instance build directory")
    parser.add_argument("--preesm-dist", help="Preesm Eclipse installation")
    parser.add_argument("--preesm-project", help="Preesm Eclipse project path")
    parser.add_argument("--preesm-name", help="Eclipse .project <name>")
    parser.add_argument("--workflow", default="Codegen.workflow",
                        help="Workflow filename (default: Codegen.workflow)")
    parser.add_argument("--num-cores", type=int, help="Core count for architecture generation")
    parser.add_argument("--timing-patch", action="store_true",
                        help="Inject wall-clock timing into main.c")
    parser.add_argument("--pi-basename",
                        help="Top-level .pi file basename (e.g. Hextract). "
                             "If omitted, uses first .pi found in Algo/.")
    parser.add_argument("--extra-setup", action="append", default=[],
                        help="Extra shell commands to run after codegen")

    args = parser.parse_args()

    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s %(levelname)s: %(message)s",
        datefmt="%H:%M:%S",
    )

    if args.mode == "preesm":
        codegen_preesm(
            output_dir=args.output_dir,
            preesm_dist=args.preesm_dist,
            preesm_project=args.preesm_project,
            preesm_name=args.preesm_name,
            workflow=args.workflow,
            num_cores=args.num_cores,
            timing_patch=args.timing_patch,
            pi_basename=args.pi_basename,
            extra_setup=args.extra_setup or None,
        )

    return 0


if __name__ == "__main__":
    sys.exit(main())
