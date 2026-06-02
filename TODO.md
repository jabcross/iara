# SACI Framework Refactoring

## Phase 1: Rename experiment_framework → saci

- [ ] Rename `tools/experiment_framework/` → `tools/saci/`
- [ ] Update all imports: `from tools.experiment_framework` → `from tools.saci`
- [ ] Update CLI entry point: `python3 -m tools.saci`
- [ ] Update README/documentation to reference SACI name
- [ ] Update CLAUDE.md directives to reference SACI
- [ ] Commit: feat(saci): rename experiment_framework to SACI

## Phase 2: Extract to independent repository

- [ ] Create `iara-saci` repository (separate GitHub project)
- [ ] Migrate `tools/saci/` as the root
- [ ] Keep minimal IaRa-specific documentation
- [ ] Add generic docs: "SACI: Scheduler-Agnostic Continuous Instrumentation framework"
- [ ] Set up PyPI package: `saci-benchmark`
- [ ] Update IaRa to depend on `saci-benchmark` as external package
- [ ] Remove `tools/saci/` from iara/ (once external dependency works)
- [ ] First release: v0.1.0

## Phase 3: Documentation & branding

- [ ] Write SACI README with folklore context
- [ ] Add examples: cholesky, sift, degridder
- [ ] Create architecture docs
- [ ] Document the IR-pun: "Scheduler-Agnostic Continuous Instrumentation"

---

# Code Review Action Items — 2026-05-28

Priority: 🔴 bug / 🟠 soon / 🔵 cleanup / 💬 discuss

## 🔴 Bugs

- [ ] **`include/Iara/Util/CommonTypes.h:12`** `using u32 = uint64_t` → `uint32_t`
- [ ] **`lib/.../Common/Codegen/Codegen.cpp`** add `#include "Iara/Passes/Common/Codegen/Codegen.h"`
- [ ] **`runtime/ring-buffer/MutexRingBuffer.cpp:108`** add nullptr check on `aligned_alloc` return

## 🔴 Removals (stale/broken files)

- [ ] **`lib/Iara/Dialect/DistributeGather.cpp`** — undefined vars, remove
- [ ] **`include/Iara/Passes/VirtualFIFO/SDF/DistributeGather.h`** — remove alongside .cpp
- [ ] **`include/Iara/Util/mermaid.h`** — UB on missing return, remove
- [ ] **`include/Iara/Util/defer.h`** — empty file, remove
- [ ] **`lib/.../VirtualFIFO/SDF/DelayAnalysis.cpp`** — 0 lines, remove
- [ ] **`test/cmake_install.cmake`** — stale artifact from LLVM build tree, remove

## 🟠 Include Guard Hygiene

- [ ] **`include/Iara/Util/rational.h`** `RATIONAL_H` → `IARA_UTIL_RATIONAL_H`
- [ ] **`include/Iara/Util/Shell.h`** `UTIL_SHELL_H` → `IARA_UTIL_SHELL_H`
- [ ] **`include/Iara/Util/Range.h`** `UTIL_UTIL_H` → `IARA_UTIL_RANGE_H`
- [ ] **`include/Iara/Util/OpCreateHelper.h`** `UTIL_OPCREATEHELPER_H` → `IARA_UTIL_OPCREATEHELPER_H`
- [ ] **`include/Iara/Util/Mlir.h`** `MLIR_IARA_MLIR_UTIL_H` → `IARA_UTIL_MLIR_H`
- [ ] **`include/Iara/Passes/VirtualFIFO/SDF/Canon.h`** `UTIL_CANON_H` → `IARA_PASSES_VIRTUALFIFO_SDF_CANON_H`
- [ ] **`include/IaraRuntime/VirtualFIFO/VirtualFIFO_Edge.h`** guard doesn't match filename
- [ ] **`include/IaraRuntime/VirtualFIFO/KeyedSemaphore.h`** guard doesn't match filename
- [ ] **`include/IaraRuntime/util/ScatterGather.h`** add missing include guard
- [ ] **`include/Iara/Util/CommonTypes.h:26`** fix `#endif` comment (wrong guard name)
- [ ] **`include/IaraRuntime/VirtualFIFO/VirtualFIFO_Node.h:45`** fix `#endif` comment

## 🟠 Dead Code & Spurious Includes

- [ ] Remove all commented-out dead code blocks (~200 lines across files). Key locations:
  - `lib/.../VirtualFIFO/Codegen/Codegen.cpp` (lines 35-71, 74-111, 192-218, 220-247, 391-420, 632-660, 703-704)
  - `lib/.../Util/Mlir.cpp` (lines 86-102)
  - `include/Iara/Util/Range.h` (lines 31-47, 53-63, 74-80, 209-224)
  - `include/IaraRuntime/VirtualFIFO/VirtualFIFO_Edge.h` (lines 70-88)
  - `include/IaraRuntime/VirtualFIFO/VirtualFIFO_Node.h` (line 13 commented include)
  - `include/IaraRuntime/VirtualFIFO/SDFSemaphores.h` (lines 34-38)
  - `include/IaraRuntime/VirtualFIFO/KeyedSemaphore.h` (lines 63-73, 97-106, 124-134)
  - `include/IaraRuntime/VirtualFIFO/VirtualFIFO_Chunk.h` (lines 16-20)
  - `lib/.../VirtualFIFO/SDF/SDF.h` (lines 39-43, 53-56)
  - `iara-opt/iara-opt.cpp` (lines 36-39)
- [ ] Remove `#include <llvm/CodeGen/GlobalISel/GIMatchTableExecutor.h>` from both `SDF.cpp` files

## 🔵 Style Fixes (one-shot)

- [ ] Rename `isKernelDeclaration()` in `ActorOp.cpp` — mutates IR, name should reflect that (e.g. `resolveKernelDeclaration`)
- [ ] Fix relative includes:
  - `include/Iara/Util/Mlir.h:4` `"OpCreateHelper.h"` → `"Iara/Util/OpCreateHelper.h"`
  - `include/Iara/Util/Range.h:4` `"CommonTypes.h"` → `"Iara/Util/CommonTypes.h"`
- [ ] Add `const` to: `isEmpty()`, `hasInterface()` (ActorOp.cpp); `isAlloc()`, `isDealloc()`, `isInoutInput()`, `isInoutOutput()`, `isPureInInput()` (NodeOp.cpp); `isInout()`, `isDeallocEdge()` (both SDF.cpp files)
- [ ] Fix include ordering across all .cpp files (convention: own header → blank → project → blank → system/LLVM/MLIR)
- [ ] Add `__pycache__/` to `.gitignore`

## 🟠 Dedicated Sessions

- [ ] **Remove `using namespace` from all headers** — use anonymous namespaces in .cpp files instead; avoid repeated `namespace iara::...` qualifiers in headers
- [ ] **Fix PIMPL leaks** — `std::unique_ptr<Impl>` in `VirtualFIFOSchedulerPass`, `RingBufferSchedulerPass`, `IaraCanonicalizePass`

## 🔵 Larger Refactors

- [ ] **`tools/config.py:410`** replace `eval()` → `ast.literal_eval()` or constrained expression evaluator
- [ ] **`cmake/IaRaApplications.cmake`** split monolithic `iara_add_application()` (625 lines) into smaller functions; align names between CTest phases (`clean`, `iara-build`, `lower`, `build`, `run`) and experiment framework CLI (`generate`, `build`, `execute`, `visualize`, `clean`, `run`)

---

# VirtualFIFO Memory Footprint — Brainstorm 2026-05-28

Full notes: `agent_workspace/Sprint-2026-05-28/virtualfifo-memory-optimizations.md`

Root cause: static single-rate expansion emits one MLIR node + per-firing semaphore per task firing → O(NB³) for Cholesky. Drives 349 MB schedule.mlir, 347s LLVM compile time, 6.3 MB rela section.

## 🔥 Tonight — §1b + §2 + kernel_id dispatch (single bundled change)

Goal: kill `.rela` and shrink `schedule.mlir` in one go.

**Design:**

1. **`kernel_id: u8` on `VirtualFIFO_Node_CodegenInfo`** — index into a per-app symbol table covering every callable the runtime dispatches: user kernels (`kernel_potrf`, …) + deduped synthetic broadcasts (`iara_broadcast_<type>_<sig>`) + the alloc/dealloc singletons. Total per app: dozens; cap 255 asserted at codegen.
2. **Codegen-emitted dispatch fn** `iara_dispatch_kernel(u8 kid, i64 seq, std::span<VirtualFIFO_Chunk> args)` — single function with `switch(kid)` over u8 ids. Each case spreads `args` per its symbol's C signature and calls the symbol directly. Per-node wrappers eliminated. PIE jump table → `.text` (PC-relative), no per-case `.rela`. Replaces all `iara_node_wrapper_*` symbols.
3. **u16 cross-ref indices on `VirtualFIFO_Edge_CodegenInfo`** — `consumer` / `producer` / `alloc_node` → `u16` index into `iara_runtime_nodes`; `next_in_chain` → `u16` index into `iara_runtime_edges`. Cap at codegen: nodes ≤ 65535, edges ≤ 65535 (~NB=56 for Cholesky).
4. **Spans → `{u16 offset, u16 count}`** on `VirtualFIFO_Node_CodegenInfo` for `input_fifos` / `output_fifos`. Backed by a single global `iara_runtime_fifo_indices: u16[]` emitted alongside the node/edge arrays.
5. **`StaticDataAccess.h` + `_Inline.cpp`** — accessors do `iara_runtime_nodes[idx]` / `iara_runtime_edges[idx]` lookups; new accessor `fireKernel(node*, seq, args)` calls `iara_dispatch_kernel`.
6. **Runtime call site** `VirtualFIFO_Node.cpp:167` → `iara::runtime::virtualfifo::fireKernel(_this, seq, args)`.
7. **§2 sidecar** — node / edge / fifo-index arrays serialized to `data.bin`; codegen emits `data.c` with `#embed "data.bin"` + extern array symbols. `schedule.mlir` keeps only the dispatch fn, scheduler init, and `extern` decls. CMake builds `data.c -O0` and links `data.o`.
8. **Strings (`name` fields)** — keep as pointers for tonight (small residual `.rela`; possible follow-up: offsets into a strings blob).

**Gate:** `15-dealloc-rehash` + cholesky `medium` set both green post-change. Compare `.rela` / `schedule.mlir` size / LLVM compile time vs `main` baseline.

**Out of scope tonight:** §1a (chain-contiguous), §3 (constant dedup), §4 (field widths), §6 (reserve), §7 (`iara` unification). §1a may become moot once §2 ships.

---

**Incremental (paper-deadline safe):**
- [ ] **#6 `reserve()` on DenseMaps** — 1h, belt-and-suspenders UAF fix (`SDF.cpp:65`, `VirtualFIFOAnalysis.cpp:144`)
- [ ] **#3 constant dedup in codegen** — 1-2d, cache `arith.constant` by value in `makeEdgeInfo`/`makeNodeInfo` → ~2× schedule.mlir reduction for uniform topologies
- [ ] **#4 shrink StaticInfo field widths** — 1d, `needs_priming`/`local_index`/`cons_arg_idx`/`num_args`/`rank` → i32/bool; saves ~40% NodeStaticInfo size
- [ ] **#1a chain-contiguous edge ordering** — 1-2d, drop `next_in_chain` field, use pointer arithmetic; adds `chain_length: u8` to first edge
- [ ] **#1b u16 cross-reference indices** — 2-3d, replace node/edge pointer fields with u16 indices; eliminates 6 MB rela section (cap: NB≈56 before overflow)
- [ ] **#2 `#embed` data sidecar** — 2-3d, emit edge/node arrays as `data.bin` + `data.c` using `#embed` (Clang 22 ✓); removes 95%+ of schedule.mlir; requires §1b first so data is pointer-free
- [ ] **#7 `iara` unification** — 1w, fold lowering+compilation into one binary, rename `iara-opt` → `iara`, deprecate `mlir-to-llvmir.sh`

**Architectural (future):**
- [ ] **#0 node clustering `--vf-cluster=K`** — weeks, cluster homogeneous expanded nodes back into template+count; linear chains work now; Cholesky requires multidimensional/recursive topology features (proper fix: recursive Cholesky once IaRa supports recursive topologies)

## 💬 Verify

- [ ] **`virtual-fifo/VirtualFIFO_Scheduler.cpp:29-30`** sentinel `IARA_EXTERNALLY_MANAGED_MEMORY` — verify pointer is never dereferenced before bit-zeroing. If safe, leave. If not, use side table or static dummy address.
