#!/bin/sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$project_dir"

cargo=${CARGO:-cargo}

features="extensions unicode end-loop-on-neg debug-trace"
mask=0
while [ "$mask" -lt 16 ]; do
    selected=""
    bit=1
    for feature in $features; do
        if [ $((mask & bit)) -ne 0 ]; then
            selected="$selected $feature"
        fi
        bit=$((bit << 1))
    done

    if [ -n "$selected" ]; then
        "$cargo" test --quiet --no-default-features --features "$selected"
    else
        "$cargo" test --quiet --no-default-features
    fi
    mask=$((mask + 1))
done

for alias in wide-char wchar debug; do
    "$cargo" test --quiet --no-default-features --features "$alias"
done

"$cargo" test --quiet --release --no-default-features
"$cargo" test --quiet --release --all-features

temp_dir=$(mktemp -d "${TMPDIR:-/tmp}/brain-unfuck-feature-matrix.XXXXXX")
trap 'rm -rf -- "$temp_dir"' EXIT HUP INT TERM

assert_status() {
    expected=$1
    actual=$2
    description=$3
    if [ "$actual" -ne "$expected" ]; then
        printf '%s: expected exit status %s, got %s\n' "$description" "$expected" "$actual" >&2
        exit 1
    fi
}

assert_output() {
    expected=$1
    actual=$2
    description=$3
    if ! cmp -s "$expected" "$actual"; then
        printf '%s: output mismatch\nexpected:\n' "$description" >&2
        od -An -tx1 "$expected" >&2
        printf 'actual:\n' >&2
        od -An -tx1 "$actual" >&2
        exit 1
    fi
}

assert_no_stderr() {
    stderr_file=$1
    description=$2
    if [ -s "$stderr_file" ]; then
        printf '%s: unexpected stderr:\n' "$description" >&2
        sed -n '1,120p' "$stderr_file" >&2
        exit 1
    fi
}

# EXTENSIONS + DEBUG: `*` exits with the cell value, emits its blue trace,
# emits no post-instruction green trace, and does not execute the trailing `.`.
printf '+++++*.' > "$temp_dir/program.bf"
{
    printf '\033[0;1;33;40m+++++*.\033[0m\n'
    printf '\033[0;1;34;40m+\033[0m\033[0;1;32;40m\001\033[0m'
    printf '\033[0;1;34;40m+\033[0m\033[0;1;32;40m\002\033[0m'
    printf '\033[0;1;34;40m+\033[0m\033[0;1;32;40m\003\033[0m'
    printf '\033[0;1;34;40m+\033[0m\033[0;1;32;40m\004\033[0m'
    printf '\033[0;1;34;40m+\033[0m\033[0;1;32;40m\005\033[0m'
    printf '\033[0;1;34;40m*\033[0m'
} > "$temp_dir/expected"
status=0
"$cargo" run --quiet --no-default-features --features "extensions debug" -- \
    "$temp_dir/program.bf" </dev/null >"$temp_dir/actual" 2>"$temp_dir/stderr" || status=$?
assert_status 5 "$status" "extensions + debug"
assert_output "$temp_dir/expected" "$temp_dir/actual" "extensions + debug"
assert_no_stderr "$temp_dir/stderr" "extensions + debug"

# UNICODE + DEBUG: a multibyte scalar survives input, program output, and the
# green cell trace without byte truncation.
printf ',.' > "$temp_dir/program.bf"
printf '\360\237\246\200' > "$temp_dir/input"
{
    printf '\033[0;1;33;40m,.\033[0m\n'
    printf '\033[0;1;34;40m,\033[0m\033[0;1;32;40m\360\237\246\200\033[0m'
    printf '\033[0;1;34;40m.\033[0m\360\237\246\200'
    printf '\033[0;1;32;40m\360\237\246\200\033[0m'
} > "$temp_dir/expected"
status=0
"$cargo" run --quiet --no-default-features --features "unicode debug" -- \
    "$temp_dir/program.bf" <"$temp_dir/input" >"$temp_dir/actual" 2>"$temp_dir/stderr" || status=$?
assert_status 0 "$status" "unicode + debug"
assert_output "$temp_dir/expected" "$temp_dir/actual" "unicode + debug"
assert_no_stderr "$temp_dir/stderr" "unicode + debug"

# END_LOOP_ON_NEG + DEBUG: unsigned zero does not skip `[`, so the body runs
# once and every instruction receives a trace with the zero-valued cell.
printf '[.]' > "$temp_dir/program.bf"
{
    printf '\033[0;1;33;40m[.]\033[0m\n'
    printf '\033[0;1;34;40m[\033[0m\033[0;1;32;40m\000\033[0m'
    printf '\033[0;1;34;40m.\033[0m\000\033[0;1;32;40m\000\033[0m'
    printf '\033[0;1;34;40m]\033[0m\033[0;1;32;40m\000\033[0m'
} > "$temp_dir/expected"
status=0
"$cargo" run --quiet --no-default-features --features "end-loop-on-neg debug" -- \
    "$temp_dir/program.bf" </dev/null >"$temp_dir/actual" 2>"$temp_dir/stderr" || status=$?
assert_status 0 "$status" "end-loop-on-neg + debug"
assert_output "$temp_dir/expected" "$temp_dir/actual" "end-loop-on-neg + debug"
assert_no_stderr "$temp_dir/stderr" "end-loop-on-neg + debug"
