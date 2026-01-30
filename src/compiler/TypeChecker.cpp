#include "TypeChecker.hpp"
#include <stdexcept>
#include <iostream>

void TypeChecker::check(std::vector<std::shared_ptr<Stmt>> &statements, bool keepState) {
    if (!keepState) {
        scopes.clear();
        beginScope(); 
    }
    for (auto &stmt : statements) {
        stmt->accept(*this);
    }
    if (!keepState) {
        endScope();
    }
}

void TypeChecker::beginScope() {
    scopes.push_back({});
}

void TypeChecker::endScope() {
    scopes.pop_back();
}

void TypeChecker::define(const std::string &name, const std::string &type) {
    if (scopes.empty()) scopes.push_back({});
    scopes.back()[name] = type;
}

std::string TypeChecker::resolve(const std::string &name) {
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
        if (it->count(name)) return (*it)[name];
    }
    return "any";
}

bool TypeChecker::isCompatible(const std::string &expected, const std::string &actual) {
    if (expected == "" || expected == "any" || actual == "any") return true;
    return expected == actual;
}

std::string TypeChecker::expressionType(const std::shared_ptr<Expr> &expr) {
    expr->accept(*this);
    return lastType;
}

void TypeChecker::error(Token token, const std::string &message) {
    std::cerr << "[Type Error] " << message << " at line " << token.line << std::endl;
    throw std::runtime_error(message);
}

void TypeChecker::visitLiteralExpr(Literal &expr) {
    if (std::holds_alternative<double>(expr.value)) lastType = "number";
    else if (std::holds_alternative<std::string>(expr.value)) lastType = "string";
    else if (std::holds_alternative<bool>(expr.value)) lastType = "bool";
    else lastType = "nil";
}

void TypeChecker::visitBinaryExpr(Binary &expr) {
    std::string left = expressionType(expr.left);
    std::string right = expressionType(expr.right);

    if (expr.op.type == TokenType::PLUS) {
        if (left == "string" || right == "string") lastType = "string";
        else lastType = "number";
    } else if (expr.op.type == TokenType::MINUS || expr.op.type == TokenType::STAR || expr.op.type == TokenType::SLASH) {
        lastType = "number";
    } else {
        lastType = "bool";
    }
}

void TypeChecker::visitGroupingExpr(Grouping &expr) {
    lastType = expressionType(expr.expression);
}

void TypeChecker::visitUnaryExpr(Unary &expr) {
    lastType = expressionType(expr.right);
}

void TypeChecker::visitVariableExpr(Variable &expr) {
    lastType = resolve(expr.name.lexeme);
}

void TypeChecker::visitAssignExpr(Assign &expr) {
    std::string varType = resolve(expr.name.lexeme);
    std::string valType = expressionType(expr.value);
    
    if (!isCompatible(varType, valType)) {
        error(expr.name, "Cannot assign type '" + valType + "' to variable '" + expr.name.lexeme + "' of type '" + varType + "'");
    }
    lastType = valType;
}

void TypeChecker::visitLogicalExpr(Logical &expr) {
    expressionType(expr.left);
    expressionType(expr.right);
    lastType = "bool";
}

void TypeChecker::visitLetStmt(Let &stmt) {
    std::string initType = "any";
    if (stmt.initializer) initType = expressionType(stmt.initializer);

    if (stmt.typeHint != "" && !isCompatible(stmt.typeHint, initType)) {
        Token t(TokenType::IDENTIFIER, "", std::monostate{}, 0); // Dummy token for error
        if (auto var = std::dynamic_pointer_cast<Variable>(stmt.pattern)) t = var->name;
        error(t, "Initializer type '" + initType + "' does not match type hint '" + stmt.typeHint + "'");
    }

    std::string finalType = (stmt.typeHint != "") ? stmt.typeHint : initType;
    
    if (auto var = std::dynamic_pointer_cast<Variable>(stmt.pattern)) {
        define(var->name.lexeme, finalType);
    }
}

void TypeChecker::visitFunctionStmt(Function &stmt) {
    define(stmt.name.lexeme, "function");
    beginScope();
    std::string oldReturn = currentReturnType;
    currentReturnType = stmt.returnType;

    for (auto &param : stmt.params) {
        define(param.name.lexeme, param.typeHint != "" ? param.typeHint : "any");
    }

    for (auto &s : stmt.body) {
        s->accept(*this);
    }

    endScope();
    currentReturnType = oldReturn;
}

void TypeChecker::visitReturnStmt(Return &stmt) {
    std::string valType = "nil";
    if (stmt.value) valType = expressionType(stmt.value);

    if (!isCompatible(currentReturnType, valType)) {
        error(stmt.keyword, "Result type '" + valType + "' does not match function return type '" + currentReturnType + "'");
    }
}

void TypeChecker::visitExpressionStmt(Expression &stmt) { stmt.expression->accept(*this); }
void TypeChecker::visitPrintStmt(Print &stmt) { stmt.expression->accept(*this); }
void TypeChecker::visitBlockStmt(Block &stmt) {
    beginScope();
    for (auto &s : stmt.statements) s->accept(*this);
    endScope();
}

void TypeChecker::visitIfStmt(If &stmt) {
    expressionType(stmt.condition);
    stmt.thenBranch->accept(*this);
    if (stmt.elseBranch) stmt.elseBranch->accept(*this);
}

void TypeChecker::visitWhileStmt(While &stmt) {
    expressionType(stmt.condition);
    stmt.body->accept(*this);
}

void TypeChecker::visitCallExpr(Call &expr) { 
    expr.callee->accept(*this);
    for (auto &arg : expr.arguments) arg->accept(*this);
    lastType = "any";
}
void TypeChecker::visitGetExpr(Get &expr) { expr.object->accept(*this); lastType = "any"; }
void TypeChecker::visitSetExpr(Set &expr) { expr.object->accept(*this); expressionType(expr.value); lastType = "any"; }
void TypeChecker::visitThisExpr(This &expr) { lastType = "any"; }
void TypeChecker::visitSuperExpr(Super &expr) { lastType = "any"; }
void TypeChecker::visitObjectExpr(ObjectExpr &expr) { for (auto const& [k, v] : expr.fields) v->accept(*this); lastType = "object"; }
void TypeChecker::visitArrayExpr(Array &expr) { for (auto &e : expr.elements) e.expr->accept(*this); lastType = "array"; }
void TypeChecker::visitIndexExpr(IndexExpr &expr) { expr.callee->accept(*this); expr.index->accept(*this); lastType = "any"; }
void TypeChecker::visitIndexSetExpr(IndexSet &expr) { expr.callee->accept(*this); expr.index->accept(*this); expressionType(expr.value); }
void TypeChecker::visitFunctionExpr(FunctionExpr &expr) { lastType = "function"; }
void TypeChecker::visitTemplateLiteralExpr(TemplateLiteral &expr) { lastType = "string"; }
void TypeChecker::visitArrowFunctionExpr(ArrowFunction &expr) { lastType = "function"; }
void TypeChecker::visitAwaitExpr(Await &expr) { expressionType(expr.expression); lastType = "any"; }
void TypeChecker::visitConstStmt(Const &stmt) { expressionType(stmt.initializer); if (auto v = std::dynamic_pointer_cast<Variable>(stmt.pattern)) define(v->name.lexeme, "any"); }
void TypeChecker::visitClassStmt(Class &stmt) { define(stmt.name.lexeme, "class"); }
void TypeChecker::visitForStmt(For &stmt) { stmt.body->accept(*this); }
void TypeChecker::visitTryStmt(Try &stmt) { stmt.tryBranch->accept(*this); beginScope(); define(stmt.catchName.lexeme, "any"); stmt.catchBranch->accept(*this); endScope(); }
void TypeChecker::visitThrowStmt(Throw &stmt) { expressionType(stmt.value); }
void TypeChecker::visitImportStmt(Import &stmt) {}
void TypeChecker::visitMatchStmt(Match &stmt) { expressionType(stmt.expression); }
