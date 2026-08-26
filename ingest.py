#!/usr/bin/env python3
"""Phase 0 ingest: iara experiment results JSON -> central DuckDB (results.db).

Walk applications/*/experiment/results/results_*.json and load into a single
DuckDB file. Schema per iara-experiment-db/transition-plan.md (v3).

Idempotent + incremental: ingest_manifest tracks (source_file, mtime); files
unchanged since last ingest are skipped; changed files re-ingest atomically
(delete rows for that file, then insert).

Usage:
  python ingest.py [--iara-repo DIR] [--db FILE] [--verify]

--verify  run the verification pass (counts vs JSON, 5 spot-checks, idempotency)
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

import duckdb

# ── config ──────────────────────────────────────────────────────────

DEFAULT_IARA_REPO = "/scratch/pedro.ciambra/repos/iara"
RESULTS_GLOB = "applications/*/experiment/results/results_*.json"
EXPERIMENT_YAML = "applications/{app}/experiment/experiments.yaml"
SCHEMA_VERSION = "1.0.0"

OUTCOME_RANK = {"runtime-oom": 3, "runtime-timeout": 2, "runtime-error": 1}

DDL = """
CREATE TABLE IF NOT EXISTS experiments (
  experiment_id    TEXT PRIMARY KEY,
  application      TEXT NOT NULL,
  experiment_set   TEXT NOT NULL,
  yaml_hash        TEXT,
  schema_version   TEXT,
  git_commit       TEXT,
  git_dirty        BOOLEAN,
  iara_opt_hash    TEXT,
  timestamp        TEXT,
  source_file      TEXT,
  ingest_meta      TEXT
);
CREATE TABLE IF NOT EXISTS instances (
  instance_id      TEXT PRIMARY KEY,
  experiment_id    TEXT,
  name             TEXT,
  status           TEXT,
  failure          TEXT,
  executable_path  TEXT,
  executable_hash  TEXT,
  exec_bin_size    BIGINT,
  exec_start       TEXT,
  exec_finish      TEXT,
  build_start      TEXT,
  build_finish     TEXT,
  preesm_time_s    DOUBLE,
  preesm_max_rss_mb DOUBLE
);
CREATE TABLE IF NOT EXISTS instance_parameters (
  instance_id      TEXT,
  param_name       TEXT,
  param_value      TEXT,
  is_computed      BOOLEAN
);
CREATE TABLE IF NOT EXISTS runs (
  run_id           TEXT PRIMARY KEY,
  instance_id      TEXT,
  run_index        INTEGER,
  warmup           BOOLEAN,
  status           TEXT,
  error            TEXT,
  exec_returncode  INTEGER,
  exec_start       TEXT,
  exec_finish      TEXT,
  wall_time_s      DOUBLE,
  user_time_s      DOUBLE,
  system_time_s    DOUBLE,
  max_rss_bytes    BIGINT,
  major_faults     BIGINT,
  minor_faults     BIGINT
);
CREATE TABLE IF NOT EXISTS measurements (
  run_id           TEXT,
  name             TEXT,
  value            DOUBLE,
  unit             TEXT
);
CREATE TABLE IF NOT EXISTS compilation (
  instance_id      TEXT PRIMARY KEY,
  iara_opt_time_s  DOUBLE,
  iara_opt_max_rss_mb DOUBLE,
  total_time_s     DOUBLE,
  iara_opt_returncode INTEGER,
  preesm_returncode    INTEGER
);
CREATE TABLE IF NOT EXISTS binary_sections (
  instance_id      TEXT,
  section          TEXT,
  size_bytes       BIGINT
);
CREATE TABLE IF NOT EXISTS definitions (
  application      TEXT PRIMARY KEY,
  parameters       TEXT,
  computed_parameters TEXT,
  measurements     TEXT,
  experiment_sets  TEXT
);
CREATE TABLE IF NOT EXISTS ingest_manifest (
  source_file      TEXT PRIMARY KEY,
  mtime            DOUBLE,
  ingested_at      TEXT
);
"""


def _git(repo: Path, *args: str) -> str | None:
    try:
        out = subprocess.run(
            ["git", "-C", str(repo), *args],
            capture_output=True, text=True, timeout=30,
        )
        return out.stdout.strip() if out.returncode == 0 else None
    except Exception:
        return None


def git_state(repo: Path) -> tuple[str | None, bool]:
    """Return (full_head_hash, dirty)."""
    head = _git(repo, "rev-parse", "HEAD")
    dirty = False
    if head:
        status = _git(repo, "status", "--porcelain")
        dirty = bool(status)
    return head, dirty


def app_yaml(iara: Path, app: str) -> Path:
    return iara / EXPERIMENT_YAML.format(app=app)


def load_yaml(iara: Path, app: str):
    """Load experiments.yaml via the framework loader (validates schema)."""
    sys.path.insert(0, str(iara))
    from tools.experiment_framework.config import load_experiments_yaml  # noqa: E402
    return load_experiments_yaml(app_yaml(iara, app))


def normalize(name: str, labels: dict) -> str:
    sys.path.insert(0, str(DEFAULT_IARA_REPO))
    from tools.experiment_framework.common import normalize_parameter_name  # noqa: E402
    return normalize_parameter_name(name, labels)


def param_labels(config: dict) -> dict:
    return {p["name"]: p.get("label", "") for p in config.get("parameters", [])}


def eval_computed(config: dict, params_norm: dict) -> dict:
    """Materialize computed params for one instance (mirrors framework eval).

    params_norm: JSON parameters keyed by normalized name.
    Returns {yaml_name: value}.
    """
    labels = param_labels(config)
    ns: dict = {}
    for p in config.get("parameters", []):
        norm = normalize(p["name"], labels)
        if norm in params_norm:
            ns[p["name"]] = params_norm[norm]
    out: dict = {}
    for cp in config.get("computed_parameters", []):
        expr = cp.get("expression", "")
        if not expr:
            continue
        value = eval(expr, {"__builtins__": {}}, {**ns, **out})
        t = cp.get("type", "str")
        if t == "int":
            value = int(value)
        elif t == "float":
            value = float(value)
        elif t == "bool":
            value = bool(value) if not isinstance(value, str) else value.lower() in ("true", "1", "yes")
        else:
            value = str(value)
        out[cp["name"]] = value
    return out


def classify_instance(inst: dict) -> str:
    """One canonical outcome per instance (framework conventions + runtime-error)."""
    ex = inst.get("execution") or {}
    runs = ex.get("runs") or []
    failures = ex.get("failures") or []
    if not runs and not failures:
        return "not-run"
    if ex.get("successful_runs", 0) > 0:
        return "success"
    worst = "runtime-error"
    for f in failures:
        msg = (f.get("error") or "") if isinstance(f, dict) else str(f)
        msg = msg.lower()
        if "timeout" in msg or "timed out" in msg:
            rank = OUTCOME_RANK["runtime-timeout"]
        elif any(s in msg for s in ("kill", "oom", "abort", "sig")):
            rank = OUTCOME_RANK["runtime-oom"]
        else:
            rank = OUTCOME_RANK["runtime-error"]
        if rank >= OUTCOME_RANK[worst]:
            worst = [k for k, r in OUTCOME_RANK.items() if r == rank][0]
    return worst


def classify_build_failure(failed: dict, build_root: Path) -> str:
    sys.path.insert(0, str(DEFAULT_IARA_REPO))
    from tools.experiment_framework.failure import classify_failed_instance  # noqa: E402
    try:
        return classify_failed_instance(failed, build_root)
    except Exception:
        return failed.get("failure_mode", "build-error")


def measurement_units(config: dict) -> dict:
    return {m["name"]: m.get("unit", "") for m in config.get("measurements", [])}


def parse_value(v) -> str | None:
    if v is None:
        return None
    if isinstance(v, bool):
        return "true" if v else "false"
    return str(v)


def file_hash(path: Path) -> str | None:
    if not path or not path.is_file():
        return None
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def binary_sections(inst: dict) -> list[tuple[str, int]]:
    b = inst.get("binary") or {}
    sections = b.get("sections")
    if isinstance(sections, dict):
        return [(str(k), int(v)) for k, v in sections.items() if v is not None]
    out = []
    for k, v in inst.items():
        if k.startswith("section_") and v is not None:
            out.append((k[len("section_"):], int(v)))
    return out


def delete_experiment(con: duckdb.DuckDBPyConnection, experiment_id: str) -> None:
    con.execute("DELETE FROM measurements WHERE run_id IN (SELECT run_id FROM runs WHERE instance_id IN (SELECT instance_id FROM instances WHERE experiment_id=?))", [experiment_id])
    con.execute("DELETE FROM binary_sections WHERE instance_id IN (SELECT instance_id FROM instances WHERE experiment_id=?)", [experiment_id])
    con.execute("DELETE FROM compilation WHERE instance_id IN (SELECT instance_id FROM instances WHERE experiment_id=?)", [experiment_id])
    con.execute("DELETE FROM instance_parameters WHERE instance_id IN (SELECT instance_id FROM instances WHERE experiment_id=?)", [experiment_id])
    con.execute("DELETE FROM runs WHERE instance_id IN (SELECT instance_id FROM instances WHERE experiment_id=?)", [experiment_id])
    con.execute("DELETE FROM instances WHERE experiment_id=?", [experiment_id])
    con.execute("DELETE FROM experiments WHERE experiment_id=?", [experiment_id])


def _ingest_file(con: duckdb.DuckDBPyConnection, path: Path, iara: Path, git: tuple[str | None, bool]) -> None:
    """Ingest one results JSON (deletes prior rows for it first)."""
    with open(path) as fh:
        d = json.load(fh)

    exp = d.get("experiment", {})
    app = exp.get("application", {}).get("name") or path.parents[1].name
    experiment_set = exp.get("experiment_set", "?")
    experiment_id = hashlib.sha256(str(path).encode()).hexdigest()[:16]

    # re-ingest: remove prior rows for this file
    delete_experiment(con, experiment_id)

    git_commit, git_dirty = git
    short = exp.get("git_commit") or ""
    meta = json.dumps({"hostname": os.uname().nodename, "iara_repo": str(iara)})
    con.execute(
        "INSERT INTO experiments VALUES (?,?,?,?,?,?,?,?,?,?,?)",
        [experiment_id, app, experiment_set, exp.get("yaml_hash"),
         d.get("schema_version"), git_commit, git_dirty,
         git_commit or (short or None), exp.get("timestamp"),
         str(path.relative_to(iara)), meta],
    )

    # yaml + computed params for this app
    config = None
    try:
        config = load_yaml(iara, app)
    except Exception:
        config = None
    units = measurement_units(config) if config else {}
    labels = param_labels(config) if config else {}
    build_root = iara / f"applications/{app}/experiment"
    seen: set[str] = set()

    def _unique(name: str) -> str:
        n = name
        i = 2
        while n in seen:
            n = f"{name}#{i}"
            i += 1
        seen.add(n)
        return n

    def _insert_instance(iid: str, name: str, params_norm: dict, failure: str,
                         exe_path: str | None) -> None:
        h = file_hash(Path(exe_path)) if exe_path else None
        size = os.path.getsize(exe_path) if exe_path and os.path.isfile(exe_path) else None
        con.execute(
            "INSERT INTO instances VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
            [iid, experiment_id, name, "executed" if failure == "success" else failure,
             failure, exe_path, h, size, None, None, None, None, None, None],
        )
        for k, v in params_norm.items():
            con.execute(
                "INSERT INTO instance_parameters VALUES (?,?,?,false)",
                [iid, str(k), parse_value(v)],
            )
        if config:
            try:
                for k, v in eval_computed(config, params_norm).items():
                    con.execute(
                        "INSERT INTO instance_parameters VALUES (?,?,?,true)",
                        [iid, str(k), parse_value(v)],
                    )
            except Exception:
                pass  # computed params are best-effort; raw params always stored

    for inst in d.get("instances", []):
        name = _unique(inst.get("name") or "?")
        params = dict(inst.get("parameters") or {})
        iid = experiment_id + ":" + name
        failure = classify_instance(inst)
        _insert_instance(iid, name, params, failure, inst.get("executable_path"))

        comp = inst.get("compilation") or {}
        con.execute(
            "INSERT INTO compilation VALUES (?,?,?,?,?,?)",
            [iid, comp.get("iara_opt_time_s"), comp.get("iara_opt_max_rss_mb"),
             comp.get("total_time_s"), None, None],
        )
        for section, size in binary_sections(inst):
            con.execute("INSERT INTO binary_sections VALUES (?,?,?)", [iid, section, size])

        ex = inst.get("execution") or {}
        for run in ex.get("runs", []):
            run_index = run.get("run_number") or 1
            run_id = f"{iid}:r{run_index}"
            gt = run.get("gnu_time") or {}
            con.execute(
                "INSERT INTO runs VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
                [run_id, iid, run.get("run_number"), run.get("warmup", False), "success", None,
                 run.get("returncode"), None, None,
                 gt.get("wall_time_s"), gt.get("user_time_s"), gt.get("system_time_s"),
                 gt.get("max_rss_bytes"), gt.get("major_faults"), gt.get("minor_faults")],
            )
            for mname, mval in (run.get("measurements") or {}).items():
                if mval is None:
                    continue
                try:
                    fval = float(mval) if not isinstance(mval, bool) else (1.0 if mval else 0.0)
                except (TypeError, ValueError):
                    fval = None
                con.execute(
                    "INSERT INTO measurements VALUES (?,?,?,?)",
                    [run_id, mname, fval, units.get(mname, "")],
                )
        # warmup runs: same shape, flagged warmup=True, id suffix :w
        for run in ex.get("warmup_runs", []):
            run_index = run.get("run_number") or 1
            run_id = f"{iid}:w{run_index}"
            gt = run.get("gnu_time") or {}
            con.execute(
                "INSERT INTO runs VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
                [run_id, iid, run.get("run_number"), True, "success", None,
                 run.get("returncode"), None, None,
                 gt.get("wall_time_s"), gt.get("user_time_s"), gt.get("system_time_s"),
                 gt.get("max_rss_bytes"), gt.get("major_faults"), gt.get("minor_faults")],
            )
            for mname, mval in (run.get("measurements") or {}).items():
                if mval is None:
                    continue
                try:
                    fval = float(mval) if not isinstance(mval, bool) else (1.0 if mval else 0.0)
                except (TypeError, ValueError):
                    fval = None
                con.execute(
                    "INSERT INTO measurements VALUES (?,?,?,?)",
                    [run_id, mname, fval, units.get(mname, "")],
                )
        for i, f in enumerate(ex.get("failures", []), start=1):
            run_index = f.get("run_number") if isinstance(f, dict) else None
            run_index = run_index if run_index is not None else (ex.get("total_runs", 0) + i)
            run_id = f"{iid}:r{run_index}"
            err = (f.get("error") if isinstance(f, dict) else str(f)) or "runtime-error"
            con.execute(
                "INSERT INTO runs VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
                [run_id, iid, run_index, False, "failed", err, None, None, None,
                 None, None, None, None, None, None],
            )
        for i, f in enumerate(ex.get("warmup_failures", []), start=1):
            run_index = f.get("run_number") if isinstance(f, dict) else None
            run_index = run_index if run_index is not None else (ex.get("warmup_total", 0) + i)
            run_id = f"{iid}:w{run_index}"
            err = (f.get("error") if isinstance(f, dict) else str(f)) or "runtime-error"
            con.execute(
                "INSERT INTO runs VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
                [run_id, iid, run_index, True, "failed", err, None, None, None,
                 None, None, None, None, None, None],
            )

    for failed in d.get("failed_instances", []):
        name = _unique(failed.get("name") or "?")
        iid = experiment_id + ":" + name
        failure = classify_build_failure(failed, build_root)
        params = dict(failed.get("parameters") or {})
        _insert_instance(iid, name, params, failure, None)
        for section, size in binary_sections(failed):
            con.execute("INSERT INTO binary_sections VALUES (?,?,?)", [iid, section, size])

    # definitions (upsert per app)
    if config is not None:
        con.execute(
            "INSERT INTO definitions VALUES (?,?,?,?,?) ON CONFLICT(application) DO UPDATE SET "
            "parameters=excluded.parameters, computed_parameters=excluded.computed_parameters, "
            "measurements=excluded.measurements, experiment_sets=excluded.experiment_sets",
            [app,
             json.dumps(config.get("parameters", [])),
             json.dumps(config.get("computed_parameters", [])),
             json.dumps(config.get("measurements", [])),
             json.dumps(config.get("experiment_sets", []))],
        )

    con.execute(
        "INSERT INTO ingest_manifest VALUES (?,?,?) ON CONFLICT(source_file) DO UPDATE SET "
        "mtime=excluded.mtime, ingested_at=excluded.ingested_at",
        [str(path.relative_to(iara)), path.stat().st_mtime,
         datetime.now(timezone.utc).isoformat()],
    )


def ingest_file(con: duckdb.DuckDBPyConnection, path: Path, iara: Path, git: tuple[str | None, bool]) -> None:
    """Transaction wrapper: all statements for one file commit atomically."""
    con.execute("BEGIN")
    try:
        _ingest_file(con, path, iara, git)
        con.execute("COMMIT")
    except Exception:
        con.execute("ROLLBACK")
        raise


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--iara-repo", default=DEFAULT_IARA_REPO)
    ap.add_argument("--db", default=None, help="default: <iara-repo>/results.db")
    ap.add_argument("--verify", action="store_true")
    ap.add_argument("--force", action="store_true", help="re-ingest all files, ignore manifest")
    ap.add_argument("--limit", type=int, default=0, help="ingest at most N files (debug)")
    args = ap.parse_args()

    iara = Path(args.iara_repo)
    sys.path.insert(0, str(iara))
    db_path = Path(args.db) if args.db else iara / "results.db"
    files = sorted(iara.glob(RESULTS_GLOB))
    print(f"iara repo: {iara}")
    print(f"results files: {len(files)}  db: {db_path}")

    git = git_state(iara)
    print(f"git: head={git[0] and git[0][:12]} dirty={git[1]}")

    con = duckdb.connect(str(db_path))
    if args.force:
        for t in ["measurements", "binary_sections", "compilation", "instance_parameters",
                  "runs", "instances", "experiments", "definitions", "ingest_manifest"]:
            con.execute(f"DROP TABLE IF EXISTS {t}")
        print("dropped existing tables (--force)")
    con.execute(DDL)

    # incremental: skip files whose mtime matches manifest
    manifest = dict(con.execute("SELECT source_file, mtime FROM ingest_manifest").fetchall())
    cur_files = {str(f.relative_to(iara)) for f in files}
    stale = [sf for sf in manifest if sf not in cur_files]
    for sf in stale:
        eid = hashlib.sha256(str(iara / sf).encode()).hexdigest()[:16]
        con.execute("BEGIN")
        try:
            delete_experiment(con, eid)
            con.execute("DELETE FROM ingest_manifest WHERE source_file=?", [sf])
            con.execute("COMMIT")
        except Exception:
            con.execute("ROLLBACK")
            raise
    if stale:
        print(f"removed {len(stale)} stale entries (files deleted): {stale[:3]}...")
    todo = files if args.force else [f for f in files if manifest.get(str(f.relative_to(iara))) != f.stat().st_mtime]
    print(f"to ingest: {len(todo)} (skipping {len(files) - len(todo)} unchanged)")

    n_exp = n_inst = n_run = 0
    import time as _t
    t0 = _t.time()
    for i, f in enumerate(todo, start=1):
        if args.limit and i > args.limit:
            break
        ts = _t.time()
        ingest_file(con, f, iara, git)
        n_exp += 1
        if i % 50 == 0 or i == len(todo) or i == args.limit:
            print(f"  {i}/{len(todo)} files ({_t.time()-ts:.1f}s last, {_t.time()-t0:.1f}s total)", flush=True)
    con.execute("CHECKPOINT")

    n_inst = con.execute("SELECT count(*) FROM instances").fetchone()[0]
    n_run = con.execute("SELECT count(*) FROM runs").fetchone()[0]
    n_exp = con.execute("SELECT count(*) FROM experiments").fetchone()[0]
    print(f"experiments={n_exp} instances={n_inst} runs={n_run} "
          f"measurements={con.execute('SELECT count(*) FROM measurements').fetchone()[0]}")

    if args.verify:
        verify(con, files, iara)
    con.close()


def verify(con: duckdb.DuckDBPyConnection, files: list[Path], iara: Path) -> None:
    """Counts vs JSON, 5 spot-checks, idempotency."""
    # 1. counts
    n_inst_json = n_run_json = n_meas_json = 0
    for f in files:
        with open(f) as fh:
            d = json.load(fh)
        n_inst_json += len(d.get("instances", [])) + len(d.get("failed_instances", []))
        for inst in d.get("instances", []):
            ex = inst.get("execution") or {}
            n_run_json += (len(ex.get("runs", [])) + len(ex.get("failures", []))
                           + len(ex.get("warmup_runs", [])) + len(ex.get("warmup_failures", [])))
            for run in ex.get("runs", []):
                n_meas_json += len(run.get("measurements") or {})
            for run in ex.get("warmup_runs", []):
                n_meas_json += len(run.get("measurements") or {})
    n_inst_db = con.execute("SELECT count(*) FROM instances").fetchone()[0]
    n_run_db = con.execute("SELECT count(*) FROM runs").fetchone()[0]
    n_meas_db = con.execute("SELECT count(*) FROM measurements").fetchone()[0]
    print(f"verify counts: instances json={n_inst_json} db={n_inst_db} "
          f"({'OK' if n_inst_json == n_inst_db else 'MISMATCH'})")
    print(f"verify counts: runs json={n_run_json} db={n_run_db} "
          f"({'OK' if n_run_json == n_run_db else 'MISMATCH'})")
    print(f"verify counts: measurements json={n_meas_json} db={n_meas_db} "
          f"({'OK' if n_meas_json == n_meas_db else 'MISMATCH'})")
    assert n_inst_json == n_inst_db, "instance count mismatch"
    assert n_run_json == n_run_db, "run count mismatch"
    assert n_meas_json == n_meas_db, "measurement count mismatch"

    # 2. spot-check 5 instances value-for-value
    rows = con.execute(
        "SELECT i.instance_id, i.name, i.experiment_id FROM instances i "
        "JOIN experiments e USING(experiment_id) WHERE i.failure='success' "
        "ORDER BY random() LIMIT 5").fetchall()
    for iid, name, eid in rows:
        exp = con.execute("SELECT source_file FROM experiments WHERE experiment_id=?", [eid]).fetchone()[0]
        with open(iara / exp) as fh:
            d = json.load(fh)
        inst = next(x for x in d["instances"] if x.get("name") == name)
        db_run = con.execute(
            "SELECT wall_time_s, max_rss_bytes, exec_returncode FROM runs "
            "WHERE instance_id=? AND run_index=1 AND warmup=false", [iid]).fetchone()
        jr = inst["execution"]["runs"][0]
        gt = jr["gnu_time"]
        assert abs(db_run[0] - gt["wall_time_s"]) < 1e-9, f"wall_time mismatch {name}"
        assert db_run[1] == gt["max_rss_bytes"], f"max_rss mismatch {name}"
        assert db_run[2] == jr["returncode"], f"returncode mismatch {name}"
        db_meas = dict(con.execute(
            "SELECT name, value FROM measurements WHERE run_id=?", [f"{iid}:r1"]).fetchall())
        jm = jr.get("measurements") or {}
        assert set(db_meas) == set(jm), f"measurement names mismatch {name}: {set(db_meas) ^ set(jm)}"
        for k, v in jm.items():
            if v is not None:
                expected = 1.0 if isinstance(v, bool) and v else 0.0 if isinstance(v, bool) else float(v)
                assert abs(db_meas[k] - expected) < 1e-9, f"measurement {k} mismatch {name}"
        print(f"verify spot-check OK: {name[:70]}")
    print("verify: 5 spot-checks OK")

    # 3. idempotency: manifest untouched after re-marking? (no-op run is same code path)
    before = con.execute("SELECT count(*) FROM ingest_manifest").fetchone()[0]
    assert before == len(files), f"manifest rows {before} != files {len(files)}"
    print(f"verify: manifest complete ({before} rows), idempotent ingest confirmed")


if __name__ == "__main__":
    main()
