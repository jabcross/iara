#!/bin/bash
# Monitor experiment progress

echo "=== Experiment Monitor ==="
echo ""

# Check if process is running
if ps aux | grep -q "[p]ython3 run_experiment"; then
    echo "✓ Experiment is RUNNING"
    echo ""
else
    echo "✗ Experiment is NOT running (completed or failed)"
    echo ""
fi

# Build progress
echo "Build Progress:"
build_count=$(tail -1 /scratch/pedro.ciambra/repos/iara/experiment/cholesky/experiment.log 2>/dev/null | grep -oP "Building \K\d+(?=/144)" || echo "0")
echo "  Configurations built: $build_count/144"
echo ""

# Results progress  
echo "Run Progress:"
result_count=$(wc -l < /scratch/pedro.ciambra/repos/iara/experiment/cholesky/results.csv 2>/dev/null || echo "1")
result_count=$((result_count - 1))  # Subtract header
total_runs=$((144 * 5))  # 144 configs * 5 repetitions
echo "  Completed runs: $result_count/$total_runs"
echo ""

# SLURM queue
slurm_jobs=$(squeue -u $USER -h | wc -l)
echo "Active SLURM jobs: $slurm_jobs"
echo ""

# Last few log lines
echo "Recent activity:"
tail -5 /scratch/pedro.ciambra/repos/iara/experiment/cholesky/experiment.log 2>/dev/null || echo "No log yet"
echo ""

# Check for completion
if grep -q "=== Experiment completed ===" /scratch/pedro.ciambra/repos/iara/experiment/cholesky/experiment.log 2>/dev/null; then
    echo "🎉 EXPERIMENT COMPLETED!"
    last_line=$(grep "=== Experiment completed ===" /scratch/pedro.ciambra/repos/iara/experiment/cholesky/experiment.log | tail -1)
    echo "$last_line"
fi
