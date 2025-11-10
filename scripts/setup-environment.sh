#!/bin/sh

echo Setting up new environment.
echo checking for dependencies...

cat <<EOF
This script will clone LLVM (ClangIR fork) in a sister directory. It also clones a Spack installation for the dependencies of the compiler, tests and experiments.

This may take a long time. This script is not appropriate if you're not setting up from a clean slate.
Press enter to continue.
EOF
read discard__


cd "$(dirname "$(readlink -f "$0")")"

pwd

cd ../..

PROJECTS_DIR=$(pwd)

if [ ! -d "$PROJECTS_DIR/iara/spack" ]; then
  echo "Cloning Spack into iara/spack..."
  git clone https://github.com/spack/spack.git "$PROJECTS_DIR/iara/spack" || { echo "Failed to clone spack"; exit 1; }
fi

# load spack environment
. "$PROJECTS_DIR/iara/spack/share/spack/setup-env.sh"

JOBS=$(nproc 2>/dev/null || echo 1)

# Create or update Spack environment from spack.yaml in project root
if [ -f "$PROJECTS_DIR/iara/spack.yaml" ]; then
  echo "Setting up Spack environment from spack.yaml..."
  
  # Try to create the environment (will fail if it already exists)
  if spack env create iara_env "$PROJECTS_DIR/iara/spack.yaml" 2>/dev/null; then
    echo "Created new Spack environment 'iara_env'"
  else
    echo "Environment 'iara_env' already exists, activating it..."
  fi
  
  # Activate the environment
  spack env activate iara_env
  
  # Concretize and install packages
  echo "Installing Spack packages (this may take a while)..."
  spack concretize -f
  spack install -j "$JOBS"
  
  # Regenerate the view
  echo "Regenerating environment view..."
  spack env view regenerate
else
  echo "Warning: No spack.yaml found in project root"
fi

source $PROJECTS_DIR/iara/spack/share/spack/setup-env.sh


sh iara/scripts/generate_env.sh
. iara/.env

echo Checking for build dependencies:
for command in cmake ninja g++ mold gdb lldb ccache /usr/bin/time fzf ; do
  if ! which $command; then
    echo "$command not found"
    exit 1
  fi
done

# Export Spack view paths for all libraries
SPACK_VIEW_PATH=$PROJECTS_DIR/iara/spack/var/spack/environments/iara_env/.spack-env/view
export C_INCLUDE_PATH=$SPACK_VIEW_PATH/include:$C_INCLUDE_PATH
export CPLUS_INCLUDE_PATH=$SPACK_VIEW_PATH/include:$CPLUS_INCLUDE_PATH
export LIBRARY_PATH=$SPACK_VIEW_PATH/lib:$LIBRARY_PATH
export LD_LIBRARY_PATH=$SPACK_VIEW_PATH/lib:$LD_LIBRARY_PATH
export PKG_CONFIG_PATH=$SPACK_VIEW_PATH/lib/pkgconfig:$PKG_CONFIG_PATH

# POLYGEIST_COMMIT=4b04755a63fce54e3965c248e6da5c8ae20b26f4

# if test -d Polygeist; then
#   echo Polygeist already exists. Aborting.
#   exit 1
# fi

cd $PROJECTS_DIR
# pwd
# git clone https://github.com/wsmoses/Polygeist.git
# cd Polygeist
# pwd
# git pull origin main
# git fetch origin $POLYGEIST_COMMIT
# git checkout $POLYGEIST_COMMIT

# /usr/bin/time sh -c 'git submodule update --init'

# clone and build polygeist with clangir

cd $PROJECTS_DIR
git clone https://github.com/llvm/clangir.git
mkdir -p clangir/build
cd clangir/build


sh -c ' \
echo "Building LLVM (ClangIR repo)". ;\
pwd ;\
cmake --debug-find -GNinja $LLVM_SOURCES/llvm -DLLVM_ENABLE_PROJECTS="mlir;clang;compiler-rt;clang-tools-extra;openmp" -DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi;libunwind" -DCLANG_ENABLE_CIR=ON -DLLVM_BUILD_EXAMPLES=ON -DLLVM_TARGETS_TO_BUILD="host" -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_ASSERTIONS=ON -DLLVM_INSTALL_UTILS=ON -DLLVM_ENABLE_RTTI=ON -DLLVM_CCACHE_BUILD=1 -DLLVM_PARALLEL_LINK_JOBS=1 -DCMAKE_EXPORT_COMPILE_COMMANDS=YES -DCMAKE_INSTALL_PREFIX=$LLVM_INSTALL -DLLVM_USE_LINKER=mold
-DCMAKE_INCLUDE_PATH=$SPACK_VIEW_PATH/include -DCMAKE_LIBRARY_PATH=$SPACK_VIEW_PATH/lib ;\
/usr/bin/time -o llvm-build-time.txt ninja ; ninja install '

# cd $POLYGEIST_DIR

# /usr/bin/time -o polygeist-cmake-and-build-time.txt sh -c ' mkdir build ;\
# cd build ;\
# pwd ;\
# cmake -G Ninja .. -DMLIR_DIR=$LLVM_DIR/build/lib/cmake/mlir -DCLANG_DIR=$LLVM_DIR/build/lib/cmake/clang -DLLVM_TARGETS_TO_BUILD="host" -DLLVM_ENABLE_ASSERTIONS=ON -DCMAKE_BUILD_TYPE=DEBUG -DLLVM_CCACHE_BUILD=1 -DLLVM_PARALLEL_LINK_JOBS=1 -DLLVM_USE_LINKER=mold ;\
# /usr/bin/time -o polygeist-build-time.txt ninja ;'

cd $PROJECTS_DIR/iara
sh scripts/build-iara.sh
