#!/bin/bash

# Run this script in a build dir in order to check the regression
# test suite

set -x

set -euo pipefail

if [[ -d /prj/qct/llvm/release ]]; then
    rel_base=/prj/qct/llvm/release/internal
else
    rel_base=/prj/dsp/qdsp6/release/
fi

tools_base=${rel_base}/internal/HEXAGON/
export QEMU_LD_PREFIX=${tools_base}/branch-8.6lnx/latest/Tools/target/hexagon-linux-musl/
export PATH=${tools_base}/branch-8.6lnx/latest/Tools/bin/:${PATH}
export PATH=${tools_base}/branch-8.6/linux64/latest/Tools/bin/:${PATH}

make build-tcg-tests-hexagon-linux-user CROSS_CC_GUEST=hexagon-linux-clang V=1 -j
make build-tcg-tests-hexagon-softmmu CROSS_CC_GUEST=hexagon-clang V=1 -j
make V=1 -j
make check-tcg CROSS_CC_GUEST=hexagon-linux-clang V=1
make check V=1
