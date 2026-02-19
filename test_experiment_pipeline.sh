#!/bin/bash
# Test the complete experiment pipeline

set -e

source .env.cached

echo "Testing Experiment Pipeline"
echo "============================"
echo

# Test 1: Verify runners work
echo "1. Testing experiment runners..."
cd applications/cholesky/experiment
python3 run_experiments --list > /dev/null && echo "   ✓ Cholesky runner works"
cd ../../..

cd applications/sift/experiment
python3 run_experiments --list > /dev/null && echo "   ✓ SIFT runner works"
cd ../../..

cd applications/degridder/experiment
python3 run_experiments --list > /dev/null && echo "   ✓ Degridder runner works"
cd ../../..

echo
echo "2. Running Cholesky dev experiment..."
cd applications/cholesky/experiment
python3 run_experiments -e dev
cd ../../..

echo
echo "=============================="
echo "Pipeline test complete!"
echo "=============================="
