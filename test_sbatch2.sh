#!/bin/bash
#SBATCH --job-name=test-cmd
set -e
source "${IARA_DIR:-.}/sorgan_env.sh"
python3 -m tools.experiment_framework run --app 05-cholesky --set quick
