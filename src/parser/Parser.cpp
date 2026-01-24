#include "Parser.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

Parser::Parser(std::vector<Token> tokens) : tokens(tokens) {}

std::vector<std::shared_ptr<Stmt>> Parser::parse() {
  std::vector<std::shared_ptr<Stmt>> statements;
  while (!isAtEnd()) {
    statements.push_back(declaration());
  }
  return statements;
}

std::shared_ptr<Stmt> Parser::declaration() {
  try {
    if (match({TokenType::CLASS}))
      return classDeclaration();
    if (match({TokenType::ASYNC})) {
      consume(TokenType::FN, "Expect 'fn' after 'async'.");
      return function("function", true);
    }
    if (match({TokenType::FN}))
      return function("function");
    if (match({TokenType::LET}))
      return varDeclaration();
    if (match({TokenType::CONST}))
      return constDeclaration();
    if (match({TokenType::IMPORT}))
      return importStatement();
    return statement();
  } catch (const std::exception &e) {
    synchronize();
    return nullptr;
  }
}

std::shared_ptr<Stmt> Parser::classDeclaration() {
  Token name = consume(TokenType::IDENTIFIER, "Expect class name.");
  std::shared_ptr<Variable> superclass = nullptr;
  if (match({TokenType::LESS})) {
    consume(TokenType::IDENTIFIER, "Expect superclass name.");
    superclass = std::make_shared<Variable>(previous());
  }
  consume(TokenType::LEFT_BRACE, "Expect '{' before class body.");
  std::vector<std::shared_ptr<Function>> methods;
  while (!check(TokenType::RIGHT_BRACE) && !isAtEnd()) {
    bool isAsync = match({TokenType::ASYNC});
    consume(TokenType::FN, "Expect 'fn' for class method.");
    methods.push_back(
        std::static_pointer_cast<Function>(function("method", isAsync)));
  }
  consume(TokenType::RIGHT_BRACE, "Expect '}' after class body.");
  return std::make_shared<Class>(name, superclass, methods);
}

std::shared_ptr<Stmt> Parser::function(std::string kind, bool isAsync) {
  Token name = consume(TokenType::IDENTIFIER, "Expect " + kind + " name.");
  consume(TokenType::LEFT_PAREN, "Expect '(' after " + kind + " name.");
  std::vector<Token> parameters;
  if (!check(TokenType::RIGHT_PAREN)) {
    do {
      if (parameters.size() >= 255) {
      }
      parameters.push_back(
          consume(TokenType::IDENTIFIER, "Expect parameter name."));
    } while (match({TokenType::COMMA}));
  }
  consume(TokenType::RIGHT_PAREN, "Expect ')' after parameters.");

  std::string returnType = "";
  if (match({TokenType::ARROW})) {
    returnType =
        consume(TokenType::IDENTIFIER, "Expect return type after '->'.").lexeme;
  }

  consume(TokenType::LEFT_BRACE, "Expect '{' before " + kind + " body.");
  std::vector<std::shared_ptr<Stmt>> body = block();
  return std::make_shared<Function>(name, parameters, body, isAsync,
                                    returnType);
}

std::shared_ptr<Stmt> Parser::varDeclaration() {
  std::shared_ptr<Expr> pat = pattern();

  std::string typeHint = "";
  if (match({TokenType::COLON})) {
    typeHint = consume(TokenType::IDENTIFIER, "Expect type after ':'.").lexeme;
  }

  std::shared_ptr<Expr> initializer = nullptr;
  if (match({TokenType::EQUAL})) {
    initializer = expression();
  }
  match({TokenType::SEMICOLON});
  return std::make_shared<Let>(pat, initializer, typeHint);
}

std::shared_ptr<Stmt> Parser::constDeclaration() {
  std::shared_ptr<Expr> pat = pattern();
  consume(TokenType::EQUAL, "Expect '=' after constant name.");
  std::shared_ptr<Expr> initializer = expression();
  match({TokenType::SEMICOLON});
  return std::make_shared<Const>(pat, initializer);
}

std::shared_ptr<Expr> Parser::pattern() {
  if (match({TokenType::IDENTIFIER})) {
    return std::make_shared<Variable>(previous());
  }

  if (match({TokenType::NUMBER, TokenType::STRING, TokenType::TRUE, TokenType::FALSE, TokenType::NIL})) {
    return std::make_shared<Literal>(previous().literal);
  }

  if (match({TokenType::LEFT_BRACKET})) {
    std::vector<std::shared_ptr<Expr>> elements;
    if (!check(TokenType::RIGHT_BRACKET)) {
      do {
        elements.push_back(pattern());
      } while (match({TokenType::COMMA}));
    }
    consume(TokenType::RIGHT_BRACKET, "Expect ']' after array pattern.");
    return std::make_shared<Array>(elements);
  }

  
  throw std::runtime_error("Expect pattern.");
}

std::shared_ptr<Stmt> Parser::matchStatement() {
  consume(TokenType::LEFT_PAREN, "Expect '(' after 'match'.");
  std::shared_ptr<Expr> expr = expression();
  consume(TokenType::RIGHT_PAREN, "Expect ')' after expression.");

  consume(TokenType::LEFT_BRACE, "Expect '{' before match body.");

  std::vector<std::pair<std::shared_ptr<Expr>, std::shared_ptr<Stmt>>> arms;
  while (!check(TokenType::RIGHT_BRACE) && !isAtEnd()) {
    std::shared_ptr<Expr> pat = pattern();
    consume(TokenType::ARROW, "Expect '->' after pattern.");
    std::shared_ptr<Stmt> body = statement();
    arms.push_back({pat, body});
  }

  consume(TokenType::RIGHT_BRACE, "Expect '}' after match body.");
  return std::make_shared<Match>(expr, arms);
}

std::shared_ptr<Stmt> Parser::importStatement() {
  Token keyword = previous();
  std::shared_ptr<Expr> file = expression();
  match({TokenType::SEMICOLON});
  return std::make_shared<Import>(keyword, file);
}

std::shared_ptr<Stmt> Parser::statement() {
  if (match({TokenType::FOR}))
    return forStatement();
  if (match({TokenType::IF}))
    return ifStatement();
  if (match({TokenType::MATCH}))
    return matchStatement();
  if (match({TokenType::PRINT}))
    return printStatement();
  if (match({TokenType::RETURN}))
    return returnStatement();
  if (match({TokenType::WHILE}))
    return whileStatement();
  if (match({TokenType::TRY}))
    return tryStatement();
  if (match({TokenType::THROW}))
    return throwStatement();
  if (match({TokenType::LEFT_BRACE}))
    return std::make_shared<Block>(block());
  return expressionStatement();
}

std::shared_ptr<Stmt> Parser::forStatement() {
  consume(TokenType::LEFT_PAREN, "Expect '(' after 'for'.");
  std::shared_ptr<Stmt> initializer;
  if (match({TokenType::SEMICOLON})) {
    initializer = nullptr;
  } else if (match({TokenType::LET})) {
    initializer = varDeclaration();
  } else {
    initializer = expressionStatement();
  }

  std::shared_ptr<Expr> condition = nullptr;
  if (!check(TokenType::SEMICOLON)) {
    condition = expression();
  }
  consume(TokenType::SEMICOLON, "Expect ';' after loop condition.");

  std::shared_ptr<Expr> increment = nullptr;
  if (!check(TokenType::RIGHT_PAREN)) {
    increment = expression();
  }
  consume(TokenType::RIGHT_PAREN, "Expect ')' after for clauses.");

  std::shared_ptr<Stmt> body = statement();

  if (increment != nullptr) {
    std::vector<std::shared_ptr<Stmt>> stmts;
    stmts.push_back(body);
    stmts.push_back(std::make_shared<Expression>(increment));
    body = std::make_shared<Block>(stmts);
  }

  if (condition == nullptr)
    condition = std::make_shared<Literal>(true);
  body = std::make_shared<While>(condition, body);

  if (initializer != nullptr) {
    std::vector<std::shared_ptr<Stmt>> stmts;
    stmts.push_back(initializer);
    stmts.push_back(body);
    body = std::make_shared<Block>(stmts);
  }

  return body;
}

std::shared_ptr<Stmt> Parser::ifStatement() {
  consume(TokenType::LEFT_PAREN, "Expect '(' after 'if'.");
  std::shared_ptr<Expr> condition = expression();
  consume(TokenType::RIGHT_PAREN, "Expect ')' after if condition.");

  std::shared_ptr<Stmt> thenBranch = statement();
  std::shared_ptr<Stmt> elseBranch = nullptr;
  if (match({TokenType::ELSE})) {
    elseBranch = statement();
  }

  return std::make_shared<If>(condition, thenBranch, elseBranch);
}

std::shared_ptr<Stmt> Parser::printStatement() {
  std::shared_ptr<Expr> value = expression();
  match({TokenType::SEMICOLON});
  return std::make_shared<Print>(value);
}

std::shared_ptr<Stmt> Parser::returnStatement() {
  Token keyword = previous();
  std::shared_ptr<Expr> value = nullptr;
  if (!check(TokenType::SEMICOLON) && !check(TokenType::RIGHT_BRACE)) {
    value = expression();
  }
  match({TokenType::SEMICOLON});
  return std::make_shared<Return>(keyword, value);
}

std::shared_ptr<Stmt> Parser::whileStatement() {
  consume(TokenType::LEFT_PAREN, "Expect '(' after 'while'.");
  std::shared_ptr<Expr> condition = expression();
  consume(TokenType::RIGHT_PAREN, "Expect ')' after while condition.");
  std::shared_ptr<Stmt> body = statement();
  return std::make_shared<While>(condition, body);
}

std::vector<std::shared_ptr<Stmt>> Parser::block() {
  std::vector<std::shared_ptr<Stmt>> statements;
  while (!check(TokenType::RIGHT_BRACE) && !isAtEnd()) {
    statements.push_back(declaration());
  }
  consume(TokenType::RIGHT_BRACE, "Expect '}' after block.");
  return statements;
}

std::shared_ptr<Stmt> Parser::expressionStatement() {
  std::shared_ptr<Expr> expr = expression();
  match({TokenType::SEMICOLON});
  return std::make_shared<Expression>(expr);
}

std::shared_ptr<Expr> Parser::expression() { return assignment(); }

std::shared_ptr<Expr> Parser::assignment() {
  std::shared_ptr<Expr> expr = or_expr();
  if (match({TokenType::EQUAL})) {
    Token equals = previous();
    std::shared_ptr<Expr> value = assignment();
    if (Variable *v = dynamic_cast<Variable *>(expr.get())) {
      return std::make_shared<Assign>(v->name, value);
    } else if (Get *g = dynamic_cast<Get *>(expr.get())) {
      return std::make_shared<Set>(g->object, g->name, value);
    } else if (IndexExpr *i = dynamic_cast<IndexExpr *>(expr.get())) {
      return std::make_shared<IndexSet>(i->callee, i->index, value);
    }
  }
  return expr;
}

std::shared_ptr<Expr> Parser::or_expr() {
  std::shared_ptr<Expr> expr = and_expr();
  while (match({TokenType::OR})) {
    Token op = previous();
    std::shared_ptr<Expr> right = and_expr();
    expr = std::make_shared<Logical>(expr, op, right);
  }
  return expr;
}

std::shared_ptr<Expr> Parser::and_expr() {
  std::shared_ptr<Expr> expr = equality();
  while (match({TokenType::AND})) {
    Token op = previous();
    std::shared_ptr<Expr> right = equality();
    expr = std::make_shared<Logical>(expr, op, right);
  }
  return expr;
}

std::shared_ptr<Expr> Parser::equality() {
  std::shared_ptr<Expr> expr = comparison();
  while (match({TokenType::BANG_EQUAL, TokenType::EQUAL_EQUAL})) {
    Token op = previous();
    std::shared_ptr<Expr> right = comparison();
    expr = std::make_shared<Binary>(expr, op, right);
  }
  return expr;
}

std::shared_ptr<Expr> Parser::comparison() {
  std::shared_ptr<Expr> expr = term();
  while (match({TokenType::GREATER, TokenType::GREATER_EQUAL, TokenType::LESS,
                TokenType::LESS_EQUAL})) {
    Token op = previous();
    std::shared_ptr<Expr> right = term();
    expr = std::make_shared<Binary>(expr, op, right);
  }
  return expr;
}

std::shared_ptr<Expr> Parser::term() {
  std::shared_ptr<Expr> expr = factor();
  while (match({TokenType::MINUS, TokenType::PLUS})) {
    Token op = previous();
    std::shared_ptr<Expr> right = factor();
    expr = std::make_shared<Binary>(expr, op, right);
  }
  return expr;
}

std::shared_ptr<Expr> Parser::factor() {
  std::shared_ptr<Expr> expr = unary();
  while (match({TokenType::SLASH, TokenType::STAR})) {
    Token op = previous();
    std::shared_ptr<Expr> right = unary();
    expr = std::make_shared<Binary>(expr, op, right);
  }
  return expr;
}

std::shared_ptr<Expr> Parser::unary() {
  if (match({TokenType::BANG, TokenType::MINUS})) {
    Token op = previous();
    std::shared_ptr<Expr> right = unary();
    return std::make_shared<Unary>(op, right);
  }

  if (match({TokenType::AWAIT})) {
    Token keyword = previous();
    std::shared_ptr<Expr> expression = unary();
    return std::make_shared<Await>(keyword, expression);
  }

  return call();
}

std::shared_ptr<Expr> Parser::call() {
  std::shared_ptr<Expr> expr = primary();
  while (true) {
    if (match({TokenType::LEFT_PAREN})) {
      expr = finishCall(expr);
    } else if (match({TokenType::DOT})) {
      Token name =
          consume(TokenType::IDENTIFIER, "Expect property name after '.'.");
      expr = std::make_shared<Get>(expr, name);
    } else if (match({TokenType::LEFT_BRACKET})) {
      std::shared_ptr<Expr> index = expression();
      Token bracket = consume(TokenType::RIGHT_BRACKET, "Expect ']' after index.");
      expr = std::make_shared<IndexExpr>(expr, bracket, index);
    } else {
      break;
    }
  }
  return expr;
}

std::shared_ptr<Expr> Parser::finishCall(std::shared_ptr<Expr> callee) {
  std::vector<std::shared_ptr<Expr>> arguments;
  if (!check(TokenType::RIGHT_PAREN)) {
    do {
      if (arguments.size() >= 255) {
      }
      arguments.push_back(expression());
    } while (match({TokenType::COMMA}));
  }
  Token paren = consume(TokenType::RIGHT_PAREN, "Expect ')' after arguments.");
  return std::make_shared<Call>(callee, paren, arguments);
}

std::shared_ptr<Expr> Parser::primary() {
  if (match({TokenType::FALSE}))
    return std::make_shared<Literal>(false);
  if (match({TokenType::TRUE}))
    return std::make_shared<Literal>(true);
  if (match({TokenType::NIL}))
    return std::make_shared<Literal>(std::monostate{});

  if (match({TokenType::NUMBER, TokenType::STRING})) {
    return std::make_shared<Literal>(previous().literal);
  }

  if (match({TokenType::BACKTICK})) {
    std::vector<std::string> strings;
    std::vector<std::shared_ptr<Expr>> expressions;

    if (check(TokenType::STRING)) {
      advance();
      strings.push_back(std::get<std::string>(previous().literal));
    } else {
      strings.push_back("");
    }

    while (match({TokenType::DOLLAR_BRACE})) {
      expressions.push_back(expression());
      consume(TokenType::RIGHT_BRACE, "Expect '}' after interpolation.");

      if (check(TokenType::STRING)) {
        advance();
        strings.push_back(std::get<std::string>(previous().literal));
      } else {
        strings.push_back("");
      }
    }

    consume(TokenType::BACKTICK, "Expect '`' after template literal.");
    return std::make_shared<TemplateLiteral>(strings, expressions);
  }

  if (match({TokenType::SUPER})) {
    Token keyword = previous();
    consume(TokenType::DOT, "Expect '.' after 'super'.");
    Token method =
        consume(TokenType::IDENTIFIER, "Expect superclass method name.");
    return std::make_shared<Super>(keyword, method);
  }

  if (match({TokenType::THIS}))
    return std::make_shared<This>(previous());

  if (match({TokenType::IDENTIFIER})) {
    return std::make_shared<Variable>(previous());
  }

  if (match({TokenType::FN})) {
    consume(TokenType::LEFT_PAREN, "Expect '(' after 'fn'.");
    std::vector<Token> parameters;
    if (!check(TokenType::RIGHT_PAREN)) {
      do {
        if (parameters.size() >= 255) {
        }
        parameters.push_back(
            consume(TokenType::IDENTIFIER, "Expect parameter name."));
      } while (match({TokenType::COMMA}));
    }
    consume(TokenType::RIGHT_PAREN, "Expect ')' after parameters.");
    consume(TokenType::LEFT_BRACE, "Expect '{' before lambda body.");
    std::vector<std::shared_ptr<Stmt>> body = block();
    return std::make_shared<FunctionExpr>(parameters, body);
  }

  if (match({TokenType::LEFT_BRACKET})) {
    std::vector<std::shared_ptr<Expr>> elements;
    if (!check(TokenType::RIGHT_BRACKET)) {
      do {
        elements.push_back(expression());
      } while (match({TokenType::COMMA}));
    }
    consume(TokenType::RIGHT_BRACKET, "Expect ']' after array elements.");
    return std::make_shared<Array>(elements);
  }

  if (match({TokenType::LEFT_PAREN})) {
    std::shared_ptr<Expr> expr = expression();
    consume(TokenType::RIGHT_PAREN, "Expect ')' after expression.");
    return std::make_shared<Grouping>(expr);
  }

  throw std::runtime_error("Expect expression.");
}

bool Parser::match(std::vector<TokenType> types) {
  for (TokenType type : types) {
    if (check(type)) {
      advance();
      return true;
    }
  }
  return false;
}

bool Parser::check(TokenType type) {
  if (isAtEnd())
    return false;
  return peek().type == type;
}

Token Parser::advance() {
  if (!isAtEnd())
    current++;
  return previous();
}

bool Parser::isAtEnd() { return peek().type == TokenType::EOF_TOKEN; }

Token Parser::peek() { return tokens[current]; }

Token Parser::previous() { return tokens[current - 1]; }

Token Parser::consume(TokenType type, std::string message) {
  if (check(type))
    return advance();
  throw std::runtime_error(message + " at line " + std::to_string(peek().line));
}

void Parser::synchronize() {
  advance();
  while (!isAtEnd()) {
    if (previous().type == TokenType::SEMICOLON)
      return;
    switch (peek().type) {
    case TokenType::CLASS:
    case TokenType::FN:
    case TokenType::LET:
    case TokenType::CONST:
    case TokenType::FOR:
    case TokenType::IF:
    case TokenType::WHILE:
    case TokenType::PRINT:
    case TokenType::RETURN:
      return;
    default:
      break;
    }
    advance();
  }
}

std::shared_ptr<Stmt> Parser::tryStatement() {
  std::shared_ptr<Stmt> tryBranch = statement();
  consume(TokenType::CATCH, "Expect 'catch' after try block.");
  consume(TokenType::LEFT_PAREN, "Expect '(' after 'catch'.");
  Token catchName = consume(TokenType::IDENTIFIER, "Expect exception name.");
  consume(TokenType::RIGHT_PAREN, "Expect ')' after exception name.");
  std::shared_ptr<Stmt> catchBranch = statement();

  return std::make_shared<Try>(tryBranch, catchName, catchBranch);
}

std::shared_ptr<Stmt> Parser::throwStatement() {
  Token keyword = previous();
  std::shared_ptr<Expr> value = expression();
  match({TokenType::SEMICOLON});
  return std::make_shared<Throw>(keyword, value);
}
