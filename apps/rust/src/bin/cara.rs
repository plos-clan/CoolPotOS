#![no_std]
#![no_main]

extern crate alloc;

use alloc::string::String;
use cara::backend::Interpreter;
use cara::frontend::{Lexer, Parser};

use coolpotos_rust_sdk::binding::*;
use coolpotos_rust_sdk::file::File;
use coolpotos_rust_sdk::{args, print, println};

#[no_mangle]
unsafe fn rmain() {
    let argument = args::iter()
        .nth(1)
        .expect("Unable to get first argument!")
        .to_str()
        .expect("Unable to convert argument to string!");

    match argument {
        "--help" | "-h" => {
            println!("Usage: cara.elf [-h|-v] <script.cara>");
            println!("  -h --help     Get cara help info");
            println!("  -v --version  Get cara version");
        }
        "-v" | "--version" => println!("Caraint CoolPotOS_Edition v0.0.1"),
        _ => {
            let mut code = String::new();
            File::open(&argument)
                .unwrap_or_else(|_| panic!("Unable to find cara source file!"))
                .read_to_string(&mut code)
                .unwrap_or_else(|_| panic!("Unable to read cara source file!"));

            let lexer = Lexer::new(code);
            let mut parser = Parser::new(lexer);

            let ast = parser.parse_compile_unit();
            cara::backend::set_printer(|args| print!("{:?}", args));

            let mut interpreter = Interpreter::new();
            interpreter.visit(&ast).unwrap();
        }
    }
}
