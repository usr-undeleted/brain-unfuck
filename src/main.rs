use std::env;
use std::ffi::OsString;
use std::fs;
use std::io::{self, IsTerminal, Read, Write};
use std::process::ExitCode;

use brain_unfuck::cli::Options;
#[cfg(feature = "unicode")]
use brain_unfuck::interpreter::initialize_unicode_locale;
use brain_unfuck::interpreter::{Interpreter, Outcome, ParseError, Program, active_command_bytes};
use brain_unfuck::terminal::TerminalMode;
use brain_unfuck::{ERR_CODE, ERR_USER, SUCCESS, VERSION, usage};

fn main() -> ExitCode {
    ExitCode::from(run(env::args_os().collect()))
}

fn run(arguments: Vec<OsString>) -> u8 {
    let invocation = arguments
        .first()
        .map(|argument| argument.to_string_lossy().into_owned())
        .unwrap_or_else(|| "bf".to_owned());

    let options = match Options::parse(arguments.into_iter().skip(1)) {
        Ok(options) => options,
        Err(error) => {
            eprint!("{}\nError: {error}\n", usage(&invocation));
            return ERR_USER;
        }
    };

    if options.help {
        eprint!("{}", usage(&invocation));
        return SUCCESS;
    }
    if options.version {
        eprint!("{VERSION}");
        return SUCCESS;
    }
    if options.files.len() > 1 {
        eprint!("{}\nError: Too many files provided\n", usage(&invocation));
        return ERR_USER;
    }

    let source_is_stdin = options.files.is_empty() && !io::stdin().is_terminal();
    let source = if let Some(path) = options.files.first() {
        match fs::read(path) {
            Ok(source) => source,
            Err(error) => {
                eprintln!("Failed to open file '{}': {error}", path.to_string_lossy());
                return ERR_USER;
            }
        }
    } else if source_is_stdin {
        let mut source = Vec::new();
        if let Err(error) = io::stdin().read_to_end(&mut source) {
            eprintln!("Failed to read stdin: {error}");
            return ERR_CODE;
        }
        source
    } else {
        eprint!("{}\nError: No file provided\n", usage(&invocation));
        return ERR_USER;
    };

    let active_commands = active_command_bytes(&source);
    if source_is_stdin && active_commands.contains(&b',') {
        eprintln!("User input cannot be used when stdin is provided (remove commas from code).");
        return ERR_USER;
    }

    #[cfg(feature = "debug-trace")]
    {
        if !active_commands.is_empty() {
            let mut output = io::stdout().lock();
            let _ = output.write_all(b"\x1b[0;1;33;40m");
            let _ = output.write_all(&active_commands);
            let _ = output.write_all(b"\x1b[0m\n");
            let _ = output.flush();
        }
    }

    let program = match Program::parse(&source) {
        Ok(program) => program,
        Err(ParseError::NoCode) => {
            eprintln!("Provided file has no valid brainfuck code.");
            return ERR_USER;
        }
        Err(error) => {
            eprintln!(
                "Can't execute (line {}): {error}",
                error.line().expect("syntax errors have a source line")
            );
            return ERR_USER;
        }
    };

    #[cfg(feature = "unicode")]
    if !initialize_unicode_locale() {
        eprintln!("Failed to set locale for unicode printing.");
        return ERR_CODE;
    }

    let _terminal = match TerminalMode::apply(options.raw, options.echo) {
        Ok(mode) => mode,
        Err(error) => {
            eprintln!("Failed to {} terminal definitions: {error}", error.action());
            return ERR_CODE;
        }
    };

    let mut input = io::stdin().lock();
    let mut output = io::stdout().lock();
    let outcome = match Interpreter::default().run(&program, &mut input, &mut output) {
        Ok(outcome) => outcome,
        Err(error) => {
            eprintln!("Interpreter I/O failed: {error}");
            return ERR_CODE;
        }
    };
    if let Err(error) = output.flush() {
        eprintln!("Failed to flush output: {error}");
        return ERR_CODE;
    }

    match outcome {
        Outcome::Complete => SUCCESS,
        Outcome::ExtensionExit(code) => code,
    }
}
