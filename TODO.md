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
