"""
Shared utilities for the IaRa experiment framework.

Functions that are used across multiple modules (builder, collector,
slurm) live here to avoid circular imports and keep responsibilities
clean.
"""

import json
import logging
import os
import re
import subprocess
from pathlib import Path
from typing import Dict, List, Optional, Any

from .config import ConfigError

logger = logging.getLogger(__name__)


# ── Subprocess helpers ──────────────────────────────────────────────


def log_subprocess_call(
    cmd: List[str],
    cwd: Optional[Path] = None,
    env: Optional[Dict[str, str]] = None,
) -> None:
    """Log a subprocess invocation at DEBUG level (verbose mode only).

    Kept at DEBUG to avoid I/O during measurement-sensitive runs.
    """
    logger.debug(f"[exec] {' '.join(cmd)}")
    if cwd:
        logger.debug(f"  cwd: {cwd}")
    if env and env != os.environ:
        logger.debug(f"  env delta: {json.dumps(env, indent=2)}")


def run_and_log(
    cmd: List[str],
    cwd: Optional[Path] = None,
    env: Optional[Dict[str, str]] = None,
    timeout: Optional[int] = None,
    capture: bool = True,
    label: str = "",
) -> subprocess.CompletedProcess:
    """Run a subprocess and log full stdout/stderr at DEBUG level.

    The command line is always logged at INFO.  Captured output goes to
    the framework log at DEBUG so ``--verbose`` shows everything without
    spamming the console.
    """
    log_subprocess_call(cmd, cwd=cwd, env=env)

    kwargs = {}
    if capture:
        kwargs['capture_output'] = True
        kwargs['text'] = True
    if timeout:
        kwargs['timeout'] = timeout
    if cwd:
        kwargs['cwd'] = cwd
    final_env = os.environ.copy()
    if env:
        final_env.update(env)
    kwargs['env'] = final_env

    result = subprocess.run(cmd, **kwargs)

    if capture:
        if result.stdout:
            logger.debug(f"  stdout ({len(result.stdout)} B):\n{result.stdout}")
        if result.stderr:
            logger.debug(f"  stderr ({len(result.stderr)} B):\n{result.stderr}")
    logger.debug(f"  returncode={result.returncode}")

    return result


# ── GNU time parsing ────────────────────────────────────────────────


def _parse_wall_time(wall_time_str: str) -> float:
    """Parse wall clock time string from GNU time -v into seconds."""
    wall_time_str = wall_time_str.strip()
    if ':' not in wall_time_str:
        try:
            return float(wall_time_str)
        except ValueError:
            raise ConfigError(f"Invalid wall time format: {wall_time_str}")

    parts = wall_time_str.split(':')
    try:
        if len(parts) == 2:
            return int(parts[0]) * 60 + float(parts[1])
        elif len(parts) == 3:
            return int(parts[0]) * 3600 + int(parts[1]) * 60 + float(parts[2])
        else:
            raise ConfigError(f"Invalid wall time format: {wall_time_str}")
    except (ValueError, TypeError) as e:
        raise ConfigError(f"Failed to parse wall time '{wall_time_str}': {e}")


def parse_time_output(time_file: Path) -> Dict[str, Any]:
    """Parse GNU time -v output file.

    Returns a dict with keys: user_time_s, system_time_s, wall_time_s,
    max_rss_bytes, minor_faults, major_faults.
    """
    if not time_file.exists():
        logger.warning(f"Time output file does not exist: {time_file}")
        return {}

    try:
        content = time_file.read_text()
    except Exception as e:
        raise ConfigError(f"Failed to read time output file {time_file}: {e}")

    result: Dict[str, Any] = {}

    m = re.search(r'User time \(seconds\):\s+(\d+\.?\d*)', content)
    if m:
        result['user_time_s'] = float(m.group(1))

    m = re.search(r'System time \(seconds\):\s+(\d+\.?\d*)', content)
    if m:
        result['system_time_s'] = float(m.group(1))

    m = re.search(r'Elapsed \(wall clock\) time \(h:mm:ss or m:ss\):\s+(.+?)$',
                  content, re.MULTILINE)
    if m:
        result['wall_time_s'] = _parse_wall_time(m.group(1).strip())

    m = re.search(r'Maximum resident set size \(kbytes\):\s+(\d+)', content)
    if m:
        result['max_rss_bytes'] = int(m.group(1)) * 1024

    m = re.search(r'Minor \(reclaiming a frame\) page faults:\s+(\d+)', content)
    if m:
        result['minor_faults'] = int(m.group(1))

    m = re.search(r'Major \(requiring I/O\) page faults:\s+(\d+)', content)
    if m:
        result['major_faults'] = int(m.group(1))

    return result


def convert_time_to_seconds(value: float, unit: str) -> float:
    """Convert a time value to seconds."""
    factors = {'us': 1e-6, 'μs': 1e-6, 'ms': 1e-3, 's': 1.0, 'sec': 1.0,
               'min': 60.0, 'h': 3600.0, 'hour': 3600.0}
    if unit not in factors:
        raise ValueError(f"Unknown time unit: {unit}")
    return value * factors[unit]


def convert_memory_to_bytes(value: float, unit: str) -> int:
    """Convert a memory value to bytes."""
    factors = {'B': 1, 'byte': 1, 'KB': 1024, 'KiB': 1024,
               'MB': 1024**2, 'MiB': 1024**2,
               'GB': 1024**3, 'GiB': 1024**3,
               'TB': 1024**4, 'TiB': 1024**4}
    if unit not in factors:
        raise ValueError(f"Unknown memory unit: {unit}")
    return int(value * factors[unit])
