use core::ffi::{c_char, c_int, c_uint, CStr};
use core::fmt::{self, Display, Write};

#[allow(unused)]
extern "C" {
    pub fn malloc(size: usize) -> *mut u8;
    pub fn free(ptr: *mut u8);
    pub fn exit(status: c_int) -> !;

    pub fn syscall_putchar(c: c_char);
    pub fn syscall_vfs_filesize(filename: *const c_char) -> c_int;
    pub fn syscall_vfs_readfile(filename: *const c_char, buffer: *mut c_char);
    pub fn syscall_vfs_writefile(filename: *const c_char, buffer: *const c_char, size: c_int);

    pub fn syscall_sysinfo(info: *mut SysInfo);
    pub fn syscall_sleep(timer: u32);
    pub fn syscall_framebuffer() -> *mut u32;
}

struct Print;

impl Write for Print {
    fn write_str(&mut self, s: &str) -> fmt::Result {
        unsafe { s.chars().for_each(|c| syscall_putchar(c as c_char)) }
        Ok(())
    }
}

#[inline]
pub fn _print(args: fmt::Arguments) {
    Print.write_fmt(args).unwrap();
}

#[macro_export]
macro_rules! print {
    ($($arg:tt)*) => (
        _print(format_args!($($arg)*))
    )
}

#[macro_export]
macro_rules! println {
    () => ($crate::print!("\n"));
    ($($arg:tt)*) => ($crate::print!("{}\n", format_args!($($arg)*)))
}

#[repr(C)]
pub struct SysInfo {
    pub osname: [c_char; 50],
    pub kenlname: [c_char; 50],
    pub cpu_vendor: [c_char; 64],
    pub cpu_name: [c_char; 64],
    pub phy_mem_size: c_uint,
    pub pci_device: c_uint,
    pub frame_width: c_uint,
    pub frame_height: c_uint,
    pub year: u32,
    pub mon: u32,
    pub day: u32,
    pub hour: u32,
    pub min: u32,
    pub sec: u32,
}

impl Default for SysInfo {
    fn default() -> Self {
        Self {
            osname: [0; 50],
            kenlname: [0; 50],
            cpu_vendor: [0; 64],
            cpu_name: [0; 64],
            phy_mem_size: 0,
            pci_device: 0,
            frame_width: 0,
            frame_height: 0,
            year: 0,
            mon: 0,
            day: 0,
            hour: 0,
            min: 0,
            sec: 0,
        }
    }
}

impl Display for SysInfo {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(
            f,
            "OS: {}\nKernel: {}\nCPU: {} {}\nMemory: {} MB\nPCI Device: {}\nFrame: {}x{}\nDate: {}-{}-{} {}:{}:{}",
            self.osname(),
            self.kenlname(),
            self.cpu_vendor(),
            self.cpu_name(),
            self.phy_mem_size,
            self.pci_device,
            self.frame_width,
            self.frame_height,
            self.year,
            self.mon,
            self.day,
            self.hour,
            self.min,
            self.sec
        )
    }
}

impl SysInfo {
    pub fn new() -> Self {
        let mut sys_info = Self::default();
        unsafe { syscall_sysinfo(&mut sys_info) };
        sys_info
    }

    pub fn osname(&self) -> &str {
        Self::cstr_to_str(&self.osname)
    }

    pub fn kenlname(&self) -> &str {
        Self::cstr_to_str(&self.kenlname)
    }

    pub fn cpu_vendor(&self) -> &str {
        Self::cstr_to_str(&self.cpu_vendor)
    }

    pub fn cpu_name(&self) -> &str {
        Self::cstr_to_str(&self.cpu_name)
    }

    fn cstr_to_str(buf: &[c_char]) -> &str {
        let cstr = unsafe { CStr::from_ptr(buf.as_ptr()) };
        cstr.to_str().unwrap_or_default()
    }
}
