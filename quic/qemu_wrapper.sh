#!/bin/bash

export QEMU_LD_PREFIX=/prj/dsp/austin/hexagon_farm/rootfs/users/scratch/rootfs
export QEMU_INSTALL=/prj/qct/coredev/hexagon/austin/scratch/users/bcain/tmp/qemu_hex

ARGS="-display none -M hexagon_testboard -cpu v67 -no-reboot -kernel"

function join_args() {
    local IFS="$1"
    shift
    echo "${*}"
}

function discard_until() {
    local is_past=0
    local found_kernel=0
    CMD_ARGS=()
    for f in ${@}
    do
        if [[ ${is_past} -eq 1 ]]; then
            if [[ ${found_kernel} -eq 0 ]]; then
               KERNEL=${f}
               found_kernel=1
            else
               CMD_ARGS+=("$f")
            fi
        fi
        if [[ "${f}" == "--" ]]; then
            is_past=1
        fi
    done
    if [[ ${is_past} -eq 0 ]]; then
        KERNEL=${1}
        for f in ${@:1}
        do
            CMD_ARGS+=("$f")
        done
    fi
}

discard_until ${*}
#args=$(discard_until ${*})
#echo kernel: ${CMD_ARGS[0]}
#echo args: ${CMD_ARGS[@]:1}
argv=$(join_args ' ' ${CMD_ARGS[@]})
#echo argv: ${argv}
echo exec ${QEMU_INSTALL}/bin/qemu-system-hexagon ${ARGS} ${KERNEL} -append \"${argv}\"
export LD_LIBRARY_PATH=${QEMU_INSTALL}/lib:${LD_LIBRARY_PATH}
exec timeout ${RUN_TIMEOUT_SEC-180} ${QEMU_INSTALL}/bin/qemu-system-hexagon ${ARGS} ${KERNEL} -append "${argv}"
