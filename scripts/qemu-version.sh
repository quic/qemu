#!/bin/sh

set -eu

dir="$1"
pkgversion="$2"
version="$3"

if [ -z "$pkgversion" ]; then
    cd "$dir"
    if [ -e .git ]; then
        pkgversion=$(git describe --match 'v*' --dirty) || :
    fi
fi

if [ -n "$pkgversion" ]; then
    fullversion="$version ($pkgversion)"
else
    fullversion="$version"
fi

sha="$(git rev-parse HEAD)"
if [ -n "$(git status --porcelain)" ]; then
    sha="$sha-dirty"
fi

# HACK/NEEDSWORK: once we have build tags, let's use them
# instead of this limited branch matching
hexagon_branch=
for v in 8_5 8_6 8_7 8_8; do
    if git merge-base --is-ancestor HEAD BRANCH_HEXAGON_$v >/dev/null 2>&1; then
        hexagon_branch=$(echo $v | tr _ .)
        break
    fi
done

cat <<EOF
#define QEMU_PKGVERSION "$pkgversion"
#define QEMU_FULL_VERSION "$fullversion"
#define QEMU_HEXAGON_SHA "$sha"
EOF
if [ -n "$hexagon_branch" ]; then
    echo "#define QEMU_HEXAGON_BRANCH \"$hexagon_branch\""
fi
