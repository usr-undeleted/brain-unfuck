#!/bin/sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
binary="$project_dir/bin/bf"
fixture=$(mktemp)
trap 'rm -f "$fixture"' EXIT

assert_contains() {
    case $1 in
        *"$2"*) ;;
        *)
            printf 'expected output to contain: %s\nactual output: %s\n' "$2" "$1" >&2
            exit 1
            ;;
    esac
}

help_output=$("$binary" --help 2>&1)
assert_contains "$help_output" "usage:"

version_output=$("$binary" --version 2>&1)
assert_contains "$version_output" "brain-unfuck 3000"

if unknown_output=$("$binary" --unknown 2>&1); then
    printf 'unknown option unexpectedly succeeded\n' >&2
    exit 1
fi
assert_contains "$unknown_output" "Unknown flag (--unknown)"

printf '++++++++[>++++++++<-]>+.# this dot must not execute: .' > "$fixture"
program_output=$("$binary" "$fixture")
if [ "$program_output" != "A" ]; then
    printf 'expected interpreter output A, got: %s\n' "$program_output" >&2
    exit 1
fi

# A page-sized program must still have a zero terminator in the digest buffer.
dd if=/dev/zero bs=4096 count=1 2>/dev/null | tr '\0' '+' > "$fixture"
"$binary" "$fixture"

printf 'CLI tests passed\n'
