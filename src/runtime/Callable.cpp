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



int FSKClass::arity() {
  std::shared_ptr<Callable> initializer = findMethod("init");
  if (initializer == nullptr)
    return 0;
  return initializer->arity();
}


std::shared_ptr<Callable> FSKClass::findMethod(std::string name) {
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

  std::shared_ptr<Callable> method = klass->findMethod(name.lexeme);
  if (method != nullptr)
    return method->bind(shared_from_this());

  throw std::runtime_error("Undefined property '" + name.lexeme + "'.");
}

void FSKInstance::set(Token name, Value value) { fields[name.lexeme] = value; }

NativeFunction::NativeFunction(int arity, NativeCallback call)
    : _arity(arity), _call(call), boundThis(nullptr) {}

NativeFunction::NativeFunction(int arity,
                               NativeMethodCallback callMethod,
                               std::shared_ptr<FSKInstance> boundThis)
    : _arity(arity), _callMethod(callMethod), boundThis(boundThis) {}

int NativeFunction::arity() { return _arity; }

Value NativeFunction::call(Interpreter &interpreter, std::vector<Value> arguments) {
  if (_callMethod) {
    return _callMethod(interpreter, arguments, boundThis);
  }
  return _call(interpreter, arguments);
}

std::string NativeFunction::toString() { return "<native fn>"; }

std::shared_ptr<Callable> NativeFunction::bind(std::shared_ptr<FSKInstance> instance) {
  if (_callMethod) {
    return std::make_shared<NativeFunction>(_arity, _callMethod, instance);
  }
  auto nf = new NativeFunction(_arity, _call);
  nf->boundThis = instance;
  return std::shared_ptr<NativeFunction>(nf);
}

Value FSKClass::call(Interpreter &interpreter, std::vector<Value> arguments) {
  auto instance = std::make_shared<FSKInstance>(shared_from_this());
  std::shared_ptr<Callable> initializer = findMethod("init");
  if (initializer != nullptr) {
    initializer->bind(instance)->call(interpreter, arguments);
  }
  return Value(instance);
}

Value FunctionCallable::call(Interpreter &interpreter, std::vector<Value> arguments) {
    auto environment = std::make_shared<Environment>(closure);
    for (size_t i = 0; i < declaration->params.size(); i++) {
        if (i < arguments.size())
            environment->define(declaration->params[i].name.lexeme, arguments[i]);
    }
    
    try {
        interpreter.executeBlock(declaration->body, environment);
    } catch (const ReturnException &returnValue) {
        return returnValue.value;
    }
    return Value(std::monostate{});
}