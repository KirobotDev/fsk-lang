#include "Lexer.hpp"
#include <iostream>
#include <map>
#include <string>
#include <variant>
#include <vector>

const std::map<std::string, TokenType> Lexer::keywords = {
    {"and", TokenType::AND},
    {"class", TokenType::CLASS},
    {"else", TokenType::ELSE},
    {"false", TokenType::FALSE},
    {"for", TokenType::FOR},
    {"fn", TokenType::FN},
    {"if", TokenType::IF},
    {"nil", TokenType::NIL},
    {"or", TokenType::OR},
    {"print", TokenType::PRINT},
    {"return", TokenType::RETURN},
    {"super", TokenType::SUPER},
    {"this", TokenType::THIS},
    {"true", TokenType::TRUE},
    {"let", TokenType::LET},
    {"const", TokenType::CONST},
    {"async", TokenType::ASYNC},
    {"await", TokenType::AWAIT},
    {"interface", TokenType::INTERFACE},
    {"import", TokenType::IMPORT},
    {"spawn", TokenType::SPAWN},
    {"while", TokenType::WHILE},
    {"try", TokenType::TRY},
    {"catch", TokenType::CATCH},
    {"throw", TokenType::THROW},
    {"match", TokenType::MATCH},
    {"new", TokenType::NEW}};

Lexer::Lexer(std::string source) : source(source) {}

bool Lexer::isAtEnd() { return current >= source.length(); }

char Lexer::advance() { return source[current++]; }

void Lexer::addToken(TokenType type) { addToken(type, std::monostate{}); }

void Lexer::addToken(TokenType type, Value literal) {
  std::string text = source.substr(start, current - start);
  tokens.emplace_back(type, text, literal, line);
}

bool Lexer::match(char expected) {
  if (isAtEnd())
    return false;
  if (source[current] != expected)
    return false;
  current++;
  return true;
}

char Lexer::peek() {
  if (isAtEnd())
    return '\0';
  return source[current];
}

char Lexer::peekNext() {
  if (current + 1 >= source.length())
    return '\0';
  return source[current + 1];
}

void Lexer::string(char quoteType) {
  std::string value = "";
  while (peek() != quoteType && !isAtEnd()) {
    if (peek() == '\n')
      line++;
    
    char c = advance();
    if (c == '\\') {
       if (isAtEnd()) break; 
       char next = advance();
       switch(next) {
           case 'n': value += '\n'; break;
           case 'r': value += '\r'; break;
           case 't': value += '\t'; break;
           case '\\': value += '\\'; break;
           case '"': value += '"'; break;
           case '\'': value += '\''; break;
           default: value += '\\'; value += next; break;
       }
    } else {
       value += c;
    }
  }

  if (isAtEnd()) {
    std::cerr << "Error: Unterminated string at line " << line << std::endl;
    return;
  }

  advance(); 

  addToken(TokenType::STRING, value);
}

bool Lexer::isDigit(char c) { return c >= '0' && c <= '9'; }

void Lexer::number() {
  while (isDigit(peek()))
    advance();

  if (peek() == '.' && isDigit(peekNext())) {
    advance();
    while (isDigit(peek()))
      advance();
  }

  addToken(TokenType::NUMBER, std::stod(source.substr(start, current - start)));
}

bool Lexer::isAlpha(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool Lexer::isAlphaNumeric(char c) { return isAlpha(c) || isDigit(c); }

void Lexer::identifier() {
  while (isAlphaNumeric(peek()))
    advance();

  std::string text = source.substr(start, current - start);
  TokenType type = TokenType::IDENTIFIER;
  if (keywords.count(text)) {
    type = keywords.at(text);
  }
  addToken(type);
}

void Lexer::scanToken() {
  if (isInTemplate && templateBraceStack.empty()) {
    if (peek() == '`') {
      scanTokenInternal();
      return;
    }
    std::string value = "";
    while (peek() != '`' && !(peek() == '$' && peekNext() == '{') &&
           !isAtEnd()) {
      if (peek() == '\n')
        line++;
      char c = advance();
      if (c == '\\') {
        if (isAtEnd())
          break;
        char next = advance();
        switch (next) {
        case 'n':
          value += '\n';
          break;
        case 'r':
          value += '\r';
          break;
        case 't':
          value += '\t';
          break;
        case '\\':
          value += '\\';
          break;
        case '`':
          value += '`';
          break;
        case '$':
          value += '$';
          break;
        case '{':
          value += '{';
          break;
        default:
          value += '\\';
          value += next;
          break;
        }
      } else {
        value += c;
      }
    }

    if (!value.empty()) {
      addToken(TokenType::STRING, value);
    }

    if (peek() == '$' && peekNext() == '{') {
      advance(); // $
      advance(); // {
      start = current;
      addToken(TokenType::DOLLAR_BRACE);
      templateBraceStack.push_back(0);
    }
    return;
  }

  scanTokenInternal();
}

void Lexer::scanTokenInternal() {
  char c = advance();
  switch (c) {
  case '(':
    addToken(TokenType::LEFT_PAREN);
    break;
  case ')':
    addToken(TokenType::RIGHT_PAREN);
    break;
  case '{':
    if (isInTemplate && !templateBraceStack.empty()) {
      templateBraceStack.back()++;
    }
    addToken(TokenType::LEFT_BRACE);
    break;
  case '}':
    if (isInTemplate && !templateBraceStack.empty()) {
      if (templateBraceStack.back() == 0) {
        templateBraceStack.pop_back();
        addToken(TokenType::RIGHT_BRACE);
        return; 
      } else {
        templateBraceStack.back()--;
      }
    }
    addToken(TokenType::RIGHT_BRACE);
    break;
  case '[':
    addToken(TokenType::LEFT_BRACKET);
    break;
  case ']':
    addToken(TokenType::RIGHT_BRACKET);
    break;
  case ',':
    addToken(TokenType::COMMA);
    break;
  case '.':
    if (match('.') && match('.')) {
      addToken(TokenType::ELLIPSIS);
    } else {
      addToken(TokenType::DOT);
    }
    break;
  case '?':
    if (match('.')) {
      addToken(TokenType::QUESTION_DOT);
    } else if (match('?')) {
      addToken(TokenType::QUESTION_QUESTION);
    } 
    break;
  case '|':
    if (match('>')) {
      addToken(TokenType::PIPE);
    }
    break;
  case ':':
    addToken(TokenType::COLON);
    break;
  case '-':
    addToken(match('>') ? TokenType::ARROW : TokenType::MINUS);
    break;
  case '+':
    addToken(TokenType::PLUS);
    break;
  case ';':
    addToken(TokenType::SEMICOLON);
    break;
  case '*':
    addToken(TokenType::STAR);
    break;
  case '!':
    addToken(match('=') ? TokenType::BANG_EQUAL : TokenType::BANG);
    break;
  case '=':
    if (match('=')) {
      addToken(TokenType::EQUAL_EQUAL);
    } else if (match('>')) {
      addToken(TokenType::FAT_ARROW);
    } else {
      addToken(TokenType::EQUAL);
    }
    break;
  case '<':
    addToken(match('=') ? TokenType::LESS_EQUAL : TokenType::LESS);
    break;
  case '>':
    addToken(match('=') ? TokenType::GREATER_EQUAL : TokenType::GREATER);
    break;
  case '/':
    if (match('/')) {
      while (peek() != '\n' && !isAtEnd())
        advance();
    } else {
      addToken(TokenType::SLASH);
    }
    break;
  case ' ':
  case '\r':
  case '\t':
    break;
  case '\n':
    line++;
    break;
  case '"':
    string('"');
    break;
  case '\'':
    string('\'');
    break;
  case '`':
    if (isInTemplate && templateBraceStack.empty()) {
      addToken(TokenType::BACKTICK);
      isInTemplate = false;
    } else {
      addToken(TokenType::BACKTICK);
      isInTemplate = true;
    }
    break;
  default:
    if (isDigit(c)) {
      number();
    } else if (isAlpha(c)) {
      identifier();
    } else {
      std::cerr << "Error: Unexpected character '" << c << "' at line " << line
                << std::endl;
    }
    break;
  }
}

std::vector<Token> Lexer::scanTokens() {
  while (!isAtEnd()) {
    start = current;
    scanToken();
  }
  tokens.emplace_back(TokenType::EOF_TOKEN, "", std::monostate{}, line);
  return tokens;
}
