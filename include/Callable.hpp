#pragma once
#include "Environment.hpp"
#include "Token.hpp"
#include "Stmt.hpp"
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>
#include <iostream>

class Interpreter;
struct FSKInstance;

struct Callable {
  virtual ~Callable() = default;
  virtual int arity() = 0;
  virtual int minArity() { return arity(); }
  virtual int maxArity() { return arity(); }
  virtual Value call(Interpreter &interpreter,
                     std::vector<Value> arguments) = 0;
  virtual std::string toString() = 0;
  virtual std::shared_ptr<Callable> bind(std::shared_ptr<struct FSKInstance> instance) { return nullptr; }
};

struct FunctionCallable : public Callable {
  std::shared_ptr<struct Function> declaration;
  std::shared_ptr<Environment> closure;

  FunctionCallable(std::shared_ptr<struct Function> declaration,
                   std::shared_ptr<Environment> closure)
      : declaration(declaration), closure(closure) {}

  int arity() override;
  int minArity() override;
  int maxArity() override { return arity(); }
  Value call(Interpreter &interpreter, std::vector<Value> arguments) override;
  std::string toString() override;
  std::shared_ptr<Callable> bind(std::shared_ptr<FSKInstance> instance) override {
      std::shared_ptr<Environment> environment = std::make_shared<Environment>(closure);
      environment->define("this", instance); // Fix: define takes value
      // We need to cast instance to Value? FSKInstance is shared_ptr.
      // Environment::define expects Value.
      // Value is variant.
      // We need to include variant/Value type visibility here?
      // Value is defined in Expr.hpp which is included via... Stmt.hpp?
      // Value is std::variant...
      return std::make_shared<FunctionCallable>(declaration, environment);
  }
};

using NativeCallback = std::function<Value(Interpreter &, std::vector<Value>)>;
using NativeMethodCallback = std::function<Value(Interpreter &, std::vector<Value>, std::shared_ptr<FSKInstance>)>;

struct NativeFunction : public Callable {
  int _arity;
  NativeCallback _call;
  NativeMethodCallback _callMethod;
  std::shared_ptr<FSKInstance> boundThis;

  NativeFunction(int arity, NativeCallback call);

  NativeFunction(int arity,
                 std::function<Value(Interpreter &, std::vector<Value>, std::shared_ptr<FSKInstance>)> callMethod,
                 std::shared_ptr<FSKInstance> boundThis);

  int arity() override;
  Value call(Interpreter &interpreter, std::vector<Value> arguments) override;
  std::string toString() override;
  std::shared_ptr<Callable> bind(std::shared_ptr<FSKInstance> instance) override;
};

struct FSKClass : public Callable,
                  public std::enable_shared_from_this<FSKClass> {
  std::shared_ptr<FSKClass> superclass;
  std::string name;
  std::map<std::string, std::shared_ptr<Callable>> methods;

  FSKClass(std::string name, std::shared_ptr<FSKClass> superclass,
           std::map<std::string, std::shared_ptr<Callable>> methods)
      : name(name), superclass(superclass), methods(methods) {}

  int arity() override;
  Value call(Interpreter &interpreter, std::vector<Value> arguments) override;
  std::string toString() override { return name; }

  std::shared_ptr<Callable> findMethod(std::string name);
};

struct FSKInstance : public std::enable_shared_from_this<FSKInstance> {
  std::shared_ptr<FSKClass> klass;
  std::map<std::string, Value> fields;

  FSKInstance(std::shared_ptr<FSKClass> klass) : klass(klass) {}

  Value get(Token name);
  void set(Token name, Value value);
  std::string toString() { return klass->name + " instance"; }
};