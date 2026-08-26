#!/usr/bin/env python3
"""Phase 1 UI: iara experiment DB explorer (Streamlit + DuckDB + Plotly).

Reads the central results.db (iara/results.db) produced by ingest.py.

Run:  streamlit run ui/app.py
      (or ./run_ui.sh; ssh -L 8501:localhost:8501 sorgan from laptop)

Selftest (no UI): python ui/app.py --selftest  — reproduces 3 means from a
known results JSON via the same query path and asserts match.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

IARA_REPO = Path("/scratch/pedro.ciambra/repos/iara")
DEFAULT_DB = IARA_REPO / "results.db"

UNIVERSAL_METRICS = {
    "wall_time_s": ("Wall time (s)", "s"),
    "user_time_s": ("User time (s)", "s"),
    "system_time_s": ("System time (s)", "s"),
    "max_rss_bytes": ("Max RSS (bytes)", "B"),
}

NULL_OPTION = "(null)"

DIMENSIONS = ["experiment_set", "git_commit", "iara_opt_hash", "timestamp",
              "failure", "status", "warmup", "run_index"]

BASE_SQL = """
SELECT
  e.experiment_id, e.application, e.experiment_set, e.git_commit,
  e.iara_opt_hash, e.timestamp, e.yaml_hash,
  i.instance_id, i.name AS instance_name, i.failure, i.status AS instance_status,
  i.executable_hash, i.exec_bin_size,
  r.run_id, r.run_index, r.warmup, r.status AS run_status, r.error,
  r.exec_returncode, r.exec_start, r.exec_finish,
  r.wall_time_s, r.user_time_s, r.system_time_s,
  r.max_rss_bytes, r.major_faults, r.minor_faults
FROM runs r
JOIN instances i USING (instance_id)
JOIN experiments e USING (experiment_id)
"""


def connect(db_path: Path = DEFAULT_DB):
    import duckdb
    return duckdb.connect(str(db_path), read_only=True)


# ── data layer ──────────────────────────────────────────────────────


def load_base(con) -> "pd.DataFrame":
    return con.execute(BASE_SQL).fetchdf()


def load_params(con, instance_ids) -> "pd.DataFrame":
    """Pivot instance_parameters -> one column per param (text values)."""
    import pandas as pd
    if not instance_ids:
        return pd.DataFrame()
    ids = list(instance_ids)
    df = con.execute(
        "SELECT instance_id, param_name, param_value, is_computed "
        "FROM instance_parameters WHERE instance_id IN (SELECT unnest(?))",
        [ids]).fetchdf()
    wide = df.pivot_table(index="instance_id", columns="param_name",
                          values="param_value", aggfunc="first")
    wide.columns = [str(c) for c in wide.columns]
    return wide.reset_index()


def load_measurements(con, run_ids) -> "pd.DataFrame":
    """Pivot measurements -> one column per measurement name (float)."""
    import pandas as pd
    if not run_ids:
        return pd.DataFrame()
    ids = list(run_ids)
    df = con.execute(
        "SELECT run_id, name, value FROM measurements "
        "WHERE run_id IN (SELECT unnest(?))", [ids]).fetchdf()
    wide = df.pivot_table(index="run_id", columns="name", values="value",
                          aggfunc="first")
    wide.columns = [str(c) for c in wide.columns]
    return wide.reset_index()


def measurement_names(con) -> list[str]:
    rows = con.execute("SELECT DISTINCT name FROM measurements ORDER BY name").fetchall()
    return [r[0] for r in rows]


def param_names(con, apps: list[str] | None = None) -> list[tuple[str, bool]]:
    """[(param_name, is_computed)] present in data (optionally filtered by app)."""
    sql = ("SELECT DISTINCT param_name, is_computed FROM instance_parameters")
    if apps:
        sql += (" WHERE instance_id IN (SELECT instance_id FROM instances "
                "WHERE experiment_id IN (SELECT experiment_id FROM experiments "
                f"WHERE application IN ({','.join('?' * len(apps))})))")
    return con.execute(sql, apps or []).fetchall()


def distinct_values(con, param: str, apps: list[str] | None = None) -> list[str]:
    sql = ("SELECT DISTINCT param_value FROM instance_parameters WHERE param_name=? ")
    args: list = [param]
    if apps:
        sql += (" AND instance_id IN (SELECT instance_id FROM instances "
                "WHERE experiment_id IN (SELECT experiment_id FROM experiments "
                f"WHERE application IN ({','.join('?' * len(apps))})))")
        args += apps
    rows = con.execute(sql, args).fetchall()
    out = [str(r[0]) for r in rows if r[0] is not None]
    # instances that have NO row for this param (e.g. preesm lacks iara params)
    n_sql = ("SELECT count(*) FROM instances i "
             "WHERE NOT EXISTS (SELECT 1 FROM instance_parameters ip "
             "WHERE ip.instance_id = i.instance_id AND ip.param_name = ?)")
    n_args: list = [param]
    if apps:
        n_sql += (" AND i.experiment_id IN (SELECT experiment_id FROM experiments "
                  f"WHERE application IN ({','.join('?' * len(apps))}))")
        n_args += apps
    if con.execute(n_sql, n_args).fetchone()[0] > 0:
        out.append(NULL_OPTION)
    return out


def date_range(con) -> tuple[str, str]:
    row = con.execute(
        "SELECT min(try_cast(timestamp AS TIMESTAMP)), max(try_cast(timestamp AS TIMESTAMP)) "
        "FROM experiments").fetchone()
    return (str(row[0]) if row[0] else "", str(row[1]) if row[1] else "")


# ── aggregation ─────────────────────────────────────────────────────


def aggregate(df, metric: str, groups: list[str], agg: str,
              error: str) -> "pd.DataFrame":
    """Group per-run rows by groups (x + facet/color dims), aggregate metric."""
    import numpy as np
    import pandas as pd
    g = df.groupby(groups, dropna=False)[metric]
    val = g.median() if agg == "median" else g.mean()
    out = val.reset_index(name="value")
    counts = df.groupby(groups, dropna=False).size().reset_index(name="count")
    out = out.merge(counts, on=groups)
    if error == "std":
        out["error"] = df.groupby(groups, dropna=False)[metric].std().reset_index(drop=True)
    elif error == "sem":
        out["error"] = (df.groupby(groups, dropna=False)[metric].std()
                        .div(np.sqrt(df.groupby(groups, dropna=False).size()))
                        .reset_index(drop=True))
    else:
        out["error"] = None
    return out


# ── UI ──────────────────────────────────────────────────────────────


def run_ui(db_path: Path) -> None:
    import pandas as pd
    import streamlit as st
    import plotly.express as px

    st.set_page_config(page_title="iara experiment DB", layout="wide")
    st.title("iara experiment DB explorer")
    st.caption(f"DB: {db_path}")

    con = connect(db_path)
    base = load_base(con)
    meas_names = measurement_names(con)
    dmin, dmax = date_range(con)

    if base.empty:
        st.error("DB empty. Run ingest.py first.")
        return

    with st.sidebar:
        st.header("Filters")
        if dmin and dmax:
            import datetime as _dt
            lo, hi = _dt.date.fromisoformat(dmin[:10]), _dt.date.fromisoformat(dmax[:10])
            dsel = st.date_input("Date range", (lo, hi), min_value=lo, max_value=hi)
        else:
            dsel = None
        apps = st.multiselect("Application", sorted(base["application"].unique()),
                              key="apps")
        app_filter = apps or None
        set_opts = sorted(base.loc[base["application"].isin(apps), "experiment_set"].unique()) if apps \
            else sorted(base["experiment_set"].unique())
        sets = st.multiselect("Experiment set", set_opts, key="sets")
        git_opts = sorted(base["git_commit"].dropna().unique())
        git = st.multiselect("git commit", git_opts, key="git")
        hash_opts = sorted(base["iara_opt_hash"].dropna().unique())
        ihash = st.multiselect("iara-opt hash", hash_opts, key="ihash")
        fail_opts = sorted(base["failure"].dropna().unique())
        fails = st.multiselect("Failure", fail_opts, key="fails")

        st.divider()
        show_computed = st.checkbox("Show computed params in filters", value=True)
        pnames = param_names(con, app_filter)
        pnames = [(n, c) for n, c in pnames if c or show_computed]
        param_sel: dict[str, list[str]] = {}
        for pname, is_comp in pnames:
            vals = distinct_values(con, pname, app_filter)
            if len(vals) > 200:
                continue
            lab = f"{pname} {'(computed)' if is_comp else ''}"
            sel = st.multiselect(lab, sorted(vals, key=str), key=f"p:{pname}")
            if sel:
                param_sel[pname] = sel

        st.divider()
        metric = st.selectbox(
            "Metric", list(UNIVERSAL_METRICS) + meas_names,
            format_func=lambda m: UNIVERSAL_METRICS.get(m, (m, ""))[0], key="metric")
        chart = st.radio("Chart", ["bar", "line", "scatter"], horizontal=True, key="chart")
        x_opts = ["instance"] + DIMENSIONS + [n for n, _ in pnames]
        x = st.selectbox("X axis", x_opts, index=0, key="x")
        facet_opts = ["(none)"] + [n for n, _ in pnames if n != x and n != "instance"]
        facet_col = st.selectbox("Facet column (outer)", facet_opts, key="facet_col")
        facet_row = st.selectbox("Facet row", facet_opts, key="facet_row")
        color = st.selectbox("Color (inner)", facet_opts, key="color")
        st.caption("Nesting priority: facet column > facet row > color (plotly-style). "
                   "Swap properties between roles to reorder grouping.")
        agg = st.radio("Aggregate", ["mean", "median"], horizontal=True, key="agg")
        err = st.selectbox("Error bars", ["none", "std", "sem"], key="err")
        include_failed = st.checkbox("Include failed runs", value=False)
        run_sel = st.checkbox("Run-level (one point per repetition)", value=False)

    # ── filtering ──
    df = base
    if dsel and len(dsel) == 2:
        df["_date"] = df["timestamp"].str[:10]
        df = df[(df["_date"] >= str(dsel[0])) & (df["_date"] <= str(dsel[1]))]
    if app_filter:
        df = df[df["application"].isin(app_filter)]
    if sets:
        df = df[df["experiment_set"].isin(sets)]
    if git:
        df = df[df["git_commit"].isin(git)]
    if ihash:
        df = df[df["iara_opt_hash"].isin(ihash)]
    if fails:
        df = df[df["failure"].isin(fails)]
    if not include_failed:
        df = df[df["run_status"] == "success"]

    if df.empty:
        st.warning("No runs match filters.")
        return

    # pivot params + measurements onto run rows
    params_wide = load_params(con, df["instance_id"].tolist())
    meas_wide = load_measurements(con, df["run_id"].tolist())
    if not params_wide.empty:
        df = df.merge(params_wide, on="instance_id", how="left")
    if not meas_wide.empty:
        df = df.merge(meas_wide, on="run_id", how="left")

    for pname, sel in param_sel.items():
        if not sel or pname not in df.columns:
            continue
        mask = df[pname].isna() if NULL_OPTION in sel else None
        vals = [v for v in sel if v != NULL_OPTION]
        if vals:
            m = df[pname].astype(str).isin(vals)
            mask = m if mask is None else (mask | m)
        df = df[mask]

    # nulls as visible groups in charts
    for col in (x, facet_col, facet_row, color):
        if col != "(none)" and col != "instance" and col in df.columns:
            df[col] = df[col].fillna(NULL_OPTION)

    # instance label: visualizer naming convention (short_name = name minus
    # <app>_<set>_ prefix; config_key = minus <app>_<set>_<scheduler>_ for ordering)
    df["short_name"] = df["instance_name"].str.replace(r"^[^_]+_[^_]+_", "", regex=True)
    df["config_key"] = df["instance_name"].str.replace(r"^[^_]+_[^_]+_[^_]+_", "", regex=True)
    df["instance"] = df["short_name"]

    st.subheader("Data")
    st.write(f"{len(df)} runs after filters")

    if metric not in df.columns:
        st.warning(f"Metric '{metric}' has no values for this selection.")
        return

    if run_sel:
        plot_df = df
        y = metric
        err_col = None
    else:
        groups = [x] + [c for c in (facet_col, facet_row, color) if c != "(none)"]
        agg_df = aggregate(df, metric, groups, agg, err)
        plot_df = agg_df
        y = "value"
        err_col = "error" if err != "none" else None

    colr = None if color == "(none)" else color
    fcol = None if facet_col == "(none)" else facet_col
    frow = None if facet_row == "(none)" else facet_row

    # visualizer-compatible ordering: bars share config adjacent, then by short name
    cat_orders = None
    if x == "instance" and "config_key" in plot_df.columns:
        order = (plot_df.drop_duplicates("instance")
                 .sort_values(["config_key", "instance"])["instance"].tolist())
        cat_orders = {"instance": order}

    fig = px.scatter(plot_df, x=x, y=y, color=colr, error_y=err_col,
                     facet_col=fcol, facet_row=frow, title=f"{metric} by {x}",
                     labels={"value": metric}, category_orders=cat_orders)
    if chart == "bar":
        fig = px.bar(plot_df, x=x, y=y, color=colr, error_y=err_col,
                     facet_col=fcol, facet_row=frow, title=f"{metric} by {x}",
                     labels={"value": metric}, category_orders=cat_orders)
    elif chart == "line":
        fig = px.line(plot_df, x=x, y=y, color=colr, error_y=err_col,
                      facet_col=fcol, facet_row=frow, title=f"{metric} by {x}",
                      labels={"value": metric}, category_orders=cat_orders)
    st.plotly_chart(fig, use_container_width=True)

    st.download_button("Download CSV", plot_df.to_csv(index=False).encode(),
                       file_name="iara-filtered.csv")
    with st.expander("Table"):
        st.dataframe(plot_df)


# ── selftest (no UI) ────────────────────────────────────────────────


def selftest(db_path: Path) -> None:
    """Reproduce 3 instance-mean wall times from a known JSON via the UI query path."""
    import pandas as pd
    con = connect(db_path)
    base = load_base(con)
    params_wide = load_params(con, base["instance_id"].tolist())
    base = base.merge(params_wide, on="instance_id", how="left")
    meas_wide = load_measurements(con, base["run_id"].tolist())
    base = base.merge(meas_wide, on="run_id", how="left")

    src = IARA_REPO / "applications/05-cholesky/experiment/results/results_2026-08-13T01-13-42Z.json"
    with open(src) as fh:
        d = json.load(fh)

    checks = [
        {"application": "05-cholesky", "experiment_set": "cores",
         "matrix-size": "20160", "number-of-blocks": "16", "scheduler": "omp-for"},
        {"application": "05-cholesky", "experiment_set": "cores",
         "matrix-size": "20160", "number-of-blocks": "16", "scheduler": "vf-omp"},
        {"application": "05-cholesky", "experiment_set": "cores",
         "matrix-size": "20160", "number-of-blocks": "16", "scheduler": "vf-enkits"},
    ]
    for c in checks:
        sub = base[(base["application"] == c["application"]) &
                   (base["experiment_set"] == c["experiment_set"]) &
                   (base["matrix-size"].astype(str) == c["matrix-size"]) &
                   (base["number-of-blocks"].astype(str) == c["number-of-blocks"]) &
                   (base["scheduler"].astype(str) == c["scheduler"])]
        db_mean = float(sub["wall_time_s"].mean())
        jmeans = []
        for inst in d["instances"]:
            p = inst.get("parameters") or {}
            if (str(p.get("matrix-size")) == c["matrix-size"]
                    and str(p.get("number-of-blocks")) == c["number-of-blocks"]
                    and str(p.get("scheduler")) == c["scheduler"]):
                for r in inst["execution"]["runs"]:
                    jmeans.append(r["gnu_time"]["wall_time_s"])
        jmean = sum(jmeans) / len(jmeans) if jmeans else float("nan")
        assert abs(db_mean - jmean) < 1e-9, f"MISMATCH {c}: db={db_mean} json={jmean}"
        print(f"selftest OK: {c['scheduler']} wall_time mean db={db_mean:.6f} json={jmean:.6f} "
              f"({len(jmeans)} runs)")
    print("selftest: 3/3 OK")


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--db", default=str(DEFAULT_DB))
    ap.add_argument("--selftest", action="store_true")
    args = ap.parse_args()
    if args.selftest:
        selftest(Path(args.db))
    else:
        run_ui(Path(args.db))
