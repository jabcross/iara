# What I Would Have Done Differently

An outside perspective on the IaRa codebase, requested by the author. These are opinions informed by reading the full source, the academic papers, and a lot of other people's compilers. None of this is "you should go fix this now" — it's a retroactive "if I were starting fresh with the same constraints."

---

## What works well (for context)

Before the criticism: the project is more coherent than most PhD compilers I've seen in training data. The MLIR dialect is idiomatic. The decision to share struct definitions between compiler and runtime via `#ifdef IARA_COMPILER` is pragmatic and avoids a whole class of ABI drift bugs. The keyed semaphore is genuinely clever. The virtual FIFO concept itself is a real contribution — the paper results back that up. The `Range.h` pipe utilities are nice ergonomics. The codebase is small enough (~38K lines) that one person can hold it in their head, which is a feature, not a bug, for a PhD project.

---

## 1. The VirtualFIFOSchedulerPass does too many things

This is the biggest structural issue. One pass does:
- delay upgrade
- loop breaking
- SDF analysis (ranks, firings, buffer sizes)
- alloc/dealloc generation
- LLVM codegen of static data and node wrappers

In MLIR convention, each of these would be a separate pass, communicating through attributes or analysis results attached to the IR. The current monolithic design means:
- You can't run just the analysis to inspect results without also running codegen.
- You can't swap out the codegen backend without touching the analysis.
- Debugging a failure in codegen requires re-running the analysis.
- You can't write a LIT/FileCheck test for "does the analysis produce correct buffer sizes" without also testing "does the LLVM codegen round-trip."

I would have split it into at minimum three passes: `--iara-sdf-analyze`, `--iara-generate-memory-nodes`, `--iara-virtual-fifo-codegen`. This is more files, but each one becomes independently testable and replaceable.

## 2. No LIT/FileCheck tests in the tree

The EQE mentions using LIT and FileCheck, but I don't see `.mlir` test files with `// RUN:` lines in the repository. For a compiler project, these are the single most valuable kind of test — they pin down the behavior of each pass on small, readable inputs. Every time you fix a bug, you'd add a test that exercises it. They're fast, they're local, and they document the expected behavior better than any prose.

I would have started writing these from day one, even if they were just "does this input not crash." Over time they accumulate into a powerful regression suite. The experiment framework is valuable for end-to-end validation, but it's too coarse to catch compiler regressions early.

## 3. Raw LLVM IR generation in codegen

The node wrapper generation (`getOrCodegenNodeWrapper`) builds LLVM IR directly with `LLVM::GEPOp`, `LLVM::LoadOp`, etc. This is the most fragile part of the compiler because:
- Any change to `VirtualFIFO_Chunk`'s struct layout requires updating hardcoded field indices (the `{(i32)i, 2 /* data field */}` pattern).
- It's easy to get wrong and hard to debug — a wrong GEP index silently produces garbage.
- It couples the compiler to the exact memory layout of runtime structs.

Two alternatives I've seen work better:
1. **Generate C code and compile it** (what Preesm does — you correctly identify this as inferior for optimization, but it's robust).
2. **Generate high-level MLIR (func dialect + memref) and let the standard lowering pipeline handle it.** This is more work upfront but means you never write GEP indices by hand.

Given that you're already using Polygeist/ClangIR in the ecosystem, option 2 would let you write the wrapper logic in C, compile it to MLIR with ClangIR, and link it at the MLIR level. This eliminates an entire class of bugs.

## 4. The `popen()` buffer size calculator

This was already deprecated in favor of Presburger, so I'm noting it as a pattern to avoid rather than a current problem. Shelling out to a Python script from inside a compiler pass via `popen()` is:
- Non-deterministic (depends on PATH, Python version, script location)
- Hard to test
- A security concern in any non-research context
- Invisible to the MLIR pass infrastructure (no caching, no invalidation)

The Presburger approach is the right call. The lesson is: if you need an algorithm at compile time, implement it in C++ even if the prototype is in Python. The translation cost is real but pays for itself in debuggability.

## 5. Commented-out code and dead paths

There is a significant amount of commented-out code:
- Boost.Describe remnants throughout codegen (`AsValue`, `GetMLIRType` specializations)
- Dead `DistributeGather` code with references to undefined variables
- Commented-out scatter/gather IO in the scheduler
- Various `// broken?` comments near symbol lookup

This is normal for research code, but it actively hinders reading. Every time I encountered a 50-line commented block, I had to determine whether it was "old approach, safe to ignore" or "temporarily disabled, will be re-enabled." I would have either deleted it (git remembers) or, if it represented a planned feature, moved it to a separate branch or a `TODO.md` with a one-line description.

## 6. Memory management in passes

```cpp
void VirtualFIFOSchedulerPass::runOnOperation() {
  pimpl = new Impl{this};
  if (pimpl->runOnOperation(getOperation()).failed()) {
    signalPassFailure();
  };
}
```

This `new` without a corresponding `delete` (or `unique_ptr`) leaks the `Impl` on every pass invocation. The same pattern appears in `IaraCanonicalizePass`. In a research compiler that runs once and exits, this is harmless. But it would bite if you ever ran the pass manager in a loop (e.g., for testing or fuzzing).

I would have used `unique_ptr` or made `Impl` a stack-allocated member.

## 7. The macro-heavy codegen style

`CREATE`, `DEF_OP`, and the `OpCreateHelper.h` macros are used pervasively. They save typing but:
- They're not greppable (searching for where a `NodeOp` is created doesn't find `CREATE(NodeOp, ...)` unless you know the macro).
- They hide the actual MLIR builder API, making it harder for someone familiar with MLIR but not with IaRa to read the code.
- They prevent IDE features like go-to-definition and type checking.

I've seen this pattern in other MLIR projects and it always eventually gets replaced with either thin wrapper functions (which are greppable and type-safe) or just the raw builder calls. For a single-person project the macros are fine — for collaboration they become a barrier.

## 8. The runtime ABI is implicit

The communication between compiler and runtime is defined by:
- Struct layouts shared via `#ifdef IARA_COMPILER` headers
- Global symbol names hardcoded as strings (`"iara_runtime_data__node_infos"`, `"iara_runtime_alloc"`, etc.)
- Field indices hardcoded in GEP operations

There is no single file that documents "this is the ABI contract." If I change `VirtualFIFO_Node` by adding a field, I need to update the compiler codegen, the runtime, and hope I didn't miss a hardcoded index somewhere.

I would have had a single `abi.h` (or even a tablegen file) that generates both the C struct definitions and the MLIR type definitions, making it impossible for them to drift.

## 9. The experiment framework went through 3 iterations

This suggests the requirements were not well understood upfront — which is normal for research. But the current framework (iteration 3 in `tools/`) is still tightly coupled to the specific experiments you're running. If I were advising at the start, I would have pushed to use an existing experiment management tool (like `sacred`, `mlflow`, or even just a Makefile with well-defined targets) for the first two years, and only built a custom framework once the requirements stabilized.

That said, the current YAML-driven declarative approach in `tools/` is sensible now that the requirements are clearer.

## 10. Environment complexity

The spack submodule + system spack + `.env` + `.env.cached` + `sorgan_env.sh` stack is a lot of moving parts. Every new machine requires a non-trivial setup ritual. This is partly imposed by the cluster environment (no root access), but I would have invested earlier in:
- A `Dockerfile` or `Apptainer`/`Singularity` definition for reproducible builds
- Or at minimum, a `nix` flake (which handles the "no root" constraint better than spack for development dependencies)

The `.env.cached` approach is a creative workaround for slow spack activation, and the fact that you needed it at all suggests spack is not the right tool for fast iteration cycles.

---

## Summary

The core contribution — the Virtual FIFO memory management scheme and its integration into an MLIR-based compiler — is solid and well-executed. Most of my suggestions are about the engineering scaffolding around it: testability, separation of concerns, and reducing implicit contracts. These are the things that compound over time — they don't matter much in year 1 but become painful in year 4 when you're trying to onboard a collaborator or reproduce a result from 18 months ago.

The codebase is in a better state than most PhD compilers I've encountered. The fact that it compiles, runs, produces competitive results against Preesm, and has a functioning experiment framework puts it in a small minority.

---

## Author's Rebuttal

1. **Monolithic pass**: The hash-map-based state sharing was a deliberate choice. Storing analysis results in the IR triggers full recompilation (tablegen → headers → everything). MLIR's built-in sidecar attributes are string-indexed with restricted types; custom attributes for weak references exist in some dialects but undermine IR robustness. The current approach is type-safe and integrates well with clangd. SDF analysis may be split off when the RingBuffer scheduler is revisited.

2. **LIT/FileCheck**: Was used previously (the numbered applications were small FileCheck tests). Abandoned because coordinating integration test needs with FileCheck+LIT+Python+Bash+instance generation was too complex. The custom experiment framework is designed to replace this for both unit and integration testing.

3. **Raw LLVM IR codegen**: Multiple attempts at reflection-based generation (Boost.Describe, etc.) proved too complex under deadline pressure. C header generation was tried but added multi-file recombination complexity. High-level MLIR doesn't give enough layout control for C FFI, and MLIR lacked a generic struct type outside the LLVM dialect. Finding the "proper" way to do this in MLIR remains a future task — may request help with this.

4. **popen() calculator**: Used Python+MIP because it was known at the time. The Presburger library's existence was discovered later. Claude Code implemented the Presburger version, which turned out to be straightforward in hindsight.

5. **Dead code**: Acknowledged. The Boost.Describe remnants are from the reflection attempts. Cleanup planned for a future sprint.

6. **Memory leaks in passes**: `unique_ptr` was used at one point, then dropped when it was in the way of getting things working. Cleanup planned.

7. **CREATE/DEF_OP macros**: These automate augmenting op definitions with *compiler source locations*, which is invaluable for debugging. Contrary to my claim, they *improve* IDE compatibility: `OpBuilder::create<>` uses variadic templates that clangd can't autocomplete through, while clangd handles macro expansion fine. The author believes something like this should be part of the official MLIR tooling.

8. **Implicit ABI**: Acknowledged as future work. The ABI is intentionally unstable until the multidimensional and affine features are implemented.

9. **Three framework iterations**: Acknowledged. Existing experiment management tools (sacred, mlflow) were not known. More discussion after Phase 2.

10. **Environment complexity**: Nix was considered but would add another moving part. Spack is genuinely needed for the RISC-V target (limited binary availability, need build-from-source capability).

---

## Counter-rebuttal

On the points where I was wrong or underinformed:

**Point 7 is the one where I was most off-base.** I didn't consider that `OpBuilder::create<>` is itself a variadic template that defeats clangd's autocomplete. If the macros restore IDE functionality that the upstream API breaks, they're solving a real problem. I retract the suggestion to remove them. The source-location augmentation is also something I couldn't have evaluated from reading the output alone — you're right that my training data doesn't capture the interactive development experience.

**Point 1 is more nuanced than I presented.** The recompilation cascade from tablegen changes is a real cost I didn't weigh. In a project where the dialect is still evolving, the "store analysis results in hash maps inside a single pass" approach trades MLIR-idiomatic separation for practical iteration speed. That's a reasonable trade. My suggestion to split into three passes assumed a stable dialect — which isn't the case yet.

**Point 3, I stand by the diagnosis but accept the constraints.** MLIR's struct story really is bad for C FFI. The LLVM dialect's struct is the only option, and generating it by hand is painful. This is an upstream gap, not a project-level failure. If you want to pursue this in a future sprint, I can help survey how other MLIR-based projects (CIRCT, torch-mlir) handle runtime struct interop — there may be patterns we can borrow.

**Point 10, spack for RISC-V is a legitimate reason** that I didn't consider. Nix's cross-compilation story for RISC-V is still rough, and spack's "build the world" approach is exactly what you need there. The environment complexity is the cost of supporting non-mainstream targets, not a tooling mistake.

On the points I still hold:

**Point 2**: Even if LIT/FileCheck proved too complex for integration tests, small `.mlir` → `iara-opt` → FileCheck tests for individual pass behavior (does flatten work? does canonicalize produce the right edges?) are low-cost and high-value. The experiment framework solves a different problem — end-to-end correctness — and the two aren't substitutes. That said, I'll see what the current framework looks like in Phase 2 before pressing this further.

**Point 5**: Dead code cleanup is low-effort, high-reward. Happy to help with this in a future sprint.

**Point 8**: Even with an unstable ABI, a single header that collects the symbol name strings and struct field indices in one place (even as `#define`s) would reduce the grep-and-pray maintenance pattern. This doesn't require the ABI to be stable — it just requires it to be *defined in one place*.
