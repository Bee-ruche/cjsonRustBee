//! The port: cJSON's leaves, one module each, rewritten in Rust.
//!
//! This file is the project's own (a candidate cannot change it). A leaf is one file, `src/<leaf>.rs`, and one
//! function, `answer`: the bytes it is given in, the bytes it answers written at the start of `out` (which is
//! zeroed and large enough for every right answer), and their count; or `Err(())`, a refusal. What each leaf
//! answers is said in `port/LEAVES.md`. A leaf uses `core` and nothing else: no `std`, no allocation, no `unsafe`.
//!
//! One leaf is built at a time (`--cfg unit="<leaf>"`, bee/reference/build-unit): a leaf's binary holds that leaf
//! and nothing of the others.
#![no_std]
#![forbid(unsafe_code)]

/// What every leaf is: bytes in, bytes out at the start of `out` and their count, or a refusal.
pub type Leaf = fn(&[u8], &mut [u8]) -> Result<usize, ()>;

#[cfg(unit = "hex4")]
pub mod hex4;
#[cfg(unit = "hex4")]
pub const ANSWER: Leaf = hex4::answer;

#[cfg(unit = "utf16")]
pub mod utf16;
#[cfg(unit = "utf16")]
pub const ANSWER: Leaf = utf16::answer;

#[cfg(unit = "minify")]
pub mod minify;
#[cfg(unit = "minify")]
pub const ANSWER: Leaf = minify::answer;

#[cfg(unit = "print_string")]
pub mod print_string;
#[cfg(unit = "print_string")]
pub const ANSWER: Leaf = print_string::answer;

#[cfg(unit = "strcasecmp")]
pub mod strcasecmp;
#[cfg(unit = "strcasecmp")]
pub const ANSWER: Leaf = strcasecmp::answer;

#[cfg(unit = "parse_number")]
pub mod parse_number;
#[cfg(unit = "parse_number")]
pub const ANSWER: Leaf = parse_number::answer;

#[cfg(unit = "compare_double")]
pub mod compare_double;
#[cfg(unit = "compare_double")]
pub const ANSWER: Leaf = compare_double::answer;

#[cfg(unit = "print_number")]
pub mod print_number;
#[cfg(unit = "print_number")]
pub const ANSWER: Leaf = print_number::answer;

#[cfg(unit = "buffer_skip_whitespace")]
pub mod buffer_skip_whitespace;
#[cfg(unit = "buffer_skip_whitespace")]
pub const ANSWER: Leaf = buffer_skip_whitespace::answer;

#[cfg(unit = "skip_utf8_bom")]
pub mod skip_utf8_bom;
#[cfg(unit = "skip_utf8_bom")]
pub const ANSWER: Leaf = skip_utf8_bom::answer;

#[cfg(unit = "update_offset")]
pub mod update_offset;
#[cfg(unit = "update_offset")]
pub const ANSWER: Leaf = update_offset::answer;

#[cfg(unit = "parse_string")]
pub mod parse_string;
#[cfg(unit = "parse_string")]
pub const ANSWER: Leaf = parse_string::answer;

#[cfg(unit = "strdup")]
pub mod strdup;
#[cfg(unit = "strdup")]
pub const ANSWER: Leaf = strdup::answer;

#[cfg(unit = "print_value")]
pub mod print_value;
#[cfg(unit = "print_value")]
pub const ANSWER: Leaf = print_value::answer;

#[cfg(unit = "parse_value")]
pub mod parse_value;
#[cfg(unit = "parse_value")]
pub const ANSWER: Leaf = parse_value::answer;
