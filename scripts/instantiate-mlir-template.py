#!/usr/bin/env python3
"""
instantiate-mlir-template — substitute «expr» markers in template files.

Usage:
    python3 scripts/instantiate-mlir-template \\
        <params.json> \\
        <template1.mlir.template> [<template2.mlir.template> ...] \\
        [<wrappers.c.template> ...] \\
        [KEY=VALUE ...] \\
        -o <output.mlir> \\
        [--wrappers-out <wrappers.c>]

Arguments:
    params.json            Application-specific parameter mapping (see below).
    *.mlir.template        One or more MLIR template files.  Processed in order;
                           all actors are coalesced into a single output module.
    *.c.template           One or more C wrapper template files (optional).
                           Concatenated and substituted; written to --wrappers-out.
    KEY=VALUE              Values for the experiment parameters listed in the JSON
                           "parameters" section (e.g. NUM_CORES=4).
    -o / --output PATH     MLIR output path (default: topology.mlir).
    --wrappers-out PATH    C wrappers output path (required if *.c.template given).

params.json schema:
    {
      "parameters":  { "YAML_NAME": "preesm_name", ... },
      "constants":   { "preesm_name": <int>, ... },
      "computed_parameters": [
        {"name": "preesm_name", "expr": "<Preesm expression>"},
        ...
      ]
    }

    - "parameters": maps experiment YAML parameter names to Preesm names.
      The corresponding KEY=VALUE must be supplied on the command line.
    - "constants": Preesm parameters with fixed values (not varied by experiment).
    - "computed_parameters": evaluated in order; may reference any previously
      defined name.  Expressions use Preesm syntax (see eval_expr below).
"""

import json
import math
import re
import sys
from pathlib import Path


# ── Preesm expression evaluator ───────────────────────────────────────────────

def _iif(cond, a, b):
    """Preesm if(cond, then, else) — evaluates both branches eagerly."""
    return a if cond else b


def geo_sum(a, r, n):
    """Geometric sum: a * (1 + r + r^2 + ... + r^{n-1})."""
    r, n = float(r), int(n)
    if r == 1.0:
        return int(a * n)
    return int(a * (1.0 - r ** n) / (1.0 - r))


def pow_div_max(b, n):
    """Largest k such that b^k divides n."""
    b, n = int(b), int(n)
    k = 0
    while n % b == 0:
        n //= b
        k += 1
    return k


SAFE_BUILTINS: dict = {
    "__builtins__": {},
    "_iif":        _iif,
    "geo_sum":     geo_sum,
    "pow_div_max": pow_div_max,
    "floor":       math.floor,
    "ln":          math.log,
    "ceil":        math.ceil,
    "sqrt":        math.sqrt,
    "min":         min,
    "max":         max,
    "abs":         abs,
}


def eval_expr(expr: str, ctx: dict) -> int:
    """
    Evaluate a Preesm expression and return an integer.

    Transformations applied before eval():
      - '^'  → '**'   (Preesm power operator)
      - 'if(' → '_iif('  (Preesm conditional — 'if' is a Python keyword)
    """
    py_expr = expr.replace("^", "**").replace("if(", "_iif(")
    try:
        result = eval(py_expr, SAFE_BUILTINS, ctx)  # noqa: S307
    except Exception as exc:
        raise ValueError(f"Cannot evaluate «{expr}»: {exc}") from exc
    return int(result)


# ── Template parsing ───────────────────────────────────────────────────────────

def extract_actor_body(template_text: str) -> str:
    """
    Strip the leading comment block and the outer 'module { ... } // module'
    wrapper, returning the inner lines (iara.actor definitions).
    """
    lines = template_text.splitlines()
    inner: list[str] = []
    in_module = False
    for line in lines:
        stripped = line.strip()
        # Comment lines at the top of the file (parameter documentation)
        if stripped.startswith("//") and not in_module:
            continue
        if stripped == "module {":
            in_module = True
            continue
        if in_module and stripped.startswith("} // module"):
            in_module = False
            continue
        if in_module:
            inner.append(line)
    return "\n".join(inner)


# ── Substitution ──────────────────────────────────────────────────────────────

def substitute_markers(text: str, ctx: dict, label: str) -> str:
    """
    Replace all «expr» markers in *text* with evaluated integers.
    *label* is used in error messages (e.g. "MLIR", "C wrappers").
    Exits the process on any substitution error.
    """
    errors: list[str] = []

    def repl(m: re.Match) -> str:
        expr = m.group(1)
        try:
            return str(eval_expr(expr, ctx))
        except ValueError as exc:
            errors.append(str(exc))
            return f"«{expr}»"

    result = re.sub(r"«([^»]+)»", repl, text)

    if errors:
        print(f"{label}: substitution failed with {len(errors)} error(s):", file=sys.stderr)
        for e in errors:
            print(f"  {e}", file=sys.stderr)
        sys.exit(1)

    remaining = re.findall(r"«[^»]+»", result)
    if remaining:
        print(
            f"{label}: BUG: {len(remaining)} unsubstituted marker(s) remain "
            f"(first: {remaining[0]!r})",
            file=sys.stderr,
        )
        sys.exit(1)

    return result


# ── Main ──────────────────────────────────────────────────────────────────────

def main() -> None:
    # ── Argument parsing ──────────────────────────────────────────────────────
    # We do manual pre-processing: positionals are either the JSON file,
    # *.mlir.template files, or KEY=VALUE pairs.  -o/--output is the sink.
    raw_args = sys.argv[1:]

    json_file: Path | None = None
    templates: list[Path] = []
    c_templates: list[Path] = []
    kv_args: dict[str, str] = {}
    output_path: Path = Path("topology.mlir")
    wrappers_out: Path | None = None

    i = 0
    while i < len(raw_args):
        arg = raw_args[i]
        if arg in ("-o", "--output"):
            i += 1
            if i >= len(raw_args):
                print("Error: -o/--output requires a path", file=sys.stderr)
                sys.exit(1)
            output_path = Path(raw_args[i])
        elif arg.startswith(("-o=", "--output=")):
            output_path = Path(arg.split("=", 1)[1])
        elif arg == "--wrappers-out":
            i += 1
            if i >= len(raw_args):
                print("Error: --wrappers-out requires a path", file=sys.stderr)
                sys.exit(1)
            wrappers_out = Path(raw_args[i])
        elif arg.startswith("--wrappers-out="):
            wrappers_out = Path(arg.split("=", 1)[1])
        elif arg in ("-h", "--help"):
            print(__doc__)
            sys.exit(0)
        elif "=" in arg and not arg.startswith("-"):
            key, val = arg.split("=", 1)
            kv_args[key] = val
        elif arg.endswith(".mlir.template"):
            templates.append(Path(arg))
        elif arg.endswith(".c.template"):
            c_templates.append(Path(arg))
        elif arg.endswith(".json"):
            if json_file is not None:
                print("Error: multiple JSON files specified", file=sys.stderr)
                sys.exit(1)
            json_file = Path(arg)
        else:
            print(f"Error: unexpected argument {arg!r}", file=sys.stderr)
            sys.exit(1)
        i += 1

    if json_file is None:
        print("Error: no params.json specified", file=sys.stderr)
        sys.exit(1)
    if not templates:
        print("Error: no .mlir.template files specified", file=sys.stderr)
        sys.exit(1)
    if c_templates and wrappers_out is None:
        print("Error: .c.template files given but --wrappers-out not specified", file=sys.stderr)
        sys.exit(1)

    # ── Load JSON and build evaluation context ─────────────────────────────────
    try:
        spec = json.loads(json_file.read_text(encoding="utf-8"))
    except Exception as exc:
        print(f"Error reading {json_file}: {exc}", file=sys.stderr)
        sys.exit(1)

    ctx: dict[str, int | float] = {}

    # 1. Fixed constants
    for preesm_name, value in spec.get("constants", {}).items():
        ctx[preesm_name] = int(value)

    # 2. Experiment parameters (from KEY=VALUE command-line args)
    for yaml_name, preesm_name in spec.get("parameters", {}).items():
        if yaml_name not in kv_args:
            print(
                f"Error: required parameter {yaml_name!r} not provided "
                f"(expected {yaml_name}=<value>)",
                file=sys.stderr,
            )
            sys.exit(1)
        try:
            ctx[preesm_name] = int(kv_args[yaml_name])
        except ValueError:
            print(
                f"Error: parameter {yaml_name}={kv_args[yaml_name]!r} "
                f"is not an integer",
                file=sys.stderr,
            )
            sys.exit(1)

    # 3. Computed parameters (evaluated in order)
    for entry in spec.get("computed_parameters", []):
        name = entry["name"]
        expr = entry["expr"]
        try:
            ctx[name] = eval_expr(expr, ctx)
        except ValueError as exc:
            print(f"Error evaluating computed parameter {name!r}: {exc}", file=sys.stderr)
            sys.exit(1)

    # ── Read and coalesce MLIR template files ─────────────────────────────────
    actor_bodies: list[str] = []
    for tmpl_path in templates:
        try:
            text = tmpl_path.read_text(encoding="utf-8")
        except Exception as exc:
            print(f"Error reading {tmpl_path}: {exc}", file=sys.stderr)
            sys.exit(1)
        actor_bodies.append(extract_actor_body(text))

    combined = "module {\n" + "\n".join(actor_bodies) + "\n} // module\n"

    mlir_result = substitute_markers(combined, ctx, "MLIR")

    # ── Write MLIR output ────────────────────────────────────────────────────
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(mlir_result, encoding="utf-8")
    print(f"Written: {output_path}")

    # ── Process C wrapper templates (if any) ─────────────────────────────────
    if c_templates and wrappers_out is not None:
        c_parts: list[str] = []
        for tmpl_path in c_templates:
            try:
                c_parts.append(tmpl_path.read_text(encoding="utf-8"))
            except Exception as exc:
                print(f"Error reading {tmpl_path}: {exc}", file=sys.stderr)
                sys.exit(1)

        c_combined = "\n".join(c_parts)
        c_result = substitute_markers(c_combined, ctx, "C wrappers")

        wrappers_out.parent.mkdir(parents=True, exist_ok=True)
        wrappers_out.write_text(c_result, encoding="utf-8")
        print(f"Written: {wrappers_out}")


if __name__ == "__main__":
    main()
