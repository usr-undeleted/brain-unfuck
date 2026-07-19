pub mod cli;
pub mod interpreter;
pub mod terminal;

pub const VERSION: &str =
    "I don't keep the versions of my programs, so uhhh, brain-unfuck 3000!!!\n";
pub const SUCCESS: u8 = 0;
pub const ERR_CODE: u8 = 1;
pub const ERR_USER: u8 = 2;

pub fn usage(invocation: &str) -> String {
    format!(
        "usage:\n\t{invocation} <file>\n\n\
         flags:\n\
         \t--help    (or) -h: pull up this help message.\n\
         \t--version (or) -v: show version message. you might not want it :p\n\
         \t--raw     (or) -r: enable raw terminal mode (user input is immediately processed).\n\
         \t--no-echo (or) -E: disable the visibility of user input.\n\n\
         {invocation} compiled at unix:{} on {} for {} (OS)\n\
         FOSS program forever, licensed under the AGPL-V3 license.\n\
         Hosted on https://github.com/usr-undeleted/brain-unfuck\n",
        env!("BRAIN_UNFUCK_BUILD_TIMESTAMP"),
        env!("BRAIN_UNFUCK_RUSTC"),
        env!("BRAIN_UNFUCK_TARGET_OS"),
    )
}
