#!/bin/bash

testdir=/prj/qct/llvm/devops/aether/hexbuild/test_trees/MASTER/test/benchmark/auto_vec/

if test $# -ne 1 || test "$1" == -h || test "$1" == --help
then
    echo "usage: $0 <qemu-system-hexagon path>"
    exit 1
fi
qemu="$1"

if ! test -d $testdir
then
    echo "Test dir not found: $testdir"
    exit 1
fi

fail=0
total=0
while IFS= read -r test
do
    testname="$(basename "$(dirname "$test")")/$(basename "$test")"
    out="$("$qemu" -kernel $test 2>&1)"
    if test $? -eq 0
    then
        echo "$testname: pass"
    else
        echo "$testname: FAIL"
        printf "========\n%s\n========\n" "$out"
        fail=$(($fail + 1))
    fi
    total=$(($total + 1))
done < <(find $testdir -type f -executable -print | grep -v '\.[^/]*$')

if test $fail -eq 0
then
    echo "All $total tests passed"
    exit 0
else
    echo "FAILED $fail / $total"
    exit 1
fi
