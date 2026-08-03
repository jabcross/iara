#!/usr/bin/env python3
"""
pi-to-iara.py — convert Preesm .pi (GraphML) to IaRa .mlir.template

One .mlir.template per .pi file; subgraph .pi files are processed recursively.
Template markers «PARAM» indicate values substituted at experiment time.

Usage:
    python3 pi-to-iara.py <input.pi> [--output-dir DIR] [--types JSON]

The --types argument accepts a JSON dict to extend/override the default type map,
e.g.: --types '{"MyStruct": "!llvm.struct<(i32, f32)>"}'
"""

import xml.etree.ElementTree as ET
import sys
import argparse
import json
import re
from pathlib import Path
from collections import deque

# ── Default type map ──────────────────────────────────────────────────────────

DEFAULT_TYPE_MAP: dict[str, str] = {
    "float":         "f32",
    "int":           "i32",
    "unsigned char": "i8",
    "char":          "i8",
    "SiftKpt": (
        "!llvm.struct<(i32, i32, f32, f32, f32, f32, f32, "
        "f32, f32, f32, f32, array<128 x f32>)>"
    ),
}


def make_tensor_type(expr: str, preesm_type: str, type_map: dict) -> str:
    """Return 'tensor<NxT>' (literal) or 'tensor<«expr»xT>' (parametric)."""
    ptype = preesm_type.strip()
    iara_type = type_map.get(ptype)
    if iara_type is None:
        raise ValueError(
            f"Unknown Preesm type: {ptype!r}. "
            f"Add to --types or DEFAULT_TYPE_MAP."
        )
    expr = expr.strip()
    try:
        n = int(float(expr))
        return f"tensor<{n}x{iara_type}>"
    except (ValueError, TypeError):
        return f"tensor<«{expr}»x{iara_type}>"


def make_template_val(expr: str) -> str:
    """Return expr as a literal int or a «expr» template marker."""
    expr = expr.strip()
    try:
        return str(int(float(expr)))
    except (ValueError, TypeError):
        return f"«{expr}»"


# ── XML helpers ───────────────────────────────────────────────────────────────

def parse_graphml(pi_path: Path):
    """
    Parse a .pi GraphML file.

    Returns (ns_prefix, graph_element, nt_func) where nt_func(tag) produces
    the namespace-qualified tag name.
    """
    tree = ET.parse(pi_path)
    root = tree.getroot()
    ns = root.tag.split("}")[0][1:] if root.tag.startswith("{") else ""

    def nt(tag: str) -> str:
        return f"{{{ns}}}{tag}" if ns else tag

    graph = root.find(nt("graph"))
    if graph is None:
        raise ValueError(f"No <graph> element found in {pi_path}")
    return ns, graph, nt


def get_data(el, key: str, nt) -> str | None:
    """Return text of <data key='key'> child, or None."""
    for d in el.findall(nt("data")):
        if d.get("key") == key and d.text:
            return d.text.strip()
    return None


def get_ports(node, kind: str, nt) -> list[dict]:
    """Return [{name, expr}, …] for <port kind=kind> children."""
    return [
        {"name": p.get("name", ""), "expr": p.get("expr", "1")}
        for p in node.findall(nt("port"))
        if p.get("kind") == kind
    ]


# ── Memory scripts (buffer pairing) ───────────────────────────────────────────

# Preesm pairs an actor's input/output buffers via per-actor "memory scripts"
# (BeanShell files attached with <data key="memoryScript">, e.g.
# Code/mem_scripts/extract.bsh).  Each `o_<out>.matchWith(off, i_<in>, off,
# size)` call aliases an output byte-range with an input byte-range.  See
# Karol Desnos, "Memory Study and Dataflow Representations for Rapid
# Prototyping of Signal Processing Applications on MPSoCs" (PhD, INSA Rennes,
# 2014), ch. 5.  The p2iaw wrapper collapses such a pair into a single inout
# argument (kernel written in-place-safe); the port annotations
# (READ_ONLY/WRITE_ONLY/default) are the safety contract, not the pairing rule.
#
# Only clean whole-buffer pairs are collapsed: a '<base>_in'/'<base>_out' port
# pair (the Preesm two-port-buffer convention) matched at offset 0 on both
# sides.  Sub-range/overlapping matches (e.g. the Split special-node
# split.bsh) are NOT inout pairs — the wrapper keeps those buffers separate.
_MEM_SCRIPT_MATCH_RE = re.compile(
    r"o_(\w+)\.matchWith\(\s*([^,]+?)\s*,\s*i_(\w+)\s*,\s*([^,]+?)\s*,\s*[^)]+\)"
)


def parse_memory_script(script_text: str) -> list[tuple[str, str]]:
    """Return [(in_port, out_port), …] whole-buffer inout pairs from a script.

    A pair qualifies iff it follows the '<base>_in'/'<base>_out' convention,
    the base matches, and both match offsets are literal zero (whole-buffer
    alias at the same base pointer, as the IaRa runtime inout assumes).
    """
    pairs: list[tuple[str, str]] = []
    for out_port, src_off, in_port, dst_off in _MEM_SCRIPT_MATCH_RE.findall(script_text):
        src_off = src_off.strip()
        dst_off = dst_off.strip()
        if src_off != "0" or dst_off != "0":
            continue
        if not (in_port.endswith("_in") and out_port.endswith("_out")):
            continue
        if in_port[:-3] != out_port[:-4]:
            continue
        pairs.append((in_port, out_port))
    return pairs


def resolve_file_up(rel: str, start: Path) -> Path | None:
    """Resolve *rel* by walking up from *start*.parent like Preesm's project
    root-relative graph_desc paths (Algo/ and Code/ are siblings)."""
    parent = start.parent
    while True:
        candidate = (parent / rel).resolve()
        if candidate.exists():
            return candidate
        if parent == parent.parent:
            return None
        parent = parent.parent


# ── Core conversion ───────────────────────────────────────────────────────────

def convert_pi(pi_path: Path, type_map: dict, p2iaw: bool = True) -> tuple[str, str, dict]:
    """
    Convert a single .pi file to .mlir.template content.

    Returns:
        (mlir_text, graph_name, {actor_id: subgraph_pi_path})

    p2iaw: when True (default), leaf actor names get the 'p2iaw_' wrapper
    prefix and the actor's memory-script-matched '<base>_in'/'<base>_out'
    whole-buffer pairs are merged into a single 'inout' port, matching the
    preesm2iara.c wrappers' calling convention (Preesm-faithful).
    """
    ns, graph, nt = parse_graphml(pi_path)
    graph_name = get_data(graph, "name", nt) or pi_path.stem

    # ── Classify nodes ────────────────────────────────────────────────────────
    cfg_in_ifaces: list[str] = []          # ordered list of external param ids
    params: list[tuple[str, str]] = []     # [(id, expr), …]
    src_nodes: dict[str, dict] = {}        # id → {name, expr}
    snk_nodes: dict[str, dict] = {}        # id → {name, expr}
    actor_nodes: dict[str, dict] = {}      # id → {loop_name, graph_desc, in_ports, out_ports}
    broadcast_nodes: dict[str, dict] = {}  # id → {in_ports, out_ports}
    delay_nodes: dict[str, str] = {}       # id → expr

    for node in graph.findall(nt("node")):
        kind = node.get("kind", "")
        nid  = node.get("id", "")

        if kind == "cfg_in_iface":
            cfg_in_ifaces.append(nid)

        elif kind == "param":
            params.append((nid, node.get("expr", "")))

        elif kind == "src":
            out_ports = get_ports(node, "output", nt)
            src_nodes[nid] = out_ports[0] if out_ports else {"name": nid, "expr": "1"}

        elif kind == "snk":
            in_ports = get_ports(node, "input", nt)
            snk_nodes[nid] = in_ports[0] if in_ports else {"name": nid, "expr": "1"}

        elif kind == "actor":
            gd      = get_data(node, "graph_desc", nt) or ""
            loop_el = node.find(nt("loop"))
            loop_name = loop_el.get("name") if loop_el is not None else nid
            is_subgraph = gd.endswith(".pi")

            if loop_el is not None and not is_subgraph:
                # Leaf actor: parameter ORDER comes from <loop><param> elements,
                # which reflects the actual C function signature.  The <port>
                # elements hold the SDF rate expressions; we merge by name.
                port_expr_map: dict[str, str] = {}
                for port in node.findall(nt("port")):
                    if port.get("kind") in ("input", "output"):
                        port_expr_map[port.get("name", "")] = port.get("expr", "1")

                in_ports_ordered: list[dict] = []
                out_ports_ordered: list[dict] = []
                for param in loop_el.findall(nt("param")):
                    if param.get("isConfig") != "false":
                        continue
                    pname = param.get("name", "")
                    direction = param.get("direction", "")
                    pexpr = port_expr_map.get(pname, "1")
                    entry = {"name": pname, "expr": pexpr}
                    if direction == "IN":
                        in_ports_ordered.append(entry)
                    elif direction == "OUT":
                        out_ports_ordered.append(entry)

                in_ports  = in_ports_ordered
                out_ports = out_ports_ordered
                inout_ports: list[dict] = []
                if p2iaw:
                    # Preesm-faithful inout collapse: driven by the actor's
                    # memory script, not by port annotations or a name allowlist.
                    mem_script = get_data(node, "memoryScript", nt)
                    if mem_script:
                        script_path = resolve_file_up(mem_script, pi_path)
                        if script_path is None:
                            print(
                                f"Warning: cannot resolve memory script "
                                f"{mem_script!r} for {loop_name!r}",
                                file=sys.stderr,
                            )
                        else:
                            out_names = {p["name"] for p in out_ports}
                            in_by_name = {p["name"]: p for p in in_ports}
                            collapsed_in: set[str] = set()
                            for in_port, out_port in parse_memory_script(
                                script_path.read_text(encoding="utf-8")
                            ):
                                if in_port in in_by_name and out_port in out_names:
                                    inout_ports.append(
                                        {"name": in_port[:-3], "expr": in_by_name[in_port]["expr"]}
                                    )
                                    collapsed_in.add(in_port)
                            if collapsed_in:
                                in_ports = [
                                    p for p in in_ports if p["name"] not in collapsed_in
                                ]
            else:
                # Subgraph actor (no <loop>): use <port> element order.
                in_ports  = get_ports(node, "input",  nt)
                out_ports = get_ports(node, "output", nt)
                inout_ports = []

            actor_nodes[nid] = {
                "loop_name":  ("p2iaw_" + loop_name)
                              if (p2iaw and loop_el is not None and not is_subgraph)
                              else loop_name,
                "graph_desc": gd,
                "in_ports":   in_ports,
                "inout_ports": inout_ports,
                "out_ports":  out_ports,
            }

        elif kind == "broadcast":
            broadcast_nodes[nid] = {
                "in_ports":  get_ports(node, "input",  nt),
                "out_ports": get_ports(node, "output", nt),
            }

        elif kind == "delay":
            delay_nodes[nid] = node.get("expr", "0")

        elif kind == "roundbuffer":
            raise NotImplementedError(
                f"roundbuffer node {nid!r} encountered in {pi_path}. "
                "pi-to-iara.py does not support roundbuffer nodes."
            )

        # Silently skip other kinds (e.g., unknown future extensions)

    # ── Collect fifo edges ────────────────────────────────────────────────────
    # (dependency edges are skipped — they carry config params, not data)
    fifos: list[dict] = []
    for edge in graph.findall(nt("edge")):
        if edge.get("kind") != "fifo":
            continue
        delay_ref  = get_data(edge, "delay", nt)
        delay_expr: str | None = None
        if delay_ref:
            # delay node's expr gives the number of initial tokens
            delay_expr = delay_nodes.get(delay_ref) or edge.get("expr") or "0"
        fifos.append({
            "source":     edge.get("source"),
            "sourceport": edge.get("sourceport"),
            "target":     edge.get("target"),
            "targetport": edge.get("targetport"),
            "type":       edge.get("type", "float"),
            "delay_expr": delay_expr,  # None → no delay attribute
        })

    # ── Assign a unique index N to each fifo ──────────────────────────────────
    for i, fe in enumerate(fifos, start=1):
        fe["_n"] = i

    # ── SSA name helpers ──────────────────────────────────────────────────────

    def source_ssa(fe: dict) -> str:
        """SSA name for the fifo's source value (%srcId_i_i or %eN_s)."""
        src = fe["source"]
        if src in src_nodes:
            return f"%{src}_i_i"
        return f"%e{fe['_n']}_s"

    def drain_ssa(fe: dict) -> str:
        """SSA name for the fifo's drain (%eN_d or %snkId_o_i)."""
        tgt = fe["target"]
        if tgt in snk_nodes:
            return f"%{tgt}_o_i"
        return f"%e{fe['_n']}_d"

    # ── Port info lookups ─────────────────────────────────────────────────────

    # (node_id, port_name) → preesm data type string (from connected fifo)
    port_type: dict[tuple, str] = {}
    for fe in fifos:
        port_type[(fe["source"], fe["sourceport"])] = fe["type"]
        port_type[(fe["target"], fe["targetport"])] = fe["type"]

    # (node_id, port_name) → fifo  [for source side]
    out_edge: dict[tuple, dict] = {}
    for fe in fifos:
        out_edge[(fe["source"], fe["sourceport"])] = fe

    # (node_id, port_name) → fifo  [for target side]
    in_edge: dict[tuple, dict] = {}
    for fe in fifos:
        in_edge[(fe["target"], fe["targetport"])] = fe

    def get_in_port_expr(nid: str, pname: str) -> str:
        """Rate expression for an input port."""
        if nid in snk_nodes and snk_nodes[nid]["name"] == pname:
            return snk_nodes[nid]["expr"]
        for pool in (actor_nodes, broadcast_nodes):
            if nid in pool:
                for p in pool[nid]["in_ports"]:
                    if p["name"] == pname:
                        return p["expr"]
                # Merged inout ports keep their '<base>_in' name on the edge.
                for p in pool[nid].get("inout_ports", []):
                    if f"{p['name']}_in" == pname:
                        return p["expr"]
        return "1"

    # ── Resolve subgraph loop names from their .pi files ──────────────────────
    # Preesm graph_desc paths are relative to the PROJECT ROOT, not the current
    # pi file.  Walk up the directory tree to find the file.
    def resolve_graph_desc(gd: str) -> Path | None:
        parent = pi_path.parent
        while True:
            candidate = (parent / gd).resolve()
            if candidate.exists():
                return candidate
            if parent == parent.parent:
                return None
            parent = parent.parent

    subgraph_paths: dict[str, Path] = {}  # actor_id → resolved sub-pi path
    for nid, info in actor_nodes.items():
        gd = info["graph_desc"]
        if gd.endswith(".pi"):
            sub_pi = resolve_graph_desc(gd)
            if sub_pi is None:
                print(f"Warning: cannot resolve {gd!r} from {pi_path}", file=sys.stderr)
                continue
            subgraph_paths[nid] = sub_pi
            try:
                _, sub_graph, sub_nt = parse_graphml(sub_pi)
                sub_name = get_data(sub_graph, "name", sub_nt) or sub_pi.stem
                info["loop_name"] = sub_name
            except Exception:
                pass  # keep the loop_name from the parent pi

    # ── Topological sort of compute nodes ─────────────────────────────────────
    # (actor_nodes ∪ broadcast_nodes)
    # Delayed edges do NOT impose ordering constraints (they break cycles).
    compute_nids = list(actor_nodes) + list(broadcast_nodes)
    incoming: dict[str, int] = {nid: 0 for nid in compute_nids}
    successors: dict[str, list] = {nid: [] for nid in compute_nids}

    for fe in fifos:
        if fe["delay_expr"] is not None:
            continue  # delayed edge — skip for ordering
        src, tgt = fe["source"], fe["target"]
        if tgt in incoming:
            incoming[tgt] += 1
            if src in successors:
                successors[src].append(tgt)

    queue: deque = deque(nid for nid in compute_nids if incoming[nid] == 0)
    topo: list[str] = []
    while queue:
        nid = queue.popleft()
        topo.append(nid)
        for dep in successors[nid]:
            incoming[dep] -= 1
            if incoming[dep] == 0:
                queue.append(dep)

    # Append any remaining nodes (part of an unresolved cycle)
    done = set(topo)
    topo.extend(nid for nid in compute_nids if nid not in done)

    # ── Emit MLIR ─────────────────────────────────────────────────────────────
    L: list[str] = []

    # Parameter documentation comment block
    L.append("// Parameters (cfg_in_iface — primary, from experiments.yaml `parameters`):")
    for cid in cfg_in_ifaces:
        L.append(f"//   {cid}")
    L.append("// Computed parameters (param — from experiments.yaml `computed_parameters`):")
    for pid, pexpr in params:
        L.append(f"//   {pid} = {pexpr}")
    L.append("")
    L.append("module {")
    L.append("")
    L.append(f"iara.actor @{graph_name} {{")
    L.append("")

    # iara.in for each src node
    for src_id, src_port in src_nodes.items():
        ptype = port_type.get((src_id, src_port["name"]), "float")
        t = make_tensor_type(src_port["expr"], ptype, type_map)
        L.append(f"  %{src_id}_i_i = iara.in : {t}")
    L.append("")

    # iara.edge for each fifo leaving a src node
    for src_id, src_port in src_nodes.items():
        fe = out_edge.get((src_id, src_port["name"]))
        if fe is None:
            continue
        ptype   = fe["type"]
        src_t   = make_tensor_type(src_port["expr"], ptype, type_map)
        tgt_expr = get_in_port_expr(fe["target"], fe["targetport"])
        dst_t   = make_tensor_type(tgt_expr, ptype, type_map)
        s_ssa   = f"%{src_id}_i_i"
        d_ssa   = drain_ssa(fe)
        delay_attr = (
            f" {{ delay = {make_template_val(fe['delay_expr'])} }}"
            if fe["delay_expr"] is not None else ""
        )
        L.append(f"  {d_ssa} = iara.edge {s_ssa} : {src_t} -> {dst_t}{delay_attr}")
    L.append("")

    # Compute nodes in topological order: iara.node then its outgoing iara.edges
    for nid in topo:
        if nid in actor_nodes:
            info      = actor_nodes[nid]
            loop_name = info["loop_name"]
            in_ports  = info["in_ports"]
            inout_ports = info["inout_ports"]
            out_ports = info["out_ports"]
        else:
            loop_name = "iara_broadcast"
            in_ports  = broadcast_nodes[nid]["in_ports"]
            inout_ports = []
            out_ports = broadcast_nodes[nid]["out_ports"]

        # Build in ( %drain:type, … ) — in loop/port order
        in_parts: list[str] = []
        for p in in_ports:
            fe = in_edge.get((nid, p["name"]))
            if fe is None:
                continue
            t     = make_tensor_type(p["expr"], fe["type"], type_map)
            d_ssa = drain_ssa(fe)
            in_parts.append(f"{d_ssa}:{t}")

        # Build inout ( %drain:type, … ) — merged <base>_in/<base>_out pairs
        inout_parts: list[str] = []
        for p in inout_ports:
            fe = in_edge.get((nid, f"{p['name']}_in"))
            if fe is None:
                continue
            t     = make_tensor_type(p["expr"], fe["type"], type_map)
            d_ssa = drain_ssa(fe)
            inout_parts.append(f"{d_ssa}:{t}")

        # Build out ( type, … ) + output SSA names — in loop/port order
        out_types: list[str] = []
        out_ssas:  list[str] = []
        for p in out_ports:
            fe = out_edge.get((nid, p["name"]))
            if fe is None:
                continue
            t = make_tensor_type(p["expr"], fe["type"], type_map)
            out_types.append(t)
            out_ssas.append(f"%e{fe['_n']}_s")

        lhs     = ", ".join(out_ssas) + " = " if out_ssas else ""
        in_str  = f"in ( {', '.join(in_parts)} )" if in_parts  else ""
        inout_str = f"inout ( {', '.join(inout_parts)} )" if inout_parts else ""
        out_str = f"out ( {', '.join(out_types)} )" if out_types else ""
        clauses = " ".join(s for s in [in_str, inout_str, out_str] if s)
        L.append(f"  {lhs}iara.node @{loop_name} {clauses}")

        # iara.edge for each fifo leaving this node's output ports
        for p in out_ports:
            fe = out_edge.get((nid, p["name"]))
            if fe is None:
                continue
            src_t    = make_tensor_type(p["expr"], fe["type"], type_map)
            tgt_expr = get_in_port_expr(fe["target"], fe["targetport"])
            dst_t    = make_tensor_type(tgt_expr, fe["type"], type_map)
            s_ssa    = f"%e{fe['_n']}_s"
            d_ssa    = drain_ssa(fe)
            delay_attr = (
                f" {{ delay = {make_template_val(fe['delay_expr'])} }}"
                if fe["delay_expr"] is not None else ""
            )
            L.append(f"  {d_ssa} = iara.edge {s_ssa} : {src_t} -> {dst_t}{delay_attr}")

        L.append("")

    # iara.out for each snk node
    for snk_id, snk_port in snk_nodes.items():
        ptype = port_type.get((snk_id, snk_port["name"]), "float")
        t = make_tensor_type(snk_port["expr"], ptype, type_map)
        L.append(f"  iara.out ( %{snk_id}_o_i : {t} )")

    L.append("")
    L.append(f"}} // iara.actor @{graph_name}")
    L.append("")
    L.append("} // module")
    L.append("")

    return "\n".join(L), graph_name, subgraph_paths


# ── Entry point ───────────────────────────────────────────────────────────────

def main() -> None:
    parser = argparse.ArgumentParser(
        description="Convert Preesm .pi files to IaRa .mlir.template"
    )
    parser.add_argument("input_pi", type=Path,
                        help="Path to root .pi file")
    parser.add_argument("--output-dir", type=Path, default=None,
                        help="Directory for .mlir.template output "
                             "(default: same directory as each .pi file)")
    parser.add_argument("--types", type=str, default=None,
                        help="JSON dict to extend/override the default type map")
    parser.add_argument("--no-p2iaw", action="store_true",
                        help="Emit bare kernel names and separate in/out ports "
                             "(disable the p2iaw_ wrapper prefix and the "
                             "<base>_in/<base>_out inout merge)")
    args = parser.parse_args()

    type_map = dict(DEFAULT_TYPE_MAP)
    if args.types:
        type_map.update(json.loads(args.types))
    p2iaw = not args.no_p2iaw

    root_pi = args.input_pi.resolve()
    if not root_pi.exists():
        print(f"Error: {root_pi} does not exist", file=sys.stderr)
        sys.exit(1)

    to_process: deque[Path] = deque([root_pi])
    processed:  set[Path]   = set()

    while to_process:
        pi_path = to_process.popleft()
        if pi_path in processed:
            continue
        processed.add(pi_path)

        print(f"Processing: {pi_path}", file=sys.stderr)
        try:
            mlir_text, graph_name, subgraph_paths = convert_pi(pi_path, type_map, p2iaw)
        except NotImplementedError as e:
            print(f"Error: {e}", file=sys.stderr)
            sys.exit(1)

        out_dir = args.output_dir if args.output_dir is not None else pi_path.parent
        out_dir.mkdir(parents=True, exist_ok=True)

        out_path = out_dir / f"{graph_name}.mlir.template"
        out_path.write_text(mlir_text, encoding="utf-8")
        print(str(out_path))

        for sub_pi in subgraph_paths.values():
            if sub_pi not in processed:
                to_process.append(sub_pi)


if __name__ == "__main__":
    main()
