import sys
from typing import Any
import pulp
import pulp.solverdir
import json

# takes lists with rates, execution counts, and delays


def generate_k(R, D):
    alpha = [pulp.LpVariable(f"alpha{i}", lowBound=0, cat="Integer")
             for i in range(len(R))]
    beta = [pulp.LpVariable(f"beta{i}", lowBound=0, cat="Integer")
            for i in range(len(R))]

    prob = pulp.LpProblem("Buffer Size", pulp.LpMinimize)
    prob += alpha[-1] * R[-1], "Size of buffer with delays"

    for i in range(len(R) - 1):
        prob += D[i] + R[i] * \
            alpha[i] == R[i + 1] * alpha[i + 1], f"alphas {i}"
        prob += alpha[i + 1] >= 1, f"remove trivial case of alpha_{i+1} = 0"
        prob += R[i] * \
            beta[i] == R[i + 1] * beta[i + 1], f"betas {i}"
        prob += beta[i + 1] >= 1, f"remove trivial case of beta_{i+1} = 0"

    prob.writeLP("buffer-size.lp")
    prob.solve(pulp.PULP_CBC_CMD(timeLimit=0.1, msg=False))
    # print(" ".join([str(int(v.varValue)) for v in prob.variables()]))
    for var in prob.variables():
        if str(var).startswith("alpha"):
            print(f"{int(var.varValue)}", end=" ")
    print("")
    for var in prob.variables():
        if str(var).startswith("beta"):
            print(f"{int(var.varValue)}", end=" ")
    print("")

    if pulp.LpStatus[prob.status] == "Optimal":
        return True

    print(f"LP problem returned with status {pulp.LpStatus[prob.status]}")

    return False


if __name__ == "__main__":
    if "--help" in sys.argv:
        print(
            "Use MLP to find the execution counts alpha, beta and gamma for a buffer. Alpha is the number of times the actor executes in the first allocation (with delays), beta is the number of times per allocation without delays, and gamma is the number of delay-less allocations."
        )
        print("Arguments: A_rate A_execcount AB_delay B_rate B_execcount BC_delay .... MN_delay N_rate N_execcount")
        print(
            "All ports except A and N are inout, and therefore the input and output rates are the same."
        )
        sys.exit(0)
    if len(sys.argv) < 4:
        print("Not enough arguments (called with ", sys.argv, " )")
        sys.exit(1)
    if (len(sys.argv) - 1) % 2 != 1:
        print("Unexpected number of arguments")
        sys.exit(1)
    rates: list[int] = []
    delays: list[int] = []

    for i in range(1, len(sys.argv)-1, 2):
        try:
            rates.append(int(sys.argv[i]))
            delays.append(int(sys.argv[i+1]))
        except ValueError:
            print("Invalid argument pair: " +
                  sys.argv[i] + " " + sys.argv[i + 1])
            sys.exit(1)

    rates.append(int(sys.argv[-1]))

    print("Solving chain:")
    print("[ ]", end="")
    for i in range(len(delays)):
        print(f"{rates[i]}---({delays[i]})--->{rates[i+1]}[ ]", end="")
    print("")

    if generate_k(rates, delays):
        exit(0)
    exit(1)
