use std::fmt;
use std::io::{self, Read, Write};

#[cfg(all(feature = "unicode", unix))]
use std::sync::Mutex;

pub const TAPE_LEN: usize = 2 << 18;
pub const LOOP_LIMIT: usize = 2 << 12;

#[cfg(feature = "unicode")]
type Cell = u32;
#[cfg(not(feature = "unicode"))]
type Cell = u8;

#[cfg(all(feature = "unicode", unix))]
static LOCALE_IO_LOCK: Mutex<()> = Mutex::new(());

#[cfg(all(feature = "unicode", unix))]
unsafe extern "C" {
    fn mbrtowc(
        character: *mut libc::wchar_t,
        bytes: *const libc::c_char,
        length: libc::size_t,
        state: *mut core::ffi::c_void,
    ) -> libc::size_t;
}

/// Selects the locale named by the process environment, matching the original
/// `USE_WCHAR` startup path. Locale conversion is process-global in libc, so
/// callers should initialize it before starting worker threads.
#[cfg(all(feature = "unicode", unix))]
pub fn initialize_unicode_locale() -> bool {
    // SAFETY: the empty C string asks libc to select the environment locale.
    // `setlocale` owns its returned storage and a null return means failure.
    !unsafe { libc::setlocale(libc::LC_ALL, c"".as_ptr()) }.is_null()
}

#[cfg(all(feature = "unicode", not(unix)))]
pub fn initialize_unicode_locale() -> bool {
    // The original interpreter only targeted a POSIX terminal environment.
    // Keep Unicode builds usable elsewhere with the UTF-8 fallback below.
    true
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
enum Instruction {
    Increment,
    Decrement,
    Left,
    Right,
    Output,
    Input,
    Open,
    Close,
    #[cfg(feature = "extensions")]
    Exit,
}

impl Instruction {
    fn from_byte(byte: u8) -> Option<Self> {
        match byte {
            b'+' => Some(Self::Increment),
            b'-' => Some(Self::Decrement),
            b'<' => Some(Self::Left),
            b'>' => Some(Self::Right),
            b'.' => Some(Self::Output),
            b',' => Some(Self::Input),
            b'[' => Some(Self::Open),
            b']' => Some(Self::Close),
            #[cfg(feature = "extensions")]
            b'*' => Some(Self::Exit),
            _ => None,
        }
    }

    fn as_byte(self) -> u8 {
        match self {
            Self::Increment => b'+',
            Self::Decrement => b'-',
            Self::Left => b'<',
            Self::Right => b'>',
            Self::Output => b'.',
            Self::Input => b',',
            Self::Open => b'[',
            Self::Close => b']',
            #[cfg(feature = "extensions")]
            Self::Exit => b'*',
        }
    }
}

#[derive(Debug, Eq, PartialEq)]
pub struct Program {
    instructions: Vec<Instruction>,
    jumps: Vec<usize>,
}

impl Program {
    pub fn parse(source: &[u8]) -> Result<Self, ParseError> {
        let (instructions, lines): (Vec<_>, Vec<_>) = digest(source).into_iter().unzip();

        if instructions.is_empty() {
            return Err(ParseError::NoCode);
        }

        let mut jumps = vec![0; instructions.len()];
        let mut stack = Vec::new();
        let mut open_count = 0;

        for (index, instruction) in instructions.iter().enumerate() {
            match instruction {
                Instruction::Open => {
                    open_count += 1;
                    stack.push(index);
                }
                Instruction::Close => {
                    let Some(open) = stack.pop() else {
                        return Err(ParseError::TooManyClosing { line: lines[index] });
                    };
                    jumps[open] = index;
                    jumps[index] = open;
                }
                _ => {}
            }
        }

        if let Some(&open) = stack.last() {
            return Err(ParseError::Unclosed { line: lines[open] });
        }
        if open_count >= LOOP_LIMIT {
            let line = instructions
                .iter()
                .enumerate()
                .filter(|(_, instruction)| matches!(instruction, Instruction::Open))
                .nth(LOOP_LIMIT - 1)
                .map(|(index, _)| lines[index])
                .unwrap_or(1);
            return Err(ParseError::TooManyLoops { line });
        }

        Ok(Self {
            instructions,
            jumps,
        })
    }

    pub fn contains_input(&self) -> bool {
        self.instructions.contains(&Instruction::Input)
    }

    pub fn command_bytes(&self) -> Vec<u8> {
        self.instructions
            .iter()
            .copied()
            .map(Instruction::as_byte)
            .collect()
    }
}

pub fn active_command_bytes(source: &[u8]) -> Vec<u8> {
    digest(source)
        .into_iter()
        .map(|(instruction, _)| instruction.as_byte())
        .collect()
}

fn digest(source: &[u8]) -> Vec<(Instruction, usize)> {
    let mut commands = Vec::new();
    let mut line = 1;
    let mut in_comment = false;

    for &byte in source {
        if in_comment {
            if byte == b'\n' {
                line += 1;
                in_comment = false;
            }
            continue;
        }

        if byte == b'#' {
            in_comment = true;
            continue;
        }
        if byte == b'\n' {
            line += 1;
            continue;
        }
        if let Some(instruction) = Instruction::from_byte(byte) {
            commands.push((instruction, line));
        }
    }

    commands
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ParseError {
    NoCode,
    TooManyClosing { line: usize },
    Unclosed { line: usize },
    TooManyLoops { line: usize },
}

impl ParseError {
    pub fn line(self) -> Option<usize> {
        match self {
            Self::NoCode => None,
            Self::TooManyClosing { line }
            | Self::Unclosed { line }
            | Self::TooManyLoops { line } => Some(line),
        }
    }
}

impl fmt::Display for ParseError {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::NoCode => formatter.write_str("Provided file has no valid brainfuck code."),
            Self::TooManyClosing { .. } => formatter.write_str("Too many closing brackets."),
            Self::Unclosed { .. } => formatter.write_str("Too many opening brackets."),
            Self::TooManyLoops { .. } => {
                formatter.write_str("Input file has more loops than can be handled. Sorry!")
            }
        }
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum Outcome {
    Complete,
    ExtensionExit(u8),
}

pub struct Interpreter {
    tape: Vec<Cell>,
    pointer: usize,
    #[cfg(all(feature = "unicode", unix))]
    wide_input_failed: bool,
}

impl Default for Interpreter {
    fn default() -> Self {
        Self {
            tape: vec![0; TAPE_LEN],
            pointer: 0,
            #[cfg(all(feature = "unicode", unix))]
            wide_input_failed: false,
        }
    }
}

impl Interpreter {
    pub fn run<R: Read, W: Write>(
        &mut self,
        program: &Program,
        input: &mut R,
        output: &mut W,
    ) -> io::Result<Outcome> {
        #[cfg(all(feature = "unicode", unix))]
        let _locale_io_guard = LOCALE_IO_LOCK
            .lock()
            .unwrap_or_else(std::sync::PoisonError::into_inner);
        #[cfg(all(feature = "unicode", unix))]
        reset_locale_conversion();
        #[cfg(all(feature = "unicode", unix))]
        {
            self.wide_input_failed = false;
        }

        let mut counter = 0;
        while counter < program.instructions.len() {
            let instruction = program.instructions[counter];

            #[cfg(feature = "debug-trace")]
            {
                output.write_all(b"\x1b[0;1;34;40m")?;
                output.write_all(&[instruction.as_byte()])?;
                output.write_all(b"\x1b[0m")?;
            }

            match instruction {
                Instruction::Increment => {
                    self.tape[self.pointer] = increment(self.tape[self.pointer]);
                    counter += 1;
                }
                Instruction::Decrement => {
                    self.tape[self.pointer] = decrement(self.tape[self.pointer]);
                    counter += 1;
                }
                Instruction::Left => {
                    self.pointer = self.pointer.checked_sub(1).unwrap_or(TAPE_LEN - 1);
                    counter += 1;
                }
                Instruction::Right => {
                    self.pointer = (self.pointer + 1) % TAPE_LEN;
                    counter += 1;
                }
                Instruction::Output => {
                    write_cell(output, self.tape[self.pointer])?;
                    counter += 1;
                }
                Instruction::Input => {
                    // ISO C flushes line-buffered output before a terminal
                    // input read. Do this explicitly so prompts remain visible.
                    output.flush()?;
                    #[cfg(all(feature = "unicode", unix))]
                    {
                        let cell = if self.wide_input_failed {
                            u32::MAX
                        } else {
                            let (cell, conversion_failed) = read_cell(input)?;
                            self.wide_input_failed = conversion_failed;
                            cell
                        };
                        self.tape[self.pointer] = cell;
                    }
                    #[cfg(not(all(feature = "unicode", unix)))]
                    {
                        self.tape[self.pointer] = read_cell(input)?;
                    }
                    counter += 1;
                }
                Instruction::Open => {
                    #[cfg(not(feature = "end-loop-on-neg"))]
                    let should_skip = self.tape[self.pointer] == 0;
                    #[cfg(feature = "end-loop-on-neg")]
                    let should_skip = is_negative(self.tape[self.pointer]);

                    counter = if should_skip {
                        program.jumps[counter] + 1
                    } else {
                        counter + 1
                    };
                }
                Instruction::Close => {
                    #[cfg(not(feature = "end-loop-on-neg"))]
                    let should_repeat = self.tape[self.pointer] != 0;
                    #[cfg(feature = "end-loop-on-neg")]
                    let should_repeat = self.tape[self.pointer] > 0;

                    counter = if should_repeat {
                        program.jumps[counter] + 1
                    } else {
                        counter + 1
                    };
                }
                #[cfg(feature = "extensions")]
                Instruction::Exit => {
                    return Ok(Outcome::ExtensionExit(self.tape[self.pointer] as u8));
                }
            }

            #[cfg(feature = "debug-trace")]
            {
                output.write_all(b"\x1b[0;1;32;40m")?;
                write_cell(output, self.tape[self.pointer])?;
                output.write_all(b"\x1b[0m")?;
            }
        }

        Ok(Outcome::Complete)
    }
}

fn increment(cell: Cell) -> Cell {
    cell.wrapping_add(1)
}

fn decrement(cell: Cell) -> Cell {
    cell.wrapping_sub(1)
}

#[cfg(feature = "end-loop-on-neg")]
fn is_negative(_cell: Cell) -> bool {
    // The original C build keeps unsigned cells when END_LOOP_ON_NEG is
    // defined, so this condition is intentionally never true.
    false
}

#[cfg(not(feature = "unicode"))]
fn read_cell<R: Read>(input: &mut R) -> io::Result<Cell> {
    let mut byte = [0];
    Ok(if input.read(&mut byte)? == 0 {
        u8::MAX
    } else {
        byte[0]
    })
}

#[cfg(all(feature = "unicode", unix))]
fn read_cell<R: Read>(input: &mut R) -> io::Result<(Cell, bool)> {
    let mut incomplete = false;
    loop {
        let mut byte = [0];
        if input.read(&mut byte)? == 0 {
            if incomplete {
                reset_locale_input();
            }
            return Ok((u32::MAX, incomplete));
        }

        let mut character = 0 as libc::wchar_t;
        // SAFETY: both pointers reference initialized storage for the stated
        // lengths. A null state selects libc's internal conversion state,
        // serialized for the complete interpreter run by `LOCALE_IO_LOCK`.
        let converted = unsafe {
            mbrtowc(
                &mut character,
                byte.as_ptr().cast(),
                byte.len(),
                std::ptr::null_mut(),
            )
        };
        if converted == libc::size_t::MAX - 1 {
            incomplete = true;
            continue;
        }
        if converted == libc::size_t::MAX {
            reset_locale_input();
            return Ok((u32::MAX, true));
        }
        return Ok((character as u32, false));
    }
}

#[cfg(all(feature = "unicode", not(unix)))]
fn read_cell<R: Read>(input: &mut R) -> io::Result<Cell> {
    let mut first = [0];
    if input.read(&mut first)? == 0 {
        return Ok(u32::MAX);
    }

    let Some(width) = utf8_width(first[0]) else {
        return Ok(u32::MAX);
    };
    let mut bytes = [0; 4];
    bytes[0] = first[0];
    if let Err(error) = input.read_exact(&mut bytes[1..width]) {
        return if error.kind() == io::ErrorKind::UnexpectedEof {
            Ok(u32::MAX)
        } else {
            Err(error)
        };
    }
    let Ok(text) = std::str::from_utf8(&bytes[..width]) else {
        return Ok(u32::MAX);
    };
    Ok(text
        .chars()
        .next()
        .expect("a decoded UTF-8 sequence is nonempty") as u32)
}

#[cfg(all(feature = "unicode", not(unix)))]
fn utf8_width(first: u8) -> Option<usize> {
    match first {
        0x00..=0x7f => Some(1),
        0xc2..=0xdf => Some(2),
        0xe0..=0xef => Some(3),
        0xf0..=0xf4 => Some(4),
        _ => None,
    }
}

#[cfg(not(feature = "unicode"))]
fn write_cell<W: Write>(output: &mut W, cell: Cell) -> io::Result<()> {
    output.write_all(&[cell])
}

#[cfg(all(feature = "unicode", unix))]
fn write_cell<W: Write>(output: &mut W, cell: Cell) -> io::Result<()> {
    if cell == 0 {
        return output.write_all(&[0]);
    }

    let characters = [cell as libc::wchar_t, 0];
    // SAFETY: `characters` is a terminated wide string. A null destination
    // asks libc for the exact required byte count without writing anything.
    let required = unsafe { libc::wcstombs(std::ptr::null_mut(), characters.as_ptr(), 0) };
    if required == libc::size_t::MAX {
        return Ok(());
    }
    let mut bytes = vec![0_u8; required + 1];
    // SAFETY: the query above determined the byte count, `bytes` includes one
    // additional slot for the terminator, and the source remains terminated.
    let converted =
        unsafe { libc::wcstombs(bytes.as_mut_ptr().cast(), characters.as_ptr(), bytes.len()) };
    if converted == libc::size_t::MAX {
        return Ok(());
    }
    output.write_all(&bytes[..converted])
}

#[cfg(all(feature = "unicode", not(unix)))]
fn write_cell<W: Write>(output: &mut W, cell: Cell) -> io::Result<()> {
    let Some(character) = char::from_u32(cell) else {
        return Ok(());
    };
    let mut bytes = [0; 4];
    output.write_all(character.encode_utf8(&mut bytes).as_bytes())
}

#[cfg(all(feature = "unicode", unix))]
fn reset_locale_conversion() {
    reset_locale_input();
}

#[cfg(all(feature = "unicode", unix))]
fn reset_locale_input() {
    // SAFETY: ISO C defines this null-input form as a reset/query of the
    // internal conversion state selected by the null state pointer.
    unsafe {
        mbrtowc(
            std::ptr::null_mut(),
            std::ptr::null(),
            0,
            std::ptr::null_mut(),
        );
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parser_ignores_non_commands_and_line_comments() {
        let program = Program::parse(b"++ words # -- ignored\n >.").unwrap();
        assert_eq!(program.command_bytes(), b"++>.");
    }

    #[test]
    fn parser_reports_real_source_lines() {
        assert_eq!(
            Program::parse(b"hello\n  ]"),
            Err(ParseError::TooManyClosing { line: 2 })
        );
        assert_eq!(
            Program::parse(b"\n[\n+"),
            Err(ParseError::Unclosed { line: 2 })
        );
    }

    #[test]
    fn parser_rejects_input_without_commands() {
        assert_eq!(
            Program::parse(b"words # + ignored"),
            Err(ParseError::NoCode)
        );
    }

    #[test]
    fn parser_enforces_the_original_total_loop_limit() {
        let accepted = b"[]".repeat(LOOP_LIMIT - 1);
        assert!(Program::parse(&accepted).is_ok());

        let source = b"[]".repeat(LOOP_LIMIT);
        assert_eq!(
            Program::parse(&source),
            Err(ParseError::TooManyLoops { line: 1 })
        );

        let mut nested = vec![b'['; LOOP_LIMIT];
        nested.extend(std::iter::repeat_n(b']', LOOP_LIMIT));
        assert_eq!(
            Program::parse(&nested),
            Err(ParseError::TooManyLoops { line: 1 })
        );
    }

    #[cfg(not(feature = "debug-trace"))]
    #[test]
    fn executes_nested_loops() {
        let program = Program::parse(b"++[>++[>+<-]<-]>>.").unwrap();
        let mut output = Vec::new();
        let result = Interpreter::default()
            .run(&program, &mut io::empty(), &mut output)
            .unwrap();
        assert_eq!(result, Outcome::Complete);
        assert_eq!(output, [4]);
    }

    #[test]
    fn flushes_output_before_reading_input() {
        use std::cell::Cell as Flag;
        use std::rc::Rc;

        struct TrackedOutput {
            flushed: Rc<Flag<bool>>,
        }

        impl Write for TrackedOutput {
            fn write(&mut self, bytes: &[u8]) -> io::Result<usize> {
                Ok(bytes.len())
            }

            fn flush(&mut self) -> io::Result<()> {
                self.flushed.set(true);
                Ok(())
            }
        }

        struct InputAfterFlush {
            flushed: Rc<Flag<bool>>,
        }

        impl Read for InputAfterFlush {
            fn read(&mut self, bytes: &mut [u8]) -> io::Result<usize> {
                assert!(self.flushed.get(), "output was not flushed before input");
                bytes[0] = 0;
                Ok(1)
            }
        }

        let flushed = Rc::new(Flag::new(false));
        let program = Program::parse(b"+.,").unwrap();
        Interpreter::default()
            .run(
                &program,
                &mut InputAfterFlush {
                    flushed: Rc::clone(&flushed),
                },
                &mut TrackedOutput {
                    flushed: Rc::clone(&flushed),
                },
            )
            .unwrap();
        assert!(flushed.get());
    }

    #[cfg(all(not(feature = "unicode"), not(feature = "debug-trace")))]
    #[test]
    fn cells_and_pointer_wrap() {
        let program = Program::parse(b"-.<+.").unwrap();
        let mut output = Vec::new();
        Interpreter::default()
            .run(&program, &mut io::empty(), &mut output)
            .unwrap();
        assert_eq!(output, [u8::MAX, 1]);
    }

    #[cfg(all(feature = "unicode", not(feature = "debug-trace")))]
    #[test]
    fn unicode_mode_reads_and_writes_scalars() {
        assert!(initialize_unicode_locale());
        let program = Program::parse(b",.").unwrap();
        let mut output = Vec::new();
        Interpreter::default()
            .run(&program, &mut "🦀".as_bytes(), &mut output)
            .unwrap();
        assert_eq!(output, "🦀".as_bytes());
    }

    #[cfg(all(feature = "unicode", not(feature = "debug-trace")))]
    #[test]
    fn unicode_mode_ignores_invalid_wide_output_like_printf() {
        let program = Program::parse(b",.").unwrap();
        let mut output = Vec::new();
        Interpreter::default()
            .run(&program, &mut io::empty(), &mut output)
            .unwrap();
        assert!(output.is_empty());
    }

    #[cfg(feature = "extensions")]
    #[test]
    fn extension_returns_the_current_cell() {
        let program = Program::parse(b"++++++*").unwrap();
        let result = Interpreter::default()
            .run(&program, &mut io::empty(), &mut io::sink())
            .unwrap();
        assert_eq!(result, Outcome::ExtensionExit(6));
    }
}
