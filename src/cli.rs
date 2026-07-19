use std::ffi::{OsStr, OsString};
use std::fmt;

#[derive(Debug, Default, Eq, PartialEq)]
pub struct Options {
    pub help: bool,
    pub version: bool,
    pub raw: bool,
    pub echo: bool,
    pub files: Vec<OsString>,
}

impl Options {
    pub fn parse(arguments: impl IntoIterator<Item = OsString>) -> Result<Self, ParseError> {
        let mut options = Self {
            echo: true,
            ..Self::default()
        };

        for argument in arguments {
            let display = argument.to_string_lossy();
            if let Some(long) = display.strip_prefix("--") {
                match long {
                    "help" => options.help = true,
                    "version" => options.version = true,
                    "raw" => options.raw = true,
                    "no-echo" => options.echo = false,
                    _ => return Err(ParseError::UnknownLong(argument)),
                }
            } else if let Some(short) = display.strip_prefix('-') {
                for flag in short.chars() {
                    match flag {
                        'h' => options.help = true,
                        'v' => options.version = true,
                        'r' => options.raw = true,
                        'E' => options.echo = false,
                        _ => {
                            return Err(ParseError::UnknownShort {
                                argument: argument.clone(),
                                flag,
                            });
                        }
                    }
                }
            } else {
                options.files.push(argument);
            }
        }

        Ok(options)
    }
}

#[derive(Debug, Eq, PartialEq)]
pub enum ParseError {
    UnknownLong(OsString),
    UnknownShort { argument: OsString, flag: char },
}

impl fmt::Display for ParseError {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::UnknownLong(argument) => {
                write!(formatter, "Unknown flag ({})!", display_os(argument))
            }
            Self::UnknownShort { argument, flag } => write!(
                formatter,
                "Unknown flag ({} -> {flag})!",
                display_os(argument)
            ),
        }
    }
}

fn display_os(value: &OsStr) -> String {
    value.to_string_lossy().into_owned()
}

#[cfg(test)]
mod tests {
    use super::*;

    fn args(values: &[&str]) -> Vec<OsString> {
        values.iter().map(OsString::from).collect()
    }

    #[test]
    fn parses_long_and_grouped_short_flags() {
        let options = Options::parse(args(&["--help", "-vrE", "program.bf"])).unwrap();
        assert!(options.help);
        assert!(options.version);
        assert!(options.raw);
        assert!(!options.echo);
        assert_eq!(options.files, args(&["program.bf"]));
    }

    #[test]
    fn reports_the_unknown_short_character() {
        assert_eq!(
            Options::parse(args(&["-rx"])),
            Err(ParseError::UnknownShort {
                argument: OsString::from("-rx"),
                flag: 'x',
            })
        );
    }

    #[test]
    fn treats_a_bare_dash_as_an_empty_flag_group() {
        let options = Options::parse(args(&["-"])).unwrap();
        assert!(options.files.is_empty());
    }
}
