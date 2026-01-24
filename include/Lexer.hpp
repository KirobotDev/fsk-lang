#pragma once
#include "Token.hpp"
#include <map>
#include <string>
#include <vector>

class Lexer {
public:
  Lexer(std::string source);
  std::vector<Token> scanTokens();

private:
  std::string source;
  std::vector<Token> tokens;
  int start = 0;
  int current = 0;
  int line = 1;

  bool isAtEnd();
  char advance();
  void addToken(TokenType type);
  void addToken(TokenType type, Value literal);
  bool match(char expected);
  char peek();
  char peekNext();
  void string(char quoteType);
  void number();
  void scanner();
  void identifier();
  void scanToken();
  void scanTokenInternal();
  bool isDigit(char c);
  bool isAlpha(char c);
  bool isAlphaNumeric(char c);

  std::vector<int> templateBraceStack;
  bool isInTemplate = false;

  static const std::map<std::string, TokenType> keywords;
};
