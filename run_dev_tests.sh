#!/bin/bash
# Simple script to run dev experiments for all three applications

# Source environment
source .env.cached

echo "======================================"
echo "Running Dev Experiments"
echo "======================================"
echo

# Cholesky
echo "1. Cholesky Dev Experiment"
echo "   Setting up and running..."
cd applications/cholesky/experiment
python3 run_experiments -e dev
cd ../../..

echo
echo "======================================"
echo "Experiment completed"
echo "======================================"
