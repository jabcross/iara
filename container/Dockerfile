FROM ubuntu:22.04

RUN apt-get update -y && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        ccache \
        cmake \
        g++ \
        gcc \
        gdb \
        gfortran \
        lldb \
        mold \
        ninja-build && \
    rm -rf /var/lib/apt/lists/*


