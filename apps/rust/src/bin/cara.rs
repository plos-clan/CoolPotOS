#![no_std]
#![no_main]

extern crate alloc;

use alloc::format;
use alloc::string::String;
use cara::backend::Interpreter;
use cara::frontend::{Lexer, Parser};

use coolpotos_rust_sdk::binding::*;
use coolpotos_rust_sdk::file::File;
use coolpotos_rust_sdk::{args, print, println};

#[no_mangle]
unsafe fn rmain() {
    let path = args::iter()
        .nth(1)
        .expect("Unable to get cara source file path!")
        .to_str()
        .expect("Unable to convert path to string!");

    println!("Path: {:?}", path);

    let mut code = String::new();
    File::open(path)
        .expect("Unable to find cara source file!")
        .read_to_string(&mut code)
        .expect("Unable to read cara source file!");
    println!("Code: {:?}", code);

    let lexer = Lexer::new(code);
    let mut parser = Parser::new(lexer);

    let ast = parser.parse_compile_unit();
    cara::backend::set_printer(|args| print!("{}", format!("{:?}", args)));

    let mut interpreter = Interpreter::new();
    interpreter.visit(&ast).unwrap();
}
