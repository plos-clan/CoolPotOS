#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 2 ]; then
    echo "usage: $0 <input.sqsh> <output.sqsh>" >&2
    exit 1
fi

input=$1
output=$2

if ! command -v unsquashfs >/dev/null 2>&1; then
    echo "unsquashfs not found" >&2
    exit 1
fi
if ! command -v mksquashfs >/dev/null 2>&1; then
    echo "mksquashfs not found" >&2
    exit 1
fi

workdir=$(mktemp -d /tmp/coolpotos-rootfs.XXXXXX)
trap 'rm -rf "$workdir"' EXIT

unsquashfs -quiet -d "$workdir/root" "$input"
mksquashfs "$workdir/root" "$output" -no-compression -noappend >/dev/null
