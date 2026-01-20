#pragma once
#include "Environment.hpp"
#include "Token.hpp"
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

class Interpreter;

struct Callable {
  virtual ~Callable() = default;
  virtual int arity() = 0;
  virtual Value call(Interpreter &interpreter,
                     std::vector<Value> arguments) = 0;
  virtual std::string toString() = 0;
};

struct FunctionCallable : public Callable {
  std::shared_ptr<struct Function> declaration;
  std::shared_ptr<Environment> closure;

  FunctionCallable(std::shared_ptr<struct Function> declaration,
                   std::shared_ptr<Environment> closure)
      : declaration(declaration), closure(closure) {}

  int arity() override;
  Value call(Interpreter &interpreter, std::vector<Value> arguments) override;
  std::string toString() override;
  std::shared_ptr<FunctionCallable> bind(std::shared_ptr<FSKInstance> instance);
};

struct NativeFunction : public Callable {
  int _arity;
  std::function<Value(Interpreter &, std::vector<Value>)> _call;

  NativeFunction(int arity,
                 std::function<Value(Interpreter &, std::vector<Value>)> call)
      : _arity(arity), _call(call) {}

  int arity() override { return _arity; }
  Value call(Interpreter &interpreter, std::vector<Value> arguments) override {
    return _call(interpreter, arguments);
  }
  std::string toString() override { return "<native fn>"; }
};

struct FSKInstance;


struct FSKClass : public Callable,
                  public std::enable_shared_from_this<FSKClass> {
  std::shared_ptr<FSKClass> superclass;
  std::string name;
  std::map<std::string, std::shared_ptr<FunctionCallable>> methods;

  FSKClass(std::string name, std::shared_ptr<FSKClass> superclass,
           std::map<std::string, std::shared_ptr<FunctionCallable>> methods)
      : name(name), superclass(superclass), methods(methods) {}

  int arity() override;
  Value call(Interpreter &interpreter, std::vector<Value> arguments) override;
  std::string toString() override { return name; }

  std::shared_ptr<FunctionCallable> findMethod(std::string name);
};

struct FSKInstance : public std::enable_shared_from_this<FSKInstance> {
  std::shared_ptr<FSKClass> klass;
  std::map<std::string, Value> fields;

  FSKInstance(std::shared_ptr<FSKClass> klass) : klass(klass) {}

  Value get(Token name);
  void set(Token name, Value value);
  std::string toString() { return klass->name + " instance"; }
};