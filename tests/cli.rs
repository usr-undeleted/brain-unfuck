use std::fs;
use std::io::Write;
#[cfg(all(unix, not(feature = "debug-trace")))]
use std::os::fd::{AsRawFd, FromRawFd, OwnedFd};
use std::path::{Path, PathBuf};
use std::process::{Command, Output, Stdio};
use std::sync::atomic::{AtomicU64, Ordering};
#[cfg(all(unix, not(feature = "debug-trace")))]
use std::time::{Duration, Instant};

static NEXT_FILE: AtomicU64 = AtomicU64::new(0);

struct TestFile(PathBuf);

impl TestFile {
    fn new(contents: &[u8]) -> Self {
        let id = NEXT_FILE.fetch_add(1, Ordering::Relaxed);
        let path =
            std::env::temp_dir().join(format!("brain-unfuck-test-{}-{id}.bf", std::process::id()));
        fs::write(&path, contents).unwrap();
        Self(path)
    }

    fn path(&self) -> &Path {
        &self.0
    }
}

impl Drop for TestFile {
    fn drop(&mut self) {
        let _ = fs::remove_file(&self.0);
    }
}

fn command() -> Command {
    Command::new(env!("CARGO_BIN_EXE_bf"))
}

fn run_file(source: &[u8], input: &[u8]) -> Output {
    let file = TestFile::new(source);
    let mut child = command()
        .arg(file.path())
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()
        .unwrap();
    child.stdin.take().unwrap().write_all(input).unwrap();
    child.wait_with_output().unwrap()
}

fn run_stdin(source: &[u8]) -> Output {
    let mut child = command()
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()
        .unwrap();
    child.stdin.take().unwrap().write_all(source).unwrap();
    child.wait_with_output().unwrap()
}

#[cfg(all(unix, not(feature = "debug-trace")))]
#[derive(Clone, Debug, Eq, PartialEq)]
struct TerminalAttributes {
    input_flags: libc::tcflag_t,
    output_flags: libc::tcflag_t,
    control_flags: libc::tcflag_t,
    local_flags: libc::tcflag_t,
    control_chars: Vec<libc::cc_t>,
    input_speed: libc::speed_t,
    output_speed: libc::speed_t,
}

#[cfg(all(unix, not(feature = "debug-trace")))]
fn terminal_attributes(descriptor: libc::c_int) -> TerminalAttributes {
    let mut attributes = std::mem::MaybeUninit::<libc::termios>::uninit();
    // SAFETY: `attributes` is valid writable storage and the caller supplies
    // an open terminal descriptor.
    assert_eq!(
        unsafe { libc::tcgetattr(descriptor, attributes.as_mut_ptr()) },
        0
    );
    // SAFETY: the successful tcgetattr call initialized `attributes`.
    let attributes = unsafe { attributes.assume_init() };
    TerminalAttributes {
        input_flags: attributes.c_iflag,
        output_flags: attributes.c_oflag,
        control_flags: attributes.c_cflag,
        local_flags: attributes.c_lflag,
        control_chars: attributes.c_cc.to_vec(),
        // SAFETY: `attributes` was initialized by tcgetattr.
        input_speed: unsafe { libc::cfgetispeed(&attributes) },
        // SAFETY: `attributes` was initialized by tcgetattr.
        output_speed: unsafe { libc::cfgetospeed(&attributes) },
    }
}

#[cfg(all(unix, not(feature = "debug-trace")))]
fn duplicate(descriptor: libc::c_int) -> OwnedFd {
    // SAFETY: dup does not borrow the descriptor and returns a new owned one.
    let duplicate = unsafe { libc::dup(descriptor) };
    assert!(
        duplicate >= 0,
        "dup failed: {}",
        std::io::Error::last_os_error()
    );
    // SAFETY: a successful dup returns a new descriptor owned by this process.
    unsafe { OwnedFd::from_raw_fd(duplicate) }
}

#[cfg(all(unix, not(feature = "debug-trace")))]
fn run_terminal_mode_case(flag: &str, raw: bool, echo: bool, input: &[u8]) {
    let file = TestFile::new(b",.");
    let mut master_descriptor = 0;
    let mut slave_descriptor = 0;
    // SAFETY: both output pointers are valid and the optional arguments may be
    // null. Successful openpty initializes both descriptors.
    assert_eq!(
        unsafe {
            libc::openpty(
                &mut master_descriptor,
                &mut slave_descriptor,
                std::ptr::null_mut(),
                std::ptr::null(),
                std::ptr::null(),
            )
        },
        0,
        "openpty failed: {}",
        std::io::Error::last_os_error()
    );
    // SAFETY: successful openpty returned two new owned descriptors.
    let mut master = unsafe { fs::File::from_raw_fd(master_descriptor) };
    // SAFETY: successful openpty returned two new owned descriptors.
    let slave = unsafe { OwnedFd::from_raw_fd(slave_descriptor) };

    let mut initial = std::mem::MaybeUninit::<libc::termios>::uninit();
    // SAFETY: `initial` is writable storage and `slave` is an open PTY.
    assert_eq!(
        unsafe { libc::tcgetattr(slave.as_raw_fd(), initial.as_mut_ptr()) },
        0
    );
    // SAFETY: tcgetattr succeeded and initialized `initial`.
    let mut initial = unsafe { initial.assume_init() };
    initial.c_lflag |= libc::ICANON | libc::ECHO;
    initial.c_cc[libc::VMIN] = 1;
    initial.c_cc[libc::VTIME] = 0;
    // SAFETY: `initial` came from this terminal and remains a valid termios.
    assert_eq!(
        unsafe { libc::tcsetattr(slave.as_raw_fd(), libc::TCSANOW, &initial) },
        0
    );

    let original = terminal_attributes(slave.as_raw_fd());
    let mut expected_changed = original.clone();
    if raw {
        expected_changed.local_flags &= !libc::ICANON;
        expected_changed.control_chars[libc::VMIN] = 1;
        expected_changed.control_chars[libc::VTIME] = 0;
    }
    if !echo {
        expected_changed.local_flags &= !libc::ECHO;
    }

    let mut child = command()
        .arg(flag)
        .arg(file.path())
        .stdin(Stdio::from(duplicate(slave.as_raw_fd())))
        .stdout(Stdio::piped())
        .stderr(Stdio::from(duplicate(slave.as_raw_fd())))
        .spawn()
        .unwrap();

    let deadline = Instant::now() + Duration::from_secs(5);
    loop {
        let current = terminal_attributes(slave.as_raw_fd());
        if current == expected_changed {
            break;
        }
        if child.try_wait().unwrap().is_some() {
            let output = child.wait_with_output().unwrap();
            panic!(
                "child exited before applying {flag}: status {:?}, stdout {:?}",
                output.status.code(),
                output.stdout
            );
        }
        if Instant::now() >= deadline {
            let _ = child.kill();
            let _ = child.wait();
            panic!(
                "timed out waiting for {flag}; got {:?}, expected {:?}",
                current, expected_changed
            );
        }
        std::thread::sleep(Duration::from_millis(5));
    }

    master.write_all(input).unwrap();
    master.flush().unwrap();

    let deadline = Instant::now() + Duration::from_secs(5);
    loop {
        if child.try_wait().unwrap().is_some() {
            break;
        }
        if Instant::now() >= deadline {
            let _ = child.kill();
            let _ = child.wait();
            panic!("timed out waiting for {flag} child to consume input");
        }
        std::thread::sleep(Duration::from_millis(5));
    }
    let output = child.wait_with_output().unwrap();

    assert!(
        output.status.success(),
        "{flag} exited with {:?}",
        output.status
    );
    assert_eq!(output.stdout, b"Z", "unexpected {flag} program output");
    assert_eq!(
        terminal_attributes(slave.as_raw_fd()),
        original,
        "{flag} did not exactly restore the terminal state"
    );
}

#[test]
fn help_and_version_flags_match_their_names() {
    for flag in ["-h", "--help"] {
        let output = command().arg(flag).output().unwrap();
        assert!(output.status.success());
        assert!(output.stdout.is_empty());
        assert!(String::from_utf8_lossy(&output.stderr).contains("usage:"));
        assert!(!String::from_utf8_lossy(&output.stderr).contains("brain-unfuck 3000"));
    }

    for flag in ["-v", "--version"] {
        let output = command().arg(flag).output().unwrap();
        assert!(output.status.success());
        assert!(output.stdout.is_empty());
        assert_eq!(
            output.stderr,
            b"I don't keep the versions of my programs, so uhhh, brain-unfuck 3000!!!\n"
        );
    }
}

#[test]
fn malformed_flags_are_user_errors_even_with_help() {
    let output = command().args(["--help", "--wat"]).output().unwrap();
    assert_eq!(output.status.code(), Some(2));
    assert!(output.stdout.is_empty());
    assert!(String::from_utf8_lossy(&output.stderr).contains("Unknown flag (--wat)!"));

    let output = command().arg("-hrx").output().unwrap();
    assert_eq!(output.status.code(), Some(2));
    assert!(String::from_utf8_lossy(&output.stderr).contains("Unknown flag (-hrx -> x)!"));
}

#[test]
fn rejects_multiple_files() {
    let output = command().args(["one.bf", "two.bf"]).output().unwrap();
    assert_eq!(output.status.code(), Some(2));
    assert!(String::from_utf8_lossy(&output.stderr).contains("Too many files provided"));
}

#[cfg(not(feature = "debug-trace"))]
#[test]
fn executes_files_and_eof_comments() {
    let output = run_file(b"++++++++[>++++++++<-]>+.# hidden .", b"");
    assert!(output.status.success());
    assert_eq!(output.stdout, b"A");
    assert!(output.stderr.is_empty());
}

#[cfg(not(feature = "debug-trace"))]
#[test]
fn executes_hello_world() {
    let output = run_file(
        b"++++++++++[>+++++++>++++++++++>+++>+<<<<-]>++.>+.+++++++..+++.>++.<<+++++++++++++++.>.+++.------.--------.>+.>.",
        b"",
    );
    assert!(output.status.success());
    assert_eq!(output.stdout, b"Hello World!\n");
}

#[cfg(not(feature = "debug-trace"))]
#[test]
fn explicit_file_wins_over_redirected_runtime_input() {
    let output = run_file(b",.", b"Z");
    assert!(output.status.success());
    assert_eq!(output.stdout, b"Z");
}

#[cfg(not(feature = "debug-trace"))]
#[test]
fn executes_program_source_from_stdin() {
    let output = run_stdin(b"++++++++[>++++++++<-]>+.");
    assert!(output.status.success());
    assert_eq!(output.stdout, b"A");
    assert!(output.stderr.is_empty());
}

#[test]
fn empty_file_and_empty_stdin_report_no_code() {
    for output in [run_file(b"", b""), run_stdin(b"")] {
        assert_eq!(output.status.code(), Some(2));
        assert!(output.stdout.is_empty());
        assert_eq!(
            output.stderr,
            b"Provided file has no valid brainfuck code.\n"
        );
    }
}

#[test]
fn rejects_runtime_input_when_stdin_contains_the_program() {
    for source in [b",.".as_slice(), b",[".as_slice()] {
        let output = run_stdin(source);
        assert_eq!(output.status.code(), Some(2));
        assert!(output.stdout.is_empty());
        assert_eq!(
            output.stderr,
            b"User input cannot be used when stdin is provided (remove commas from code).\n"
        );
    }
}

#[cfg(all(unix, not(feature = "debug-trace")))]
#[test]
fn interactive_no_argument_invocation_reports_no_file() {
    let mut master_descriptor = 0;
    let mut slave_descriptor = 0;
    // SAFETY: successful openpty initializes both descriptor outputs.
    assert_eq!(
        unsafe {
            libc::openpty(
                &mut master_descriptor,
                &mut slave_descriptor,
                std::ptr::null_mut(),
                std::ptr::null(),
                std::ptr::null(),
            )
        },
        0
    );
    // SAFETY: openpty returned two new descriptors that are now owned here.
    let _master = unsafe { OwnedFd::from_raw_fd(master_descriptor) };
    // SAFETY: openpty returned two new descriptors that are now owned here.
    let slave = unsafe { OwnedFd::from_raw_fd(slave_descriptor) };

    let output = command()
        .stdin(Stdio::from(duplicate(slave.as_raw_fd())))
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .output()
        .unwrap();
    assert_eq!(output.status.code(), Some(2));
    assert!(output.stdout.is_empty());
    let stderr = String::from_utf8(output.stderr).unwrap();
    assert!(stderr.contains("Error: No file provided"));
    assert!(!stderr.contains("Not enough arguments"));
}

#[test]
fn reports_no_code_and_real_syntax_lines() {
    let output = run_file(b"prose # + hidden", b"");
    assert_eq!(output.status.code(), Some(2));
    assert_eq!(
        output.stderr,
        b"Provided file has no valid brainfuck code.\n"
    );

    let output = run_file(b"ignored\n][", b"");
    assert_eq!(output.status.code(), Some(2));
    assert_eq!(
        output.stderr,
        b"Can't execute (line 2): Too many closing brackets.\n"
    );
}

#[cfg(all(not(feature = "unicode"), not(feature = "debug-trace")))]
#[test]
fn wraps_cells_and_both_tape_edges() {
    let output = run_file(b"-.+.<+>.<.", b"");
    assert!(output.status.success());
    assert_eq!(output.stdout, [u8::MAX, 0, 0, 1]);

    let mut source = Vec::with_capacity(524_291);
    source.push(b'+');
    source.extend(std::iter::repeat_n(b'>', 524_288));
    source.push(b'.');
    let output = run_file(&source, b"");
    assert!(output.status.success());
    assert_eq!(output.stdout, [1]);
}

#[cfg(all(not(feature = "unicode"), not(feature = "debug-trace")))]
#[test]
fn input_eof_becomes_255() {
    let output = run_file(b",.", b"");
    assert!(output.status.success());
    assert_eq!(output.stdout, [u8::MAX]);
}

#[cfg(not(feature = "debug-trace"))]
#[test]
fn repeatedly_reads_and_writes_until_a_nul_cell() {
    let output = run_file(b",[.,]", b"abc\0");
    assert!(output.status.success());
    assert_eq!(output.stdout, b"abc");
    assert!(output.stderr.is_empty());
}

#[cfg(all(not(feature = "end-loop-on-neg"), not(feature = "debug-trace")))]
#[test]
fn normal_loop_mode_skips_a_zero_loop() {
    let output = run_file(b"[.]", b"");
    assert!(output.status.success());
    assert!(output.stdout.is_empty());
}

#[cfg(not(feature = "debug-trace"))]
#[test]
fn exact_allocation_boundaries_are_safe() {
    for length in [1_023, 1_024, 1_025, 4_096, 4_097] {
        let mut source = vec![b'+'; length - 1];
        source.push(b'.');

        #[cfg(not(feature = "unicode"))]
        let expected = vec![((length - 1) % 256) as u8];
        #[cfg(feature = "unicode")]
        let expected = char::from_u32((length - 1) as u32)
            .unwrap()
            .to_string()
            .into_bytes();

        for output in [run_file(&source, b""), run_stdin(&source)] {
            assert!(output.status.success(), "source length {length} failed");
            assert_eq!(output.stdout, expected, "source length {length}");
            assert!(output.stderr.is_empty(), "source length {length}");
        }
    }
}

#[cfg(all(unix, not(feature = "debug-trace")))]
#[test]
fn raw_mode_is_applied_while_running_and_restored_afterward() {
    run_terminal_mode_case("-r", true, true, b"Z");
}

#[cfg(all(unix, not(feature = "debug-trace")))]
#[test]
fn no_echo_mode_is_applied_while_running_and_restored_afterward() {
    run_terminal_mode_case("-E", false, false, b"Z\n");
}

#[cfg(all(unix, not(feature = "debug-trace")))]
#[test]
fn clustered_raw_no_echo_mode_is_applied_and_restored_afterward() {
    run_terminal_mode_case("-rE", true, false, b"Z");
}

#[cfg(not(feature = "extensions"))]
#[cfg(not(feature = "debug-trace"))]
#[test]
fn extension_character_is_ignored_by_default() {
    let output = run_file(b"+++++*.", b"");
    assert!(output.status.success());
    assert_eq!(output.stdout, [5]);
}

#[cfg(all(feature = "extensions", not(feature = "debug-trace")))]
#[test]
fn extension_exits_with_the_current_cell() {
    let output = run_file(b"+++++*.", b"");
    assert_eq!(output.status.code(), Some(5));
    assert!(output.stdout.is_empty());
    assert!(output.stderr.is_empty());
}

#[cfg(all(feature = "unicode", not(feature = "debug-trace")))]
#[test]
fn unicode_mode_round_trips_a_scalar() {
    let output = run_file(b",.,.", "🦀é".as_bytes());
    assert!(output.status.success());
    assert_eq!(output.stdout, "🦀é".as_bytes());
}

#[cfg(all(feature = "unicode", unix, not(feature = "debug-trace")))]
#[test]
fn unicode_mode_obeys_the_process_locale() {
    let file = TestFile::new(b",.");
    let mut child = command()
        .arg(file.path())
        .env("LC_ALL", "C")
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()
        .unwrap();
    child
        .stdin
        .take()
        .unwrap()
        .write_all("🦀".as_bytes())
        .unwrap();
    let output = child.wait_with_output().unwrap();
    assert!(output.status.success());
    assert!(output.stdout.is_empty());
    assert!(output.stderr.is_empty());

    let output = command()
        .arg(file.path())
        .env("LC_ALL", "brain-unfuck-invalid-locale")
        .stdin(Stdio::null())
        .output()
        .unwrap();
    assert_eq!(output.status.code(), Some(1));
    assert!(output.stdout.is_empty());
    assert_eq!(
        output.stderr,
        b"Failed to set locale for unicode printing.\n"
    );
}

#[cfg(all(feature = "unicode", unix, not(feature = "debug-trace")))]
#[test]
fn unicode_conversion_errors_remain_at_wide_eof() {
    let file = TestFile::new(b",.,.");
    let mut child = command()
        .arg(file.path())
        .env("LC_ALL", "C")
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()
        .unwrap();
    child
        .stdin
        .take()
        .unwrap()
        .write_all(&[0xff, b'A'])
        .unwrap();
    let output = child.wait_with_output().unwrap();
    assert!(output.status.success());
    assert!(output.stdout.is_empty());
    assert!(output.stderr.is_empty());
}

#[cfg(all(feature = "end-loop-on-neg", not(feature = "debug-trace")))]
#[test]
fn negative_loop_mode_executes_a_zero_loop_once() {
    let output = run_file(b"[.]", b"");
    assert!(output.status.success());
    assert_eq!(output.stdout, [0]);
}

#[cfg(feature = "debug-trace")]
#[test]
fn debug_mode_emits_colored_source_instruction_and_cell() {
    let output = run_file(b"+", b"");
    assert!(output.status.success());
    assert_eq!(
        output.stdout,
        b"\x1b[0;1;33;40m+\x1b[0m\n\x1b[0;1;34;40m+\x1b[0m\x1b[0;1;32;40m\x01\x1b[0m"
    );
}

#[cfg(feature = "debug-trace")]
#[test]
fn debug_source_prelude_is_emitted_before_syntax_errors() {
    let output = run_file(b"[", b"");
    assert_eq!(output.status.code(), Some(2));
    assert_eq!(output.stdout, b"\x1b[0;1;33;40m[\x1b[0m\n");
    assert_eq!(
        output.stderr,
        b"Can't execute (line 1): Too many opening brackets.\n"
    );
}

#[cfg(all(feature = "extensions", feature = "debug-trace"))]
#[test]
fn extension_debug_trace_stops_immediately_after_exit_instruction() {
    let output = run_file(b"+++++*.", b"");
    assert_eq!(output.status.code(), Some(5));
    assert!(output.stderr.is_empty());

    let mut expected = b"\x1b[0;1;33;40m+++++*.\x1b[0m\n".to_vec();
    for cell in 1_u8..=5 {
        expected.extend_from_slice(b"\x1b[0;1;34;40m+\x1b[0m\x1b[0;1;32;40m");
        expected.push(cell);
        expected.extend_from_slice(b"\x1b[0m");
    }
    expected.extend_from_slice(b"\x1b[0;1;34;40m*\x1b[0m");
    assert_eq!(output.stdout, expected);
}
