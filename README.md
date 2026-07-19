# brain-unfuck

A small Brainfuck interpreter written in Rust. It keeps the behavior and
compile-time modes of the original C implementation while using bounds-checked
storage, validated loop jumps, and automatic terminal restoration on normal
process exit and Rust error paths.

## Build

Rust 1.85 or newer is required.

```sh
cargo build --release
# or, for the original make-based workflow:
make compile
```

Both commands build the `bf` executable. `make compile` also copies it to
`bin/bf`.

Run the tests with:

```sh
cargo test --all-targets
make feature-test       # all 16 compatibility-feature combinations
make parity-test        # differential corpus against the final C revision
```

The optional differential test requires Git and a C compiler (`clang` by
default). If the final C oracle is absent from a shallow checkout, the script
fetches its published branch. Set `CC`, `C_ORACLE_REV`, `C_ORACLE_REPOSITORY`,
`C_ORACLE_REF`, or `UTF8_LOCALE` to override its defaults.

## Usage

```text
bf [flags] <file>
command-producing-pipeline | bf [flags]
```

When a file is supplied, it is always the program source and stdin remains
available to the Brainfuck `,` instruction. With no file, redirected stdin is
used as program source; such programs cannot contain `,` because the source has
already consumed stdin.

Flags may appear before or after the file. Short flags may be grouped.

- `-h`, `--help`: display help
- `-v`, `--version`: display version information
- `-r`, `--raw`: make terminal input noncanonical (`VMIN=1`, `VTIME=0`)
- `-E`, `--no-echo`: disable terminal input echo

The terminal flags use the terminal attached to stderr, matching the original
program. They currently require a Unix target.

## Language behavior

- The tape contains exactly 524,288 zero-initialized cells.
- Default cells are wrapping unsigned bytes.
- The data pointer wraps at both ends of the tape.
- `#` comments out the remainder of its current physical line.
- All other non-command bytes are ignored.
- Brackets are validated before execution and report their original source line.
- At least one active command is required.
- EOF read by `,` becomes `255` in the default byte-cell mode.

The supported commands are the conventional `<>+-[],.` set. The optional
`extensions` feature additionally recognizes `*`, which immediately exits with
the low eight bits of the current cell.

## Compatibility features

The original preprocessor modes are available as Cargo features:

| Original macro | Cargo feature | Effect |
| --- | --- | --- |
| `EXTENSIONS` | `extensions` | Enable the `*` exit instruction |
| `USE_WCHAR` | `wide-char` | Use `u32` cells and locale-aware wide-character input/output |
| `END_LOOP_ON_NEG` | `end-loop-on-neg` | Use the original `< 0` / `> 0` loop predicates |
| `DEBUG` | `debug-trace` | Emit the original ANSI-colored execution trace |

For convenience, `unicode` and `wchar` select the same mode as `wide-char`,
while `debug` aliases `debug-trace`. Wide-character mode initializes the
process locale from `LC_ALL`, `LC_CTYPE`, and `LANG`, just like the C build.

Examples:

```sh
cargo build --release --features extensions
cargo build --release --features "wide-char debug-trace"
make compile FEATURES="extensions wide-char"
```

## Exit status

- `0`: successful execution, help, or version output
- `1`: interpreter, I/O, or terminal failure
- `2`: invocation or source-program error
- `0..255`: current cell value when `*` terminates an extensions-enabled build

## License

GNU Affero General Public License v3.0, as provided in `LICENSE`.
