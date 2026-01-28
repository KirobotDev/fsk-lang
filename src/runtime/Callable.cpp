#include "Callable.hpp"
#include "Interpreter.hpp"
#include <iostream>

int FunctionCallable::arity() { return declaration->params.size(); }

int FunctionCallable::minArity() {
  int min = 0;
  for (const auto &param : declaration->params) {
    if (param.defaultValue == nullptr)
      min++;
    else
      break;
  }
  return min;
}

std::string FunctionCallable::toString() {
  return "<fn " + declaration->name.lexeme + ">";
}

// FunctionCallable::call moved to header


int FSKClass::arity() {
  std::shared_ptr<FunctionCallable> initializer = findMethod("init");
  if (initializer == nullptr)
    return 0;
  return initializer->arity();
}

// FSKClass::call moved to header

std::shared_ptr<FunctionCallable> FSKClass::findMethod(std::string name) {
  if (methods.count(name))
    return methods[name];

  if (superclass != nullptr) {
    return superclass->findMethod(name);
  }

  return nullptr;
}

Value FSKInstance::get(Token name) {
  if (fields.count(name.lexeme)) {
    return fields[name.lexeme];
  }

  std::shared_ptr<FunctionCallable> method = klass->findMethod(name.lexeme);
  if (method != nullptr)
    return method->bind(shared_from_this());

  throw std::runtime_error("Undefined property '" + name.lexeme + "'.");
}

void FSKInstance::set(Token name, Value value) { fields[name.lexeme] = value; }
