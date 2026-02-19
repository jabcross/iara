# IaRa — Project Context

## What IaRa Is

IaRa is a compiler for static dataflow graphs that uses a dynamic runtime for memory management. It targets MLIR/LLVM and competes primarily with:

- **OpenMP** (task-based parallelism) — the mainstream baseline
- **Preesm** — a dataflow compiler for signal processing, used in the radio astronomy and image processing communities

## Key Directories

| Directory | Contents |
|---|---|
| `iara-opt/` | Compiler driver (main entry point) |
| `lib/` | Compiler passes and transformations (MLIR dialects) |
| `include/` | Compiler headers (dialect definitions, `.td` tablegen files) |
| `runtime/` | Dynamic runtime embedded into compiled applications. Two implementations: `ring-buffer/` (older) and `virtual-fifo/` (current). `common/` has the EnkiTS work-stealing backend. |
| `scripts/` | Utility scripts accumulated over the years (not all still used) |
| `tools/` | Python testing/experiment framework (current iteration) |
| `tools/experiment_framework/` | Core framework: `cli.py`, `builder.py`, `generator.py`, `collector.py`, `notebook.py` |
| `applications/` | Test cases and experiment applications |
| `experiments/` | Precursor Python framework (legacy, replaced by `tools/`) |
| `agent_history/` | Previous Claude Code session logs (large, do not read in full) |
| `pdfs/` | Academic papers for this project (not committed to git) |

## Applications

Applications in `applications/` fall into two categories:

### Unit/Regression Tests

Small tests for individual compiler features (numbered 01-12). Self-explanatory from source.

### Experiment Applications

Full applications compared against state-of-the-art baselines:

**Cholesky** (`applications/05-cholesky/`)
- Parallel blocked Cholesky factorization
- Baseline: OpenMP TaskGraph implementation
- Compilation modes:
  - Baseline (SotA): `sequential`, `omp-parallel-for`, `omp-task`, `enkits`
  - IaRa (our contribution): `vf-sequential`, `vf-omp`, `vf-enkits`
- Status: **done**, needs large measurement collection

**Degridder** (`applications/13-degridder/`)
- Degridding step of radio astronomy pipeline: takes a sky image (frequency space) and antenna array description, computes visibilities
- Comparison against Preesm implementation by Anaëlle Cloarec (adapted from a different implementation, see WAMCA article)
- Uses SEP tool to convert `.fits` images to CSV format expected by the application
- Status: **segfault with regenerated input CSVs**. Worked previously with original inputs that were lost.

**SIFT** (`applications/08-sift/`)
- Scale-Invariant Feature Transform for image processing
- Comparison against Preesm implementation
- Status: **not yet adapted** to the current framework. Will be worked on after Degridder.

## Testing Framework

The testing framework in `tools/` has evolved through three iterations:
1. Bespoke bash scripts (original)
2. Precursor Python application in `experiments/` (legacy)
3. Current unified Python framework in `tools/experiment_framework/`

The current framework aims to unify definition, building, execution, measurement, analysis, and visualization of experiments in a single declarative tool. Each application has an `experiments.yaml` that defines its configuration space.

## Academic Papers (pdfs/)

- `Mestrado_Pedro.pdf` (50p) — Pedro's master's thesis
- `Multidimensional_dataflow_with_ownership_semantics-4.pdf` (4p) — Ownership semantics paper
- `Pedro___EQE-9.pdf` (40p) — PhD qualifying exam document
- `SiPS_22___dfmlir.pdf` (6p) — SiPS 2022 paper on dataflow MLIR
- `WAMCA_25___IARA____Dynamic_Memory_Allocation-4.pdf` (11p) — WAMCA 2025 paper on IaRa dynamic memory allocation (references the Degridder comparison)
