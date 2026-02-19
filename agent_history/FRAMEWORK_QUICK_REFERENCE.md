# Framework Quick Reference Card

**Print this out or bookmark it for quick lookup**

---

## Command Cheat Sheet

### Run Full Pipeline (Recommended)

```bash
# Complete pipeline: generate → build → execute → visualize
python3 -m tools.experiment_framework run --app <app> --set <set>

# Example
python3 -m tools.experiment_framework run --app 05-cholesky --set regression

# With skip flags (for incremental runs)
python3 -m tools.experiment_framework run \
    --app 05-cholesky \
    --set regression \
    --skip-generate    # Use existing CMakeLists.txt
    --skip-build       # Use existing binaries
    --skip-execute     # Use existing measurements

# Backward compatibility wrapper
scripts/experiment run --app 05-cholesky --set regression
```

### Generate Test Configuration

```bash
# Basic usage
python3 -m tools.experiment_framework generate --app <app> --set <set>

# Examples
python3 -m tools.experiment_framework generate --app 05-cholesky --set regression
python3 -m tools.experiment_framework generate --app 08-sift --set scaling
python3 -m tools.experiment_framework generate --app demo --set quick_test

# With options
python3 -m tools.experiment_framework generate \
    --app 05-cholesky \
    --set regression \
    --verbose      # More output
```

### Build Generated Tests

```bash
# Build all tests
python3 -m tools.experiment_framework build --app <app> --set <set>

# Example
python3 -m tools.experiment_framework build --app 05-cholesky --set regression

# Check results
ls -la applications/05-cholesky/experiment/results/
cat applications/05-cholesky/experiment/results/results_*.json
```

### Execute Tests

```bash
# Execute all built instances
python3 -m tools.experiment_framework execute --app <app> --set <set>

# Example
python3 -m tools.experiment_framework execute --app 05-cholesky --set regression

# With options
python3 -m tools.experiment_framework execute \
    --app 05-cholesky \
    --set regression \
    --repetitions 5 \
    --timeout 600
```

### Generate Visualizations

```bash
# Generate plots and notebook
python3 -m tools.experiment_framework visualize --app <app> --set <set>

# Example
python3 -m tools.experiment_framework visualize --app 05-cholesky --set regression

# Use specific results file
python3 -m tools.experiment_framework visualize \
    --app 05-cholesky \
    --set regression \
    --results applications/05-cholesky/experiment/results/results_20260116T123456Z.json
```

---

## Directory Structure

```
applications/<app>/
├── experiment/
│   ├── experiments.yaml           ← Create this (your config)
│   ├── CMakeLists.txt            ← Generated (do not edit)
│   ├── .experiments.yaml.hash     ← Generated (do not edit)
│   └── results/
│       └── results_*.json         ← Generated (build output)
└── code/
    └── (your application code)
```

---

## YAML File Template

```yaml
application:
  name: "app-name"
  entry: "executable-name"
  source_dir: "../code"
  build:
    type: "cmake"
    extra_linker_args: ["-lm"]

parameters:
  - name: "param1"
    type: "int"
    description: "Description"
  - name: "scheduler"
    type: "string"

computed_parameters:
  - name: "derived"
    type: "int"
    expression: "param1 * 2"

constraints:
  - expression: "param1 < 10000"
    description: "Skip huge cases"

experiment_sets:
  - name: "test"
    parameters:
      param1: [100, 500]
      scheduler: ["sequential"]

execution:
  timeout: 300
  repetitions: 3

output:
  results_dir: "./results"
```

---

## Instance Naming Pattern

```
<app>_<scheduler>_<param1>_<value1>_<param2>_<value2>...

Examples:
05-cholesky_sequential_matrix_size_2048_num_blocks_1
05-cholesky_omp-for_matrix_size_2048_num_blocks_2
demo_sequential_iterations_1_size_100
```

**Rules:**
- App name first
- Scheduler second
- Other params alphabetically
- Values sanitized (spaces→hyphens, lowercase)
- Computed params excluded

---

## Parameter Types

| Type | Example | Notes |
|------|---------|-------|
| `int` | `1024`, `2048` | Integer values |
| `float` | `1.5`, `2.0` | Floating point |
| `string` | `"sequential"`, `"omp"` | Text values |
| `bool` | `true`, `false` | Boolean |

---

## Experiment Set Definition

```yaml
experiment_sets:
  - name: "set_name"
    description: "What this set tests"
    parameters:
      param1: [value1, value2]  # List of values
      param2: [value3, value4]
```

**Generates:** Cartesian product of all value combinations

Example:
```yaml
parameters:
  param1: [A, B]
  param2: [1, 2]
  param3: [X]
```

**Generates 4 combinations:**
- A, 1, X
- A, 2, X
- B, 1, X
- B, 2, X

---

## Python API Quick Usage

```python
from tools.experiment_framework import (
    load_experiments_yaml,
    get_parameter_combinations,
    generate_cmakelists,
    compute_yaml_hash,
    check_yaml_changed
)
from pathlib import Path

# Load config
config = load_experiments_yaml(Path("applications/app/experiment/experiments.yaml"))

# Get combinations
combos = get_parameter_combinations(config, "set_name")
print(f"Generated {len(combos)} combinations")

# Generate CMakeLists.txt
num_instances = generate_cmakelists(
    Path("applications/app/experiment/experiments.yaml"),
    Path("applications/app/experiment/CMakeLists.txt"),
    "set_name"
)
print(f"Generated {num_instances} test instances")
```

---

## JSON Results Structure

```json
{
  "schema_version": "1.0.0",
  "experiment": {
    "application": "05-cholesky",
    "experiment_set": "regression",
    "timestamp": "2026-01-14T12:34:56Z",
    "git_commit": "abc12345",
    "yaml_hash": "2a3f5b7c..."
  },
  "instances": [
    {
      "name": "05-cholesky_sequential_matrix_size_2048_num_blocks_1",
      "parameters": {
        "scheduler": "sequential",
        "matrix_size": 2048,
        "num_blocks": 1
      },
      "compilation": {
        "iara_opt_time_s": 1.234,
        "mlir_llvm_time_s": 0.567,
        "clang_time_s": 0.890,
        "total_time_s": 2.691
      },
      "binary": {
        "total_size_bytes": 156234
      }
    }
  ],
  "failed_instances": [],
  "statistics": {
    "total_instances": 42,
    "successful_count": 42,
    "failed_count": 0,
    "success_rate": 1.0
  }
}
```

---

## Common Tasks

### View Experiment YAML
```bash
cat applications/05-cholesky/experiment/experiments.yaml
```

### Count How Many Tests Will Be Generated
```bash
python3 << 'EOF'
from tools.experiment_framework import load_experiments_yaml, get_parameter_combinations
from pathlib import Path

config = load_experiments_yaml(Path("applications/05-cholesky/experiment/experiments.yaml"))
for set_name in [s["name"] for s in config["experiment_sets"]]:
    combos = get_parameter_combinations(config, set_name)
    print(f"{set_name}: {len(combos)}")
EOF
```

### List All Instances That Will Be Generated
```bash
grep "NAME " applications/05-cholesky/experiment/CMakeLists.txt | \
    sed 's/.*NAME "//' | sed 's/".*//'
```

### Parse Results JSON
```python
import json
from pathlib import Path

results_file = Path("applications/05-cholesky/experiment/results/results_*.json")
with open(results_file) as f:
    data = json.load(f)

# Access data
for instance in data["instances"]:
    print(f"{instance['name']}: {instance['compilation']['total_time_s']:.2f}s")
```

### Check If YAML Changed
```bash
python3 << 'EOF'
from tools.experiment_framework import check_yaml_changed
from pathlib import Path

yaml_path = Path("applications/05-cholesky/experiment/experiments.yaml")
hash_file = Path("applications/05-cholesky/experiment/.experiments.yaml.hash")
changed = check_yaml_changed(yaml_path, hash_file)
print(f"Changed: {changed}")
EOF
```

### Verify CMake Can Parse Generated File
```bash
cmake -N -B /tmp/test -S applications/05-cholesky/experiment && echo "✓ OK" || echo "✗ FAILED"
```

---

## Troubleshooting

| Problem | Solution |
|---------|----------|
| "Unknown CMake command iara_add_test_instance" | Regenerate with latest framework |
| "YAML validation error" | Check YAML syntax (colons, indentation) |
| "No such file or directory" | Create directories: `mkdir -p applications/app/experiment` |
| "Parameter not found in expression" | Check computed_parameter expression syntax |
| "Constraint expression error" | Use Python syntax (and, or, not, ==, !=, >, <) |

---

## Common Constraint Examples

```yaml
constraints:
  # Skip large sizes
  - expression: "size < 10000"

  # Don't combine specific values
  - expression: "not (scheduler == 'sequential' and size > 5000)"

  # Require relationship between params
  - expression: "total_work < 1000000"  # Using computed param

  # Multiple conditions
  - expression: "scheduler != 'omp' or size < 5000"

  # Member test
  - expression: "size in [1024, 2048, 4096]"
```

---

## Expression Language

Computed parameters and constraints use Python expressions with access to:
- All parameters as variables: `size`, `scheduler`, etc.
- Basic math: `+`, `-`, `*`, `/`, `%`
- Comparisons: `==`, `!=`, `<`, `>`, `<=`, `>=`
- Logic: `and`, `or`, `not`
- Membership: `in`, `not in`

**Examples:**
```python
size * iterations                     # Arithmetic
size <= 2048                          # Comparison
scheduler != "sequential"             # String comparison
not (size > 5000 and iterations > 100)  # Logic
size in [1024, 2048, 4096]           # Membership
```

---

## Files Generated/Used

| File | Type | Created By | Purpose |
|------|------|-----------|---------|
| `experiments.yaml` | Input | User | Experiment definition |
| `CMakeLists.txt` | Output | Generator | Test configuration |
| `.experiments.yaml.hash` | Output | Generator | Change detection |
| `results_*.json` | Output | Builder | Build results |

---

## Phase Status

**Phase 1: Configuration** ✓ Complete
- Load and validate YAML
- Generate parameter combinations
- Create CMakeLists.txt

**Phase 2: Build System** ✓ Complete
- Build instances with CMake
- Measure compilation timing
- Track errors with retries
- Output results as JSON

**Phase 3: Measurement** ✓ Complete
- Execute instances
- Collect measurements
- Compute statistics

**Phase 4: Visualization** ✓ Complete
- Generate plots
- Create notebooks
- Execute notebooks

**Phase 5: CLI Integration** ✓ Complete
- Complete CLI interface
- Pipeline orchestration
- Backward compatibility wrapper

---

## Resources

- **User Guide:** `/scratch/pedro.ciambra/repos/iara/EXPERIMENT_FRAMEWORK_GUIDE.md`
- **Hands-On Demo:** `/scratch/pedro.ciambra/repos/iara/HANDS_ON_DEMO.md`
- **Full Spec:** `/home/pedro.ciambra/.claude/plans/agent_workspace/UNIFIED_EXPERIMENT_FRAMEWORK_SPEC.md`
- **Implementation Plans:** `/home/pedro.ciambra/.claude/plans/agent_workspace/*.md`

---

## Quick Links

**View existing applications:**
```bash
ls -1 applications/*/experiment/experiments.yaml
```

**Create new application:**
```bash
mkdir -p applications/my-app/{experiment,code}
# Then copy template and edit
```

**Run demo:**
```bash
cd /scratch/pedro.ciambra/repos/iara
# Follow HANDS_ON_DEMO.md
```

---

**Last Updated:** 2026-01-16 | Status: All Phases Complete | Production Ready
