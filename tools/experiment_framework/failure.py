"""
Failure-outcome classification for IaRa experiment results.

Maps every experiment instance (successful or failed) to exactly one outcome
bucket, consumed by the automatic failure-mode figures (feasibility matrix,
failure markers on the universal plots, RAM-threshold line).

Bucket taxonomy:
    success            — built and executed with measured statistics
    build-timeout      — build/setup phase exceeded its timeout
    build-oom          — build/setup phase killed or out of memory
    build-error        — well-behaved build/setup failure
    runtime-oom        — run phase: OS-killed (SIGKILL) or malloc abort (SIGABRT)
    runtime-timeout    — run phase exceeded the CTest timeout
    not-run            — skipped (skip-larger policy) / never executed
"""

import logging
import re
from pathlib import Path
from typing import Any, Dict, List

logger = logging.getLogger(__name__)

# Outcome buckets (exactly one per instance)
OUTCOME_SUCCESS = "success"
OUTCOME_BUILD_TIMEOUT = "build-timeout"
OUTCOME_BUILD_OOM = "build-oom"
OUTCOME_BUILD_ERROR = "build-error"
OUTCOME_RUNTIME_OOM = "runtime-oom"
OUTCOME_RUNTIME_TIMEOUT = "runtime-timeout"
OUTCOME_NOT_RUN = "not-run"

OUTCOMES = [
    OUTCOME_SUCCESS,
    OUTCOME_BUILD_TIMEOUT,
    OUTCOME_BUILD_OOM,
    OUTCOME_BUILD_ERROR,
    OUTCOME_RUNTIME_OOM,
    OUTCOME_RUNTIME_TIMEOUT,
    OUTCOME_NOT_RUN,
]

OUTCOME_LABELS = {
    OUTCOME_SUCCESS: "Success",
    OUTCOME_BUILD_TIMEOUT: "Build timeout",
    OUTCOME_BUILD_OOM: "Build OOM",
    OUTCOME_BUILD_ERROR: "Build error",
    OUTCOME_RUNTIME_OOM: "Runtime OOM",
    OUTCOME_RUNTIME_TIMEOUT: "Runtime timeout",
    OUTCOME_NOT_RUN: "Not run",
}

# Stable color scale shared by the feasibility matrix and the failure markers.
OUTCOME_COLORS = {
    OUTCOME_SUCCESS: "#2ca02c",
    OUTCOME_BUILD_TIMEOUT: "#ff7f0e",
    OUTCOME_BUILD_OOM: "#9467bd",
    OUTCOME_BUILD_ERROR: "#8c564b",
    OUTCOME_RUNTIME_OOM: "#d62728",
    OUTCOME_RUNTIME_TIMEOUT: "#e377c2",
    OUTCOME_NOT_RUN: "#7f7f7f",
}

# Marker glyph per outcome (✗ = OOM, ▲ = timeout, ■ = build error, · = not run).
OUTCOME_GLYPHS = {
    OUTCOME_SUCCESS: "",
    OUTCOME_BUILD_TIMEOUT: "\u25b2",
    OUTCOME_BUILD_OOM: "\u2717",
    OUTCOME_BUILD_ERROR: "\u25a0",
    OUTCOME_RUNTIME_OOM: "\u2717",
    OUTCOME_RUNTIME_TIMEOUT: "\u25b2",
    OUTCOME_NOT_RUN: "\u00b7",
}

# References a phase log inside an error message: "Full output: <path>".
_LOG_REF = re.compile(r"Full output:\s*(\S+\.log)")

# CTest failure markers found in run-*.log tails (lowercased).
_KILLED = "subprocess killed"
_ABORTED = "subprocess aborted"
_TIMEOUT = "***timeout"


def classify_failed_instance(failed: Dict[str, Any],
                             build_root: Path) -> str:
    """Map one ``failed_instances`` entry to an outcome bucket.

    Classification order:
      1. ``failure_mode == 'skipped'`` (skip-larger policy) → ``not-run``.
      2. Any error message referencing a ``run-*.log`` that exists: read its
         tail and map CTest markers — ``Subprocess killed``/``aborted`` →
         ``runtime-oom`` (OS kill / malloc abort), ``***Timeout`` →
         ``runtime-timeout``.  This disambiguates run-phase failures that the
         build-phase classifier stamps ``build-oom`` (e.g. SIFT copy arm at
         20 cores).
      3. Fall back to ``failure_mode`` for the build-phase buckets.
    """
    if failed.get("failure_mode") == "skipped":
        return OUTCOME_NOT_RUN

    build_root = Path(build_root)
    for err in failed.get("errors", []):
        msg = err.get("message", "") if isinstance(err, dict) else str(err)
        m = _LOG_REF.search(msg)
        if not m:
            continue
        log_path = Path(m.group(1))
        if not log_path.is_absolute():
            log_path = build_root / log_path
        if not log_path.name.startswith("run-") or not log_path.exists():
            continue
        try:
            with open(log_path, "r", errors="replace") as f:
                text = f.read()[-12000:].lower()
        except OSError:
            continue
        if _KILLED in text or _ABORTED in text:
            return OUTCOME_RUNTIME_OOM
        if _TIMEOUT in text:
            return OUTCOME_RUNTIME_TIMEOUT

    mode = failed.get("failure_mode", OUTCOME_BUILD_ERROR)
    if mode in (OUTCOME_BUILD_TIMEOUT, OUTCOME_BUILD_OOM, OUTCOME_BUILD_ERROR,
                "skipped"):
        return mode
    return OUTCOME_BUILD_ERROR


def _failure_detail(failed: Dict[str, Any], limit: int = 300) -> str:
    """Short human-readable failure detail (last error message, truncated)."""
    errors = failed.get("errors", []) or []
    if not errors:
        return "no error details"
    last = errors[-1]
    msg = last.get("message", "") if isinstance(last, dict) else str(last)
    return msg[:limit] + ("..." if len(msg) > limit else "")


def build_outcome_rows(results: Dict[str, Any],
                       build_root: Path) -> List[Dict[str, Any]]:
    """Merge successful + failed instances into one plot-ready row list.

    Each row carries ``failure_outcome``, ``failure_glyph`` and
    ``failure_detail``; successful rows keep their full result payload
    (execution/statistics/binary/compilation), failed rows keep their
    ``parameters`` so they can be placed on the same chart axes.
    """
    rows: List[Dict[str, Any]] = []
    for inst in results.get("instances", []):
        row = dict(inst)
        row["failure_outcome"] = OUTCOME_SUCCESS
        row["failure_glyph"] = OUTCOME_GLYPHS[OUTCOME_SUCCESS]
        row["failure_detail"] = ""
        rows.append(row)

    for failed in results.get("failed_instances", []):
        outcome = classify_failed_instance(failed, build_root)
        row: Dict[str, Any] = {
            "name": failed.get("name", "unknown"),
            "parameters": dict(failed.get("parameters", {})),
            "failure_outcome": outcome,
            "failure_glyph": OUTCOME_GLYPHS[outcome],
            "failure_detail": _failure_detail(failed),
        }
        # Flattened keys mirror what successful instances carry (Vega-Lite
        # accesses both nested and flat forms elsewhere in the pipeline).
        for k, v in row["parameters"].items():
            row[f"parameters.{k}"] = v
        rows.append(row)

    return rows
