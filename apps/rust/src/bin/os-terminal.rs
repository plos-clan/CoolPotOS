#![no_std]
#![no_main]

extern crate alloc;

use alloc::boxed::Box;
use alloc::format;
use core::slice;
use os_terminal::font::BitmapFont;
use os_terminal::{DrawTarget, Rgb888, Terminal};

use coolpotos_rust_sdk::binding::*;
use coolpotos_rust_sdk::{args, println};

struct Display {
    width: usize,
    height: usize,
    buffer: &'static mut [u32],
}

impl Display {
    pub fn new() -> Self {
        let sys_info = SysInfo::new();
        let buffer = unsafe { syscall_framebuffer() };

        let width = sys_info.frame_width as usize;
        let height = sys_info.frame_height as usize;
        let buffer = unsafe { slice::from_raw_parts_mut(buffer, width * height) };

        Self {
            width,
            height,
            buffer,
        }
    }
}

impl DrawTarget for Display {
    fn size(&self) -> (usize, usize) {
        (self.width, self.height)
    }

    #[inline(always)]
    fn draw_pixel(&mut self, x: usize, y: usize, color: Rgb888) {
        let value = (color.0 as u32) << 16 | (color.1 as u32) << 8 | color.2 as u32;
        self.buffer[y * self.width + x] = value;
    }
}

#[no_mangle]
unsafe fn rmain() {
    let command = args::iter()
        .nth(1)
        .unwrap_or_else(|| {
            println!("Usage: os-terminal <command>");
            println!("Available commands: sysinfo | bench");
            exit(1);
        })
        .to_str()
        .expect("Unable to convert command to string!");

    let display = Display::new();
    let mut terminal = Terminal::new(display);
    terminal.set_font_manager(Box::new(BitmapFont));

    match command {
        "sysinfo" => {
            let colors = [
                "\x1b[31m", // Red
                "\x1b[32m", // Green
                "\x1b[33m", // Yellow
                "\x1b[34m", // Blue
                "\x1b[35m", // Magenta
                "\x1b[36m", // Cyan
            ];

            // Clear the screen
            terminal.write_bstr(b"\x1b[2J");

            let frame = [
                "     \\     ",
                "    \\ \\    ",
                "   \\ \\ \\   ",
                "  \\ \\ \\ \\  ",
                "   \\ \\ \\   ",
                "    \\ \\    ",
                "     \\     ",
            ];

            // Move the cursor to the top left corner
            terminal.write_bstr(b"\x1b[H");

            // Print the colored frame
            for (j, line) in frame.iter().enumerate() {
                let color = colors[j % colors.len()];
                terminal.write_bstr(format!("{}{}{}\n", color, line, "\x1b[0m").as_bytes());
            }

            // Move the cursor to a new line
            terminal.write_bstr(b"\n");

            let sys_info = SysInfo::new();
            terminal.write_bstr(format!("{}\n", sys_info).as_bytes());
        },
        "bench" => {
            let count_time = args::iter()
                .nth(2)
                .unwrap_or_else(|| {
                    println!("Usage: os-terminal bench <count>");
                    exit(1);
                })
                .to_str()
                .expect("Unable to convert count to string!")
                .parse::<usize>()
                .expect("Unable to parse count!");

            for i in 0..count_time {
                terminal.write_bstr(format!("Cool: {}\n", i).as_bytes());
            }
        },
        _ => {
            println!("Unknown command: {}", command);
            exit(1);
        }
    }
}
