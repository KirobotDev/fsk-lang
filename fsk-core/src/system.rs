use sysinfo::{System, SystemExt, CpuExt};
use std::ffi::CString;
use libc::c_char;

#[no_mangle]
pub extern "C" fn fsk_system_get_info() -> *mut c_char {
    let mut sys = System::new_all();
    sys.refresh_all();

    let info = serde_json::json!({
        "os_name": sys.name(),
        "os_version": sys.os_version(),
        "host_name": sys.host_name(),
        "total_memory": sys.total_memory(),
        "used_memory": sys.used_memory(),
        "cpu_count": sys.cpus().len(),
    });

    CString::new(info.to_string()).unwrap().into_raw()
}
