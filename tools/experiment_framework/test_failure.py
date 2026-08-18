"""
Unit tests for failure.py — outcome classification of failed instances.
"""

import json
import tempfile
import unittest
from pathlib import Path

from .failure import (
    OUTCOME_BUILD_ERROR,
    OUTCOME_BUILD_OOM,
    OUTCOME_BUILD_TIMEOUT,
    OUTCOME_NOT_RUN,
    OUTCOME_RUNTIME_OOM,
    OUTCOME_RUNTIME_TIMEOUT,
    OUTCOME_SUCCESS,
    build_outcome_rows,
    classify_failed_instance,
)


def _failed(name="app_set_sched_cpu-cores_20", mode="build-error", errors=None):
    return {
        "name": name,
        "failure_mode": mode,
        "errors": errors or [{"phase": "build", "message": "boom"}],
        "parameters": {"scheduler": "vf-omp", "cpu-cores": 20},
    }


class TestClassifyFailedInstance(unittest.TestCase):
    def test_skipped_maps_to_not_run(self):
        self.assertEqual(
            classify_failed_instance(_failed(mode="skipped"), Path(".")),
            OUTCOME_NOT_RUN,
        )

    def test_build_modes_map_directly(self):
        self.assertEqual(
            classify_failed_instance(_failed(mode="build-oom"), Path(".")),
            OUTCOME_BUILD_OOM,
        )
        self.assertEqual(
            classify_failed_instance(_failed(mode="build-timeout"), Path(".")),
            OUTCOME_BUILD_TIMEOUT,
        )
        self.assertEqual(
            classify_failed_instance(_failed(mode="build-error"), Path(".")),
            OUTCOME_BUILD_ERROR,
        )

    def test_run_log_killed_maps_to_runtime_oom(self):
        with tempfile.TemporaryDirectory() as tmp:
            log = Path(tmp) / "run-app_set_sched_cpu-cores_20.log"
            log.write_text("... Subprocess killed***Exception:  18.04 sec ...")
            failed = _failed(mode="build-oom", errors=[{
                "phase": "build",
                "message": f"Phase 'run' failed. Full output: {log}",
            }])
            self.assertEqual(
                classify_failed_instance(failed, Path(tmp)), OUTCOME_RUNTIME_OOM
            )

    def test_run_log_timeout_maps_to_runtime_timeout(self):
        with tempfile.TemporaryDirectory() as tmp:
            log = Path(tmp) / "run-app_set_sched_cpu-cores_20.log"
            log.write_text("... Test #5: run-... ***Timeout  600.03 sec ...")
            failed = _failed(mode="build-timeout", errors=[{
                "phase": "build",
                "message": f"Phase 'run' failed. Full output: {log}",
            }])
            self.assertEqual(
                classify_failed_instance(failed, Path(tmp)),
                OUTCOME_RUNTIME_TIMEOUT,
            )

    def test_missing_run_log_falls_back_to_failure_mode(self):
        failed = _failed(mode="build-oom", errors=[{
            "phase": "build",
            "message": "Phase 'run' failed. Full output: no/such/run-1.log",
        }])
        self.assertEqual(
            classify_failed_instance(failed, Path(".")), OUTCOME_BUILD_OOM
        )


class TestBuildOutcomeRows(unittest.TestCase):
    def test_merge_marks_success_and_failure(self):
        results = {
            "instances": [{"name": "app_set_sched_cpu-cores_1",
                           "parameters": {"scheduler": "vf-omp", "cpu-cores": 1},
                           "execution": {"statistics": {"wall_time": {"mean": 1.0}}}}],
            "failed_instances": [
                _failed(name="app_set_sched_cpu-cores_20", mode="build-oom"),
            ],
        }
        rows = build_outcome_rows(results, Path("."))
        self.assertEqual(len(rows), 2)
        by_name = {r["name"]: r for r in rows}
        self.assertEqual(by_name["app_set_sched_cpu-cores_1"]["failure_outcome"],
                         OUTCOME_SUCCESS)
        self.assertEqual(by_name["app_set_sched_cpu-cores_1"]["failure_glyph"], "")
        failed_row = by_name["app_set_sched_cpu-cores_20"]
        self.assertEqual(failed_row["failure_outcome"], OUTCOME_BUILD_OOM)
        self.assertEqual(failed_row["failure_glyph"], "\u2717")
        self.assertEqual(failed_row["parameters"]["cpu-cores"], 20)
        # Flattened keys mirror successful instances
        self.assertEqual(failed_row["parameters.cpu-cores"], 20)


if __name__ == "__main__":
    unittest.main()
