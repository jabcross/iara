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
    # Ceiling division: supports non-uniform block sizes (e.g. 2048/20=102.4 -> 103)
    ts = (dim + nb - 1) // nb
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


# Generates blocked cholesky topology (upper triangle version)

if __name__ == "__main__":
    assert (len(sys.argv) == 3)
    dim = int(sys.argv[1])
    nb = int(sys.argv[2])

    # pad to next multiple of nb
    ts = (dim + nb - 1) // nb
    tasks = generate(dim, nb)

    # accumulate state as in-out edges. Track the next id for that i,j block
    deps = {}
    edges = {}

    print("iara.actor @run {")

    labels = ", ".join(
        [f"%e_{row}_{col}_0" for col in range(nb) for row in range(nb)])

    print(f"{labels} = ", end="")

    types = ", ".join([f"tensor<{ts*ts}x{datatype}>" for i in range(nb*nb)])

    print(f"iara.node @kernel_split out ( {types} ) ")

    for row in range(nb):
        for col in range(nb):
            # print(f"    interface out_{x}_{y}->e_{x}_{y}_0;")
            edges[(row, col)] = 0
            deps[(row, col)] = f"e_{row}_{col}_0"

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
        kernel_inputs = kernels[task['kernel']]['in']
        kernel_outputs = kernels[task['kernel']]['out']
        for i in kernel_inputs:
            row, col = task[i]
            edge_name = f"e_{row}_{col}_{edges[(row, col)]}"
            reads[edge_name] += 1
            input_edges.append((row, col))
            if (i not in kernel_outputs): # not inout
                inputs.append(f"%{edge_name}")
                input_types.append(f"tensor<{ts*ts}x{datatype}>")
            else:
                inouts.append(f"%{edge_name}")
                inout_types.append(f"tensor<{ts*ts}x{datatype}>")
        for i in kernel_outputs:
            row, col = task[i]
            edges[(row, col)] += 1
            edge_name = f"e_{row}_{col}_{edges[(row, col)]}"
            # print(
            #     f"    interface out_{i}->{edge_name};")
            writes[edge_name] += 1
            incremented_edges.append((row, col))
            outputs.append(f"%{edge_name}")
            output_types.append(f"tensor<{ts*ts}x{datatype}>")
        # print('}')
        print(f"  {', '.join(outputs)} = ", end="")
        print(f"iara.node @{task['kernel']}", end="")
        if len(inputs) > 0:
            print(
                f" in ( {', '.join([f"{input}:{in_type}" for input, in_type in zip(inputs, input_types)])} )")
        if len(inouts) > 0:
            print(
                f" inout ( {', '.join([f"{inout}:{inout_type}" for inout, inout_type in zip(inouts, inout_types)])} )")
        if len(outputs) > 0:
            print(f" out ( {', '.join(output_types)} )")

    for edge in incremented_edges:
        edges[edge]

    assert all([writes[edge] == 1 for edge in writes])


    print(f"  iara.node @kernel_join in ({", ".join([f"%e_{row}_{col}_{edges[(row, col)]} : tensor<{ts*ts}x{datatype}>" for (
        row, col), ty in zip([(row, col) for col in range(nb) for row in range(nb)], range(nb*nb))])})", end="")

    print("}")
