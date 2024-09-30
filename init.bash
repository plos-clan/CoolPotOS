#!/usr/bin/bash
set -o nounset
set -o errtrace
set -o pipefail
function CATCH_ERROR {
    local __LEC=$? __i
    echo "Traceback (most recent call last):" >&2
    for ((__i = ${#FUNCNAME[@]} - 1; __i >= 0; --__i)); do
        printf '  File %q line %s in %q\n' >&2 \
            "${BASH_SOURCE[$__i]}" \
            "${BASH_LINENO[$__i]}" \
            "${FUNCNAME[$__i]}"
    done
    echo "Error: [ExitCode: ${__LEC}]" >&2
    exit "${__LEC}"
}
trap CATCH_ERROR ERR

if [ "${1-}" = -h ]; then
    echo Used to create necessary empty directories, etc
    exit
fi
[ $# = 0 ]

mkdir -p target
find ./apps -type d -print0 | while read -rd '' dir; do
    echo "${dir}/out/alloc"
done | xargs mkdir -vp
