from collections import defaultdict
import sys

datatype = "f64"

kernels = {
    "kernel_potrf": {
        "in": ["A"],
        "out": ["A"]
    },
    "kernel_trsm": {
        "in": ["A", "B"],
        "out": ["B"]

    },
    "kernel_gemm": {
        "in": ["A", "B", "C"],
        "out": ["C"]
    },
    "kernel_syrk": {
        "in": ["A", "B"],
        "out": ["B"]
    },
}


def generate(dim: int, nb: int):
    tasks = []
    ts = dim // nb
    for k in range(nb):
        tasks.append({"kernel": "kernel_potrf", "A": (k, k)})
        for i in range(k + 1, nb):
            tasks.append({"kernel": "kernel_trsm", "A": (k, k), "B": (k, i)})
        for l in range(k+1, nb):
            for j in range(k+1, l):
                task = {"kernel": "kernel_gemm", "A": (
                    k, l), "B": (k, j), "C": (j, l)}
                print(task, file=sys.stderr)
                tasks.append(task)
            task = {"kernel": "kernel_syrk", "A": (k, l), "B": (l, l)}
            print(task, file=sys.stderr)
            tasks.append(task)
    return tasks


if __name__ == "__main__":
    assert (len(sys.argv) == 3)
    dim = int(sys.argv[1])
    nb = int(sys.argv[2])
    ts = dim // nb
    tasks = generate(dim, nb)
    deps = {}
    edges = {}

    # print("dif main {")

    for kernel, val in kernels.items():
        print(f"""
iara.actor @{kernel} {{
              """)
        ins = val["in"]
        outs = val["out"]

        counter = 1
        for i in val["in"]:
            inout = "inout" if i in val["out"] else ""
            print(f" %{counter} = iara.in {inout} : tensor<{ts * ts}x{datatype}>")
            counter += 1
        for i in outs:
            print(f"  iara.out : tensor<{ts * ts}x{datatype}>")
        print("""  iara.dep
} { kernel }
""")

    print("""
iara.actor @kernel_split {
""")

    for x in range(nb):
        for y in range(nb):
            print(f"  iara.out : tensor<{ts * ts}x{datatype}>")

    print("""
} { kernel }
""")

    print("""
iara.actor @kernel_join {
""")

    for x in range(nb):
        for y in range(nb):
            print(f"    iara.in : tensor<{ts * ts}x{datatype}>")

    print("""  iara.dep
} { kernel }
""")

    print("iara.actor @run {")

    print("  ", end="")
    # print(labels)
    labels = ", ".join([f"%e_{x}_{y}_0" for x in range(nb) for y in range(nb)])

    print(f"{labels} = ", end="")

    types = ", ".join([f"tensor<{ts*ts}x{datatype}>" for i in range(nb*nb)])
    print(f"iara.node @kernel_split out : {types}")

    for x in range(nb):
        for y in range(nb):
            # print(f"    interface out_{x}_{y}->e_{x}_{y}_0;")
            edges[(x, y)] = 0
            deps[(x, y)] = f"e_{x}_{y}_0"

    counter = 0
    incremented_edges = []
    reads = defaultdict(int)
    writes = defaultdict(int)
    for task in tasks:
        kernel = task['kernel']
        counter += 1
        input_edges = []
        # print(f"    type: {task['kernel']};")
        outputs = []
        output_types = []
        inputs = []
        input_types = []
        inouts = []
        inout_types = []
        for i in kernels[task['kernel']]['in']:
            x, y = task[i]
            edge_name = f"e_{x}_{y}_{edges[(x, y)]}"
            # print(f"    interface {edge_name}->in_{i};")
            reads[edge_name] += 1
            input_edges.append((x, y))
            if (i not in kernels[task['kernel']]['out']):
                inputs.append(f"%{edge_name}")
                input_types.append(f"tensor<{ts*ts}x{datatype}>")
            else:
                inouts.append(f"%{edge_name}")
                inout_types.append(f"tensor<{ts*ts}x{datatype}>")
        for i in kernels[task['kernel']]['out']:
            x, y = task[i]
            edges[(x, y)] += 1
            edge_name = f"e_{x}_{y}_{edges[(x, y)]}"
            # print(
            #     f"    interface out_{i}->{edge_name};")
            writes[edge_name] += 1
            incremented_edges.append((x, y))
            outputs.append(f"%{edge_name}")
            output_types.append(f"tensor<{ts*ts}x{datatype}>")
        # print('}')
        print(f"  {', '.join(outputs)} = ", end="")
        print(f"iara.node @{task['kernel']}", end="")
        if len(inputs) > 0:
            print(f" in {', '.join(inputs)} : {', '.join(input_types)}", end="")
        if len(inouts) > 0:
            print(
                f" inout {', '.join(inouts)} : {', '.join(inout_types)}", end="")
        if len(outputs) > 0:
            print(f" out : {', '.join(output_types)}", end="")
        print()

    for edge in incremented_edges:
        edges[edge]

    assert all([writes[edge] == 1 for edge in writes])


print(f"  iara.node @kernel_join in ", end="")

labels = ", ".join([f"%e_{x}_{y}_{edges[(x,y)]}" for x in range(nb)
                    for y in range(nb)])
types = ", ".join([f"tensor<{ts*ts}x{datatype}>" for i in range(nb*nb)])

print(f"{labels} : {types}")

print("  iara.dep")
print("} { flat }")
