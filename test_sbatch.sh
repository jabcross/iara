#!/bin/bash
#SBATCH --job-name=test-python
set -e
source /scratch/pedro.ciambra/repos/iara/sorgan_env.sh
which python3
python3 --version
