use libloading::{Library, Symbol};
use std::sync::{Arc, Mutex};
use std::collections::HashMap;
use std::ffi::{CStr, CString};
use libc::{c_char, c_void};
use lazy_static::lazy_static;

lazy_static! {
    static ref LIBRARIES: Mutex<HashMap<u64, Arc<Library>>> = Mutex::new(HashMap::new());
    static ref NEXT_LIB_ID: Mutex<u64> = Mutex::new(1);
}

#[no_mangle]
pub extern "C" fn fsk_ffi_open(path: *const c_char) -> u64 {
    let path_str = unsafe { CStr::from_ptr(path).to_string_lossy() };
    
    let lib = unsafe { Library::new(path_str.as_ref()) };

    match lib {
        Ok(l) => {
            let mut libs = LIBRARIES.lock().unwrap();
            let mut id_gen = NEXT_LIB_ID.lock().unwrap();
            let id = *id_gen;
            *id_gen += 1;
            libs.insert(id, Arc::new(l));
            id
        }
        Err(_) => 0 
    }
}


#[no_mangle]
pub extern "C" fn fsk_ffi_call_void_string(lib_id: u64, symbol: *const c_char, arg: *const c_char) -> i32 {
    let libs = LIBRARIES.lock().unwrap();
    if let Some(lib) = libs.get(&lib_id) {
        let symbol_str = unsafe { CStr::from_ptr(symbol).to_string_lossy() };
        let arg_str = unsafe { CStr::from_ptr(arg) }; // Keep as CStr for C function
        
        unsafe {
            let func: Result<Symbol<unsafe extern "C" fn(*const c_char) -> i32>, _> = lib.get(symbol_str.as_bytes());
            match func {
                Ok(f) => {
                    return f(arg_str.as_ptr());
                }
                Err(_) => return -1 
            }
        }
    }
    -2 
}

#[no_mangle]
pub extern "C" fn fsk_ffi_call_int_int(lib_id: u64, symbol: *const c_char, arg: i32) -> i32 {
    let libs = LIBRARIES.lock().unwrap();
    if let Some(lib) = libs.get(&lib_id) {
        let symbol_str = unsafe { CStr::from_ptr(symbol).to_string_lossy() };
        
        unsafe {
            let func: Result<Symbol<unsafe extern "C" fn(i32) -> i32>, _> = lib.get(symbol_str.as_bytes());
            match func {
                Ok(f) => {
                    return f(arg);
                }
                Err(_) => return -1
            }
        }
    }
    -2
}

#[no_mangle]
pub extern "C" fn fsk_ffi_close(lib_id: u64) {
    let mut libs = LIBRARIES.lock().unwrap();
    libs.remove(&lib_id);
}

#[no_mangle]
pub extern "C" fn fsk_ffi_call_void(lib_id: u64, symbol: *const c_char) {
    let libs = LIBRARIES.lock().unwrap();
    if let Some(lib) = libs.get(&lib_id) {
        let symbol_str = unsafe { CStr::from_ptr(symbol).to_string_lossy() };
        unsafe {
            let func: Result<Symbol<unsafe extern "C" fn()>, _> = lib.get(symbol_str.as_bytes());
            if let Ok(f) = func { f(); }
        }
    }
}

#[no_mangle]
pub extern "C" fn fsk_ffi_call_bool(lib_id: u64, symbol: *const c_char) -> bool {
    let libs = LIBRARIES.lock().unwrap();
    if let Some(lib) = libs.get(&lib_id) {
        let symbol_str = unsafe { CStr::from_ptr(symbol).to_string_lossy() };
        unsafe {
            let func: Result<Symbol<unsafe extern "C" fn() -> bool>, _> = lib.get(symbol_str.as_bytes());
            if let Ok(f) = func { return f(); }
        }
    }
    false
}

#[no_mangle]
pub extern "C" fn fsk_ffi_call_void_int_int_string(lib_id: u64, symbol: *const c_char, a1: i32, a2: i32, a3: *const c_char) {
    let libs = LIBRARIES.lock().unwrap();
    if let Some(lib) = libs.get(&lib_id) {
        let symbol_str = unsafe { CStr::from_ptr(symbol).to_string_lossy() };
        let s3 = unsafe { CStr::from_ptr(a3) };
        unsafe {
            let func: Result<Symbol<unsafe extern "C" fn(i32, i32, *const c_char)>, _> = lib.get(symbol_str.as_bytes());
            if let Ok(f) = func { f(a1, a2, s3.as_ptr()); }
        }
    }
}

#[no_mangle]
pub extern "C" fn fsk_ffi_call_void_int_int(lib_id: u64, symbol: *const c_char, a1: i32, a2: i32) {
    let libs = LIBRARIES.lock().unwrap();
    if let Some(lib) = libs.get(&lib_id) {
        let symbol_str = unsafe { CStr::from_ptr(symbol).to_string_lossy() };
        unsafe {
            let func: Result<Symbol<unsafe extern "C" fn(i32, i32)>, _> = lib.get(symbol_str.as_bytes());
            if let Ok(f) = func { f(a1, a2); }
        }
    }
}

#[no_mangle]
pub extern "C" fn fsk_ffi_call_void_4int(lib_id: u64, symbol: *const c_char, a1: i32, a2: i32, a3: i32, a4: i32) {
    let libs = LIBRARIES.lock().unwrap();
    if let Some(lib) = libs.get(&lib_id) {
        let symbol_str = unsafe { CStr::from_ptr(symbol).to_string_lossy() };
        unsafe {
            let func: Result<Symbol<unsafe extern "C" fn(i32, i32, i32, i32)>, _> = lib.get(symbol_str.as_bytes());
            if let Ok(f) = func { f(a1, a2, a3, a4); }
        }
    }
}

#[no_mangle]
pub extern "C" fn fsk_ffi_call_void_5int(lib_id: u64, symbol: *const c_char, a1: i32, a2: i32, a3: i32, a4: i32, a5: i32) {
    let libs = LIBRARIES.lock().unwrap();
    if let Some(lib) = libs.get(&lib_id) {
        let symbol_str = unsafe { CStr::from_ptr(symbol).to_string_lossy() };
        unsafe {
            // NOTE: On x64, a 4-byte struct (like Raylib Color) is often passed as a 32-bit register.
            let func: Result<Symbol<unsafe extern "C" fn(i32, i32, i32, i32, i32)>, _> = lib.get(symbol_str.as_bytes());
            if let Ok(f) = func { f(a1, a2, a3, a4, a5); }
        }
    }
}

#[no_mangle]
pub extern "C" fn fsk_ffi_call_void_6int(lib_id: u64, symbol: *const c_char, a1: i32, a2: i32, a3: i32, a4: i32, a5: i32, a6: i32) {
    let libs = LIBRARIES.lock().unwrap();
    if let Some(lib) = libs.get(&lib_id) {
        let symbol_str = unsafe { CStr::from_ptr(symbol).to_string_lossy() };
        unsafe {
            let func: Result<Symbol<unsafe extern "C" fn(i32, i32, i32, i32, i32, i32)>, _> = lib.get(symbol_str.as_bytes());
            if let Ok(f) = func { f(a1, a2, a3, a4, a5, a6); }
        }
    }
}

#[no_mangle]
pub extern "C" fn fsk_ffi_call_void_2int_float_int(lib_id: u64, symbol: *const c_char, a1: i32, a2: i32, a3: f32, a4: i32) {
    let libs = LIBRARIES.lock().unwrap();
    if let Some(lib) = libs.get(&lib_id) {
        let symbol_str = unsafe { CStr::from_ptr(symbol).to_string_lossy() };
        unsafe {
            let func: Result<Symbol<unsafe extern "C" fn(i32, i32, f32, i32)>, _> = lib.get(symbol_str.as_bytes());
            if let Ok(f) = func { f(a1, a2, a3, a4); }
        }
    }
}

#[no_mangle]
pub extern "C" fn fsk_ffi_call_void_8int(lib_id: u64, symbol: *const c_char, a1: i32, a2: i32, a3: i32, a4: i32, a5: i32, a6: i32, a7: i32, a8: i32) {
    let libs = LIBRARIES.lock().unwrap();
    if let Some(lib) = libs.get(&lib_id) {
        let symbol_str = unsafe { CStr::from_ptr(symbol).to_string_lossy() };
        unsafe {
            let func: Result<Symbol<unsafe extern "C" fn(i32, i32, i32, i32, i32, i32, i32, i32)>, _> = lib.get(symbol_str.as_bytes());
            if let Ok(f) = func { f(a1, a2, a3, a4, a5, a6, a7, a8); }
        }
    }
}

#[no_mangle]
pub extern "C" fn fsk_ffi_call_void_string_4int(lib_id: u64, symbol: *const c_char, s1: *const c_char, a2: i32, a3: i32, a4: i32, a5: i32) {
    let libs = LIBRARIES.lock().unwrap();
    if let Some(lib) = libs.get(&lib_id) {
        let symbol_str = unsafe { CStr::from_ptr(symbol).to_string_lossy() };
        let arg_s1 = unsafe { CStr::from_ptr(s1) };
        unsafe {
            let func: Result<Symbol<unsafe extern "C" fn(*const c_char, i32, i32, i32, i32)>, _> = lib.get(symbol_str.as_bytes());
            if let Ok(f) = func { f(arg_s1.as_ptr(), a2, a3, a4, a5); }
        }
    }
}
