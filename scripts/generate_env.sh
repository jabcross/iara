#!/bin/sh
# DEPRECATED: Environment variables are now defined directly in sorgan_env.sh.
# This script and the .env file it generates are no longer used.
# Regenerate the fast-startup cache with: source sorgan_env.sh && source scripts/cache-environment.sh
exit 0

cd "$(dirname "$(readlink -f "$0")")/../.."
echo -n 'Projects dir: '
pwd
PROJECTS_DIR=$(pwd)

if [[ -e $PROJECTS_DIR/iara/machine_specific_env.sh ]] ; then
    source $PROJECTS_DIR/iara/machine_specific_env.sh
fi

cat <<EOF >$PROJECTS_DIR/iara/.env
export IARA_DIR="$PROJECTS_DIR/iara"
export PROJECTS_DIR="$PROJECTS_DIR"
# export POLYGEIST_DIR="$PROJECTS_DIR/Polygeist"
# export POLYGEIST_BUILD="\$POLYGEIST_DIR/build"
export LLVM_SOURCES="$PROJECTS_DIR/clangir"
export LLVM_INSTALL="$PROJECTS_DIR/llvm-install"
export LLVM_BUILD="\$LLVM_SOURCES/build"
export PATH="\$IARA_DIR/scripts:\$LLVM_INSTALL/bin:\$IARA_DIR/build/bin:\$LLVM_SOURCES/build/bin:\$PATH:"
export PS1="(iara)\$PS1"

# Export Spack view paths for all libraries
SPACK_VIEW_PATH=\$IARA_DIR/spack/var/spack/environments/iara_env/.spack-env/view
export SPACK_VIEW_PATH
export C_INCLUDE_PATH=\$SPACK_VIEW_PATH/include:\$C_INCLUDE_PATH
export CPLUS_INCLUDE_PATH=\$SPACK_VIEW_PATH/include:\$CPLUS_INCLUDE_PATH
export LIBRARY_PATH=\$SPACK_VIEW_PATH/lib:\$LIBRARY_PATH
export LD_LIBRARY_PATH=\$SPACK_VIEW_PATH/lib:\$LLVM_INSTALL/lib:\$LLVM_INSTALL/lib/x86_64-unknown-linux-gnu:\$LD_LIBRARY_PATH
export PKG_CONFIG_PATH=\$SPACK_VIEW_PATH/lib/pkgconfig:\$PKG_CONFIG_PATH
export LDFLAGS="-L\$SPACK_VIEW_PATH/lib \$LDFLAGS"

EOF

echo "Created .env (with Spack activation)"
