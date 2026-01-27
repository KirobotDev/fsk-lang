use std::ffi::{CStr, CString};
use libc::{c_char, c_void};
use reqwest;
use std::sync::{Arc, Mutex};
use std::collections::HashMap;
use tokio::sync::oneshot;
use axum::{routing::any, Router, body::Body, http::Request, response::Response};
use std::net::SocketAddr;

pub struct ResponseHandle {
    pub tx: oneshot::Sender<(u16, String)>,
}

lazy_static::lazy_static! {
    pub static ref PENDING_REQUESTS: Arc<Mutex<HashMap<u64, ResponseHandle>>> = Arc::new(Mutex::new(HashMap::new()));
    pub static ref NEXT_REQ_ID: Arc<Mutex<u64>> = Arc::new(Mutex::new(1));
}

#[no_mangle]
pub extern "C" fn fsk_fetch_blocking(url: *const c_char) -> *mut c_char {
    let url = unsafe {
        CStr::from_ptr(url).to_string_lossy().into_owned()
    };

    match reqwest::blocking::get(&url) {
        Ok(res) => {
            match res.text() {
                Ok(text) => {
                    let c_str = CString::new(text).unwrap();
                    c_str.into_raw()
                }
                Err(_) => {
                    let c_str = CString::new("Error: Could not read response text").unwrap();
                    c_str.into_raw()
                }
            }
        }
        Err(e) => {
            let c_str = CString::new(format!("Error: {}", e)).unwrap();
            c_str.into_raw()
        }
    }
}

pub type FskHttpCallback = extern "C" fn(u64, *const c_char, *const c_char, *const c_char, *mut c_void);

#[no_mangle]
pub extern "C" fn fsk_start_server(port: u16, callback: FskHttpCallback, context: *mut c_void) {
    let context_addr = context as usize;
    let rt = tokio::runtime::Runtime::new().unwrap();
    rt.block_on(async {
        let app = Router::new().fallback(any(move |req: Request<Body>| async move {
            let method = req.method().to_string();
            let path = req.uri().path().to_string();
            
            // Collect body
            let body_bytes: axum::body::Bytes = hyper::body::to_bytes(req.into_body()).await.unwrap_or_default();
            let body_str = String::from_utf8_lossy(&body_bytes).to_string();

            let req_id = {
                let mut id_gen = NEXT_REQ_ID.lock().unwrap();
                let id = *id_gen;
                *id_gen += 1;
                id
            };

            let (tx, rx) = oneshot::channel();
            {
                let mut pending = PENDING_REQUESTS.lock().unwrap();
                pending.insert(req_id, ResponseHandle { tx });
            }

            // Signal Fsk
            let c_method = CString::new(method).unwrap();
            let c_path = CString::new(path).unwrap();
            let c_body = CString::new(body_str).unwrap();
            
            unsafe {
                callback(req_id, c_method.as_ptr(), c_path.as_ptr(), c_body.as_ptr(), context_addr as *mut c_void);
            }

            // Wait for Fsk to respond via fsk_http_respond
            match rx.await {
                Ok((status, body)) => {
                    Response::builder()
                        .status(status)
                        .body(Body::from(body))
                        .unwrap()
                }
                Err(_) => {
                    Response::builder()
                        .status(500)
                        .body(Body::from("Internal Server Error (Fsk dropped request)"))
                        .unwrap()
                }
            }
        }));

        let addr = SocketAddr::from(([0, 0, 0, 0], port));
        println!("[RUST] Fsk Server listening on http://{}", addr);
        axum::Server::bind(&addr)
            .serve(app.into_make_service())
            .await
            .unwrap();
    });
}

#[no_mangle]
pub extern "C" fn fsk_http_respond(req_id: u64, status: u16, body: *const c_char) {
    let body_str = unsafe {
        CStr::from_ptr(body).to_string_lossy().into_owned()
    };

    let mut pending = PENDING_REQUESTS.lock().unwrap();
    if let Some(handle) = pending.remove(&req_id) {
        let _ = handle.tx.send((status, body_str));
    }
}
