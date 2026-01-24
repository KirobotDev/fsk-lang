#pragma once
#include <memory>
#include <string>
#include <variant>
#include <vector>

enum class TokenType {
  LEFT_PAREN,
  RIGHT_PAREN,
  LEFT_BRACE,
  RIGHT_BRACE,
  COMMA,
  DOT,
  MINUS,
  PLUS,
  SEMICOLON,
  SLASH,
  STAR,
  ARROW, 
  COLON, 

  LEFT_BRACKET,
  RIGHT_BRACKET,

  BANG,
  BANG_EQUAL,
  EQUAL,
  EQUAL_EQUAL,
  GREATER,
  GREATER_EQUAL,
  LESS,
  LESS_EQUAL,

  IDENTIFIER,
  STRING,
  NUMBER,

  AND,
  CLASS,
  ELSE,
  FALSE,
  FN,
  FOR,
  IF,
  NIL,
  OR,
  PRINT,
  RETURN,
  SUPER,
  THIS,
  TRUE,
  LET,
  CONST,
  ASYNC,
  AWAIT,
  INTERFACE,
  IMPORT,
  SPAWN,
  WHILE,
  TRY,
  CATCH,
  THROW,
  MATCH,
  BACKTICK,
  DOLLAR_BRACE,

  EOF_TOKEN
};

struct Callable;
struct FSKInstance;
struct FSKArray;
using Value =
    std::variant<std::string, double, bool, std::monostate,
                 std::shared_ptr<Callable>, std::shared_ptr<FSKInstance>,
                 std::shared_ptr<FSKArray>>;

struct FSKArray {
  std::vector<Value> elements;
  FSKArray(std::vector<Value> elements) : elements(elements) {}
};

struct Token {
  TokenType type;
  std::string lexeme;
  Value literal;
  int line;

  Token(TokenType type, std::string lexeme, Value literal, int line)
      : type(type), lexeme(lexeme), literal(literal), line(line) {}
};
