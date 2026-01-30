#include "Compiler.hpp"
#include <stdexcept>

Compiler::CompiledCode Compiler::compile(std::vector<std::shared_ptr<Stmt>> statements) {
    bytecode.clear();
    constants.clear();
    registers.clear();
    globalNames.clear();
    isInsideFunction = false;
    nextRegister = 0;

    for (const auto &stmt : statements) {
        stmt->accept(*this);
    }

    emit(255); 
    return {bytecode, constants};
}

uint8_t Compiler::reserveRegister() {
    return nextRegister++;
}

uint8_t Compiler::addConstant(nlohmann::json value) {
    constants.push_back(value);
    return (uint8_t)(constants.size() - 1);
}

void Compiler::emit(uint8_t byte) {
    bytecode.push_back(byte);
}

void Compiler::emit(uint8_t b1, uint8_t b2) {
    bytecode.push_back(b1);
    bytecode.push_back(b2);
}

void Compiler::emit(uint8_t b1, uint8_t b2, uint8_t b3) {
    bytecode.push_back(b1);
    bytecode.push_back(b2);
    bytecode.push_back(b3);
}

void Compiler::emit(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4) {
    bytecode.push_back(b1);
    bytecode.push_back(b2);
    bytecode.push_back(b3);
    bytecode.push_back(b4);
}

void Compiler::visitLiteralExpr(Literal &expr) {
    uint8_t reg = reserveRegister();
    nlohmann::json val;

    if (std::holds_alternative<double>(expr.value)) {
        val = std::get<double>(expr.value);
    } else if (std::holds_alternative<std::string>(expr.value)) {
        val = std::get<std::string>(expr.value);
    } else if (std::holds_alternative<bool>(expr.value)) {
        val = std::get<bool>(expr.value);
    } else {
        val = nullptr;
    }

    uint8_t constIdx = addConstant(val);
    emit(0, reg, constIdx); 
    lastRegister = reg;
}

void Compiler::visitBinaryExpr(Binary &expr) {
    auto leftLit = std::dynamic_pointer_cast<Literal>(expr.left);
    auto rightLit = std::dynamic_pointer_cast<Literal>(expr.right);

    if (leftLit && rightLit && std::holds_alternative<double>(leftLit->value) && std::holds_alternative<double>(rightLit->value)) {
        double leftVal = std::get<double>(leftLit->value);
        double rightVal = std::get<double>(rightLit->value);
        double result = 0;
        bool foldable = true;

        switch (expr.op.type) {
            case TokenType::PLUS: result = leftVal + rightVal; break;
            case TokenType::MINUS: result = leftVal - rightVal; break;
            case TokenType::STAR: result = leftVal * rightVal; break;
            case TokenType::SLASH: if (rightVal != 0) result = leftVal / rightVal; else foldable = false; break;
            case TokenType::EQUAL_EQUAL: result = (leftVal == rightVal ? 1.0 : 0.0); break;
            case TokenType::GREATER: result = (leftVal > rightVal ? 1.0 : 0.0); break;
            case TokenType::LESS: result = (leftVal < rightVal ? 1.0 : 0.0); break;
            default: foldable = false; break;
        }

        if (foldable) {
            uint8_t reg = reserveRegister();
            uint8_t constIdx = addConstant(result);
            emit(0, reg, constIdx); 
            lastRegister = reg;
            return;
        }
    }

    expr.left->accept(*this);
    uint8_t leftReg = lastRegister;
    expr.right->accept(*this);
    uint8_t rightReg = lastRegister;

    uint8_t dest = reserveRegister();
    uint8_t op = 0;

    switch (expr.op.type) {
        case TokenType::PLUS: op = 1; break;
        case TokenType::MINUS: op = 2; break;
        case TokenType::STAR: op = 3; break;
        case TokenType::SLASH: op = 4; break;
        case TokenType::EQUAL_EQUAL: op = 8; break;
        case TokenType::GREATER: op = 9; break;
        case TokenType::LESS: op = 10; break;
        default: throw std::runtime_error("Opcode non supporté dans le compilateur VM.");
    }

    emit(op, dest, leftReg, rightReg);
    lastRegister = dest;
}

void Compiler::visitGroupingExpr(Grouping &expr) {
    expr.expression->accept(*this);
}

void Compiler::visitVariableExpr(Variable &expr) {
    if (registers.count(expr.name.lexeme)) {
        lastRegister = registers[expr.name.lexeme];
    } else if (globalNames.count(expr.name.lexeme)) {
        uint8_t dest = reserveRegister();
        uint8_t nameIdx = addConstant(expr.name.lexeme);
        emit(19, dest, nameIdx); 
        lastRegister = dest;
    } else {
        if (!isInsideFunction) {
            globalNames[expr.name.lexeme] = true;
            uint8_t dest = reserveRegister();
            uint8_t nameIdx = addConstant(expr.name.lexeme);
            emit(19, dest, nameIdx);
            lastRegister = dest;
        } else {
            throw std::runtime_error("Variable indéfinie: " + expr.name.lexeme);
        }
    }
}

void Compiler::visitAssignExpr(Assign &expr) {
    expr.value->accept(*this);
    uint8_t srcReg = lastRegister;
    
    if (registers.count(expr.name.lexeme)) {
        uint8_t destReg = registers[expr.name.lexeme];
        emit(11, destReg, srcReg); 
        lastRegister = destReg;
    } else if (globalNames.count(expr.name.lexeme)) {
        uint8_t nameIdx = addConstant(expr.name.lexeme);
        emit(20, nameIdx, srcReg); 
    } else {
        throw std::runtime_error("Assignement à une variable non définie dans la VM.");
    }
}

void Compiler::visitCallExpr(Call &expr) {
    expr.callee->accept(*this);
    uint8_t funcReg = lastRegister;
    
    for (size_t i = 0; i < expr.arguments.size(); i++) {
        expr.arguments[i]->accept(*this);
        emit(11, (uint8_t)(funcReg + 1 + i), lastRegister); 
    }
    
    uint8_t destReg = reserveRegister();
    emit(17, destReg, funcReg, (uint8_t)expr.arguments.size()); 
    lastRegister = destReg;
}

void Compiler::visitFunctionStmt(Function &stmt) {
    globalNames[stmt.name.lexeme] = true;

    emit(6, 0); 
    size_t jumpIdx = bytecode.size() - 1;
    size_t funcStart = bytecode.size();
    
    std::map<std::string, uint8_t> oldRegisters = registers;
    uint8_t oldNextRegister = nextRegister;
    bool oldIsInside = isInsideFunction;
    
    registers.clear();
    isInsideFunction = true;
    nextRegister = 0;
    
    for (size_t i = 0; i < stmt.params.size(); i++) {
        registers[stmt.params[i].name.lexeme] = (uint8_t)i;
    }
    nextRegister = (uint8_t)stmt.params.size();
    
    for (auto &s : stmt.body) {
        s->accept(*this);
    }
    
    uint8_t nilReg = reserveRegister();
    uint8_t nilIdx = addConstant(nullptr);
    emit(0, nilReg, nilIdx); 
    emit(18, nilReg); 
    
    bytecode[jumpIdx] = (uint8_t)bytecode.size();
    
    registers = oldRegisters;
    nextRegister = oldNextRegister;
    isInsideFunction = oldIsInside;
    
    nlohmann::json func;
    func["bytecode_idx"] = funcStart;
    func["arity"] = stmt.params.size();
    func["is_func"] = true;
    
    uint8_t constIdx = addConstant(func);
    uint8_t reg = reserveRegister();
    emit(0, reg, constIdx); 
    
    uint8_t nameIdx = addConstant(stmt.name.lexeme);
    emit(20, nameIdx, reg); 
}

void Compiler::visitReturnStmt(Return &stmt) {
    if (stmt.value != nullptr) {
        stmt.value->accept(*this);
        emit(18, lastRegister); 
    } else {
        uint8_t reg = reserveRegister();
        uint8_t constIdx = addConstant(nullptr);
        emit(0, reg, constIdx);
        emit(18, reg);
    }
}

void Compiler::visitObjectExpr(ObjectExpr &expr) {
    uint8_t objReg = reserveRegister();
    emit(12, objReg); 
    
    for (auto const& [key, value] : expr.fields) {
        value->accept(*this);
        uint8_t valReg = lastRegister;
        uint8_t keyIdx = addConstant(key);
        emit(13, objReg, keyIdx, valReg); 
    }
    lastRegister = objReg;
}

void Compiler::visitArrayExpr(Array &expr) {
    uint8_t arrReg = reserveRegister();
    emit(15, arrReg); 
    
    for (auto &element : expr.elements) {
        element.expr->accept(*this);
        uint8_t valReg = lastRegister;
        emit(16, arrReg, valReg); 
    }
    lastRegister = arrReg;
}

void Compiler::visitGetExpr(Get &expr) {
    expr.object->accept(*this);
    uint8_t objReg = lastRegister;
    uint8_t keyIdx = addConstant(expr.name.lexeme);
    uint8_t destReg = reserveRegister();
    emit(14, destReg, objReg, keyIdx); 
    lastRegister = destReg;
}

void Compiler::visitSetExpr(Set &expr) {
    expr.object->accept(*this);
    uint8_t objReg = lastRegister;
    expr.value->accept(*this);
    uint8_t valReg = lastRegister;
    uint8_t keyIdx = addConstant(expr.name.lexeme);
    emit(13, objReg, keyIdx, valReg); // SetProp
}

void Compiler::visitIndexExpr(IndexExpr &expr) {
    expr.callee->accept(*this);
    uint8_t objReg = lastRegister;
    expr.index->accept(*this);
}

void Compiler::visitIndexSetExpr(IndexSet &expr) {
    expr.callee->accept(*this);
    uint8_t objReg = lastRegister;
    expr.index->accept(*this);
    uint8_t idxReg = lastRegister;
    expr.value->accept(*this);
    uint8_t valReg = lastRegister;
}

void Compiler::visitPrintStmt(Print &stmt) {
    stmt.expression->accept(*this);
    emit(5, lastRegister); 
}

void Compiler::visitExpressionStmt(Expression &stmt) {
    stmt.expression->accept(*this);
}

void Compiler::visitLetStmt(Let &stmt) {
    uint8_t varReg = 0;
    std::string name = "";
    bool isGlobal = !isInsideFunction;

    if (auto var = std::dynamic_pointer_cast<Variable>(stmt.pattern)) {
        name = var->name.lexeme;
        if (isGlobal) {
            globalNames[name] = true;
            // No local register for globals
            varReg = reserveRegister(); 
        } else {
            if (!registers.count(name)) {
                registers[name] = reserveRegister();
            }
            varReg = registers[name];
        }
    }

    if (stmt.initializer != nullptr) {
        stmt.initializer->accept(*this);
        emit(11, varReg, lastRegister); // Move
        if (isGlobal && name != "") {
            uint8_t nameIdx = addConstant(name);
            emit(20, nameIdx, varReg); // StoreGlobal
        }
    }
}

void Compiler::visitBlockStmt(Block &stmt) {
    for (const auto &s : stmt.statements) {
        s->accept(*this);
    }
}

void Compiler::visitIfStmt(If &stmt) {
    stmt.condition->accept(*this);
    uint8_t condReg = lastRegister;
    emit(7, condReg, 0); 
    size_t jumpIdx = bytecode.size() - 1;
    stmt.thenBranch->accept(*this);
    if (stmt.elseBranch != nullptr) {
        emit(6, 0); 
        size_t elseJumpIdx = bytecode.size() - 1;
        bytecode[jumpIdx] = (uint8_t)bytecode.size();
        stmt.elseBranch->accept(*this);
        bytecode[elseJumpIdx] = (uint8_t)bytecode.size();
    } else {
        bytecode[jumpIdx] = (uint8_t)bytecode.size();
    }
}

void Compiler::visitWhileStmt(While &stmt) {
    size_t loopStart = bytecode.size();
    stmt.condition->accept(*this);
    uint8_t condReg = lastRegister;
    emit(7, condReg, 0); 
    size_t jumpIdx = bytecode.size() - 1;
    stmt.body->accept(*this);
    emit(6, (uint8_t)loopStart); 
    bytecode[jumpIdx] = (uint8_t)bytecode.size();
}

void Compiler::visitLogicalExpr(Logical &expr) {}
void Compiler::visitUnaryExpr(Unary &expr) {}
void Compiler::visitThisExpr(This &expr) {}
void Compiler::visitSuperExpr(Super &expr) {}
void Compiler::visitFunctionExpr(FunctionExpr &expr) {}
void Compiler::visitTemplateLiteralExpr(TemplateLiteral &expr) {}
void Compiler::visitArrowFunctionExpr(ArrowFunction &expr) {}
void Compiler::visitAwaitExpr(Await &expr) {}
void Compiler::visitConstStmt(Const &stmt) {}
void Compiler::visitClassStmt(Class &stmt) {}
void Compiler::visitForStmt(For &stmt) {}
void Compiler::visitTryStmt(Try &stmt) {}
void Compiler::visitThrowStmt(Throw &stmt) {}
void Compiler::visitImportStmt(Import &stmt) {}
void Compiler::visitMatchStmt(Match &stmt) {}
