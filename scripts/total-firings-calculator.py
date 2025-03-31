import sys
from typing import Any
import pulp
import pulp.solverdir
import json


class Buffer:
    alpha = 0
    beta = 0


class Chain:
    chain_id = 0
    beta_epochs = None


class Node:
    id = 0
    buffers: list[Buffer] = []
    chain: Chain
    total_firings = None

    def __init__(self):
        self.buffers = []


class ProblemSolve:
    nodes: dict[int, Node] = {}
    chain_nodes: list[list[int]] = []
    chains: dict[int, Chain] = {}
    prob: pulp.LpProblem
    file = None

    def __init__(self, path):
        self.file = open(path, 'r')

    def parse_file(self):
        num_nodes = int(self.file.readline())
        for i in range(num_nodes):
            line = list(map(int, self.file.readline().split()))
            node = Node()
            node.id = line[0]
            for j in range(len(line[1:])//2):
                buffer = Buffer()
                buffer.alpha = line[1+2*j]
                buffer.beta = line[2+2*j]
                node.buffers.append(buffer)
            self.nodes[node.id] = node
        num_chains = int(self.file.readline())
        for i in range(num_chains):
            if i not in self.chains:
                self.chains[i] = Chain()
                self.chains[i].chain_id = i
            node_ids = list(map(int, self.file.readline().split()))
            self.chain_nodes.append(node_ids)
            for id in node_ids:
                self.nodes[id].chain = self.chains[i]
            alloc_node = self.nodes[node_ids[0]]
            assert (len(alloc_node.buffers) == 1)
            alloc_node.buffers[0].alpha == 1
            alloc_node.buffers[0].beta == 1
            dealloc_node = self.nodes[node_ids[-1]]
            assert (len(dealloc_node.buffers) == 1)
            dealloc_node.buffers[0].alpha == 1
            dealloc_node.buffers[0].beta == 1

    def create_model(self):
        # total_firings_all = pulp.LpVariable(
        #     f"total_firings_all", lowBound=1, cat="Integer")
        self.prob = pulp.LpProblem("Total_Firings", pulp.LpMinimize)

        total_firings_all = 0

        for id, node in self.nodes.items():
            node.total_firings = pulp.LpVariable(
                f"total_firings_node_{node.id}", lowBound=1, cat="Integer")
            if node.chain.beta_epochs is None:
                node.chain.beta_epochs = pulp.LpVariable(
                    f"beta_epochs_chain_{node.chain.chain_id}", lowBound=0, cat="Integer")
            # for i, buffer in enumerate(node.buffers):
            #     self.prob += buffer.alpha + buffer.beta * \
            #         node.chain.beta_epochs == node.total_firings
            total_firings_all += node.total_firings
        self.prob += total_firings_all

    def print_result(self):
        with open("total_firings.output.txt", "w") as f:
            for i, node in self.nodes.items():
                f.write(f"{i} {int(node.total_firings.varValue)}\n")

    def run(self):
        self.parse_file()
        self.create_model()
        self.prob.writeLP("total_firings.lp")
        self.prob.solve(pulp.PULP_CBC_CMD(timeLimit=0.1, msg=False))

        if pulp.LpStatus[self.prob.status] != "Optimal":
            44
            exit(1)
        self.print_result()
        exit(0)


if __name__ == "__main__":
    if "--help" in sys.argv or len(sys.argv) != 2:
        print(
            "Use MLP to find the total firing number of each node."
        )
        print(''' Takes graph description from file:
num_nodes
node_id num_buffers_node1 buf1_alpha buf1_beta buf2_alpha buf2_beta ...
node_id num_buffers_node2 ...
num_buffer_chains
node_1 node_2 ...
node_3 node_4 ...
''')
        exit(0)

    solver = ProblemSolve(sys.argv[1])
    solver.run()
