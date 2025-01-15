# Docker image to build DDNet for Linux and Windows (32 bit, 64 bit)
# Usage:
# 1. Build image
# docker build -t ddnet - < Dockerfile
# 2. Run the DDNet build script
# docker run -it -v PATH_TO_DDNET:/ddnet:ro -v PATH_TO_OUTPUT_DIRECTORY:/build ddnet ./build-all.sh
FROM debian:12

RUN apt-get update && apt-get install -y gcc-mingw-w64-x86-64-posix \
        g++-mingw-w64-x86-64-posix \
        gcc-mingw-w64-i686-posix \
        g++-mingw-w64-i686-posix \
        wget \
        git \
        ca-certificates \
        build-essential \
        python3 \
        libcurl4-openssl-dev \
        cmake \
        libcurl4-openssl-dev \
        libsqlite3-dev \
        libssl-dev \
        spirv-tools \
        curl

RUN printf '#!/bin/bash\n \
        set -x\n \
        mkdir /build\n \
        mkdir /build/linux\n \
        cd /build/linux\n \
        pwd\n \
        cmake /ddnet && make -j$(nproc) \n \
        mkdir /build/win64\n \
        cd /build/win64\n \
        pwd\n \
        cmake -DCMAKE_TOOLCHAIN_FILE=/ddnet/cmake/toolchains/mingw64.toolchain /ddnet && make -j$(nproc) \n \
        mkdir /build/win32\n \
        cd /build/win32\n \
        pwd\n \
        cmake -DCMAKE_TOOLCHAIN_FILE=/ddnet/cmake/toolchains/mingw32.toolchain /ddnet && make -j$(nproc) \n' \
        > build-all.sh
RUN chmod +x build-all.sh
RUN mkdir /build
