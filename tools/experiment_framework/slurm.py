"""
Slurm integration for the IaRa experiment framework.

Submits test instances as sbatch jobs, monitoring completion and
collecting results.  Used when ``--slurm`` is passed to ``execute``
or ``run`` commands.
"""

import logging
import os
import subprocess
import tempfile
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Dict, List, Optional, Any

logger = logging.getLogger(__name__)

# Global tracking of active slurm jobs, so cancellation handlers can scancel them.
_active_jobs: set = set()


def register_active_job(job_id: int):
    """Track a slurm job so it can be cancelled on shutdown."""
    _active_jobs.add(job_id)


def unregister_active_job(job_id: int):
    """Remove a completed/cancelled job from tracking."""
    _active_jobs.discard(job_id)


def cancel_all_jobs():
    """Cancel all tracked slurm jobs (called on graceful shutdown)."""
    for job_id in list(_active_jobs):
        try:
            subprocess.run(['scancel', str(job_id)], capture_output=True, timeout=10)
            logger.info("Cancelled slurm job %d", job_id)
        except Exception as e:
            logger.warning("Failed to cancel slurm job %d: %s", job_id, e)
        _active_jobs.discard(job_id)


def check_slurm_available() -> bool:
    """Return True if sbatch/squeue/scancel are available."""
    return all(
        subprocess.run(['which', c], capture_output=True).returncode == 0
        for c in ('sbatch', 'squeue', 'scancel')
    )


def _make_batch_script(
    executable: Path,
    env_vars: Dict[str, str],
    timeout: int,
    job_name: str,
    output_dir: Path,
    partition: Optional[str] = None,
    cpus: int = 48,
) -> str:
    """Generate a self-contained sbatch script.

    The script:
      - sets up the environment by sourcing sorgan_env.sh
      - runs the executable under /usr/bin/time -v
      - writes stdout/stderr to <output_dir>/<job_name>.{out,err}
      - writes the GNU time output to <output_dir>/<job_name>.time
    """
    output_dir.mkdir(parents=True, exist_ok=True)

    env_exports = '\n'.join(f'export {k}="{v}"' for k, v in env_vars.items())

    partition_line = f"#SBATCH --partition={partition}\n" if partition else ""

    script = f'''#!/bin/bash
#SBATCH --job-name={job_name}
#SBATCH --output={output_dir}/{job_name}.out
#SBATCH --error={output_dir}/{job_name}.err
#SBATCH --time={timeout // 60}:{timeout % 60:02d}
#SBATCH --ntasks=1
#SBATCH --cpus-per-task={cpus}
#SBATCH --exclusive
{partition_line}
set -e

# Restore the IaRa environment on the compute node
source "${{IARA_DIR:-/scratch/$USER/repos/iara}}/sorgan_env.sh"

{env_exports}

/usr/bin/time -v -o {output_dir}/{job_name}.time {executable}
'''
    return script


def submit_job(
    executable: Path,
    env_vars: Dict[str, str],
    timeout: int,
    job_name: str,
    output_dir: Path,
    nodelist: Optional[str] = None,
    partition: Optional[str] = None,
    cpus: int = 48,
) -> Dict[str, Any]:
    """Submit a single test instance to Slurm and wait for completion.

    Returns a dict with keys:
        job_id: int          – Slurm job ID
        success: bool        – did the job complete successfully?
        error: Optional[str] – error message if any
        stdout: str          – program output
        stderr: str          – program stderr
        returncode: int      – exit code of the command
    """
    if not check_slurm_available():
        return {
            'job_id': 0,
            'success': False,
            'error': 'Slurm commands (sbatch/squeue) not available',
            'stdout': '',
            'stderr': '',
            'returncode': -1,
        }

    script = _make_batch_script(executable, env_vars, timeout,
                                 job_name, output_dir, partition, cpus)

    with tempfile.NamedTemporaryFile(
        mode='w', suffix='.sh', delete=False, prefix=f'sbatch_{job_name}_'
    ) as f:
        f.write(script)
        script_path = f.name

    try:
        cmd = ['sbatch', '--parsable']
        if nodelist:
            cmd += [f'--nodelist={nodelist}']
        cmd += [script_path]

        logger.info(f'Submitting Slurm job: {" ".join(cmd)}')
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)

        if result.returncode != 0:
            return {
                'job_id': 0,
                'success': False,
                'error': f'sbatch failed: {result.stderr.strip()}',
                'stdout': '',
                'stderr': '',
                'returncode': -1,
            }

        job_id = int(result.stdout.strip())
        register_active_job(job_id)
        logger.info(f'Submitted job {job_id} ({job_name})')

        # Query which node the job was assigned to
        node = 'unknown'
        try:
            node_result = subprocess.run(
                ['squeue', '-j', str(job_id), '-h', '-o', '%N'],
                capture_output=True, text=True, timeout=10
            )
            node = node_result.stdout.strip() or 'unknown'
        except Exception:
            pass
        print(f"  [Slurm] Job {job_id} → {node}", file=sys.stderr)
        logger.info(f'Job {job_id} assigned to node(s): {node}')

        # Poll until the job finishes
        start = time.time()
        while True:
            elapsed = time.time() - start
            if elapsed > timeout + 120:  # grace period
                subprocess.run(['scancel', str(job_id)], capture_output=True)
                return {
                    'job_id': job_id,
                    'success': False,
                    'error': f'Job {job_id} exceeded timeout',
                    'stdout': '',
                    'stderr': '',
                    'returncode': -1,
                }

            status_result = subprocess.run(
                ['squeue', '-j', str(job_id), '-h', '-o', '%T'],
                capture_output=True, text=True, timeout=10
            )
            status = status_result.stdout.strip()
            if not status:
                break  # job is done (no longer in queue)
            logger.debug(f'Job {job_id} status: {status}')
            time.sleep(5)

        # Collect output
        out_file = output_dir / f'{job_name}.out'
        err_file = output_dir / f'{job_name}.err'
        time_file = output_dir / f'{job_name}.time'

        stdout = out_file.read_text() if out_file.exists() else ''
        stderr = err_file.read_text() if err_file.exists() else ''

        logger.info(f'Job {job_id} completed (stdout={len(stdout)} bytes, stderr={len(stderr)} bytes)')

        # Check sacct for exit code
        sacct_result = subprocess.run(
            ['sacct', '-j', str(job_id), '--format=ExitCode', '--noheader', '-P',
             '-n', '--delimiter=,'],
            capture_output=True, text=True, timeout=10
        )
        returncode = 0
        for line in sacct_result.stdout.strip().split('\n'):
            parts = line.split(',')
            if len(parts) >= 2 and parts[0].strip() and parts[1].strip():
                try:
                    returncode = int(parts[1].split(':')[0])
                except ValueError:
                    pass

        return {
            'job_id': job_id,
            'node': node,
            'success': returncode == 0,
            'error': None if returncode == 0 else f'Exit code {returncode}',
            'stdout': stdout,
            'stderr': stderr,
            'returncode': returncode,
        }

    finally:
        Path(script_path).unlink(missing_ok=True)
        try:
            if job_id:
                unregister_active_job(job_id)
        except NameError:
            pass
