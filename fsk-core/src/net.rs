use tokio::net::{TcpListener, TcpStream};
use tokio_tungstenite::{accept_async, tungstenite::protocol::Message};
use futures_util::{StreamExt, SinkExt};
use std::ffi::{CString, CStr};
use libc::{c_char, c_void};
use std::sync::{Arc, Mutex};
use std::collections::HashMap;

pub type FskWsCallback = extern "C" fn(u32, *const c_char, *mut c_void);

lazy_static::lazy_static! {
    pub static ref WS_SOCKETS: Arc<Mutex<HashMap<u32, tokio::sync::mpsc::UnboundedSender<Message>>>> = Arc::new(Mutex::new(HashMap::new()));
    pub static ref NEXT_WS_ID: Arc<Mutex<u32>> = Arc::new(Mutex::new(1));
}

#[no_mangle]
pub extern "C" fn fsk_ws_listen(port: u16, callback: FskWsCallback, context: *mut c_void) {
    let context_addr = context as usize;
    let rt = tokio::runtime::Runtime::new().unwrap();
    rt.block_on(async {
        let addr = format!("0.0.0.0:{}", port);
        let listener = TcpListener::bind(&addr).await.expect("Failed to bind WS");
        println!("[RUST] WebSocket Server listening on ws://{}", addr);

        while let Ok((stream, _)) = listener.accept().await {
            let callback = callback;
            let context_addr = context_addr;

            tokio::spawn(async move {
                if let Ok(ws_stream) = accept_async(stream).await {
                    let ws_id = {
                        let mut id_gen = NEXT_WS_ID.lock().unwrap();
                        let id = *id_gen;
                        *id_gen += 1;
                        id
                    };

                    let (mut write, mut read) = ws_stream.split();
                    let (tx, mut rx) = tokio::sync::mpsc::unbounded_channel::<Message>();
                    
                    {
                        let mut sockets = WS_SOCKETS.lock().unwrap();
                        sockets.insert(ws_id, tx);
                    }

                    tokio::spawn(async move {
                        while let Some(msg) = rx.recv().await {
                            if write.send(msg).await.is_err() { break; }
                        }
                    });

                    while let Some(msg) = read.next().await {
                        if let Ok(msg) = msg {
                            if msg.is_text() || msg.is_binary() {
                                let txt = msg.to_text().unwrap_or_default();
                                let c_txt = CString::new(txt).unwrap();
                                unsafe {
                                    callback(ws_id, c_txt.as_ptr(), context_addr as *mut c_void);
                                }
                            }
                        } else {
                            break;
                        }
                    }

                    {
                        let mut sockets = WS_SOCKETS.lock().unwrap();
                        sockets.remove(&ws_id);
                    }
                }
            });
        }
    });
}

#[no_mangle]
pub extern "C" fn fsk_ws_send(ws_id: u32, message: *const c_char) {
    let msg_str = unsafe { CStr::from_ptr(message).to_string_lossy().into_owned() };
    let sockets = WS_SOCKETS.lock().unwrap();
    if let Some(tx) = sockets.get(&ws_id) {
        let _ = tx.send(Message::Text(msg_str));
    }
}
