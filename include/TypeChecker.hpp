#pragma once
#include "Expr.hpp"
#include "Stmt.hpp"
#include <map>
#include <string>
#include <vector>
#include <memory>

class TypeChecker : public ExprVisitor, public StmtVisitor {
public:
    TypeChecker() { beginScope(); } // Init global scope
    void check(std::vector<std::shared_ptr<Stmt>> &statements, bool keepState = false);

    void visitBinaryExpr(Binary &expr) override;
    void visitGroupingExpr(Grouping &expr) override;
    void visitLiteralExpr(Literal &expr) override;
    void visitUnaryExpr(Unary &expr) override;
    void visitVariableExpr(Variable &expr) override;
    void visitAssignExpr(Assign &expr) override;
    void visitLogicalExpr(Logical &expr) override;
    void visitCallExpr(Call &expr) override;
    void visitGetExpr(Get &expr) override;
    void visitSetExpr(Set &expr) override;
    void visitThisExpr(This &expr) override;
    void visitSuperExpr(Super &expr) override;
    void visitObjectExpr(ObjectExpr &expr) override;
    void visitArrayExpr(Array &expr) override;
    void visitIndexExpr(IndexExpr &expr) override;
    void visitIndexSetExpr(IndexSet &expr) override;
    void visitFunctionExpr(FunctionExpr &expr) override;
    void visitTemplateLiteralExpr(TemplateLiteral &expr) override;
    void visitArrowFunctionExpr(ArrowFunction &expr) override;
    void visitAwaitExpr(Await &expr) override;

    void visitExpressionStmt(Expression &stmt) override;
    void visitPrintStmt(Print &stmt) override;
    void visitLetStmt(Let &stmt) override;
    void visitConstStmt(Const &stmt) override;
    void visitBlockStmt(Block &stmt) override;
    void visitIfStmt(If &stmt) override;
    void visitWhileStmt(While &stmt) override;
    void visitFunctionStmt(Function &stmt) override;
    void visitReturnStmt(Return &stmt) override;
    void visitClassStmt(Class &stmt) override;
    void visitForStmt(For &stmt) override;
    void visitTryStmt(Try &stmt) override;
    void visitThrowStmt(Throw &stmt) override;
    void visitImportStmt(Import &stmt) override;
    void visitMatchStmt(Match &stmt) override;

private:
    std::vector<std::map<std::string, std::string>> scopes;
    std::string currentReturnType;
    std::string lastType;

    void beginScope();
    void endScope();
    void define(const std::string &name, const std::string &type);
    std::string resolve(const std::string &name);
    
    bool isCompatible(const std::string &expected, const std::string &actual);
    std::string expressionType(const std::shared_ptr<Expr> &expr);
    void error(Token token, const std::string &message);
};
