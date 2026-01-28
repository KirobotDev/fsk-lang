use lsp_server::{Connection, Message, Request, Response, Notification};
use lsp_types::{
    InitializeParams, ServerCapabilities, TextDocumentSyncKind, 
    TextDocumentSyncOptions, OneOf, SymbolInformation, SymbolKind, Location, Url,
    CompletionItem, CompletionItemKind, CompletionParams, Documentation
};
use serde_json::Value;
use std::collections::HashMap;
use std::error::Error;

mod analysis;
use analysis::Analysis;

fn main() -> Result<(), Box<dyn Error + Send + Sync>> {
    eprintln!("Starting Fsk LSP...");

    let (connection, io_threads) = Connection::stdio();

    let server_capabilities = serde_json::to_value(&ServerCapabilities {
        text_document_sync: Some(TextDocumentSyncOptions {
            open_close: Some(true),
            change: Some(TextDocumentSyncKind::FULL),
            ..Default::default()
        }),
        document_symbol_provider: Some(OneOf::Left(true)),
        definition_provider: Some(OneOf::Left(true)),
        completion_provider: Some(lsp_types::CompletionOptions {
            resolve_provider: Some(false),
            trigger_characters: Some(vec![".".to_string()]),
            ..Default::default()
        }),
        ..Default::default()
    }).unwrap();

    let initialization_params = connection.initialize(server_capabilities)?;
    eprintln!("Fsk LSP Initialized!");

    let mut documents: HashMap<String, String> = HashMap::new();

    for msg in &connection.receiver {
        match msg {
            Message::Request(req) => {
                if connection.handle_shutdown(&req)? {
                    return Ok(());
                }
                
                let resp = handle_request(&req, &documents);
                connection.sender.send(Message::Response(resp))?;
            }
            Message::Response(_) => {}
            Message::Notification(not) => {
                handle_notification(&not, &mut documents);
            }
        }
    }

    io_threads.join()?;
    Ok(())
}

fn handle_notification(not: &Notification, docs: &mut HashMap<String, String>) {
    match not.method.as_str() {
        "textDocument/didOpen" => {
            if let Ok(params) = serde_json::from_value::<lsp_types::DidOpenTextDocumentParams>(not.params.clone()) {
                docs.insert(params.text_document.uri.to_string(), params.text_document.text);
            }
        }
        "textDocument/didChange" => {
             if let Ok(params) = serde_json::from_value::<lsp_types::DidChangeTextDocumentParams>(not.params.clone()) {
                if let Some(change) = params.content_changes.first() {
                    docs.insert(params.text_document.uri.to_string(), change.text.clone());
                }
            }
        }
        _ => {}
    }
}

fn handle_request(req: &Request, docs: &HashMap<String, String>) -> Response {
    match req.method.as_str() {
        "textDocument/documentSymbol" => {
            let params: lsp_types::DocumentSymbolParams = serde_json::from_value(req.params.clone()).unwrap();
            let uri = params.text_document.uri.to_string();
            
            if let Some(text) = docs.get(&uri) {
                let analysis = Analysis::new(text);
                let symbols: Vec<SymbolInformation> = analysis.symbols.into_iter().map(|sym| {
                    #[allow(deprecated)]
                    SymbolInformation {
                        name: sym.name,
                        kind: sym.kind,
                        tags: None,
                        deprecated: None,
                        location: Location {
                            uri: params.text_document.uri.clone(),
                            range: lsp_types::Range {
                                start: lsp_types::Position { line: sym.line, character: sym.character },
                                end: lsp_types::Position { line: sym.line, character: sym.character + 5 } // approx
                            }
                        },
                        container_name: None,
                    }
                }).collect();
                
                return Response::new_ok(req.id.clone(), symbols);
            }
        }
        "textDocument/completion" => {
             let params: CompletionParams = serde_json::from_value(req.params.clone()).unwrap();
             let uri = params.text_document_position.text_document.uri.to_string();
             
             if let Some(text) = docs.get(&uri) {
                 let analysis = Analysis::new(text);
                 let items: Vec<CompletionItem> = analysis.symbols.into_iter().map(|sym| {
                     CompletionItem {
                         label: sym.name,
                         kind: Some(match sym.kind {
                             SymbolKind::FUNCTION => CompletionItemKind::FUNCTION,
                             SymbolKind::CLASS => CompletionItemKind::CLASS,
                             _ => CompletionItemKind::VARIABLE
                         }),
                         detail: Some("Fsk Symbol".to_string()),
                         ..Default::default()
                     }
                 }).collect();
                 
                 let mut all_items = items;
                 for kw in ["fn", "class", "let", "return", "if", "else", "print", "import", "async", "await"] {
                     all_items.push(CompletionItem {
                         label: kw.to_string(),
                         kind: Some(CompletionItemKind::KEYWORD),
                         ..Default::default()
                     });
                 }

                 return Response::new_ok(req.id.clone(), all_items);
             }
        }
        "textDocument/definition" => {
             let params: lsp_types::TextDocumentPositionParams = serde_json::from_value(req.params.clone()).unwrap();
             return Response::new_ok(req.id.clone(), Value::Null);
        }
        _ => {}
    }
    Response::new_ok(req.id.clone(), Value::Null)
}
