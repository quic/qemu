#!/bin/bash

this_dir=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
set -euo pipefail
test_prog_dir=${1}
qemu_bin_dir=${2}
log_output_dir=${3}

echo singlestep:
${this_dir}/run_qemu_tests.py \
    --binary-dir ${test_prog_dir} \
    --timeout-sec ${TIMEOUT_SEC-120} \
    --output-dir ${log_output_dir}/singlestep \
    --enable-singlestep

echo paranoid:
${this_dir}/run_qemu_tests.py \
    --binary-dir ${test_prog_dir} \
    --timeout-sec ${TIMEOUT_SEC-120} \
    --output-dir ${log_output_dir}/paranoid \
    --enable-paranoid

echo icount '1': DISABLED
echo ${this_dir}/run_qemu_tests.py \
    --binary-dir ${test_prog_dir} \
    --timeout-sec ${TIMEOUT_SEC-120} \
    --qemu-bin-dir ${qemu_bin_dir} \
    --output-dir ${log_output_dir}/icount \
    --icount 1

echo MTTCG: DISABLED
echo ${this_dir}/run_qemu_tests.py \
    --binary-dir ${test_prog_dir} \
    --timeout-sec ${TIMEOUT_SEC-120} \
    --output-dir ${log_output_dir}/mttcg \
    --enable-mttcg
