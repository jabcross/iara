#!/usr/bin/env python3
"""
Degridder experiment runner using the generalized experiment framework.

Uses AppExperimentRunner base class for all generic functionality.
Only implements Degridder-specific logic:
- Test directory naming convention
- Parameter to environment variable mapping
"""

import sys
from pathlib import Path

# Add framework to path
sys.path.insert(0, str(Path(__file__).parent.parent.parent / 'experiment-framework'))

from app_runner import AppExperimentRunner
from base_runner import create_cli_parser


class DegridderRunner(AppExperimentRunner):
    """Degridder-specific experiment runner."""

    def create_test_directory(self, params) -> Path:
        """Create test directory with Degridder-specific naming convention."""
        num_cores = params['num_cores']
        num_chunk = params['num_chunk']
        scheduler = params['scheduler']

        dir_name = f"XX-degridder-{num_cores}cores_{num_chunk}chunks_{scheduler}"
        dir_path = Path('test') / 'Iara' / dir_name

        # Create directory
        dir_path.mkdir(parents=True, exist_ok=True)

        # Write parameters.sh
        params_file = dir_path / 'parameters.sh'
        with open(params_file, 'w') as f:
            f.write(f"export NUM_CORES={num_cores}\n")
            f.write(f"export NUM_CHUNK={num_chunk}\n")
            f.write(f"export NUM_KERNEL_SUPPORT={params['num_supports']}\n")
            f.write(f"export GRID_SIZE={params['grid_size']}\n")
            f.write(f"export SCHEDULER={scheduler}\n")

        # Write CMakeLists.txt
        cmake_file = dir_path / 'CMakeLists.txt'
        build_config = self.config.get('application', {}).get('build', {})
        defines_str = ' '.join([f'-D{d}' for d in build_config.get('defines', [])])
        linker_args_str = ' '.join(build_config.get('extra_linker_args', []))

        cmake_content = f"""# Build configuration from experiments.yaml
set(EXTRA_KERNEL_ARGS "{defines_str}" CACHE STRING "Extra kernel compilation arguments")
set(EXTRA_LINKER_ARGS "{linker_args_str}" CACHE STRING "Extra linker arguments")

# NOTE: Runtime parameters (NUM_CORES, NUM_CHUNK, NUM_KERNEL_SUPPORT, GRID_SIZE, SCHEDULER)
# are sourced from parameters.sh by the parent test/CMakeLists.txt
"""

        with open(cmake_file, 'w') as f:
            f.write(cmake_content)

        return dir_path

    def get_parameter_exports(self, params) -> dict:
        """Map Degridder parameters to environment variables."""
        return {
            'NUM_CORES': str(params['num_cores']),
            'NUM_CHUNK': str(params['num_chunk']),
            'NUM_KERNEL_SUPPORT': str(params['num_supports']),
            'GRID_SIZE': str(params['grid_size']),
            'SCHEDULER': params['scheduler'],
        }


if __name__ == '__main__':
    parser = create_cli_parser()
    args = parser.parse_args()

    try:
        runner = DegridderRunner('experiments.yaml', 'degridder', 'degridder')

        if args.list:
            runner.run()
        else:
            runner.run(args.experiment_set)

    except Exception as e:
        print(f"ERROR: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc()
        sys.exit(1)
