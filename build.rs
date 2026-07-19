use std::env;
use std::process::Command;
use std::time::{SystemTime, UNIX_EPOCH};

fn main() {
    println!("cargo:rerun-if-changed=src");
    println!("cargo:rerun-if-env-changed=SOURCE_DATE_EPOCH");

    let timestamp = env::var("SOURCE_DATE_EPOCH").unwrap_or_else(|_| {
        SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .map(|duration| duration.as_secs().to_string())
            .unwrap_or_else(|_| "unknown".to_owned())
    });

    let rustc = env::var_os("RUSTC")
        .and_then(|compiler| Command::new(compiler).arg("--version").output().ok())
        .filter(|output| output.status.success())
        .and_then(|output| String::from_utf8(output.stdout).ok())
        .map(|version| version.trim().to_owned())
        .unwrap_or_else(|| "rustc (unknown version)".to_owned());

    let target_os =
        friendly_os(&env::var("CARGO_CFG_TARGET_OS").unwrap_or_else(|_| "unknown".to_owned()));

    println!("cargo:rustc-env=BRAIN_UNFUCK_BUILD_TIMESTAMP={timestamp}");
    println!("cargo:rustc-env=BRAIN_UNFUCK_RUSTC={rustc}");
    println!("cargo:rustc-env=BRAIN_UNFUCK_TARGET_OS={target_os}");
}

fn friendly_os(target: &str) -> &'static str {
    match target {
        "aix" => "IBM AIX",
        "android" => "Android",
        "dragonfly" => "DragonFly BSD",
        "freebsd" => "FreeBSD",
        "haiku" => "Haiku",
        "hurd" => "GNU/Hurd",
        "illumos" => "illumos",
        "ios" => "iOS",
        "linux" => "GNU Linux",
        "macos" => "macOS",
        "netbsd" => "NetBSD",
        "openbsd" => "OpenBSD",
        "solaris" => "Solaris",
        "windows" => "Windows",
        _ => "Unknown",
    }
}
