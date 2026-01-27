use rusqlite::{Connection, params};
use std::ffi::{CStr, CString};
use libc::c_char;
use std::sync::{Arc, Mutex};
use std::collections::HashMap;

lazy_static::lazy_static! {
    static ref DB_CONNECTIONS: Arc<Mutex<HashMap<u32, Connection>>> = Arc::new(Mutex::new(HashMap::new()));
    static ref NEXT_DB_ID: Arc<Mutex<u32>> = Arc::new(Mutex::new(1));
}

#[no_mangle]
pub extern "C" fn fsk_sql_open(path: *const c_char) -> u32 {
    let path = unsafe { CStr::from_ptr(path).to_string_lossy().into_owned() };
    match Connection::open(path) {
        Ok(conn) => {
            let mut id_gen = NEXT_DB_ID.lock().unwrap();
            let id = *id_gen;
            *id_gen += 1;
            DB_CONNECTIONS.lock().unwrap().insert(id, conn);
            id
        }
        Err(_) => 0,
    }
}

#[no_mangle]
pub extern "C" fn fsk_sql_query(db_id: u32, query: *const c_char) -> *mut c_char {
    let query_str = unsafe { CStr::from_ptr(query).to_string_lossy().into_owned() };
    let connections = DB_CONNECTIONS.lock().unwrap();
    
    if let Some(conn) = connections.get(&db_id) {
        let mut stmt = match conn.prepare(&query_str) {
            Ok(s) => s,
            Err(e) => return CString::new(format!("Error: {}", e)).unwrap().into_raw(),
        };

        let col_count = stmt.column_count();
        let col_names: Vec<String> = (0..col_count)
            .map(|i| stmt.column_name(i).unwrap().to_string())
            .collect();

        let rows = stmt.query_map([], |row| {
            let mut map = serde_json::Map::new();
            for i in 0..col_count {
                let name = &col_names[i];
                let val: serde_json::Value = match row.get_ref(i).unwrap() {
                    rusqlite::types::ValueRef::Null => serde_json::Value::Null,
                    rusqlite::types::ValueRef::Integer(i) => serde_json::json!(i),
                    rusqlite::types::ValueRef::Real(f) => serde_json::json!(f),
                    rusqlite::types::ValueRef::Text(t) => serde_json::json!(String::from_utf8_lossy(t)),
                    rusqlite::types::ValueRef::Blob(b) => serde_json::json!(hex::encode(b)),
                };
                map.insert(name.clone(), val);
            }
            Ok(serde_json::Value::Object(map))
        }).unwrap();

        let results: Vec<serde_json::Value> = rows.filter_map(|r| r.ok()).collect();
        CString::new(serde_json::json!(results).to_string()).unwrap().into_raw()
    } else {
        CString::new("Error: Invalid DB ID").unwrap().into_raw()
    }
}
