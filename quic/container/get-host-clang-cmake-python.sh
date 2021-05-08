#!/bin/bash

# must run as superuser
set -euo pipefail

set -x

# clang+llvm
cd /opt
wget --quiet https://github.com/llvm/llvm-project/releases/download/llvmorg-12.0.0/clang+llvm-12.0.0-x86_64-linux-gnu-ubuntu-16.04.tar.xz
tar xf clang+llvm-12.0.0-x86_64-linux-gnu-ubuntu-16.04.tar.xz
ln -sf clang+llvm-12.0.0-x86_64-linux-gnu-ubuntu-16.04 clang-latest
sh -c 'echo "export PATH=/opt/clang-latest/bin:\${PATH}" > /etc/profile.d/clang-latest.sh'
sh -c 'echo "/opt/clang-latest/lib" > /etc/ld.so.conf.d/llvm-libs-x86_64-linux-gnu.conf'
ldconfig
rm clang+llvm-12.0.0-x86_64-linux-gnu-ubuntu-16.04.tar.xz

# cmake
cd /tmp ; wget --quiet ${CMAKE_URL}
cd /opt ; tar xf /tmp/$(basename ${CMAKE_URL})
ln -sf cmake-${CMAKE_VER}-Linux-x86_64 cmake-latest
cd -
sh -c 'echo "export PATH=/opt/cmake-latest/bin:\${PATH}" > /etc/profile.d/cmake-latest.sh'

# python
cd /root/hexagon-toolchain
wget --quiet ${PYTHON_SRC_URL} -O cpython.tar.xz
mkdir cpython
cd cpython
tar xf ../cpython.tar.xz --strip-components=1
rm ../cpython.tar.xz
./configure --prefix=/opt/python3
make -j
make install
sh -c 'echo "export PATH=/opt/python3/bin:\${PATH}" > /etc/profile.d/py3-latest.sh'


# ninja
cd /opt 
mkdir ninja-latest 
cd ninja-latest

wget --quiet https://github.com/ninja-build/ninja/releases/download/v1.10.2/ninja-linux.zip
unzip ninja-linux.zip 
sh -c 'echo "export PATH=/opt/ninja-latest:\${PATH}" > /etc/profile.d/ninja-latest.sh'
rm ninja-linux.zip 
