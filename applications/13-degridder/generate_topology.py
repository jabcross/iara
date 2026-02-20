"""
Generate topology.mlir for degridder based on parameters.

This script reads a topology.mlir.template and substitutes parameters
to generate the actual topology.mlir file for a specific experiment instance.

Usage:
    python3 generate_topology.py <size> <num_supports> <num_chunk> > topology.mlir

Args:
    size: Dataset size (small, medium, large)
    num_supports: Kernel support size
    num_chunk: Number of chunks
"""

import sys


def generate_topology(size: str, num_supports: int, num_chunk: int) -> str:
    """
    Generate topology.mlir content for degridder with given parameters.

    Args:
        size: Dataset size (small, medium, large)
        num_supports: Kernel support size
        num_chunk: Number of chunks

    Returns:
        Generated topology.mlir content as string
    """
    # Parameters based on size
    size_params = {
        'small': {
            'GRID_SIZE': 2560,
            'NUM_VISIBILITIES': 3924480,
            'NUM_SCENARIO': 1,
        },
        'medium': {
            'GRID_SIZE': 3840,
            'NUM_VISIBILITIES': 5886720,
            'NUM_SCENARIO': 2,
        },
        'large': {
            'GRID_SIZE': 5120,
            'NUM_VISIBILITIES': 7848960,
            'NUM_SCENARIO': 3,
        }
    }

    size_config = size_params.get(size, size_params['large'])

    # Fixed parameters from old degridder
    NUM_KERNELS = 17
    OVERSAMPLING_FACTOR = 16

    # Calculated parameters
    NUMBER_SAMPLE_IN_KERNEL = (num_supports + 1) * OVERSAMPLING_FACTOR * (num_supports + 1) * OVERSAMPLING_FACTOR
    TOTAL_KERNELS_SAMPLES = NUM_KERNELS * NUMBER_SAMPLE_IN_KERNEL
    NUM_VISIB_D_N_CHUNK = size_config['NUM_VISIBILITIES'] // num_chunk

    # Read template
    with open(__file__.replace('generate_topology.py', 'experiment/topology.mlir.template'), 'r') as f:
        content = f.read()

    # Apply substitutions
    substitutions = {
        'GRID_SIZE_VALUE': str(size_config['GRID_SIZE']),
        'NUM_KERNEL_SUPPORT_VALUE': str(num_supports),
        'OVERSAMPLING_FACTOR_VALUE': str(OVERSAMPLING_FACTOR),
        'NUMBER_SAMPLE_IN_KERNEL_VALUE': str(NUMBER_SAMPLE_IN_KERNEL),
        'NUM_KERNELS_VALUE': str(NUM_KERNELS),
        'NUM_SCENARIO_VALUE': str(size_config['NUM_SCENARIO']),
        'NUM_VISIBILITIES_VALUE': str(size_config['NUM_VISIBILITIES']),
        'TOTAL_KERNELS_SAMPLES_VALUE': str(TOTAL_KERNELS_SAMPLES),
        'NUM_CHUNK_VALUE': str(num_chunk),
        'NUM_VISIB_D_N_CHUNK_VALUE': str(NUM_VISIB_D_N_CHUNK),
    }

    for placeholder, value in substitutions.items():
        content = content.replace(placeholder, value)

    return content


if __name__ == "__main__":
    if len(sys.argv) != 4:
        print("Usage: python3 generate_topology.py <size> <num_supports> <num_chunk>", file=sys.stderr)
        sys.exit(1)

    size = sys.argv[1]
    num_supports = int(sys.argv[2])
    num_chunk = int(sys.argv[3])

    try:
        topology = generate_topology(size, num_supports, num_chunk)
        print(topology)
    except Exception as e:
        print(f"Error generating topology: {e}", file=sys.stderr)
        sys.exit(1)
