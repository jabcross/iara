#!/bin/sh
cd "$(dirname "$(readlink -f "$0")")/../.."
echo -n 'Projects dir: '
pwd
PROJECTS_DIR=$(pwd)

cat <<EOF >$PROJECTS_DIR/iara/.env
export IARA_DIR="$PROJECTS_DIR/iara"
export PROJECTS_DIR="$PROJECTS_DIR"
# export POLYGEIST_DIR="$PROJECTS_DIR/Polygeist"
# export POLYGEIST_BUILD="\$POLYGEIST_DIR/build"
export LLVM_SOURCES="$PROJECTS_DIR/clangir"
export LLVM_INSTALL="$HOME/llvm-install"
export LLVM_BUILD="\$LLVM_SOURCES/build"
export PATH="\$PATH:\$IARA_DIR/scripts:\$LLVM_INSTALL/bin:\$IARA_DIR/build/bin:\$LLVM_SOURCES/build/bin"
export PS1="(iara)\$PS1"

# Activate Spack environment
source \$IARA_DIR/spack/share/spack/setup-env.sh
spack env activate iara_env

# Export Spack view paths for all libraries
SPACK_VIEW_PATH=\$PROJECTS_DIR/iara/spack/var/spack/environments/iara_env/.spack-env/view
export SPACK_VIEW_PATH
export C_INCLUDE_PATH=\$SPACK_VIEW_PATH/include:\$C_INCLUDE_PATH
export CPLUS_INCLUDE_PATH=\$SPACK_VIEW_PATH/include:\$CPLUS_INCLUDE_PATH
export LIBRARY_PATH=\$SPACK_VIEW_PATH/lib:\$LIBRARY_PATH
export LD_LIBRARY_PATH=\$SPACK_VIEW_PATH/lib:\$LLVM_INSTALL/lib:\$LLVM_INSTALL/lib/x86_64-unknown-linux-gnu:\$LD_LIBRARY_PATH
export PKG_CONFIG_PATH=\$SPACK_VIEW_PATH/lib/pkgconfig:\$PKG_CONFIG_PATH
export LDFLAGS="-L\$SPACK_VIEW_PATH/lib \$LDFLAGS"

EOF

echo "Created .env (with Spack activation)"
