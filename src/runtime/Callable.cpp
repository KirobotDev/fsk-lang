#include "Callable.hpp"
#include "Interpreter.hpp"

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

Value FunctionCallable::call(Interpreter &interpreter,
                             std::vector<Value> arguments) {
  std::shared_ptr<Environment> environment =
      std::make_shared<Environment>(closure);
  
  for (size_t i = 0; i < declaration->params.size(); ++i) {
    Value val;
    if (i < arguments.size()) {
      val = arguments[i];
    } else if (declaration->params[i].defaultValue != nullptr) {
      val = interpreter.evaluate(declaration->params[i].defaultValue);
    } else {
      val = std::monostate{};
    }
    environment->define(declaration->params[i].name.lexeme, val);
  }

  try {
    interpreter.executeBlock(declaration->body, environment);
  } catch (const Value &returnValue) {
    return returnValue;
  }

  return std::monostate{};
}

std::shared_ptr<FunctionCallable>
FunctionCallable::bind(std::shared_ptr<FSKInstance> instance) {
  std::shared_ptr<Environment> environment =
      std::make_shared<Environment>(closure);
  environment->define("this", instance);
  return std::make_shared<FunctionCallable>(declaration, environment);
}

int FSKClass::arity() {
  std::shared_ptr<FunctionCallable> initializer = findMethod("init");
  if (initializer == nullptr)
    return 0;
  return initializer->arity();
}

Value FSKClass::call(Interpreter &interpreter, std::vector<Value> arguments) {
  auto instance = std::make_shared<FSKInstance>(shared_from_this());

  std::shared_ptr<FunctionCallable> initializer = findMethod("init");
  if (initializer != nullptr) {
    initializer->bind(instance)->call(interpreter, arguments);
  }

  return instance;
}

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
