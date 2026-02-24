#!/usr/bin/env bash
# Preesm codegen script for 13-degridder
# Generates a parametric Preesm scenario from a template (substituting NUM_CHUNK
# and NUM_KERNEL_SUPPORT), then runs the Preesm Eclipse CLI to produce C code.
set -euo pipefail

echo "=== Preesm codegen ==="

# --- Validate required env vars ---
: "${SIZE:?Must set SIZE (small/medium/large)}"
: "${NUM_CORES:?Must set NUM_CORES}"
: "${NUM_CHUNK:?Must set NUM_CHUNK}"
: "${NUM_KERNEL_SUPPORT:?Must set NUM_KERNEL_SUPPORT}"
: "${PATH_TO_TEST_BUILD_DIR:?Must set PATH_TO_TEST_BUILD_DIR}"
: "${IARA_DIR:?Must set IARA_DIR}"

# --- Locate Preesm repository (sibling to IaRa) ---
PREESM_REPO="${PREESM_DEGRIDDER_REPO:-${IARA_DIR}/../degridder}"
if [ ! -d "$PREESM_REPO" ]; then
  echo "ERROR: Preesm degridder repo not found at $PREESM_REPO" >&2
  echo "Set PREESM_DEGRIDDER_REPO to override." >&2
  exit 1
fi

# --- Generate .slam architecture + .scenario from Python script ---
GENERATOR="$IARA_DIR/applications/13-degridder/generate_preesm_scenario.py"

SCENARIO_NAME=$(python3 "$GENERATOR" \
  --num-cores "$NUM_CORES" \
  --num-chunk "$NUM_CHUNK" \
  --num-kernel-support "$NUM_KERNEL_SUPPORT" \
  --size "$SIZE" \
  --preesm-repo "$PREESM_REPO")

echo "Generated scenario: $SCENARIO_NAME"

# --- Run Preesm CLI ---
# CLI script: $IARA_DIR/preesm/commandLinePreesm.sh
# Usage: commandLinePreesm.sh <PREESM_DIR> <APP_DIR> <WORKFLOW> <SCENARIO>
#   PREESM_DIR  = path to Preesm Eclipse installation (contains ./eclipse binary)
#   APP_DIR     = Eclipse project dir (must contain .project, Workflows/, Scenarios/)
#   WORKFLOW    = workflow filename (relative to APP_DIR/Workflows/)
#   SCENARIO    = scenario filename (relative to APP_DIR/Scenarios/)

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

# --- Generate generated_data_path.h (same as codegen.sh) ---
DATA_ROOT_PATH="$IARA_DIR/applications/13-degridder/data"
GENERATED_HEADER="$PATH_TO_TEST_BUILD_DIR/generated_data_path.h"

cat > "$GENERATED_HEADER" << EOF
#ifndef GENERATED_DATA_PATH_H
#define GENERATED_DATA_PATH_H
#define DATA_ROOT_PATH "$DATA_ROOT_PATH"
#endif // GENERATED_DATA_PATH_H
EOF
echo "Generated $GENERATED_HEADER"

# --- Set up paths that Preesm binary expects at runtime ---
# Preesm's degridder/Code/src/top.c uses relative paths ../data/ and writes
# output to ../output/, relative to the working directory where a.out is run.
# a.out is run from PATH_TO_TEST_BUILD_DIR, so we create a symlink and dir
# at the parent level so those relative paths resolve correctly.
DATA_SRC="$IARA_DIR/applications/13-degridder/data"
ln -sfn "$DATA_SRC" "${PATH_TO_TEST_BUILD_DIR}/../data"
mkdir -p "${PATH_TO_TEST_BUILD_DIR}/../output"
# Also create output/ in build dir for any files written there
mkdir -p "${PATH_TO_TEST_BUILD_DIR}/output"
echo "=== Preesm codegen done ==="
