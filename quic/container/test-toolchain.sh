#!/bin/bash

STAMP=${1-$(date +"%Y_%b_%d")}

set -euo pipefail

test_llvm() {
	cd ${BASE}
	mkdir -p obj_test-suite
	cd obj_test-suite

	cmake -G Ninja \
		-DCMAKE_BUILD_TYPE=Release \
		-C../llvm-test-suite/cmake/caches/O3.cmake \
		-DTEST_SUITE_CXX_ABI:STRING=libc++abi \
		-DTEST_SUITE_RUN_UNDER:STRING="${TOOLCHAIN_BIN}/qemu_wrapper.sh" \
		-DTEST_SUITE_RUN_BENCHMARKS:BOOL=ON \
		-DTEST_SUITE_LIT_FLAGS:STRING="--max-tests=10" \
		-DTEST_SUITE_LIT:FILEPATH="${BASE}/obj_llvm/bin/llvm-lit" \
		-DBENCHMARK_USE_LIBCXX:BOOL=ON \
		-DCMAKE_SYSTEM_NAME:STRING=Linux \
		-DCMAKE_C_COMPILER:STRING="${TOOLCHAIN_BIN}/hexagon-unknown-linux-musl-clang" \
		-DCMAKE_CXX_COMPILER:STRING="${TOOLCHAIN_BIN}/hexagon-unknown-linux-musl-clang++" \
		-DSMALL_PROBLEM_SIZE:BOOL=ON \
		../llvm-test-suite
	ninja
#	ninja check \ || /bin/true
	${BASE}/obj_llvm/bin/llvm-lit -v --max-tests=40 . \
	       	2>&1 | tee ${RESULTS_DIR}/llvm-test-suite.log || /bin/true
}

test_qemu() {
	cd ${BASE}
	cd obj_qemu

	make check V=1 --keep-going 2>&1 | tee ${RESULTS_DIR}/qemu_test_check.log
	PATH=${TOOLCHAIN_INSTALL}/x86_64-linux-gnu/bin:$PATH \
		QEMU_LD_PREFIX=${HEX_TOOLS_TARGET_BASE} \
		CROSS_CFLAGS="-G0 -O0 -mv65 -fno-builtin" \
		make check-tcg TIMEOUT=180 CROSS_CC_GUEST=hexagon-unknown-linux-musl-clang V=1 --keep-going 2>&1 | tee ${RESULTS_DIR}/qemu_test_check-tcg.log || /bin/true
	qemu_result=${?}
}

test_libc() {
	cd ${BASE}
	mkdir -p obj_libc-test/
	cd obj_libc-test

	rm -f ../libc-test/config.mak
	cat ../libc-test/config.mak.def - <<EOF >> ../libc-test/config.mak
CFLAGS+=${MUSL_CFLAGS}
EOF

	set +e
	PATH=${TOOLCHAIN_INSTALL}/x86_64-linux-gnu/bin/:$PATH \
		CC=${TOOLCHAIN_BIN}/hexagon-unknown-linux-musl-clang \
		QEMU_LD_PREFIX=${HEX_TOOLS_TARGET_BASE} \
		make V=1 \
		--directory=../libc-test \
		B=${PWD} \
		CROSS_COMPILE=hexagon-unknown-linux-musl- \
		AR=llvm-ar \
		RANLIB=llvm-ranlib \
		RUN_WRAP=${TOOLCHAIN_BIN}/qemu_wrapper.sh
	libc_result=${?}
	set -e
	cp ./REPORT ${RESULTS_DIR}/libc_test_REPORT
	head ./REPORT $(find ${PWD} -name '*.err' | sort) > ${RESULTS_DIR}/libc_test_failures_err.log
}

# needs google benchmark changes to count hexagon cycles in order to build:
#test_llvm
# skipped for now
#test_libc 2>&1 | tee ${RESULTS_DIR}/libc_test_detail.log
qemu_result=99
test_qemu



if [[ ${MAKE_TARBALLS-0} -eq 1 ]]; then
    XZ_OPT="-8 --threads=0" tar cJf ${BASE}/hexagon_tests_${STAMP}.tar.xz  -C $(dirname ${RESULTS_DIR}) $(basename ${RESULTS_DIR})

fi

echo done
echo libc: ${libc_result}
echo qemu: ${qemu_result}
exit ${qemu_result}
#exit $(( ${libc_result} + ${qemu_result} ))
