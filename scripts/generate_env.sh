PROJECTS_DIR=$(dirname "$(readlink -f "$0")")/../..

cat <<EOF >.env
export IARA_DIR="$PROJECTS_DIR/iara"
export POLYGEIST_DIR="$PROJECTS_DIR/Polygeist"
export POLYGEIST_BUILD="$POLYGEIST_DIR/build"
export LLVM_DIR="$PROJECTS_DIR/Polygeist/llvm-project"
export LLVM_BUILD="$LLVM_DIR/build"
export PATH="$POLYGEIST_BUILD/bin:$LLVM_BUILD/bin:$IARA_DIR/build/bin:\$PATH"
export PS1="(iara)\$PS1"
EOF
