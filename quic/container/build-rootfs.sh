#!/bin/bash

STAMP=${1-$(date +"%Y_%b_%d")}

set -euo pipefail


build_cpython() {
	cd ${BASE}
	mkdir -p obj_host_cpython
	cd obj_host_cpython
	../cpython/configure \
	      --disable-ipv6 \
              --with-ensurepip=no \
              --prefix=${TOOLCHAIN_INSTALL}/x86_64-linux-gnu/
	make -j
	make install

	cd ${BASE}

	mkdir -p obj_cpython
	cd obj_cpython
	cat <<EOF > ./config.site
ac_cv_file__dev_ptmx=no
ac_cv_file__dev_ptc=no
EOF
	PATH=${TOOLCHAIN_INSTALL}/x86_64-linux-gnu/bin/:$PATH \
		CONFIG_SITE=${PWD}/config.site \
       		READELF=llvm-readelf \
       		CC=hexagon-unknown-linux-musl-clang \
		CFLAGS="-mlong-calls -mv65 -static" \
		LDFLAGS="-static"  CPPFLAGS="-static" \
		../cpython/configure \
	       		--host=hexagon-unknown-linux-musl \
	       		--build=x86_64-linux-gnu \
	       		--disable-ipv6 \
                        --disable-shared \
                        --with-ensurepip=no \
                        --prefix=${ROOTFS}
	PATH=${TOOLCHAIN_INSTALL}/x86_64-linux-gnu/bin/:$PATH make -j
	PATH=${TOOLCHAIN_INSTALL}/x86_64-linux-gnu/bin/:$PATH make test || /bin/true
	PATH=${TOOLCHAIN_INSTALL}/x86_64-linux-gnu/bin/:$PATH make install
}

build_busybox() {
	cd ${BASE}
	mkdir -p obj_busybox
	cd obj_busybox

	PATH=${TOOLCHAIN_INSTALL}/x86_64-linux-gnu/bin/:$PATH \
		make -f ../busybox/Makefile defconfig \
		KBUILD_SRC=../busybox/ \
		AR=llvm-ar \
		RANLIB=llvm-ranlib \
		STRIP=llvm-strip \
	       	ARCH=hexagon \
		CFLAGS="-mlong-calls" \
       		CC=hexagon-unknown-linux-musl-clang \
		CROSS_COMPILE=hexagon-unknown-linux-musl-

	PATH=${TOOLCHAIN_INSTALL}/x86_64-linux-gnu/bin/:$PATH \
		make -j install \
		AR=llvm-ar \
		RANLIB=llvm-ranlib \
		STRIP=llvm-strip \
	       	ARCH=hexagon \
		KBUILD_VERBOSE=1 \
		CFLAGS="-G0 -mlong-calls" \
		CONFIG_PREFIX=${ROOTFS} \
       		CC=hexagon-unknown-linux-musl-clang \
		CROSS_COMPILE=hexagon-unknown-linux-musl-

}

build_dropbear() {
	cd ${BASE}
	mkdir -p obj_dropbear
	cd obj_dropbear
	echo FIXME TODO
}


get_src_tarballs() {
	cd ${BASE}

	wget --quiet ${BUSYBOX_SRC_URL} -O busybox.tar.bz2
	mkdir busybox
	cd busybox
	tar xf ../busybox.tar.bz2 --strip-components=1
	cd -

}
TOOLCHAIN_INSTALL_REL=${TOOLCHAIN_INSTALL}
TOOLCHAIN_INSTALL=$(readlink -f ${TOOLCHAIN_INSTALL})
TOOLCHAIN_BIN=${TOOLCHAIN_INSTALL}/x86_64-linux-gnu/bin
HEX_SYSROOT=${TOOLCHAIN_INSTALL}/x86_64-linux-gnu/target/hexagon-unknown-linux-musl
HEX_TOOLS_TARGET_BASE=${HEX_SYSROOT}/usr
ROOT_INSTALL_REL=${ROOT_INSTALL}
ROOTFS=$(readlink -f ${ROOT_INSTALL})
RESULTS_DIR=$(readlink -f ${RESULTS})

BASE=$(readlink -f ${PWD})

cp -ra ${HEX_SYSROOT}/usr ${ROOTFS}/

get_src_tarballs

# Needs patch to avoid reloc error:
#build_kernel
build_busybox
#build_dropbear
#build_cpython

# Recipe still needs tweaks:
#	ld.lld: error: crt1.c:(function _start_c: .text._start_c+0x5C): relocation R_HEX_B22_PCREL out of range: 2688980 is not in [-2097152, 2097151]; references __libc_start_main
#	>>> defined in ... hexagon-unknown-linux-musl/usr/lib/libc.so
#build_canadian_clang

if [[ ${MAKE_TARBALLS-0} -eq 1 ]]; then
    XZ_OPT="-8 --threads=0" tar cJf ${BASE}/hexagon_rootfs_${STAMP}.tar.xz  -C $(dirname ${ROOT_INSTALL_REL}) $(basename ${ROOT_INSTALL_REL})

fi

