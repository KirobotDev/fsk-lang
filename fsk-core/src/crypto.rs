use ring::digest;
use std::ffi::{CStr, CString};
use libc::c_char;

#[no_mangle]
pub extern "C" fn fsk_crypto_sha256(input: *const c_char) -> *mut c_char {
    let input = unsafe { CStr::from_ptr(input).to_bytes() };
    let actual研 = digest::digest(&digest::SHA256, input);
    let hex = hex::encode(actual研.as_ref());
    CString::new(hex).unwrap().into_raw()
}

#[no_mangle]
pub extern "C" fn fsk_crypto_md5(input: *const c_char) -> *mut c_char {

    let input = unsafe { CStr::from_ptr(input).to_bytes() };
    let digest = md5::compute(input);
    let hex = format!("{:x}", digest);
    CString::new(hex).unwrap().into_raw()
}
