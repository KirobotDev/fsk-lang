#pragma once
#include "Expr.hpp"
#include "Stmt.hpp"
#include "Token.hpp"
#include <memory>
#include <vector>

class Parser {
public:
  Parser(std::vector<Token> tokens);
  std::vector<std::shared_ptr<Stmt>> parse();

private:
  std::vector<Token> tokens;
  int current = 0;

  std::shared_ptr<Stmt> declaration();
  std::shared_ptr<Stmt> classDeclaration();
  std::shared_ptr<Stmt> function(std::string kind, bool isAsync = false);
  std::shared_ptr<Stmt> varDeclaration();
  std::shared_ptr<Stmt> constDeclaration();
  std::shared_ptr<Stmt> importStatement();
  std::shared_ptr<Stmt> statement();
  std::shared_ptr<Stmt> forStatement();
  std::shared_ptr<Stmt> ifStatement();
  std::shared_ptr<Stmt> matchStatement();
  std::shared_ptr<Stmt> printStatement();
  std::shared_ptr<Stmt> returnStatement();
  std::shared_ptr<Stmt> whileStatement();
  std::shared_ptr<Stmt> tryStatement();
  std::shared_ptr<Stmt> throwStatement();
  std::vector<std::shared_ptr<Stmt>> block();
  std::shared_ptr<Stmt> expressionStatement();

  std::shared_ptr<Expr> expression();
  std::shared_ptr<Expr> pattern();
  std::shared_ptr<Expr> assignment();
  std::shared_ptr<Expr> or_expr();
  std::shared_ptr<Expr> and_expr();
  std::shared_ptr<Expr> equality();
  std::shared_ptr<Expr> comparison();
  std::shared_ptr<Expr> term();
  std::shared_ptr<Expr> factor();
  std::shared_ptr<Expr> unary();
  std::shared_ptr<Expr> call();
  std::shared_ptr<Expr> finishCall(std::shared_ptr<Expr> callee);
  std::shared_ptr<Expr> primary();

  bool match(std::vector<TokenType> types);
  bool check(TokenType type);
  Token advance();
  bool isAtEnd();
  Token peek();
  Token previous();
  Token consume(TokenType type, std::string message);
  void synchronize();
};
