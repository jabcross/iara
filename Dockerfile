# GCC support can be specified at major, minor, or micro version
# (e.g. 8, 8.2 or 8.2.0).
# See https://hub.docker.com/r/library/gcc/ for all supported GCC
# tags from Docker Hub.
# See https://docs.docker.com/samples/library/gcc/ for more on how to use this image
FROM gcc:latest

# These commands copy your files into the specified directory in the image
# and set that as the working location

WORKDIR /root/repos

RUN apt update && apt install -y --no-install-recommends cmake ninja-build ccache lldb gdb time

RUN git clone --branch stable https://github.com/rui314/mold.git \
  && cd mold \
  && ./install-build-deps.sh \
  && cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=c++ -B build \
  && cmake --build build -j$(nproc) \
  && cmake --build build --target install

RUN cd /root/repos
RUN PROJECTS_DIR=`pwd`

RUN git clone https://github.com/jabcross/iara.git

RUN sh iara/scripts/generate_env.sh
RUN . iara/.env

ENV POLYGEIST_COMMIT=4b04755a63fce54e3965c248e6da5c8ae20b26f4

RUN cd $PROJECTS_DIR
RUN git clone https://github.com/wsmoses/Polygeist.git
WORKDIR  Polygeist
RUN ls -la
RUN git pull origin main
RUN git fetch origin $POLYGEIST_COMMIT
RUN git checkout $POLYGEIST_COMMIT

RUN /usr/bin/time sh -c 'git submodule update --init'

RUN /usr/bin/time -o llvm-cmake-and-build-time.txt sh -c '\
  . /root/repos/iara/.env \
  cd llvm-project ;\
  echo Building LLVM. ;\
  mkdir build ;\
  cd build ;\
  pwd ;\
  cmake -GNinja $LLVM_DIR/llvm -DLLVM_ENABLE_PROJECTS="mlir;clang" -DLLVM_BUILD_EXAMPLES=ON -DLLVM_TARGETS_TO_BUILD="host" -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_ASSERTIONS=ON -DLLVM_INSTALL_UTILS=ON -DLLVM_ENABLE_RTTI=ON -DLLVM_CCACHE_BUILD=1 -DLLVM_PARALLEL_LINK_JOBS=1 -DLLVM_USE_LINKER=mold -DCMAKE_EXPORT_COMPILE_COMMANDS=YES ;\
  /usr/bin/time -o llvm-build-time.txt ninja '

RUN cd $POLYGEIST_DIR
RUN /usr/bin/time -o polygeist-cmake-and-build-time.txt sh -c ' mkdir build ;\
cd build ;\
pwd ;\
cmake -G Ninja .. -DMLIR_DIR=$LLVM_DIR/build/lib/cmake/mlir -DCLANG_DIR=$LLVM_DIR/build/lib/cmake/clang -DLLVM_TARGETS_TO_BUILD="host" -DLLVM_ENABLE_ASSERTIONS=ON -DCMAKE_BUILD_TYPE=DEBUG -DLLVM_CCACHE_BUILD=1 -DLLVM_PARALLEL_LINK_JOBS=1 -DLLVM_USE_LINKER=mold ;\
/usr/bin/time -o polygeist-build-time.txt ninja ;'

RUN cd $PROJECTS_DIR/iara
RUN /usr/bin/time -o iara-build-time.txt sh scripts/build.sh

LABEL Name=iara Version=0.0.1
