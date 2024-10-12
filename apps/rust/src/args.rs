use core::ffi::CStr;
use core::ffi::{c_char, c_int};

static mut ARGC: c_int = 0;
static mut ARGV: *const *const c_char = core::ptr::null();

pub unsafe fn init(argc: c_int, argv: *const *const c_char) {
    ARGC = argc;
    ARGV = argv;
}

pub unsafe fn iter() -> Iter {
    Iter {
        next: ARGV,
        end: ARGV.offset(ARGC as isize),
    }
}

pub struct Iter {
    next: *const *const c_char,
    end: *const *const c_char,
}

impl Iterator for Iter {
    type Item = &'static CStr;

    fn next(&mut self) -> Option<Self::Item> {
        if self.next == self.end {
            None
        } else {
            let ptr = unsafe { *self.next };
            let c_str = unsafe { CStr::from_ptr(ptr) };
            self.next = unsafe { self.next.offset(1) };
            Some(c_str)
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        let len = self.len();
        (len, Some(len))
    }
}

impl ExactSizeIterator for Iter {
    fn len(&self) -> usize {
        (self.end as usize - self.next as usize) / core::mem::size_of::<*const c_char>()
    }
}
