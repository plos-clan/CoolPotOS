#![no_std]
#![no_main]
#![feature(alloc_error_handler)]

extern crate alloc;

pub mod args;
pub mod binding;
pub mod file;
pub mod memory;

use core::ffi::{c_char, c_int};
use core::panic::PanicInfo;
use binding::*;

#[panic_handler]
unsafe fn panic(info: &PanicInfo) -> ! {
    println!("panicked: {}", info.message());
    exit(1);
}

extern "Rust" {
    fn rmain();
}

#[no_mangle]
unsafe extern "C" fn main(argc: c_int, argv: *const *const c_char) -> c_int {
    args::init(argc, argv);
    rmain();
    0
}
