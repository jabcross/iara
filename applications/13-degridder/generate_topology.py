"""
Generate topology.mlir for degridder based on parameters.

Reads the block-arg-parameter topology.mlir.template and substitutes the four
numeric literals inside its `default_params` dict. Everything else (computed
sizes like TOTAL_KERNELS_SAMPLES, the multi-rate edge sizes, the arith chains)
is expressed in the template itself in terms of the four block-arg params and
resolved by --iara-param-materialize at compile time.

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
        'small': {'GRID_SIZE': 2560, 'NUM_VISIBILITIES': 3924480},
        'medium': {'GRID_SIZE': 3840, 'NUM_VISIBILITIES': 5886720},
        'large': {'GRID_SIZE': 5120, 'NUM_VISIBILITIES': 7848960},
    }

    size_config = size_params.get(size, size_params['large'])

    # Read template
    with open(__file__.replace('generate_topology.py', 'experiment/topology.mlir.template'), 'r') as f:
        content = f.read()

    # Apply substitutions (only the default_params literals remain)
    substitutions = {
        'GRID_SIZE_VALUE': str(size_config['GRID_SIZE']),
        'NUM_VISIBILITIES_VALUE': str(size_config['NUM_VISIBILITIES']),
        'NUM_KERNEL_SUPPORT_VALUE': str(num_supports),
        'NUM_CHUNK_VALUE': str(num_chunk),
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
