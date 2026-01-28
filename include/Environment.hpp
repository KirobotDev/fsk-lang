#pragma once
#include "Token.hpp"
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <variant>
#include <iostream>

class Environment : public std::enable_shared_from_this<Environment> {
public:
  Environment() : enclosing(nullptr) {}
  Environment(std::shared_ptr<Environment> enclosing) : enclosing(enclosing) {}

  void define(std::string name, Value value) { 
      values[name] = value; 
  }

  Value get(Token name) { return get(name.lexeme); }

  Value get(std::string name) {
    if (values.count(name)) {
      return values[name];
    }

    if (enclosing != nullptr)
      return enclosing->get(name);

    throw std::runtime_error("Undefined variable '" + name + "'.");
  }

  void assign(Token name, Value value) {
    if (values.count(name.lexeme)) {
      values[name.lexeme] = value;
      return;
    }

    if (enclosing != nullptr) {
      enclosing->assign(name, value);
      return;
    }

    throw std::runtime_error("Undefined variable '" + name.lexeme + "'.");
  }

  std::shared_ptr<Environment> getEnclosing() { return enclosing; }

private:
  std::shared_ptr<Environment> enclosing;
  std::map<std::string, Value> values;
};
