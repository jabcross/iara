#!/usr/bin/env bash
# Preesm codegen script for 13-degridder
# Generates Preesm scheduling code from pre-existing scenarios.
#
# Future work: generate parametric scenarios dynamically
# (analogous to generate_topology.py for IaRa).
set -euo pipefail

echo "=== Preesm codegen ==="

# --- Validate required env vars ---
: "${SIZE:?Must set SIZE (small/medium/large)}"
: "${NUM_CORES:?Must set NUM_CORES}"
: "${PATH_TO_TEST_BUILD_DIR:?Must set PATH_TO_TEST_BUILD_DIR}"
: "${IARA_DIR:?Must set IARA_DIR}"

# --- Locate Preesm repository (sibling to IaRa) ---
PREESM_REPO="${PREESM_DEGRIDDER_REPO:-${IARA_DIR}/../degridder}"
if [ ! -d "$PREESM_REPO" ]; then
  echo "ERROR: Preesm degridder repo not found at $PREESM_REPO" >&2
  echo "Set PREESM_DEGRIDDER_REPO to override." >&2
  exit 1
fi

SCENARIOS_DIR="$PREESM_REPO/Scenarios"

# --- Map (SIZE, NUM_CORES) → scenario file ---
# Available parametric scenarios: parametric_scenario_large_{1,2,4,6,8,64}_cores.scenario
# TODO: Add small/medium parametric scenario generation.
SCENARIO_FILE="${SCENARIOS_DIR}/parametric_scenario_${SIZE}_${NUM_CORES}_cores.scenario"

if [ ! -f "$SCENARIO_FILE" ]; then
  echo "ERROR: Scenario not found: $SCENARIO_FILE" >&2
  echo "Available scenarios:" >&2
  ls "$SCENARIOS_DIR"/*.scenario 2>/dev/null | sed 's/^/  /' >&2
  exit 1
fi

echo "Using scenario: $SCENARIO_FILE"

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

SCENARIO_FILENAME="$(basename "$SCENARIO_FILE")"

mkdir -p "${PATH_TO_TEST_BUILD_DIR}/generated"

echo "Running Preesm: workflow=Codegen.workflow scenario=$SCENARIO_FILENAME"
bash "$PREESM_SCRIPT" "$PREESM_DIR" "$PREESM_REPO" "Codegen.workflow" "$SCENARIO_FILENAME"

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

# --- Create output directory ---
mkdir -p output
echo "=== Preesm codegen done ==="
