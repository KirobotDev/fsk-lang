#pragma once
#include "Expr.hpp"
#include <memory>
#include <vector>

struct StmtVisitor;

struct Stmt {
  virtual ~Stmt() = default;
  virtual void accept(StmtVisitor &visitor) = 0;
};

struct Expression;
struct Print;
struct Let;
struct Const;
struct Block;
struct If;
struct While;
struct Function;
struct Return;
struct Class;
struct Async;
struct Try;
struct Throw;
struct For;
struct Import;
struct Match;

struct StmtVisitor {
  virtual void visitExpressionStmt(Expression &stmt) = 0;
  virtual void visitPrintStmt(Print &stmt) = 0;
  virtual void visitLetStmt(Let &stmt) = 0;
  virtual void visitConstStmt(Const &stmt) = 0;
  virtual void visitBlockStmt(Block &stmt) = 0;
  virtual void visitIfStmt(If &stmt) = 0;
  virtual void visitWhileStmt(While &stmt) = 0;
  virtual void visitFunctionStmt(Function &stmt) = 0;
  virtual void visitReturnStmt(Return &stmt) = 0;
  virtual void visitClassStmt(Class &stmt) = 0;
  virtual void visitForStmt(For &stmt) = 0;
  virtual void visitTryStmt(Try &stmt) = 0;
  virtual void visitThrowStmt(Throw &stmt) = 0;
  virtual void visitImportStmt(Import &stmt) = 0;
  virtual void visitMatchStmt(Match &stmt) = 0;
};

struct Expression : Stmt {
  std::shared_ptr<Expr> expression;
  Expression(std::shared_ptr<Expr> expression) : expression(expression) {}
  void accept(StmtVisitor &visitor) override {
    visitor.visitExpressionStmt(*this);
  }
};

struct Print : Stmt {
  std::shared_ptr<Expr> expression;
  Print(std::shared_ptr<Expr> expression) : expression(expression) {}
  void accept(StmtVisitor &visitor) override { visitor.visitPrintStmt(*this); }
};

struct Let : Stmt {
  std::shared_ptr<Expr> pattern;
  std::shared_ptr<Expr> initializer;
  std::string typeHint; 

  Let(std::shared_ptr<Expr> pattern, std::shared_ptr<Expr> initializer, std::string typeHint = "")
      : pattern(pattern), initializer(initializer), typeHint(typeHint) {}
  void accept(StmtVisitor &visitor) override { visitor.visitLetStmt(*this); }
};

struct Const : Stmt {
  std::shared_ptr<Expr> pattern;
  std::shared_ptr<Expr> initializer;
  Const(std::shared_ptr<Expr> pattern, std::shared_ptr<Expr> initializer)
      : pattern(pattern), initializer(initializer) {}
  void accept(StmtVisitor &visitor) override { visitor.visitConstStmt(*this); }
};

struct Block : Stmt {
  std::vector<std::shared_ptr<Stmt>> statements;
  Block(std::vector<std::shared_ptr<Stmt>> statements)
      : statements(statements) {}
  void accept(StmtVisitor &visitor) override { visitor.visitBlockStmt(*this); }
};

struct If : Stmt {
  std::shared_ptr<Expr> condition;
  std::shared_ptr<Stmt> thenBranch;
  std::shared_ptr<Stmt> elseBranch;
  If(std::shared_ptr<Expr> condition, std::shared_ptr<Stmt> thenBranch,
     std::shared_ptr<Stmt> elseBranch)
      : condition(condition), thenBranch(thenBranch), elseBranch(elseBranch) {}
  void accept(StmtVisitor &visitor) override { visitor.visitIfStmt(*this); }
};

struct While : Stmt {
  std::shared_ptr<Expr> condition;
  std::shared_ptr<Stmt> body;
  While(std::shared_ptr<Expr> condition, std::shared_ptr<Stmt> body)
      : condition(condition), body(body) {}
  void accept(StmtVisitor &visitor) override { visitor.visitWhileStmt(*this); }
};

struct Function : Stmt {
  Token name;
  std::vector<Token> params;
  std::vector<std::shared_ptr<Stmt>> body;
  bool isAsync;
  std::string returnType;

  Function(Token name, std::vector<Token> params,
           std::vector<std::shared_ptr<Stmt>> body, bool isAsync,
           std::string returnType = "")
      : name(name), params(params), body(body), isAsync(isAsync),
        returnType(returnType) {}
  void accept(StmtVisitor &visitor) override {
    visitor.visitFunctionStmt(*this);
  }
};

struct Return : Stmt {
  Token keyword;
  std::shared_ptr<Expr> value;
  Return(Token keyword, std::shared_ptr<Expr> value)
      : keyword(keyword), value(value) {}
  void accept(StmtVisitor &visitor) override { visitor.visitReturnStmt(*this); }
};

struct Class : Stmt {
  Token name;
  std::shared_ptr<Variable> superclass;
  std::vector<std::shared_ptr<Function>> methods;
  Class(Token name, std::shared_ptr<Variable> superclass,
        std::vector<std::shared_ptr<Function>> methods)
      : name(name), superclass(superclass), methods(methods) {}
  void accept(StmtVisitor &visitor) override { visitor.visitClassStmt(*this); }
};

struct Try : Stmt {
  std::shared_ptr<Stmt> tryBranch;
  Token catchName;
  std::shared_ptr<Stmt> catchBranch;

  Try(std::shared_ptr<Stmt> tryBranch, Token catchName,
      std::shared_ptr<Stmt> catchBranch)
      : tryBranch(tryBranch), catchName(catchName), catchBranch(catchBranch) {}
  void accept(StmtVisitor &visitor) override { visitor.visitTryStmt(*this); }
};

struct Throw : Stmt {
  Token keyword;
  std::shared_ptr<Expr> value;

  Throw(Token keyword, std::shared_ptr<Expr> value)
      : keyword(keyword), value(value) {}
  void accept(StmtVisitor &visitor) override { visitor.visitThrowStmt(*this); }
};

struct For : Stmt {
  std::shared_ptr<Stmt> initializer;
  std::shared_ptr<Expr> condition;
  std::shared_ptr<Expr> increment;
  std::shared_ptr<Stmt> body;

  For(std::shared_ptr<Stmt> initializer, std::shared_ptr<Expr> condition,
      std::shared_ptr<Expr> increment, std::shared_ptr<Stmt> body)
      : initializer(initializer), condition(condition), increment(increment),
        body(body) {}
  void accept(StmtVisitor &visitor) override { visitor.visitForStmt(*this); }
};

struct Import : Stmt {
  Token keyword;
  std::shared_ptr<Expr> file;

  Import(Token keyword, std::shared_ptr<Expr> file)
      : keyword(keyword), file(file) {}
  void accept(StmtVisitor &visitor) override { visitor.visitImportStmt(*this); }
};

struct Match : Stmt {
  std::shared_ptr<Expr> expression;
  std::vector<std::pair<std::shared_ptr<Expr>, std::shared_ptr<Stmt>>> arms;

  Match(std::shared_ptr<Expr> expression,
        std::vector<std::pair<std::shared_ptr<Expr>, std::shared_ptr<Stmt>>> arms)
      : expression(expression), arms(arms) {}
  void accept(StmtVisitor &visitor) override { visitor.visitMatchStmt(*this); }
};
