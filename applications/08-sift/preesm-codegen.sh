#!/usr/bin/env bash
# Preesm codegen script for 08-sift
# Generates a Preesm scenario for the requested core count, runs the Preesm
# Eclipse CLI to produce C code, then copies it to the instance build dir.
set -euo pipefail

echo "=== Preesm codegen (SIFT) ==="

# --- Validate required env vars ---
: "${NUM_CORES:?Must set NUM_CORES}"
: "${PATH_TO_TEST_BUILD_DIR:?Must set PATH_TO_TEST_BUILD_DIR}"
: "${IARA_DIR:?Must set IARA_DIR}"

# --- Locate Preesm SIFT repository ---
PREESM_REPO="${PREESM_SIFT_REPO:-${IARA_DIR}/../preesm-apps/SIFT}"
if [ ! -d "$PREESM_REPO" ]; then
  echo "ERROR: Preesm SIFT repo not found at $PREESM_REPO" >&2
  echo "Set PREESM_SIFT_REPO to override." >&2
  exit 1
fi

# --- Generate .slam architecture + .scenario from Python script ---
GENERATOR="$IARA_DIR/applications/08-sift/generate_preesm_scenario.py"

SCENARIO_NAME=$(python3 "$GENERATOR" \
  --num-cores "$NUM_CORES" \
  --preesm-repo "$PREESM_REPO")

echo "Generated scenario: $SCENARIO_NAME"

# --- Run Preesm CLI ---
PREESM_SCRIPT="${IARA_DIR}/preesm/commandLinePreesm.sh"
PREESM_DIR="${PREESM_DIR:-/scratch/pedro.ciambra/bin/preesm-dir}"

if [ ! -f "$PREESM_SCRIPT" ]; then
  echo "ERROR: Preesm CLI script not found: $PREESM_SCRIPT" >&2
  exit 1
fi
if [ ! -x "$PREESM_DIR/eclipse" ]; then
  echo "ERROR: Preesm installation not found at $PREESM_DIR" >&2
  echo "Set PREESM_DIR=/path/to/preesm-dir" >&2
  exit 1
fi

mkdir -p "${PATH_TO_TEST_BUILD_DIR}/generated"

echo "Running Preesm: workflow=Codegen.workflow scenario=$SCENARIO_NAME"
bash "$PREESM_SCRIPT" "$PREESM_DIR" "$PREESM_REPO" "Codegen.workflow" "$SCENARIO_NAME"

# Preesm writes generated code to $PREESM_REPO/Code/generated/
# Copy to instance build dir
cp "$PREESM_REPO/Code/generated/"*.c "${PATH_TO_TEST_BUILD_DIR}/generated/" 2>/dev/null || true
cp "$PREESM_REPO/Code/generated/"*.h "${PATH_TO_TEST_BUILD_DIR}/generated/" 2>/dev/null || true

echo "Preesm generated $(ls "${PATH_TO_TEST_BUILD_DIR}/generated/"*.c 2>/dev/null | wc -l) C files"

# --- Patch generated/main.c to add wall-clock timing output ---
# Preesm-generated main.c has no timing; patch it to measure and print
# "Wall time: N s" in the same format as the IaRa (vf-omp) binary.
# The patch is idempotent: it checks for clock_gettime before modifying.
GENERATED_MAIN="${PATH_TO_TEST_BUILD_DIR}/generated/main.c"
if [ -f "$GENERATED_MAIN" ]; then
  python3 - "$GENERATED_MAIN" << 'PYEOF'
import sys
path = sys.argv[1]
with open(path, 'r') as f:
    content = f.read()
# Idempotent: only patch if not already patched
if 'clock_gettime' not in content:
    # Add time.h after stdio.h
    content = content.replace('#include <stdio.h>', '#include <stdio.h>\n#include <time.h>')
    # Insert clock start before communicationInit()
    content = content.replace(
        '  communicationInit();\n',
        '  struct timespec _t0, _t1;\n'
        '  clock_gettime(CLOCK_MONOTONIC, &_t0);\n'
        '  communicationInit();\n'
    )
    # Insert clock stop and printf before the final return 0
    idx = content.rfind('  return 0;\n}')
    if idx >= 0:
        tail = (
            '  clock_gettime(CLOCK_MONOTONIC, &_t1);\n'
            '  double _wt = (double)(_t1.tv_sec - _t0.tv_sec)'
            ' + (double)(_t1.tv_nsec - _t0.tv_nsec) * 1e-9;\n'
            '  printf("Wall time: %lf s\\n", _wt);\n'
            '  return 0;\n}'
        )
        content = content[:idx] + tail + content[idx + len('  return 0;\n}'):]
    with open(path, 'w') as f:
        f.write(content)
    print("Patched timing into: " + path)
else:
    print("Timing already present in: " + path)
PYEOF
fi

# --- Set up dat -> data symlink for PROJECT_ROOT_PATH ---
# Code/src/filenames.c uses PROJECT_ROOT_PATH "/dat/img1.pgm".
# IaRaApplications.cmake sets PROJECT_ROOT_PATH = IARA_DIR/applications/08-sift.
# Create dat -> data symlink so the path resolves correctly.
SIFT_APP_DIR="$IARA_DIR/applications/08-sift"
if [ ! -e "${SIFT_APP_DIR}/dat" ]; then
  ln -sfn "data" "${SIFT_APP_DIR}/dat"
  echo "Created symlink: ${SIFT_APP_DIR}/dat -> data"
fi

echo "=== Preesm codegen (SIFT) done ==="
