use logos::Logos;
use lsp_types::{Position, Range, SymbolKind};

#[derive(Logos, Debug, PartialEq)]
pub enum Token {
    #[token("fn")]
    Fn,
    #[token("class")]
    Class,
    #[token("let")]
    Let,
    #[token("const")]
    Const,
    
    #[regex("[a-zA-Z_][a-zA-Z0-9_]*")]
    Identifier,

    #[token("{")]
    LBrace,
    #[token("}")]
    RBrace,

    #[regex(r"//.*", logos::skip)]
    Comment,
    #[regex(r"[ \t\n\f]+", logos::skip)]
    Whitespace,

    #[error]
    Error,
}

#[derive(Debug, Clone)]
pub struct FskSymbol {
    pub name: String,
    pub kind: SymbolKind,
    pub line: u32,
    pub character: u32,
}

pub struct Analysis {
    pub symbols: Vec<FskSymbol>,
}

impl Analysis {
    pub fn new(source: &str) -> Self {
        let mut symbols = Vec::new();
        let mut lexer = Token::lexer(source);
        
        let line_starts: Vec<usize> = std::iter::once(0)
            .chain(source.match_indices('\n').map(|(i, _)| i + 1))
            .collect();

        let get_pos = |offset: usize| -> (u32, u32) {
            let line_idx = line_starts.binary_search(&offset).unwrap_or_else(|x| x - 1);
            let line_start = line_starts[line_idx];
            (line_idx as u32, (offset - line_start) as u32)
        };

        while let Some(token) = lexer.next() {
            match token {
                Token::Fn => {
                    if let Some(Token::Identifier) = lexer.next() {
                        let name = lexer.slice().to_string();
                        let (line, col) = get_pos(lexer.span().start);
                        symbols.push(FskSymbol {
                            name,
                            kind: SymbolKind::FUNCTION,
                            line,
                            character: col
                        });
                    }
                }
                Token::Class => {
                    if let Some(Token::Identifier) = lexer.next() {
                        let name = lexer.slice().to_string();
                        let (line, col) = get_pos(lexer.span().start);
                        symbols.push(FskSymbol {
                            name,
                            kind: SymbolKind::CLASS,
                            line,
                            character: col
                        });
                    }
                }
                Token::Let | Token::Const => {
                     if let Some(Token::Identifier) = lexer.next() {
                        let name = lexer.slice().to_string();
                        let (line, col) = get_pos(lexer.span().start);
                        symbols.push(FskSymbol {
                            name,
                            kind: SymbolKind::VARIABLE,
                            line,
                            character: col
                        });
                    }
                },
                _ => {}
            }
        }

        Self { symbols }
    }
}
