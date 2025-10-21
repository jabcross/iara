#!/bin/sh
cd "$(dirname "$(readlink -f "$0")")/../.."
pwd
PROJECTS_DIR=$(pwd)

cat <<EOF >$PROJECTS_DIR/iara/.env
export IARA_DIR="$PROJECTS_DIR/iara"
# export POLYGEIST_DIR="$PROJECTS_DIR/Polygeist"
# export POLYGEIST_BUILD="\$POLYGEIST_DIR/build"
export LLVM_SOURCES="$PROJECTS_DIR/clangir"
export LLVM_INSTALL="$HOME/llvm-install"
export LLVM_BUILD="\$LLVM_SOURCES/build"
export PATH="\$PATH:\$IARA_DIR/scripts:\$LLVM_INSTALL/bin:\$IARA_DIR/build/bin:\$LLVM_SOURCES/build/bin"
export PS1="(iara)\$PS1"
EOF
