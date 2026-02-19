#!/usr/bin/env python3
"""
Generate CMakeLists.txt files for Cholesky test instances.

This script creates test instance CMakeLists.txt files that:
1. Source parameters.sh for runtime parameters (MATRIX_SIZE, NUM_BLOCKS, SCHEDULER)
2. Read build configuration from experiments.yaml
3. Set CMake cache variables for EXTRA_KERNEL_ARGS and EXTRA_LINKER_ARGS
   that will be used by the parent test/CMakeLists.txt

The build configuration is read from the YAML and not from shell exports,
ensuring a clean separation between runtime and build configuration.
"""

import os
import sys
import glob
import yaml
from pathlib import Path


def read_experiments_yaml(yaml_path):
    """Load experiments.yaml configuration."""
    with open(yaml_path, 'r') as file:
        return yaml.safe_load(file)


def generate_cmakelists(pattern="05-cholesky"):
    """Generate CMakeLists.txt files for matching test directories."""
    base_dir = "test/Iara"
    yaml_path = Path(__file__).parent.parent / 'cholesky' / 'experiment' / 'experiments.yaml'

    if not yaml_path.exists():
        print(f"Error: {yaml_path} not found", file=sys.stderr)
        return 0

    config = read_experiments_yaml(yaml_path)
    build_config = config.get('application', {}).get('build', {})
    defines = build_config.get('defines', [])
    extra_linker_args = build_config.get('extra_linker_args', [])

    # Convert to CMake list format
    defines_str = ' '.join([f'-D{d}' for d in defines])
    linker_args_str = ' '.join(extra_linker_args)

    matching_dirs = glob.glob(os.path.join(base_dir, f"{pattern}-*"))
    created_files = []

    for dir_path in matching_dirs:
        parameters_sh = os.path.join(dir_path, "parameters.sh")
        if os.path.isfile(parameters_sh):
            cmake_file_path = os.path.join(dir_path, "CMakeLists.txt")
            with open(cmake_file_path, 'w') as cmake_file:
                cmake_file.write("# Build configuration from experiments.yaml\n")
                cmake_file.write(f"set(EXTRA_KERNEL_ARGS \"{defines_str}\" CACHE STRING \"Extra kernel compilation arguments\")\n")
                cmake_file.write(f"set(EXTRA_LINKER_ARGS \"{linker_args_str}\" CACHE STRING \"Extra linker arguments\")\n")
                cmake_file.write("\n")
                cmake_file.write("# NOTE: Runtime parameters (MATRIX_SIZE, NUM_BLOCKS, SCHEDULER, BLOCK_SIZE)\n")
                cmake_file.write("# are sourced from parameters.sh by the parent test/CMakeLists.txt\n")

            created_files.append(cmake_file_path)

    if created_files:
        print("Created the following CMakeLists.txt files:")
        for file in created_files:
            print(f"  {file}")
        print(f"\nTotal: {len(created_files)} files")
    else:
        print("No CMakeLists.txt files were created.")

    return len(created_files)


if __name__ == "__main__":
    pattern = sys.argv[1] if len(sys.argv) > 1 else "05-cholesky"
    generate_cmakelists(pattern)
