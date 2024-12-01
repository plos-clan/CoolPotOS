#!/usr/bin/bash
set -o nounset
set -o errtrace
set -o pipefail
set -o monitor
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

shopt -s nullglob
shopt -s globstar

CC_BIN=./i686-linux/bin/i686-elf-gcc
ASM_BIN=./i686-linux/bin/i686-elf-as
NASM_BIN=nasm
CAS_BIN=./i686-linux/bin/i686-elf-as
LD_BIN=./i686-linux/bin/i686-elf-g++

declare -A use_asm_files=(
    [boot.asm]=1
)

hash cat find mktemp mkfifo
hash "${CC_BIN}" "${NASM_BIN}" "${CAS_BIN}" "${LD_BIN}"

function help_msg {
    printf '%q [Options]\n\n' "${0##*/}"
    cat <<- EOF
	Options:
	    -t <count>          multi proccess count
	    -l                  enable verbose logs
	    -h                  show help
	EOF
}

declare -i threadc=4 verbose=0

while getopts ht:l OPT; do case "$OPT" in
    h) help_msg; exit;;
    t)
        if ! [[ $OPTARG =~ [1-9][0-9]* ]]; then
            printf '%q: invalid thread count %q\n' "$0" "$OPTARG" >&2
            exit 2
        fi
        threadc=OPTARG
        ;;
    l) verbose=1;;
    *)
        [ $OPTIND -ge $# ] && OPTIND=$#
        printf '%q: parse args failed, near by %q\n' "$0" "${!OPTIND}" >&2
        exit 2
esac done
if [ "${OPTIND}" -le $# ]; then
    printf '%q: unexpected arg %q\n' "$0" "${!OPTIND}" >&2
    exit 2
fi

EXIT_TRAPS=()
trap 'for ((i=${#EXIT_TRAPS[*]}; --i >= 0;)); do
    cmd=${EXIT_TRAPS[i]}
    eval "$cmd" || echo "trap fail $? ${cmd@Q}" >&2
done' exit

tmp=$(mktemp -d)
EXIT_TRAPS+=("command rm -r -- ${tmp@Q}")

mkfifo -- "$tmp/lock"
exec 3<>"$tmp/lock"

for ((i = 0; i < threadc; ++i)); do
    echo >&3
done

function log {
    echo "===> ${*}" >&2
}
function run {
    local -i __LEC=0
    [ -e "$tmp/run_exit" ] && return
    read -ru3
    {
        ((verbose)) && echo "-> ${*@Q}" >&2
        "$@" || {
            __LEC=$?
            echo "Failed ($__LEC): ${*@Q}" >&2
            echo $__LEC > "$tmp/run_exit"
        }
        echo >&3
    } &
}
function wait_step {
    wait
    if [ -e "$tmp/run_exit" ]; then
        return "$(< "$tmp/run_exit")"
    fi
}
function cc {
    run command "${CC_BIN}" "$@"
}
function asm {
    run command "${ASM_BIN}" "$@"
}
function nasm {
    run command "${NASM_BIN}" -f elf32 "$@"
}
function cas {
    run command "${CAS_BIN}" "$@"
}
function ld {
    run command "${LD_BIN}" "$@"
}

include=(-I src/include)
c_flags=(-O0 -w -std=gnu99 -ffreestanding -c -Wincompatible-pointer-types)
ld_flags=(-T linker.ld -ffreestanding -nostdlib -lgcc)
kernel_output=isodir/sys/kernel.elf

c_builds=(src/{driver,kernel,util,sysapp,network,fs})

function build_c {
    find "${c_builds[@]}" -name '*.c' -print0 | while read -rd '' file; do
        name=${file##*/}
        cc "${c_flags[@]}" "${file}" "${include[@]}" -o "target/${name%.*}.o"
        wait_step # 似乎有奇怪的问题不能让它并行
    done
}
function build_as {
    find src -name '*.S' -print0 | while read -rd '' file; do
        name=${file##*/}
        cas "${file}" -o "target/${name%.*}.o"
    done
}
function build_asm {
    find src -name '*.asm' -print0 | while read -rd '' file; do
        name=${file##*/}
        if [ -n "${use_asm_files[$name]-}" ]; then
            asm "${file}" -o "target/${name%.*}.o"
        else
            nasm "${file}" -o "target/${name%.*}.o"
        fi
    done
}
function link_obj {
    local objs
    mapfile -td '' objs < <(find ./target -name '*.o' -print0)
    ld "${ld_flags[@]}" -o "${kernel_output}" "${objs[@]}"
}

test -e target && rm -r target
mkdir -p target

log "[20%]: Building C source file..."
build_c
wait_step

log "[40%]: Building AS source file..."
build_as
wait_step

log "[60%]: Building asm source file..."
build_asm
wait_step

log "[80%]: Linking object file..."
link_obj
wait_step

log "Clean apps output"
run make -i -r -C apps clean
wait_step

log "Make apps"
run make -r -C apps
wait_step

log "Building iso..."
run grub-mkrescue -o cpos.iso isodir
wait_step

log 'Start VM'
qemu-system-i386 -cdrom cpos.iso -serial stdio -device sb16 \
    -net nic,model=pcnet -m 4096
wait_step
