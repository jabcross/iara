#!/bin/sh
SCRIPT_DIR=$(dirname "$(readlink -f "$0")")

# Check that environment is loaded
if [ -z "$IARA_DIR" ]; then
	echo "Error: IARA_DIR not set. Please source .env first:"
	echo "  source $SCRIPT_DIR/../.env"
	exit 1
fi

# Verify Spack view paths are accessible
SPACK_VIEW_PATH=$PROJECTS_DIR/iara/spack/var/spack/environments/iara_env/.spack-env/view
if [ ! -d "$SPACK_VIEW_PATH" ]; then
	echo "Warning: Spack view not found at $SPACK_VIEW_PATH"
	echo "Build may fail if dependencies are not found."
fi

# Create build directory if it doesn't exist
mkdir -p $IARA_DIR/build
cd $IARA_DIR/build

# Run CMake configuration (CMakeLists.txt will read environment variables)
# Only configure if CMakeCache.txt doesn't exist or if forced
if [ ! -f CMakeCache.txt ] || [ "$1" = "--reconfigure" ]; then
	echo "Configuring CMake (reading settings from environment)..."
	cmake -G "Ninja" ..
	if [ $? -ne 0 ]; then
		echo "CMake configuration failed. Make sure you sourced .env"
		exit 1
	fi
	
	# Remove problematic flag from compile_commands.json
	sed -i 's/ -fno-lifetime-dse//' compile_commands.json
fi

# Build
echo "Building with Ninja..."
ninja
