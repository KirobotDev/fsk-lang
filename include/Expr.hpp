#pragma once
#include "Token.hpp"
#include <memory>
#include <vector>

struct Stmt;
struct FunctionExpr;
struct ExprVisitor;

struct Expr {
  virtual ~Expr() = default;
  virtual void accept(ExprVisitor &visitor) = 0;
};

struct Binary;
struct Grouping;
struct Literal;
struct Unary;
struct Variable;
struct Assign;
struct Logical;
struct Call;
struct Get;
struct Set;
struct This;
struct Super;
struct Await;
struct Array;
struct IndexExpr;
struct IndexSet;

struct ExprVisitor {
  virtual void visitBinaryExpr(Binary &expr) = 0;
  virtual void visitGroupingExpr(Grouping &expr) = 0;
  virtual void visitLiteralExpr(Literal &expr) = 0;
  virtual void visitUnaryExpr(Unary &expr) = 0;
  virtual void visitVariableExpr(Variable &expr) = 0;
  virtual void visitAssignExpr(Assign &expr) = 0;
  virtual void visitLogicalExpr(Logical &expr) = 0;
  virtual void visitCallExpr(Call &expr) = 0;
  virtual void visitGetExpr(Get &expr) = 0;
  virtual void visitSetExpr(Set &expr) = 0;
  virtual void visitThisExpr(This &expr) = 0;
  virtual void visitSuperExpr(Super &expr) = 0;
  virtual void visitAwaitExpr(Await &expr) = 0;
  virtual void visitArrayExpr(Array &expr) = 0;
  virtual void visitIndexExpr(IndexExpr &expr) = 0;
  virtual void visitIndexSetExpr(IndexSet &expr) = 0;
  virtual void visitFunctionExpr(FunctionExpr &expr) = 0;
};

struct Binary : Expr {
  std::shared_ptr<Expr> left;
  Token op;
  std::shared_ptr<Expr> right;
  Binary(std::shared_ptr<Expr> left, Token op, std::shared_ptr<Expr> right)
      : left(left), op(op), right(right) {}
  void accept(ExprVisitor &visitor) override { visitor.visitBinaryExpr(*this); }
};

struct Grouping : Expr {
  std::shared_ptr<Expr> expression;
  Grouping(std::shared_ptr<Expr> expression) : expression(expression) {}
  void accept(ExprVisitor &visitor) override {
    visitor.visitGroupingExpr(*this);
  }
};

struct Literal : Expr {
  Value value;
  Literal(Value value) : value(value) {}
  void accept(ExprVisitor &visitor) override {
    visitor.visitLiteralExpr(*this);
  }
};

struct Unary : Expr {
  Token op;
  std::shared_ptr<Expr> right;
  Unary(Token op, std::shared_ptr<Expr> right) : op(op), right(right) {}
  void accept(ExprVisitor &visitor) override { visitor.visitUnaryExpr(*this); }
};

struct Variable : Expr {
  Token name;
  Variable(Token name) : name(name) {}
  void accept(ExprVisitor &visitor) override {
    visitor.visitVariableExpr(*this);
  }
};

struct Assign : Expr {
  Token name;
  std::shared_ptr<Expr> value;
  Assign(Token name, std::shared_ptr<Expr> value) : name(name), value(value) {}
  void accept(ExprVisitor &visitor) override { visitor.visitAssignExpr(*this); }
};

struct Logical : Expr {
  std::shared_ptr<Expr> left;
  Token op;
  std::shared_ptr<Expr> right;
  Logical(std::shared_ptr<Expr> left, Token op, std::shared_ptr<Expr> right)
      : left(left), op(op), right(right) {}
  void accept(ExprVisitor &visitor) override {
    visitor.visitLogicalExpr(*this);
  }
};

struct Call : Expr {
  std::shared_ptr<Expr> callee;
  Token paren;
  std::vector<std::shared_ptr<Expr>> arguments;
  Call(std::shared_ptr<Expr> callee, Token paren,
       std::vector<std::shared_ptr<Expr>> arguments)
      : callee(callee), paren(paren), arguments(arguments) {}
  void accept(ExprVisitor &visitor) override { visitor.visitCallExpr(*this); }
};

struct Get : Expr {
  std::shared_ptr<Expr> object;
  Token name;
  Get(std::shared_ptr<Expr> object, Token name) : object(object), name(name) {}
  void accept(ExprVisitor &visitor) override { visitor.visitGetExpr(*this); }
};

struct Set : Expr {
  std::shared_ptr<Expr> object;
  Token name;
  std::shared_ptr<Expr> value;
  Set(std::shared_ptr<Expr> object, Token name, std::shared_ptr<Expr> value)
      : object(object), name(name), value(value) {}
  void accept(ExprVisitor &visitor) override { visitor.visitSetExpr(*this); }
};

struct This : Expr {
  Token keyword;
  This(Token keyword) : keyword(keyword) {}
  void accept(ExprVisitor &visitor) override { visitor.visitThisExpr(*this); }
};

struct Super : Expr {
  Token keyword;
  Token method;
  Super(Token keyword, Token method) : keyword(keyword), method(method) {}
  void accept(ExprVisitor &visitor) override { visitor.visitSuperExpr(*this); }
};
struct Await : Expr {
  Token keyword;
  std::shared_ptr<Expr> expression;
  Await(Token keyword, std::shared_ptr<Expr> expression)
      : keyword(keyword), expression(expression) {}
  void accept(ExprVisitor &visitor) override { visitor.visitAwaitExpr(*this); }
};

struct Array : Expr {
  std::vector<std::shared_ptr<Expr>> elements;
  Array(std::vector<std::shared_ptr<Expr>> elements) : elements(elements) {}
  void accept(ExprVisitor &visitor) override { visitor.visitArrayExpr(*this); }
};

struct IndexExpr : Expr {
  std::shared_ptr<Expr> callee;
  Token bracket;
  std::shared_ptr<Expr> index;
  IndexExpr(std::shared_ptr<Expr> callee, Token bracket, std::shared_ptr<Expr> index)
      : callee(callee), bracket(bracket), index(index) {}
  void accept(ExprVisitor &visitor) override { visitor.visitIndexExpr(*this); }
};

struct IndexSet : Expr {
    std::shared_ptr<Expr> callee;
    std::shared_ptr<Expr> index;
    std::shared_ptr<Expr> value;
    IndexSet(std::shared_ptr<Expr> callee, std::shared_ptr<Expr> index, std::shared_ptr<Expr> value)
        : callee(callee), index(index), value(value) {}
    void accept(ExprVisitor &visitor) override { visitor.visitIndexSetExpr(*this); }
};

struct FunctionExpr : Expr {
  std::vector<Token> params;
  std::vector<std::shared_ptr<Stmt>> body;
  FunctionExpr(std::vector<Token> params, std::vector<std::shared_ptr<Stmt>> body)
      : params(params), body(body) {}
  void accept(ExprVisitor &visitor) override { visitor.visitFunctionExpr(*this); }
};
