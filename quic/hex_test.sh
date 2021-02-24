#!/bin/bash

# Run this script in a build dir in order to check the regression
# test suite

set -x

set -euo pipefail
#export QEMU_LD_PREFIX=/prj/dsp/qdsp6/users/tsimpson/qemu-hexagon-test/linux_sys_lib
export QEMU_LD_PREFIX=/prj/dsp/austin/hexagon_farm/rootfs/users/scratch/rootfs/

export PATH=/prj/dsp/qdsp6/release/internal/HEXAGON/branch-8.5lnx/latest/Tools/bin/:${PATH}
export PATH=/prj/dsp/qdsp6/release/internal/HEXAGON/branch-8.5/linux64/latest/Tools/bin/:${PATH}

make build-tcg-tests-hexagon-linux-user CROSS_CC_GUEST=hexagon-linux-clang V=1 -j
make build-tcg-tests-hexagon-softmmu CROSS_CC_GUEST=hexagon-clang V=1 -j
make V=1 -j
make check V=1
make check-tcg CROSS_CC_GUEST=hexagon-linux-clang V=1
