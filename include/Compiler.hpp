#pragma once
#include "Token.hpp"
#include "Expr.hpp"
#include "Stmt.hpp"
#include "json.hpp"
#include <vector>
#include <map>
#include <string>
#include <memory>
#include <variant>

class Compiler : public ExprVisitor, public StmtVisitor {
public:
    struct CompiledCode {
        std::vector<uint8_t> bytecode;
        std::vector<nlohmann::json> constants;
    };

    CompiledCode compile(std::vector<std::shared_ptr<Stmt>> statements);

    void visitBinaryExpr(Binary &expr) override;
    void visitGroupingExpr(Grouping &expr) override;
    void visitLiteralExpr(Literal &expr) override;
    void visitLogicalExpr(Logical &expr) override;
    void visitVariableExpr(Variable &expr) override;
    void visitAssignExpr(Assign &expr) override;
    void visitCallExpr(Call &expr) override;
    void visitGetExpr(Get &expr) override;
    void visitSetExpr(Set &expr) override;
    void visitThisExpr(This &expr) override;
    void visitSuperExpr(Super &expr) override;
    void visitObjectExpr(ObjectExpr &expr) override;
    void visitArrayExpr(Array &expr) override;
    void visitIndexExpr(IndexExpr &expr) override;
    void visitIndexSetExpr(IndexSet &expr) override;
    void visitUnaryExpr(Unary &expr) override;
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
    std::vector<uint8_t> bytecode;
    std::vector<nlohmann::json> constants;
    std::map<std::string, uint8_t> registers;
    std::map<std::string, bool> globalNames;
    bool isInsideFunction = false;
    uint8_t nextRegister = 0;
    uint8_t lastRegister = 0;

    void emit(uint8_t byte);
    void emit(uint8_t b1, uint8_t b2);
    void emit(uint8_t b1, uint8_t b2, uint8_t b3);
    void emit(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4);
    
    uint8_t addConstant(nlohmann::json value);
    uint8_t reserveRegister();
};
