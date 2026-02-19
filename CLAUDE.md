# IaRa Project — Agent Directives

IaRa is a compiler for static dataflow with a dynamic runtime for memory management, competing with OpenMP and the Preesm dataflow compiler. This is a PhD project.

## Current Sprint

See `SPRINT.md` for the current task plan. See `agent_workspace/PROJECT_CONTEXT.md` for domain background and project structure.

## Workspace Conventions

- Use `agent_workspace/` for all agent-generated files (notes, scratch, analysis).
- Create `agent_workspace/Sprint-<YYYY-MM-DD>/` for each sprint session.
- **Do not place markdown or documentation files in the main `iara/` directory.** CLAUDE.md is the sole exception.
- Do not commit agent workspace files to git unless explicitly asked.

## Context Strategy

The full project source is ~38K lines (~150K tokens), well within the 1M context window. The sprint plan includes onboarding phases that bulk-load the codebase so implementation phases benefit from full project knowledge.

During onboarding, read broadly — the goal is complete codebase coverage.

**Exclusion rules — never read these unless explicitly asked:**
- `spack/` — git submodule, thousands of irrelevant files
- `build*/` directories — compiler build output
- `build-sift/` — 38K lines of generated IR
- `manual-builds/` — generated schedule files
- `results/` directories inside applications
- `agent_history/` — logs from previous Claude Code sessions; reading these will cause confusion with the current sprint
- `experiments/` — legacy precursor framework, superseded by `tools/`; ask permission before reading
- Generated IR files: `*.llvm.mlir`, `*.llvm.mlir.2`
- Binary files, object files, `.o`, `.a`, `.so`

**Before reading any file:** verify it is text and check its size.

Prompt caching is active. Avoid actions that invalidate the cache (reordering reads, unnecessary tool calls between cached blocks). When in doubt, ask the user.

## Reasoning Effort

Sprint phases specify a default effort level (low, medium, high).

- **Low** = get files into context with minimal analysis.
- **Medium** = understand structure and relationships.
- **High** = deep analysis, write notes, formulate questions.
- **NOTETAKING steps always override to high**, regardless of phase default.

## Notetaking Protocol

NOTETAKING steps appear periodically in the sprint plan. When you encounter one:

1. Increase reasoning effort to high.
2. Write concise notes in `agent_workspace/Sprint-<date>/`.
3. Commit key findings to long-term memory.
4. Ask the user any questions that arose.
5. Keep notes short and factual — bullet points, not prose.

## Collaboration

- **Ask questions when in doubt.** Do not make large assumptions about intent.
- When a step says "discuss with user" or "wait for go-ahead", stop and ask before proceeding.
- Treat checkpoints as save points — ensure context quality is high before moving on.

## Build & Environment

- Spack: local install (git submodule) with system upstream at `/export/spack`.
- Environment cached in `.env.cached`. Regenerate: `source sorgan_env.sh && source scripts/cache-environment.sh`.
- LLVM/ClangIR at `$LLVM_INSTALL`. Compiler builds to `build/bin/`.
- Python venv at `.venv/`. Testing framework in `tools/`.
