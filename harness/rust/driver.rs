//! The driver of ONE leaf of the port: « driver [cases [answers]] » reads a file of cases (/cases/cases) and
//! writes a file of answers (/answers/answers), one answer per case
//! (the judge's framing, the same as the reference's driver). It is the project's own, never a candidate's.
//!
//! It is the only code built with a leaf that is not the leaf: it holds the only `unsafe` (reading and writing
//! through the C library), gives each call a buffer zeroed anew, and ends the process at once when a leaf panics.
#![no_std]
#![no_main]

extern crate port;

use core::ffi::{c_char, c_int, c_void};

#[link(name = "c")]
extern "C" {
    fn open(path: *const c_char, flags: c_int, ...) -> c_int;
    fn read(fd: c_int, buffer: *mut c_void, count: usize) -> isize;
    fn write(fd: c_int, buffer: *const c_void, count: usize) -> isize;
    fn close(fd: c_int) -> c_int;
    fn malloc(size: usize) -> *mut c_void;
    fn calloc(count: usize, size: usize) -> *mut c_void;
    fn free(pointer: *mut c_void);
    fn abort() -> !;
}

#[panic_handler]
fn panic(_: &core::panic::PanicInfo) -> ! {
    unsafe { abort() }
}

#[no_mangle]
pub extern "C" fn rust_eh_personality() {}

const MOST: usize = 96 << 20; // more than the judge ever gives one run

fn write_all(fd: c_int, mut bytes: &[u8]) -> bool {
    while !bytes.is_empty() {
        let done = unsafe { write(fd, bytes.as_ptr() as *const c_void, bytes.len()) };
        if done <= 0 {
            return false;
        }
        bytes = &bytes[done as usize..];
    }
    true
}

fn drive(cases_path: *const c_char, answers_path: *const c_char) -> c_int {
    let input = unsafe { open(cases_path, 0) };
    if input < 0 {
        return 3;
    }
    let block = unsafe { malloc(MOST) } as *mut u8;
    if block.is_null() {
        return 3;
    }
    let mut size = 0usize;
    loop {
        let done = unsafe { read(input, block.add(size) as *mut c_void, MOST - size) };
        if done < 0 || size == MOST {
            return 3;
        }
        if done == 0 {
            break;
        }
        size += done as usize;
    }
    unsafe { close(input) };
    let cases: &[u8] = unsafe { core::slice::from_raw_parts(block, size) };
    // O_WRONLY | O_CREAT | O_TRUNC
    let output = unsafe { open(answers_path, 0o1 | 0o100 | 0o1000, 0o644 as c_int) };
    if output < 0 {
        return 3;
    }
    let (mut at, mut index) = (0usize, 0u32);
    while at + 4 <= size {
        let length = u32::from_le_bytes([cases[at], cases[at + 1], cases[at + 2], cases[at + 3]]) as usize;
        at += 4;
        if length > size - at {
            return 3;
        }
        let room = 6 * length + 16;
        let buffer = unsafe { calloc(room, 1) } as *mut u8; // zeroed anew: nothing of one call reaches the next through it
        // The case in a block of its own, of exactly its length, as the reference's driver gives it: what lies
        // around a leaf's input is not the next case, nor this one's length.
        let given = unsafe { malloc(if length == 0 { 1 } else { length }) } as *mut u8;
        if buffer.is_null() || given.is_null() {
            return 3;
        }
        unsafe { core::ptr::copy_nonoverlapping(cases.as_ptr().add(at), given, length) };
        let input: &[u8] = unsafe { core::slice::from_raw_parts(given, length) };
        let out: &mut [u8] = unsafe { core::slice::from_raw_parts_mut(buffer, room) };
        let answer = port::ANSWER(input, out);
        let mut head = [0u8; 9];
        head[..4].copy_from_slice(&index.to_le_bytes());
        let body: &[u8] = match answer {
            Ok(count) if count <= room => {
                head[5..].copy_from_slice(&(count as u32).to_le_bytes());
                &out[..count]
            }
            Ok(_) => return 4,
            Err(()) => {
                head[4] = 1;
                &[]
            }
        };
        if !write_all(output, &head) || !write_all(output, body) {
            return 3;
        }
        unsafe { free(buffer as *mut c_void) };
        unsafe { free(given as *mut c_void) };
        index += 1;
        at += length;
    }
    if at != size || unsafe { close(output) } != 0 {
        return 3;
    }
    0
}

#[no_mangle]
pub extern "C" fn main(count: c_int, arguments: *const *const c_char) -> c_int {
    let given = |index: isize, otherwise: &'static [u8]| -> *const c_char {
        if (count as isize) > index { unsafe { *arguments.offset(index) } } else { otherwise.as_ptr() as *const c_char }
    };
    drive(given(1, b"/cases/cases\0"), given(2, b"/answers/answers\0"))
}
