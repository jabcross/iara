import sys
from typing import Any
from sympy import Eq, lcm, solve_linear_system, symbols, simplify, solve, Expr, Symbol
from sympy.solvers.diophantine import diophantine, diop_solve
from sympy.solvers.simplex import lpmin
from sympy.core.numbers import Rational


def get_k_constraints(rates, delays, k_syms) -> list[Expr]:
    k_constraints: list[Expr] = []
    params: list[Symbol] = []
    param_constraints: list[Expr] = []
    last_expr_b: Expr = None
    last_param: Symbol = None
    for i in range(len(rates) - 1):
        RA = rates[i]
        RB = rates[i+1]
        DAB = delays[i]
        equation = DAB + RA*k_syms[i] - RB*k_syms[i+1]
        result = diophantine(equation, f"p{i}")
        kexpr_a: Expr
        kexpr_b: Expr
        kexpr_a, kexpr_b = list(result)[0]
        param: Symbol = list(kexpr_a.free_symbols)[0]
        last_constraint = None
        if last_expr_b is not None:
            last_constraint = Eq(last_expr_b, kexpr_a)
            result2 = diophantine(last_constraint, f"p2{i+1}")
            expr2a, expr2b = list(result2)[0]
            param2: Symbol = list(expr2a.free_symbols)[0]
            param_constraints += [last_constraint,
                                  Eq(last_param, expr2a), expr2b]
        last_expr_b = kexpr_b
        param: Symbol = list(kexpr_a.free_symbols)[0]
        params.append(param)
        print("param: ", param)

        print("diophantine result", result)
        if not result:
            print("No solution")
            sys.exit(2)
        k_constraints += [Eq(equation, 0), Eq(k_syms[i],
                                              kexpr_a), Eq(k_syms[i+1], kexpr_b), param >= 0]

        if last_constraint is not None:
            k_constraints += [last_constraint]
    print("param constraints", param_constraints)
    param_constraints_result = solve(param_constraints)
    print("param constraints result:", param_constraints_result)
    for k, v in param_constraints_result.items():
        k_constraints += [Eq(k, v)]
    return k_constraints


if __name__ == "__main__":
    if "--help" in sys.argv:
        print(
            "Use diophantine equations to find the execution counts (k) size for a buffer, with and without delays, given in, inout and out rates and delay sizes."
        )
        print("Arguments: A_rate AB_delay B_rate BC_delay .... MN_delay N_rate")
        print(
            "All ports except A and N are inout, and therefore the input and output rates are the same."
        )
        sys.exit(0)
    if len(sys.argv) < 4:
        print("Not enough arguments")
        sys.exit(1)
    if len(sys.argv) % 2 == 1:
        print("Expecting odd number of arguments")
        sys.exit(1)
    rates: list[int] = []
    delays: list[int] = []

    print(sys.argv)

    for i in range(1, len(sys.argv)-1, 2):
        try:
            rates.append(int(sys.argv[i]))
            delays.append(int(sys.argv[i + 1]))
        except ValueError:
            print("Invalid argument pair: " +
                  sys.argv[i] + " " + sys.argv[i + 1])
            sys.exit(1)

    rates.append(int(sys.argv[-1]))
    k_syms = [symbols(f"k{i}", integer=True)
              for i in range(len(rates))]

    k_exprs_delays = get_k_constraints(rates, delays, k_syms)
    # k_exprs_no_delays = get_k_constraints(
    #     rates, [0] * (len(rates) - 1), k_syms)

    # increasing_constraints = [Eq(delays[i] + rates[i]*k_syms[i], rates[i+1]*k_syms[i+1])
    #                           for i in range(len(delays))]

    # all_equal = [Eq(rates[i]*k_syms[i], rates[i+1]*k_syms[i+1])
    #              for i in range(len(rates)-1)]

    ineqs: list[Expr] = [k >= 1 for k in k_syms]

    def format_output(title, k_constraints, extra_constraints=[]):
        print(title)
        S = symbols("S", integer=True)
        size_constraint = [Eq(S, rates[-1] * k_syms[-1])]
        constraints = k_constraints + ineqs + \
            extra_constraints + size_constraint
        print("Constraints: ", constraints)
        size, results = lpmin(S, constraints)
        print("First results:", size, results)
        # for all parameters, find smallest and add constraint of it being integer
        denoms = [d.denominator for d in results.values()
                  if isinstance(d, Rational)]
        _lcm = lcm(denoms)
        results = {k: v * _lcm for k, v in list(results.items())[1:]}
        print(results)

    format_output("With delays:", k_exprs_delays)
    # format_output("Without delays:", k_exprs_no_delays)
