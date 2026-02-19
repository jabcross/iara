# Phase 1 Notes: Compiler & Runtime

## Compiler Architecture

### Dialect (`include/Iara/Dialect/`, `lib/Iara/Dialect/`)
- **IaraDialect**: MLIR dialect with namespace `iara`, registered types: `Custom`, `Fifo`
- **Core ops**: `ActorOp`, `NodeOp`, `EdgeOp`, `InPortOp`, `OutPortOp`
  - `ActorOp`: Container for dataflow graphs. Has `sym_name`, optional `params`. Region is Graph kind. Can be flat, task-form. Distinguishes kernel declarations (no nodes) from definitions.
  - `NodeOp`: References an implementation by `FlatSymbolRefAttr impl`. Has `params`, `in`, `inout` segments. Results = inout outputs + pure outputs. `isAlloc()`/`isDealloc()` check for runtime alloc/dealloc nodes.
  - `EdgeOp`: Connects node outputs to inputs. Type mismatch = multi-rate edge. Has optional `delay` attribute (integer or DenseArray).
  - `InPortOp`/`OutPortOp`: Interface ports for actors. InPortOp can be `inout`.
- **Broadcast.cpp**: Generates memcpy-based broadcast implementations as LLVM functions. Handles inout (first output reuses input) vs copy variants.
- **DistributeGather.cpp**: For SIFT port — maps fork/join actors (round-robin token distribution, not copies). Currently has some dead code; may be revisited in SIFT sprint.

### Passes

1. **FlattenPass** (`-flatten`): Inlines hierarchical actors. Processes by depth (leaves first). Fixes double-edges after flattening.
2. **IaraCanonicalizePass**: Canonicalizes types (wraps scalars in `tensor<1x...>`), expands implicit edges, inserts and specializes broadcasts.
3. **VirtualFIFOSchedulerPass** (`-virtual-fifo-scheduler`): Main compilation pipeline:
   - `upgradeDelaysToDenseArrays` → `breakLoops` → `analyzeAndAnnotate` → `generateAllocsAndFrees` → `codegenStaticData`
   - **SDF Analysis**: Node ranks (BFS), total firings (flow ratios), admissibility validation
   - **VirtualFIFOAnalysis**: Analyzes inout chains, calculates buffer sizes (α/β via Presburger — default), inserts delay copy nodes
   - **BufferSizeCalculator**: Presburger-based (default). Legacy Python script fallback exists but is deprecated.
   - **GenerateMemoryManagementNodes**: Transforms pure in/out into inout with alloc/dealloc nodes
   - **BreakLoops**: Breaks self-loops with copy nodes
   - **Codegen**: LLVM globals for node/edge static data, node wrappers
4. **RingBufferSchedulerPass**: Older scheduler, deprioritized — may be finished in future sprint.

### Key Concepts
- **Inout chains**: Edges sharing memory through producer→consumer. Tracked via `followInoutChainForwards/Backwards`.
- **Buffer size calculation**: Diophantine equations for block alignment (α/β values per rate in chain).
- **Node wrappers**: LLVM functions extracting data pointers from VirtualFIFO_Chunk spans, calling kernels.
- `IARA_DIR` env var required at compile time for legacy buffer size script.

## Runtime Architecture

### Virtual FIFO (`runtime/virtual-fifo/`)
- **VirtualFIFO_Chunk**: `{allocated*, virtual_offset, data*, data_size}`. `take_front`/`take_back` split chunks.
- **VirtualFIFO_Edge**: StaticInfo (13 i64 fields) + CodegenInfo (pointers to consumer/producer/alloc, delay data, next chain).
  - `push()`: Slices chunk by consumer rate, calls `consumer->consume()` per slice
  - `propagate_delays()`: Copies delay data into first chunk
  - `getConsumerSlice()`: Maps virtual offset → (firing seq, offset, size)
- **VirtualFIFO_Node**: StaticInfo + CodegenInfo (wrapper, FIFOs) + RuntimeInfo (semaphore).
  - `consume()` → keyed semaphore → `fire()` when all deps met
  - `prime()` → called by run_iteration, ensures allocs, arrives at semaphore
  - `fire()` → submits work-stealing task: call wrapper then push to output FIFOs
  - `fireAlloc()` / `ensureAlloc()` → on-demand memory allocation

### Keyed Semaphore
- `gtl::parallel_flat_hash_map` with 4-way sharding and mutexes
- `arrive(key, resources, total)`: first→init, every→accumulate, last→fire and erase
- Normal nodes: total = arg_bytes + needs_priming

### Scheduler Flow
1. `iara_runtime_init()`: Init parallelism, init nodes, fire initial allocs (block 0)
2. `iara_runtime_run_iteration()`: Submit prime() tasks for all priming nodes
3. `iara_runtime_exec(fn)`: OpenMP parallel/single wrapper

### Scripts
- `mlir-to-llvmir.sh`: MLIR → LLVM IR lowering pipeline
- `calc-buf-size.sh`: Deprecated Python buffer size solver
- `cache-environment.sh` / `generate_env.sh`: Environment management
- `build_tests.sh`: Generate → cmake → ctest pipeline
