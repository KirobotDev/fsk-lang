use std::ffi::CString;
use libc::c_char;

#[no_mangle]
pub extern "C" fn fsk_free_string(s: *mut c_char) {
    unsafe {
        if s.is_null() { return; }
        let _ = CString::from_raw(s);
    }
}
