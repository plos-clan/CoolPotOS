use crate::binding::*;
use alloc::vec::Vec;
use alloc::{string::String, vec};
use core::ffi::c_char;
use no_std_io::io::{self, Read};

pub struct File {
    filename: Vec<c_char>,
}

impl File {
    pub fn open(path: &str) -> Result<Self, &'static str> {
        let c_filename: Vec<_> = {
            let mut c_string = path.as_bytes().to_vec();
            c_string.push(0);
            c_string.iter().map(|&b| b as c_char).collect()
        };

        let size = unsafe { syscall_vfs_filesize(c_filename.as_ptr()) };

        if size < 0 {
            return Err("Unable to find file");
        }

        Ok(File {
            filename: c_filename,
        })
    }

    pub fn read_to_string(&mut self, buffer: &mut String) -> io::Result<()> {
        let size = unsafe { syscall_vfs_filesize(self.filename.as_ptr()) };
        if size < 0 {
            return Err(io::Error::new(io::ErrorKind::NotFound, "File not found"));
        }

        let mut read_buffer = vec![0u8; size as usize];
        unsafe {
            syscall_vfs_readfile(
                self.filename.as_ptr(),
                read_buffer.as_mut_ptr() as *mut c_char,
            );
        }

        let content = core::str::from_utf8(&read_buffer)
            .map_err(|_| io::Error::new(io::ErrorKind::InvalidData, "Invalid UTF-8"))?;

        buffer.push_str(content);
        Ok(())
    }
}

impl Read for File {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        let size = unsafe { syscall_vfs_filesize(self.filename.as_ptr()) };

        if size < 0 {
            return Err(io::Error::new(io::ErrorKind::NotFound, "File not found"));
        }

        let mut read_buffer = vec![0u8; size as usize];

        unsafe {
            syscall_vfs_readfile(
                self.filename.as_ptr(),
                read_buffer.as_mut_ptr() as *mut c_char,
            );
        }

        let bytes_to_copy = buf.len().min(read_buffer.len());
        buf[..bytes_to_copy].copy_from_slice(&read_buffer[..bytes_to_copy]);

        Ok(bytes_to_copy)
    }
}
