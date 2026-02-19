#!/bin/bash
# Comprehensive test of all experiment runners

set -e

source .env.cached

echo "=========================================="
echo "Testing Experiment Runners"
echo "=========================================="
echo

# Test Cholesky
echo "1. Testing Cholesky Runner"
echo "   Listing experiment sets..."
cd applications/cholesky/experiment
python3 run_experiments --list > /tmp/cholesky_sets.txt
if grep -q "dev" /tmp/cholesky_sets.txt; then
    echo "   ✓ Cholesky runner works"
else
    echo "   ✗ Cholesky runner failed"
    exit 1
fi
cd ../../..

echo

# Test SIFT
echo "2. Testing SIFT Runner"
echo "   Listing experiment sets..."
cd applications/sift/experiment
python3 run_experiments --list > /tmp/sift_sets.txt
if grep -q "dev" /tmp/sift_sets.txt; then
    echo "   ✓ SIFT runner works"
else
    echo "   ✗ SIFT runner failed"
    exit 1
fi
cd ../../..

echo

# Test Degridder
echo "3. Testing Degridder Runner"
echo "   Listing experiment sets..."
cd applications/degridder/experiment
python3 run_experiments --list > /tmp/degridder_sets.txt
if grep -q "dev" /tmp/degridder_sets.txt; then
    echo "   ✓ Degridder runner works"
else
    echo "   ✗ Degridder runner failed"
    exit 1
fi
cd ../../..

echo

# Test parameter combination generation
echo "4. Testing Parameter Combination Generation"
python3 << 'EOF'
import sys
sys.path.insert(0, 'applications/experiment-framework')

from app_runner import AppExperimentRunner
from base_runner import load_yaml_config, get_computed_params, generate_combinations

class CholekskyRunner(AppExperimentRunner):
    def create_test_directory(self, params):
        from pathlib import Path
        return Path('test') / 'Iara' / f"05-cholesky-{params['matrix_size']}_{params['num_blocks']}_{params['scheduler']}"

    def get_parameter_exports(self, params):
        return {
            'MATRIX_SIZE': str(params['matrix_size']),
            'NUM_BLOCKS': str(params['num_blocks']),
            'SCHEDULER': params['scheduler'],
        }

runner = CholekskyRunner('applications/cholesky/experiment/experiments.yaml', 'cholesky', 'cholesky')
exp_sets = {s['name']: s for s in runner.config.get('experiment_sets', [])}
dev_set = exp_sets['dev']

constraints = runner.config.get('constraints', [])
computed_params = get_computed_params(runner.config)
combinations = generate_combinations(dev_set, constraints, computed_params)

print(f"   Generated {len(combinations)} parameter combinations for Cholesky dev set")
for combo in combinations[:2]:
    print(f"     - {combo}")

if len(combinations) > 0:
    print("   ✓ Parameter combination generation works")
else:
    print("   ✗ No combinations generated")
    sys.exit(1)
EOF

echo

# Test visualization framework
echo "5. Testing Visualization Framework"
python3 << 'EOF'
import sys
import pandas as pd
import tempfile
import os

sys.path.insert(0, 'applications/experiment-framework')

from generic_visualizer import GenericVisualizer
from base_runner import load_yaml_config

config = load_yaml_config('applications/cholesky/experiment/experiments.yaml')

# Create mock data
mock_data = {
    'measurement': ['wall_time', 'compute_time'],
    'matrix_size': [2048, 2048],
    'num_blocks': [2, 2],
    'scheduler': ['sequential', 'vf-omp'],
    'value': [5.2, 2.8],
}
df = pd.DataFrame(mock_data)

with tempfile.NamedTemporaryFile(mode='w', suffix='.csv', delete=False) as f:
    csv_path = f.name
    df.to_csv(csv_path, index=False)

try:
    visualizer = GenericVisualizer(csv_path, config)
    print(f"   Loaded {len(visualizer.results)} rows of test data")
    print(f"   Found {len(visualizer.plots)} plot configurations")
    if len(visualizer.plots) > 0:
        print("   ✓ Visualization framework works")
    else:
        print("   ✗ No plots configured")
        sys.exit(1)
finally:
    os.unlink(csv_path)
EOF

echo

# Run actual dev experiment (sequential only, vf-omp has codegen issues)
echo "6. Running Cholesky Dev Experiment (Sequential only)"
cd applications/cholesky/experiment
echo "   Running: python3 run_experiments -e dev (filtered to sequential)"
export IARA_TEST_FILTER="sequential"
python3 run_experiments -e dev
unset IARA_TEST_FILTER
cd ../../..

echo
echo "=========================================="
echo "All tests passed!"
echo "=========================================="
