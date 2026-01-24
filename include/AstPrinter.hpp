#pragma once
#include "Expr.hpp"
#include "Stmt.hpp"
#include <iostream>
#include <string>
#include <vector>

class AstPrinter : public ExprVisitor, public StmtVisitor {
public:
  void print(std::vector<std::shared_ptr<Stmt>> statements) {
    for (const auto &stmt : statements) {
      if (stmt)
        stmt->accept(*this);
    }
  }

  void visitExpressionStmt(Expression &stmt) override {
    std::cout << "(expr ";
    stmt.expression->accept(*this);
    std::cout << ")" << std::endl;
  }

  void visitPrintStmt(Print &stmt) override {
    std::cout << "(print ";
    stmt.expression->accept(*this);
    std::cout << ")" << std::endl;
  }

  void visitLetStmt(Let &stmt) override {
    std::cout << "(let ";
    stmt.pattern->accept(*this);
    if (stmt.initializer) {
      std::cout << " = ";
      stmt.initializer->accept(*this);
    }
    std::cout << ")" << std::endl;
  }

  void visitConstStmt(Const &stmt) override {
    std::cout << "(const ";
    stmt.pattern->accept(*this);
    std::cout << " = ";
    stmt.initializer->accept(*this);
    std::cout << ")" << std::endl;
  }

  void visitBlockStmt(Block &stmt) override {
    std::cout << "(block " << std::endl;
    for (const auto &s : stmt.statements) {
      if (s)
        s->accept(*this);
    }
    std::cout << ")" << std::endl;
  }

  void visitIfStmt(If &stmt) override {
    std::cout << "(if ";
    stmt.condition->accept(*this);
    std::cout << " then ";
    stmt.thenBranch->accept(*this);
    if (stmt.elseBranch) {
      std::cout << " else ";
      stmt.elseBranch->accept(*this);
    }
    std::cout << ")" << std::endl;
  }

  void visitWhileStmt(While &stmt) override {
    std::cout << "(while ";
    stmt.condition->accept(*this);
    std::cout << " do ";
    stmt.body->accept(*this);
    std::cout << ")" << std::endl;
  }

  void visitFunctionStmt(Function &stmt) override {
    std::cout << "(" << (stmt.isAsync ? "async " : "") << "fn "
              << stmt.name.lexeme << "(";
    for (size_t i = 0; i < stmt.params.size(); ++i) {
      std::cout << stmt.params[i].lexeme
                << (i < stmt.params.size() - 1 ? ", " : "");
    }
    std::cout << ") block)" << std::endl;
  }

  void visitReturnStmt(Return &stmt) override {
    std::cout << "(return";
    if (stmt.value) {
      std::cout << " ";
      stmt.value->accept(*this);
    }
    std::cout << ")" << std::endl;
  }

  void visitClassStmt(Class &stmt) override {
    std::cout << "(class " << stmt.name.lexeme << " block)" << std::endl;
  }

  void visitTryStmt(Try &stmt) override {
    std::cout << "(try block)" << std::endl;
  }

  void visitThrowStmt(Throw &stmt) override {
    std::cout << "(throw ";
    stmt.value->accept(*this);
    std::cout << ")" << std::endl;
  }

  void visitMatchStmt(Match &stmt) override {
    std::cout << "(match ";
    stmt.expression->accept(*this);
    std::cout << " arms)" << std::endl;
  }

  void visitImportStmt(Import &stmt) override {
    std::cout << "(import ";
    stmt.file->accept(*this);
    std::cout << ")" << std::endl;
  }

  void visitForStmt(For &stmt) override {
    std::cout << "(for)" << std::endl;
  }

  void visitBinaryExpr(Binary &expr) override {
    std::cout << "(" << expr.op.lexeme << " ";
    expr.left->accept(*this);
    std::cout << " ";
    expr.right->accept(*this);
    std::cout << ")";
  }

  void visitGroupingExpr(Grouping &expr) override {
    std::cout << "(group ";
    expr.expression->accept(*this);
    std::cout << ")";
  }

  void visitLiteralExpr(Literal &expr) override {
    if (std::holds_alternative<double>(expr.value)) {
      std::cout << std::get<double>(expr.value);
    } else if (std::holds_alternative<std::string>(expr.value)) {
      std::cout << "\"" << std::get<std::string>(expr.value) << "\"";
    } else if (std::holds_alternative<bool>(expr.value)) {
      std::cout << (std::get<bool>(expr.value) ? "true" : "false");
    } else {
      std::cout << "nil";
    }
  }

  void visitUnaryExpr(Unary &expr) override {
    std::cout << "(" << expr.op.lexeme << " ";
    expr.right->accept(*this);
    std::cout << ")";
  }

  void visitVariableExpr(Variable &expr) override {
    std::cout << expr.name.lexeme;
  }

  void visitAssignExpr(Assign &expr) override {
    std::cout << "(assign " << expr.name.lexeme << " = ";
    expr.value->accept(*this);
    std::cout << ")";
  }

  void visitLogicalExpr(Logical &expr) override {
    std::cout << "(" << expr.op.lexeme << " ";
    expr.left->accept(*this);
    std::cout << " ";
    expr.right->accept(*this);
    std::cout << ")";
  }

  void visitCallExpr(Call &expr) override {
    std::cout << "(call ";
    expr.callee->accept(*this);
    std::cout << " args: ";
    for (const auto &arg : expr.arguments) {
      arg->accept(*this);
      std::cout << " ";
    }
    std::cout << ")";
  }

  void visitGetExpr(Get &expr) override {
    expr.object->accept(*this);
    std::cout << "." << expr.name.lexeme;
  }

  void visitSetExpr(Set &expr) override {
    expr.object->accept(*this);
    std::cout << "." << expr.name.lexeme << " = ";
    expr.value->accept(*this);
  }

  void visitThisExpr(This &expr) override { std::cout << "this"; }

  void visitSuperExpr(Super &expr) override {
    std::cout << "super." << expr.method.lexeme;
  }

  void visitAwaitExpr(Await &expr) override {
    std::cout << "(await ";
    expr.expression->accept(*this);
    std::cout << ")";
  }

  void visitFunctionExpr(FunctionExpr &expr) override {
    std::cout << "(fn args: ";
    for (const auto &param : expr.params) {
      std::cout << param.lexeme << " ";
    }
    std::cout << " block)";
  }

  void visitTemplateLiteralExpr(TemplateLiteral &expr) override {
    std::cout << "(template-literal)";
  }

  void visitArrayExpr(Array &expr) override {
    std::cout << "[";
    for (size_t i = 0; i < expr.elements.size(); i++) {
        expr.elements[i]->accept(*this);
        if (i < expr.elements.size() - 1) std::cout << ", ";
    }
    std::cout << "]";
  }

  void visitIndexExpr(IndexExpr &expr) override {
    expr.callee->accept(*this);
    std::cout << "[";
    expr.index->accept(*this);
    std::cout << "]";
  }

  void visitIndexSetExpr(IndexSet &expr) override {
    expr.callee->accept(*this);
    std::cout << "[";
    expr.index->accept(*this);
    std::cout << "] = ";
    expr.value->accept(*this);
  }
};
