#![no_std]
#![no_main]

extern crate alloc;

use coolpotos_rust_sdk::args;
use coolpotos_rust_sdk::binding::*;
use coolpotos_rust_sdk::println;

#[no_mangle]
unsafe fn rmain() {
    for (i, arg) in args::iter().enumerate() {
        let arg = arg.to_string_lossy();
        println!("arg {}: {}", i, arg);
    }
}
