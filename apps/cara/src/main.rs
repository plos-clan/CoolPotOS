#![no_std]
#![no_main]
#![feature(naked_functions)]

use core::panic::PanicInfo;

// #[naked]
// pub extern "C" fn syscall2(_eax: usize, _edi: usize, _esi: usize) -> usize {
//     unsafe {
//         core::arch::asm!(
//             "mov eax, edi",
//             "mov edi, esi",
//             "mov esi, edx",
//             "syscall",
//             "ret",
//             options(noreturn)
//         )
//     }
// }

pub unsafe extern "C" fn syscall1(syscall_number: usize, arg1: usize) -> isize {
    let rets: isize;
    core::arch::asm!(
        "int 31h",
        in("eax") syscall_number,
        in("ebx") arg1,
        lateout("eax") rets,
        options(nostack, preserves_flags)
    );
    rets
}

// #[naked]
// pub unsafe extern "C" fn print(buffer_address: *const u8, buffer_len: usize) {
//     core::arch::asm!(
//         "mov eax, {0}",
//         "mov ebx, [esp + 4]",
//         "mov ecx, [esp + 8]",
//         "int 31h",
//         "ret",
//         const 2,
//         options(noreturn)
//     );
// }

#[no_mangle]
unsafe extern "C" fn _start() -> ! {
    const PRINT_SYSCALL_NUMBER: usize = 2;
    let hello = "Hello, World!\0";
    syscall1(PRINT_SYSCALL_NUMBER, hello.as_ptr() as usize);
    // print(hello.as_ptr(), hello.len());
    loop {}
}


#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {}
}
