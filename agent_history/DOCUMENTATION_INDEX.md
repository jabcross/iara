# IaRa Experiment Framework - Documentation Index

**Where to start based on your needs**

---

## For First-Time Users

Start here and follow in order:

1. **[EXPERIMENT_FRAMEWORK_GUIDE.md](./EXPERIMENT_FRAMEWORK_GUIDE.md)**
   - Overview of what the framework does
   - Creating new applications
   - Defining experiments in YAML
   - Understanding results
   - ~45 min read

2. **[WALKTHROUGH_WITH_OUTPUT.md](./WALKTHROUGH_WITH_OUTPUT.md)**
   - Actual commands and their output
   - Run along to verify everything works
   - See real examples
   - ~30 min hands-on

3. **[HANDS_ON_DEMO.md](./HANDS_ON_DEMO.md)**
   - Interactive exercises
   - Try generating configurations
   - Inspect generated files
   - Create your own mini app
   - ~15 min to complete

4. **[FRAMEWORK_QUICK_REFERENCE.md](./FRAMEWORK_QUICK_REFERENCE.md)**
   - Bookmark this for daily reference
   - Quick command syntax
   - Common tasks
   - Troubleshooting table

---

## For Different User Types

### I want to define experiments for my application
→ Read: [EXPERIMENT_FRAMEWORK_GUIDE.md](./EXPERIMENT_FRAMEWORK_GUIDE.md) Section 2-3
→ Copy: The YAML template from Section 3
→ Follow: Section 4 "Generating Test Configuration"

### I want to see it in action
→ Run: [WALKTHROUGH_WITH_OUTPUT.md](./WALKTHROUGH_WITH_OUTPUT.md)
→ Follow: Each command and verify the output
→ Check: Your own outputs match expected outputs

### I want to learn the concepts
→ Read: [EXPERIMENT_FRAMEWORK_GUIDE.md](./EXPERIMENT_FRAMEWORK_GUIDE.md) Section 1
→ Study: The YAML file template in Section 3
→ Understand: How parameter combinations work (YAML Section 5)

### I want the quick answer
→ Use: [FRAMEWORK_QUICK_REFERENCE.md](./FRAMEWORK_QUICK_REFERENCE.md)
→ Find: Your command or task in the cheat sheet
→ Copy-paste: The example command

### I want to use the Python API
→ Look: [EXPERIMENT_FRAMEWORK_GUIDE.md](./EXPERIMENT_FRAMEWORK_GUIDE.md) Section 8
→ Study: [WALKTHROUGH_WITH_OUTPUT.md](./WALKTHROUGH_WITH_OUTPUT.md) Session 8
→ Reference: [FRAMEWORK_QUICK_REFERENCE.md](./FRAMEWORK_QUICK_REFERENCE.md) Python API section

### I'm debugging something
→ Check: [FRAMEWORK_QUICK_REFERENCE.md](./FRAMEWORK_QUICK_REFERENCE.md) Troubleshooting
→ Read: [EXPERIMENT_FRAMEWORK_GUIDE.md](./EXPERIMENT_FRAMEWORK_GUIDE.md) Section 11
→ Try: Commands from [WALKTHROUGH_WITH_OUTPUT.md](./WALKTHROUGH_WITH_OUTPUT.md)

---

## Document Descriptions

### EXPERIMENT_FRAMEWORK_GUIDE.md
**Length:** ~500 lines | **Time:** 45 minutes
**Contains:**
- Complete overview of framework capabilities
- Step-by-step guide to create new applications
- Detailed YAML file structure explanation
- How to generate test configuration
- How to build and understand results
- Common tasks with examples
- Troubleshooting guide

**Best for:** Learning the framework from scratch

### WALKTHROUGH_WITH_OUTPUT.md
**Length:** ~400 lines | **Time:** 30 minutes hands-on
**Contains:**
- Actual terminal commands and their output
- 12 complete sessions showing real usage
- Expected output for each command
- Verification that everything works
- Python API examples
- Status reporting

**Best for:** Seeing it in action, verifying your setup

### HANDS_ON_DEMO.md
**Length:** ~350 lines | **Time:** 15 minutes to complete
**Contains:**
- Interactive exercises
- Step-by-step instructions you can follow
- Create demo application
- Analyze generated configurations
- Inspect hash files
- Work with Python API
- Cleanup instructions

**Best for:** Getting hands-on experience

### FRAMEWORK_QUICK_REFERENCE.md
**Length:** ~200 lines | **Time:** Lookup reference
**Contains:**
- Command cheat sheet
- Directory structure template
- YAML file template
- Parameter types reference
- Common tasks (copy-paste ready)
- Python code snippets
- Troubleshooting table
- Quick links

**Best for:** Daily reference while working

---

## By Task

### "I want to create a new application with experiments"

```
1. Read: EXPERIMENT_FRAMEWORK_GUIDE.md Section 2 (Creating New Application)
2. Copy: YAML template from FRAMEWORK_QUICK_REFERENCE.md
3. Edit: Your experiments.yaml with your parameters
4. Generate: python3 -m tools.experiment_framework generate --app <app> --set <set>
5. Verify: Your CMakeLists.txt was created
6. Next: When Phase 3 ready, run build command
```

### "I want to understand what was generated"

```
1. Generate: python3 -m tools.experiment_framework generate --app <app> --set <set>
2. Look at: The generated CMakeLists.txt file
3. Read: EXPERIMENT_FRAMEWORK_GUIDE.md Section 6 (Understanding Results)
4. Try: Commands from WALKTHROUGH_WITH_OUTPUT.md to inspect files
5. Analyze: Use Python API from HANDS_ON_DEMO.md Section 8.1
```

### "I want to see actual output"

```
1. Start: WALKTHROUGH_WITH_OUTPUT.md
2. Follow: Each section in order (Sessions 1-12)
3. Run: The commands shown
4. Compare: Your output with expected output
5. Verify: That everything matches
```

### "I want the quick answer to a specific question"

```
1. Search: FRAMEWORK_QUICK_REFERENCE.md for your topic
2. If not there: Check EXPERIMENT_FRAMEWORK_GUIDE.md Section 10 (Common Tasks)
3. Still stuck: Look at relevant section in WALKTHROUGH_WITH_OUTPUT.md
4. Still need help: Check HANDS_ON_DEMO.md for interactive explanation
```

---

## Framework Status

**Phase 1: Configuration** ✓ COMPLETE
- YAML loading and validation
- Parameter combination generation
- CMakeLists.txt generation
- Change detection via hashing

**Phase 2: Build System** ✓ COMPLETE
- Build execution with CMake
- Compilation timing (3 phases)
- Binary size extraction
- Error tracking and retries
- JSON results output

**Phase 3: Measurement** ⏳ Coming Next
- Execute instances
- Collect runtime measurements
- Compute statistics
- Extend JSON output

**Phase 4: Visualization** ⏳ Coming Later
- Generate Vega-Lite plots
- Create Jupyter notebooks
- Analysis and statistics

**Phase 5: CLI Integration** ⏳ Coming Later
- Complete CLI interface
- Orchestration commands
- Full pipeline automation

---

## Key Concepts

### Parameters
Values that vary across tests. Examples:
- `scheduler`: sequential, omp-for, omp-task, vf-omp
- `matrix_size`: 1024, 2048, 4096
- `num_blocks`: 1, 2, 4, 8

### Computed Parameters
Derived from other parameters. Example:
- `block_size = matrix_size // num_blocks`

### Constraints
Combinations to skip. Example:
- `not (size > 5000 and iterations > 100)`

### Experiment Sets
Groups of related tests. Example:
- "regression": Quick smoke tests
- "scaling": Study impact of size
- "full": Comprehensive testing

### Instance Names
Unique identifier for each test:
```
<app>_<scheduler>_<param1>_<value1>_<param2>_<value2>
```

### Generated Files
- **CMakeLists.txt**: Test configuration (do not edit)
- **.experiments.yaml.hash**: Change detection (do not edit)
- **results_*.json**: Build results (generated when you build)

---

## Quick Start Commands

```bash
# Generate test configuration
python3 -m tools.experiment_framework generate --app 05-cholesky --set regression

# View what was generated
cat applications/05-cholesky/experiment/CMakeLists.txt | head -50

# Count instances
grep -c "iara_add_test_instance" applications/05-cholesky/experiment/CMakeLists.txt

# Verify CMake can parse it
cmake -N -B /tmp/test -S applications/05-cholesky/experiment && echo "✓ OK"

# Analyze with Python
python3 << 'EOF'
from tools.experiment_framework import load_experiments_yaml, get_parameter_combinations
from pathlib import Path

config = load_experiments_yaml(Path("applications/05-cholesky/experiment/experiments.yaml"))
combos = get_parameter_combinations(config, "regression")
print(f"Generated {len(combos)} test instances")
EOF
```

---

## Recommended Reading Order

**New User (first time):**
1. EXPERIMENT_FRAMEWORK_GUIDE.md (Overview section)
2. WALKTHROUGH_WITH_OUTPUT.md (Sessions 1-5)
3. HANDS_ON_DEMO.md (Part 1-4)
4. FRAMEWORK_QUICK_REFERENCE.md (bookmark for later)

**Developer (want to use it now):**
1. FRAMEWORK_QUICK_REFERENCE.md (command cheat sheet)
2. WALKTHROUGH_WITH_OUTPUT.md (specific session for your task)
3. HANDS_ON_DEMO.md (if you get stuck)

**Architect (want to understand design):**
1. EXPERIMENT_FRAMEWORK_GUIDE.md (Overview + Architecture)
2. Related spec files in `/home/pedro.ciambra/.claude/plans/agent_workspace/`

---

## File Location Reference

### In Repository
```
/scratch/pedro.ciambra/repos/iara/
├── EXPERIMENT_FRAMEWORK_GUIDE.md      ← Full user guide
├── WALKTHROUGH_WITH_OUTPUT.md         ← Real output examples
├── HANDS_ON_DEMO.md                   ← Interactive exercises
├── FRAMEWORK_QUICK_REFERENCE.md       ← Command cheat sheet
├── DOCUMENTATION_INDEX.md             ← This file
└── applications/
    ├── 05-cholesky/experiment/experiments.yaml
    ├── 08-sift/experiment/experiments.yaml
    └── 13-degridder/experiment/experiments.yaml
```

### In Plans Directory
```
/home/pedro.ciambra/.claude/plans/agent_workspace/
├── UNIFIED_EXPERIMENT_FRAMEWORK_SPEC.md  ← Full specification
├── PHASE_1_2_COMPLETION_STATUS.md        ← Project status
├── task_1.1_package_structure.md         ← Task specs
├── task_1.2_yaml_hashing.md
├── task_1.3_parameter_generation.md
├── task_1.4_cmake_generation.md
├── task_1.5_phase1_integration.md
├── task_2.1_build_execution.md
├── ... (more task specs and completion reports)
└── README.md                             ← Workspace overview
```

---

## Common Workflows

### Workflow 1: Create and Generate (Most Common)
```
1. Create YAML file (experiments.yaml)
2. Run: python3 -m tools.experiment_framework generate --app <app> --set <set>
3. Verify: cmake -N -B /tmp/test -S applications/<app>/experiment
4. Done: CMakeLists.txt and hash file created
```

### Workflow 2: Analyze What Was Generated
```
1. Run generate command
2. Use Python API to inspect
3. Look at generated CMakeLists.txt
4. Count instances
5. Review parameter combinations
```

### Workflow 3: Make Changes and Regenerate
```
1. Edit experiments.yaml
2. Run generate command again
3. Framework detects change via hash
4. New CMakeLists.txt created automatically
```

### Workflow 4: Manage Multiple Experiment Sets
```
1. Define multiple sets in YAML (quick, baseline, full)
2. Generate each set: generate --app <app> --set quick, baseline, full
3. Each set has its own CMakeLists.txt
4. Can manage independently
```

---

## Troubleshooting Quick Links

| Problem | Solution |
|---------|----------|
| CMake parse error | Run: `cmake -N -B /tmp/test -S <app_dir>` to see error |
| YAML validation error | Check syntax in experiments.yaml (indentation, colons) |
| Missing directories | Create: `mkdir -p applications/app/{experiment,code}` |
| Change not detected | Check hash file hasn't been manually edited |
| Instance names look wrong | Review parameter naming in YAML |
| Constraint not working | Test expression in Python: `eval(expression, {}, params)` |

See full troubleshooting in:
- [FRAMEWORK_QUICK_REFERENCE.md](./FRAMEWORK_QUICK_REFERENCE.md) - Quick answers
- [EXPERIMENT_FRAMEWORK_GUIDE.md](./EXPERIMENT_FRAMEWORK_GUIDE.md) Section 11 - Detailed help

---

## Next Steps After Reading

### Immediate (Try it)
- [ ] Create `demo-app/experiment/experiments.yaml`
- [ ] Run: `python3 -m tools.experiment_framework generate --app demo-app --set test`
- [ ] Verify: CMakeLists.txt was created

### Soon (Get comfortable)
- [ ] Create your own application YAML
- [ ] Generate multiple experiment sets
- [ ] Inspect generated configurations
- [ ] Try Python API examples

### Later (When Phase 3 ready)
- [ ] Build the test instances
- [ ] Collect measurements
- [ ] Analyze results
- [ ] Generate visualizations

---

## Support

**Questions about:**
- Using the framework → See docs above
- Framework design → See `/home/pedro.ciambra/.claude/plans/agent_workspace/` specs
- Bugs or issues → Check GitHub issues (framework is new)
- Contributing → Please do! Improvements welcome

---

## Summary

This documentation set covers:
- ✓ How to define experiments (YAML structure)
- ✓ How to generate test configurations
- ✓ How to understand what was generated
- ✓ How to use the Python API
- ✓ How to troubleshoot problems
- ✓ Real examples with actual output
- ✓ Quick reference for daily use

**Start with [EXPERIMENT_FRAMEWORK_GUIDE.md](./EXPERIMENT_FRAMEWORK_GUIDE.md) and you'll be productive in 45 minutes.**

---

**Last Updated:** 2026-01-14 | **Status:** Phase 1 & 2 Complete | **Next:** Phase 3 Ready for Implementation
