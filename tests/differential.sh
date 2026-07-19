#!/bin/sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$project_dir"

cargo=${CARGO:-cargo}
cc=${CC:-clang}
oracle_revision=${C_ORACLE_REV:-1b3d476}
oracle_repository=${C_ORACLE_REPOSITORY:-https://github.com/Vaspyyy/brain-unfuck.git}
oracle_ref=${C_ORACLE_REF:-refs/heads/agent/fix-cli-flags-and-mmap}
utf8_locale=${UTF8_LOCALE:-C.utf8}

for command in git tar "$cc" "$cargo" cmp od locale tr; do
    if ! command -v "$command" >/dev/null 2>&1; then
        printf 'required command not found: %s\n' "$command" >&2
        exit 1
    fi
done

if ! git cat-file -e "$oracle_revision^{commit}" 2>/dev/null; then
    printf 'C oracle %s is not local; fetching %s from %s\n' \
        "$oracle_revision" "$oracle_ref" "$oracle_repository" >&2
    if ! git fetch --quiet --no-tags "$oracle_repository" "$oracle_ref" || \
        ! git cat-file -e "$oracle_revision^{commit}" 2>/dev/null; then
        printf 'C oracle revision is unavailable: %s\n' "$oracle_revision" >&2
        exit 1
    fi
fi

charmap=$(LC_ALL="$utf8_locale" locale charmap 2>/dev/null || true)
case $charmap in
    UTF-8|UTF8) ;;
    *)
        printf 'UTF8_LOCALE must name an installed UTF-8 locale (got %s: %s)\n' \
            "$utf8_locale" "${charmap:-unavailable}" >&2
        exit 1
        ;;
esac

temp_dir=$(mktemp -d "${TMPDIR:-/tmp}/brain-unfuck-differential.XXXXXX")
trap 'rm -rf -- "$temp_dir"' EXIT HUP INT TERM
mkdir -p "$temp_dir/oracle"
git archive "$oracle_revision" | tar -x -C "$temp_dir/oracle"

build_pair() {
    name=$1
    rust_features=$2
    c_defines=$3

    # The definition list is controlled by the fixed calls below; intentional
    # word splitting turns it into separate compiler arguments.
    # shellcheck disable=SC2086
    "$cc" -std=gnu99 -Wall -Wextra $c_defines \
        -I "$temp_dir/oracle/src" \
        "$temp_dir/oracle/src/brain-unfuck.c" \
        "$temp_dir/oracle/src/utilities.c" \
        -o "$temp_dir/c-$name"

    if [ -n "$rust_features" ]; then
        "$cargo" build --quiet --release --no-default-features \
            --features "$rust_features" --target-dir "$temp_dir/rust-$name"
    else
        "$cargo" build --quiet --release --no-default-features \
            --target-dir "$temp_dir/rust-$name"
    fi

    c_binary=$temp_dir/c-$name
    rust_binary=$temp_dir/rust-$name/release/bf
}

run_case() {
    description=$1
    locale_name=$2
    program=$3
    input=$4

    case_id=$(printf '%s' "$description" | tr -c 'A-Za-z0-9' '_')
    program_file=$temp_dir/$case_id.bf
    c_stdout=$temp_dir/$case_id.c.stdout
    c_stderr=$temp_dir/$case_id.c.stderr
    rust_stdout=$temp_dir/$case_id.rust.stdout
    rust_stderr=$temp_dir/$case_id.rust.stderr
    printf '%s' "$program" >"$program_file"

    c_status=0
    printf '%b' "$input" | LC_ALL="$locale_name" "$c_binary" "$program_file" \
        >"$c_stdout" 2>"$c_stderr" || c_status=$?
    rust_status=0
    printf '%b' "$input" | LC_ALL="$locale_name" "$rust_binary" "$program_file" \
        >"$rust_stdout" 2>"$rust_stderr" || rust_status=$?

    if [ "$c_status" -ne "$rust_status" ]; then
        printf '%s: status differs (C=%s Rust=%s)\n' \
            "$description" "$c_status" "$rust_status" >&2
        exit 1
    fi
    if ! cmp -s "$c_stdout" "$rust_stdout"; then
        printf '%s: stdout differs\nC:\n' "$description" >&2
        od -An -tx1 "$c_stdout" >&2
        printf 'Rust:\n' >&2
        od -An -tx1 "$rust_stdout" >&2
        exit 1
    fi
    if ! cmp -s "$c_stderr" "$rust_stderr"; then
        printf '%s: stderr differs\nC:\n' "$description" >&2
        od -An -tx1 "$c_stderr" >&2
        printf 'Rust:\n' >&2
        od -An -tx1 "$rust_stderr" >&2
        exit 1
    fi
}

run_stdin_source_case() {
    description=$1
    source=$2

    case_id=$(printf '%s' "$description" | tr -c 'A-Za-z0-9' '_')
    c_stdout=$temp_dir/$case_id.c.stdout
    c_stderr=$temp_dir/$case_id.c.stderr
    rust_stdout=$temp_dir/$case_id.rust.stdout
    rust_stderr=$temp_dir/$case_id.rust.stderr

    c_status=0
    printf '%s' "$source" | LC_ALL=C "$c_binary" \
        >"$c_stdout" 2>"$c_stderr" || c_status=$?
    rust_status=0
    printf '%s' "$source" | LC_ALL=C "$rust_binary" \
        >"$rust_stdout" 2>"$rust_stderr" || rust_status=$?

    if [ "$c_status" -ne "$rust_status" ] || \
        ! cmp -s "$c_stdout" "$rust_stdout" || \
        ! cmp -s "$c_stderr" "$rust_stderr"; then
        printf '%s: piped-source behavior differs\n' "$description" >&2
        printf 'C status/stdout/stderr: %s\n' "$c_status" >&2
        od -An -tx1 "$c_stdout" "$c_stderr" >&2
        printf 'Rust status/stdout/stderr: %s\n' "$rust_status" >&2
        od -An -tx1 "$rust_stdout" "$rust_stderr" >&2
        exit 1
    fi
}

build_pair default "" ""
run_case 'default nested loops' C '++[>++[>+<-]<-]>>.' ''
run_case 'default wrapping cells' C '-.+.' ''
run_case 'default comments' C '++++++++[>++++++++<-]>+.# ignored .' ''
run_case 'default repeated input' C ',[.,]' 'abc\000'
run_case 'default EOF input' C ',.' ''
run_stdin_source_case 'stdin comma beats syntax validation' ',['

build_pair extensions extensions '-DEXTENSIONS'
run_case 'extensions exit' C '+++++*.' ''

build_pair unicode unicode '-DUSE_WCHAR'
run_case 'unicode adjacent scalars' "$utf8_locale" ',.,.' \
    '\360\237\246\200\303\251'
run_case 'unicode C locale rejection' C ',.' '\360\237\246\200'
run_case 'unicode conversion error state' "$utf8_locale" ',.,.' '\377A'

build_pair end-loop-on-neg end-loop-on-neg '-DEND_LOOP_ON_NEG'
run_case 'negative-loop zero entry' C '[.]' ''

build_pair debug debug-trace '-DDEBUG'
run_case 'debug trace' C '+.' ''

build_pair all 'extensions unicode end-loop-on-neg debug-trace' \
    '-DEXTENSIONS -DUSE_WCHAR -DEND_LOOP_ON_NEG -DDEBUG'
run_case 'all features' "$utf8_locale" ',.*' 'A'

printf 'Rust output and status matched C revision %s for all differential cases.\n' \
    "$oracle_revision"
