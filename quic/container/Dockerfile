
FROM ubuntu:16.04

# Install common build utilities
RUN apt update && \
    DEBIAN_FRONTEND=noninteractive apt install -yy eatmydata && \
    DEBIAN_FRONTEND=noninteractive eatmydata \
    apt install -y --no-install-recommends \
        bison \
        cmake \
        flex \
        rsync \
        wget \
	build-essential \
	curl \
	xz-utils \
	ca-certificates \
	ccache \
	git \
	software-properties-common \
	unzip

RUN cat /etc/apt/sources.list | sed "s/^deb\ /deb-src /" >> /etc/apt/sources.list

RUN apt update && \
    DEBIAN_FRONTEND=noninteractive eatmydata \
    apt build-dep -yy --arch-only qemu clang python

ENV TOOLCHAIN_INSTALL /usr/local/clang+llvm-12.0.0-cross-hexagon-unknown-linux-musl/
ENV ROOT_INSTALL /usr/local/hexagon-unknown-linux-musl-rootfs
ENV RESULTS /usr/local/hexagon-test-results
ENV MAKE_TARBALLS 1
ENV HOST_LLVM_VERSION 10
ENV CMAKE_VER 3.16.6
ENV CMAKE_URL https://github.com/Kitware/CMake/releases/download/v3.16.6/cmake-3.16.6-Linux-x86_64.tar.gz

# d28af7c654d8db0b68c175db5ce212d74fb5e9bc aka llvmorg-12.0.0
ENV LLVM_SRC_URL https://github.com/llvm/llvm-project/archive/d28af7c654d8db0b68c175db5ce212d74fb5e9bc.tar.gz
# 15106f7dc3290ff3254611f265849a314a93eb0e qemu/qemu 2 May 2021, hexagon scalar core support
# 628eea52b33dae2ea2112c85c2c95e9f8832b846 quic/qemu 23 Apr 2021, latest hexagon core + HVX support
ENV QEMU_SHA 15106f7dc3290ff3254611f265849a314a93eb0e
ENV MUSL_SRC_URL https://github.com/quic/musl/archive/aff74b395fbf59cd7e93b3691905aa1af6c0778c.tar.gz
ENV LINUX_SRC_URL https://cdn.kernel.org/pub/linux/kernel/v5.x/linux-5.6.18.tar.xz

ENV PYTHON_SRC_URL https://www.python.org/ftp/python/3.9.5/Python-3.9.5.tar.xz
ADD get-host-clang-cmake-python.sh /root/hexagon-toolchain/get-host-clang-cmake-python.sh
RUN cd /root/hexagon-toolchain && ./get-host-clang-cmake-python.sh

ADD build-toolchain.sh /root/hexagon-toolchain/build-toolchain.sh
RUN cd /root/hexagon-toolchain && ./build-toolchain.sh 12.0.0

ENV BUSYBOX_SRC_URL https://busybox.net/downloads/busybox-1.33.1.tar.bz2
ADD build-rootfs.sh /root/hexagon-toolchain/build-rootfs.sh
RUN cd /root/hexagon-toolchain && ./build-rootfs.sh

