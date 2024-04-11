#!/bin/sh

echo Setting up new environment.
echo checking for dependencies...

for command in cmake ninja g++ mold gdb lldb ccache; do
  if ! which $command; then
    echo "$command not found"
    exit 1
  fi
done

cd "$(dirname "$(readlink -f "$0")")/../.."

PROJECTS_DIR=$(pwd)

sh iara/scripts/generate_env.sh
. iara/.env

POLYGEIST_COMMIT=4b04755a63fce54e3965c248e6da5c8ae20b26f4

# if test -d Polygeist; then
#   echo Polygeist already exists. Aborting.
#   exit 1
# fi


cd $PROJECTS_DIR
pwd
git clone https://github.com/wsmoses/Polygeist.git --depth 1
cd Polygeist
pwd
git pull origin main
git fetch origin $POLYGEIST_COMMIT
git checkout $POLYGEIST_COMMIT

if ! test -d llvm-project ; then
( GIT_TRACE=1 GIT_CURL_VERBOSE=1 git submodule update --init ) ;
fi

/usr/bin/time -o llvm-cmake-and-build-time.txt sh -c ' cd llvm-project ;\
echo Building LLVM. ;\
mkdir build ;\
cd build ;\
pwd ;\
cmake -GNinja $LLVM_DIR/llvm -DLLVM_ENABLE_PROJECTS="mlir;clang" -DLLVM_BUILD_EXAMPLES=ON -DLLVM_TARGETS_TO_BUILD="host" -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_ASSERTIONS=ON -DLLVM_INSTALL_UTILS=ON -DLLVM_ENABLE_RTTI=ON -DLLVM_CCACHE_BUILD=1 -DLLVM_PARALLEL_LINK_JOBS=1 -DLLVM_USE_LINKER=mold -DCMAKE_EXPORT_COMPILE_COMMANDS=YES ;\
/usr/bin/time -o llvm-build-time.txt ninja '

cd $POLYGEIST_DIR

/usr/bin/time -o polygeist-cmake-and-build-time.txt sh -c ' mkdir build ;\
cd build ;\
pwd ;\
cmake -G Ninja .. -DMLIR_DIR=$LLVM_DIR/build/lib/cmake/mlir -DCLANG_DIR=$LLVM_DIR/build/lib/cmake/clang -DLLVM_TARGETS_TO_BUILD="host" -DLLVM_ENABLE_ASSERTIONS=ON -DCMAKE_BUILD_TYPE=DEBUG -DLLVM_CCACHE_BUILD=1 -DLLVM_PARALLEL_LINK_JOBS=1 -DLLVM_USE_LINKER=mold ;\
/usr/bin/time -o polygeist-build-time.txt ninja ;'

cd $PROJECTS_DIR/iara
/usr/bin/time -o iara-build-time.txt sh scripts/build.sh