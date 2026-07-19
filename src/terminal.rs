use std::io;

#[derive(Debug)]
pub enum TerminalError {
    Get(io::Error),
    Set(io::Error),
    Unsupported(io::Error),
}

impl TerminalError {
    pub fn action(&self) -> &'static str {
        match self {
            Self::Get(_) => "get old",
            Self::Set(_) | Self::Unsupported(_) => "set new",
        }
    }
}

impl std::fmt::Display for TerminalError {
    fn fmt(&self, formatter: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Get(error) | Self::Set(error) | Self::Unsupported(error) => error.fmt(formatter),
        }
    }
}

impl std::error::Error for TerminalError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match self {
            Self::Get(error) | Self::Set(error) | Self::Unsupported(error) => Some(error),
        }
    }
}

#[cfg(unix)]
pub struct TerminalMode {
    descriptor: libc::c_int,
    original: libc::termios,
}

#[cfg(not(unix))]
pub struct TerminalMode;

impl TerminalMode {
    pub fn apply(raw: bool, echo: bool) -> Result<Option<Self>, TerminalError> {
        if !raw && echo {
            return Ok(None);
        }

        apply_platform(raw, echo).map(Some)
    }
}

#[cfg(unix)]
fn apply_platform(raw: bool, echo: bool) -> Result<TerminalMode, TerminalError> {
    apply_to(libc::STDERR_FILENO, raw, echo)
}

#[cfg(unix)]
fn apply_to(descriptor: libc::c_int, raw: bool, echo: bool) -> Result<TerminalMode, TerminalError> {
    let mut original = std::mem::MaybeUninit::<libc::termios>::uninit();

    // SAFETY: `original` points to valid writable storage and `descriptor` is
    // a process-owned file descriptor. A zero return initializes the value.
    if unsafe { libc::tcgetattr(descriptor, original.as_mut_ptr()) } == -1 {
        return Err(TerminalError::Get(io::Error::last_os_error()));
    }
    // SAFETY: the successful `tcgetattr` call above initialized `original`.
    let original = unsafe { original.assume_init() };
    let mut changed = original;

    if raw {
        changed.c_lflag &= !libc::ICANON;
        changed.c_cc[libc::VMIN] = 1;
        changed.c_cc[libc::VTIME] = 0;
    }
    if !echo {
        changed.c_lflag &= !libc::ECHO;
    }

    // SAFETY: `changed` is a valid termios value derived from `tcgetattr`.
    if unsafe { libc::tcsetattr(descriptor, libc::TCSAFLUSH, &changed) } == -1 {
        return Err(TerminalError::Set(io::Error::last_os_error()));
    }

    Ok(TerminalMode {
        descriptor,
        original,
    })
}

#[cfg(not(unix))]
fn apply_platform(_raw: bool, _echo: bool) -> Result<TerminalMode, TerminalError> {
    Err(TerminalError::Unsupported(io::Error::new(
        io::ErrorKind::Unsupported,
        "terminal modes are only supported on Unix targets",
    )))
}

#[cfg(unix)]
impl Drop for TerminalMode {
    fn drop(&mut self) {
        // SAFETY: `original` came from `tcgetattr` for this descriptor. There
        // is nothing useful to do if restoration fails during cleanup.
        unsafe {
            libc::tcsetattr(self.descriptor, libc::TCSAFLUSH, &self.original);
        }
    }
}

#[cfg(all(test, unix))]
mod tests {
    use super::*;

    #[test]
    fn applies_and_restores_requested_terminal_flags() {
        let mut master = 0;
        let mut slave = 0;
        // SAFETY: all pointers either address valid integers or are null for
        // optional openpty outputs.
        assert_eq!(
            unsafe {
                libc::openpty(
                    &mut master,
                    &mut slave,
                    std::ptr::null_mut(),
                    std::ptr::null(),
                    std::ptr::null(),
                )
            },
            0
        );

        let original = get_attributes(slave);
        {
            let _guard = apply_to(slave, true, false).unwrap();
            let changed = get_attributes(slave);
            assert_eq!(changed.c_lflag & libc::ICANON, 0);
            assert_eq!(changed.c_lflag & libc::ECHO, 0);
            assert_eq!(changed.c_cc[libc::VMIN], 1);
            assert_eq!(changed.c_cc[libc::VTIME], 0);
        }
        let restored = get_attributes(slave);
        assert_eq!(restored.c_lflag, original.c_lflag);
        assert_eq!(restored.c_cc, original.c_cc);

        // SAFETY: both descriptors were returned by `openpty` and are no
        // longer used after these calls.
        unsafe {
            libc::close(master);
            libc::close(slave);
        }
    }

    fn get_attributes(descriptor: libc::c_int) -> libc::termios {
        let mut attributes = std::mem::MaybeUninit::<libc::termios>::uninit();
        // SAFETY: `attributes` is valid writable storage for tcgetattr.
        assert_eq!(
            unsafe { libc::tcgetattr(descriptor, attributes.as_mut_ptr()) },
            0
        );
        // SAFETY: tcgetattr succeeded and initialized the value.
        unsafe { attributes.assume_init() }
    }
}
