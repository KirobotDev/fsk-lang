#include "Interpreter.hpp"
#include "Callable.hpp"
#include "Lexer.hpp"
#include "Parser.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <thread>
#include <fstream>
#include <functional>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <cstring>
#include <vector>
#include <array>
#include <memory>
#include <cstdio>

Interpreter::Interpreter() {
  globals = std::make_shared<Environment>();
  environment = globals;

  globals->define(
      "clock", std::make_shared<NativeFunction>(0, [](Interpreter &interp,
                                                      std::vector<Value> args) {
        auto now = std::chrono::system_clock::now().time_since_epoch();
        return (double)std::chrono::duration_cast<std::chrono::milliseconds>(
                   now)
                   .count() /
               1000.0;
      }));

  globals->define("input",
                  std::make_shared<NativeFunction>(
                      1, [](Interpreter &interp, std::vector<Value> args) {
                        std::cout << interp.stringify(args[0]);
                        std::string line;
                        std::getline(std::cin, line);
                        return Value(line);
                      }));

  globals->define("sqrt",
                  std::make_shared<NativeFunction>(
                      1, [](Interpreter &interp, std::vector<Value> args) {
                        if (!std::holds_alternative<double>(args[0]))
                          throw std::runtime_error("sqrt attend un nombre.");
                        return Value(std::sqrt(std::get<double>(args[0])));
                      }));

  globals->define("abs",
                  std::make_shared<NativeFunction>(
                      1, [](Interpreter &interp, std::vector<Value> args) {
                        if (!std::holds_alternative<double>(args[0]))
                          throw std::runtime_error("abs attend un nombre.");
                        return Value(std::abs(std::get<double>(args[0])));
                      }));

  globals->define("readFile",
                  std::make_shared<NativeFunction>(
                      1, [](Interpreter &interp, std::vector<Value> args) {
                        if (!std::holds_alternative<std::string>(args[0]))
                          throw std::runtime_error(
                              "readFile attend un chemin de fichier (string).");
                        std::ifstream file(std::get<std::string>(args[0]), std::ios::binary);
                        if (!file.is_open())
                          return Value(std::monostate{});
                        
                        std::string content((std::istreambuf_iterator<char>(file)),
                                             std::istreambuf_iterator<char>());
                        return Value(content);
                      }));

  globals->define("writeFile",
                  std::make_shared<NativeFunction>(
                      2, [](Interpreter &interp, std::vector<Value> args) {
                        if (!std::holds_alternative<std::string>(args[0]) ||
                            !std::holds_alternative<std::string>(args[1]))
                          throw std::runtime_error(
                              "writeFile attend (chemin, contenu).");
                        std::ofstream file(std::get<std::string>(args[0]));
                        if (!file.is_open())
                          return Value(false);
                        file << std::get<std::string>(args[1]);
                        return Value(true);
                      }));

  globals->define("discordPost",
                  std::make_shared<NativeFunction>(
                      2, [](Interpreter &interp, std::vector<Value> args) {
                        std::cout << "[DISCORD MOCK] Envoi du message à "
                                  << interp.stringify(args[0]) << " : "
                                  << interp.stringify(args[1]) << std::endl;
                        return Value(true);
                      }));

  globals->define("discordLogin",
                  std::make_shared<NativeFunction>(
                      1, [](Interpreter &interp, std::vector<Value> args) {
                        std::cout << "[DISCORD MOCK] Connexion avec le token : "
                                  << interp.stringify(args[0]) << std::endl;
                        return Value(true);
                      }));

  std::map<std::string, std::shared_ptr<FunctionCallable>> methods;
  auto fskClass =
      std::make_shared<FSKClass>("FSK", nullptr, methods);
  auto fskInstance = std::make_shared<FSKInstance>(fskClass);

  fskInstance->fields["random"] = std::make_shared<NativeFunction>(
      2, [](Interpreter &interp, std::vector<Value> args) {
        if (!std::holds_alternative<double>(args[0]) ||
            !std::holds_alternative<double>(args[1])) {
          throw std::runtime_error("random attend (min, max) nombres.");
        }
        int min = (int)std::get<double>(args[0]);
        int max = (int)std::get<double>(args[1]);
        return Value((double)(min + rand() % (max - min + 1)));
      });

  fskInstance->fields["exec"] = std::make_shared<NativeFunction>(
      1, [](Interpreter &interp, std::vector<Value> args) {
        if (!std::holds_alternative<std::string>(args[0])) {
          throw std::runtime_error("exec attend une commande (string).");
        }
        std::string command = std::get<std::string>(args[0]);
        int result = std::system(command.c_str());
        return Value((double)result);
      });

  fskInstance->fields["version"] = std::make_shared<NativeFunction>(
      0, [](Interpreter &interp, std::vector<Value> args) {
        return Value(std::string("1.1.0"));
      });
  
  fskInstance->fields["sleep"] = std::make_shared<NativeFunction>(
      1, [](Interpreter &interp, std::vector<Value> args) {
        if (!std::holds_alternative<double>(args[0])) {
            throw std::runtime_error("sleep attend des millisecondes (nombre).");
        }
        int ms = (int)std::get<double>(args[0]);
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        return Value(true);
      });

  fskInstance->fields["startServer"] = std::make_shared<NativeFunction>(
      2, [](Interpreter &interp, std::vector<Value> args) {
        if (!std::holds_alternative<double>(args[0])) {
          throw std::runtime_error("startServer attend un port (nombre).");
        }
        int port = (int)std::get<double>(args[0]);

        if (!std::holds_alternative<std::shared_ptr<Callable>>(args[1])) {
           throw std::runtime_error("startServer attend une fonction handler.");
        }
        auto handler = std::get<std::shared_ptr<Callable>>(args[1]);

        int server_fd, new_socket;
        struct sockaddr_in address;
        int opt = 1;
        int addrlen = sizeof(address);

        if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
          throw std::runtime_error("Socket failed");
        }

        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
                       sizeof(opt))) {
          throw std::runtime_error("Setsockopt failed");
        }

        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port);

        if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
          throw std::runtime_error("Bind failed");
        }

        if (listen(server_fd, 3) < 0) {
          throw std::runtime_error("Listen failed");
        }

        std::cout << "Server started on port " << port << std::endl;

        while (true) {
          if ((new_socket = accept(server_fd, (struct sockaddr *)&address,
                                   (socklen_t *)&addrlen)) < 0) {
            perror("accept");
            continue;
          }

          std::this_thread::sleep_for(std::chrono::milliseconds(200));

           int content_length = 0;
          std::string request = "";
          char buffer[4096];
          
          while (request.find("\r\n\r\n") == std::string::npos) {
             int valread = read(new_socket, buffer, 4096);
             if (valread <= 0) break;
             request.append(buffer, valread);
          }
          
          size_t cl_pos = request.find("Content-Length: ");
          if (cl_pos != std::string::npos) {
              size_t cl_end = request.find("\r\n", cl_pos);
              std::string cl_str = request.substr(cl_pos + 16, cl_end - (cl_pos + 16));
              content_length = std::stoi(cl_str);
          }
          
          size_t header_end = request.find("\r\n\r\n") + 4;
          while (request.length() - header_end < content_length) {
             int valread = read(new_socket, buffer, 4096);
             if (valread <= 0) break;
             request.append(buffer, valread);
          }

          std::vector<Value> handlerArgs;
          handlerArgs.push_back(Value(request));
          std::string responseBody = "";
          std::cout << "DEBUG: Calling handler..." << std::endl;
          try {
             Value responseVal = handler->call(interp, handlerArgs);
             std::cout << "DEBUG: Handler returned without throw." << std::endl;
             if (std::holds_alternative<std::string>(responseVal)) {
                responseBody = std::get<std::string>(responseVal);
             }
          } catch (const Value &returnValue) {
             std::cout << "DEBUG: Caught Value exception." << std::endl;
             if (std::holds_alternative<std::string>(returnValue)) {
                responseBody = std::get<std::string>(returnValue);
             }
          } catch (const std::exception &e) {
             std::cout << "DEBUG: Handler threw std::exception: " << e.what() << std::endl;
          } catch (...) {
             std::cout << "DEBUG: Handler threw unknown exception." << std::endl;
          }

          std::string httpResponse = "HTTP/1.1 200 OK\nContent-Type: text/html\nContent-Length: " + std::to_string(responseBody.length()) + "\n\n" + responseBody;
          
          send(new_socket, httpResponse.c_str(), httpResponse.length(), 0);
          close(new_socket);
        }
        return Value(true);
      });

  fskInstance->fields["shell"] = std::make_shared<NativeFunction>(
      1, [](Interpreter &interp, std::vector<Value> args) {
        if (!std::holds_alternative<std::string>(args[0])) {
          throw std::runtime_error("shell attend une commande (string).");
        }
        std::string cmd = std::get<std::string>(args[0]);
        std::array<char, 128> buffer;
        std::string result;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
        if (!pipe) {
           return Value(std::string("Error: popen failed"));
        }
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
           result += buffer.data();
        }
        return Value(result);
      });

  fskInstance->fields["indexOf"] = std::make_shared<NativeFunction>(
      2, [](Interpreter &interp, std::vector<Value> args) {
        if (!std::holds_alternative<std::string>(args[0]) ||
            !std::holds_alternative<std::string>(args[1])) {
          throw std::runtime_error("indexOf: (haystack, needle) required.");
        }
        std::string h = std::get<std::string>(args[0]);
        std::string n = std::get<std::string>(args[1]);
        size_t pos = h.find(n);
        if (pos == std::string::npos) return Value(-1.0);
        return Value((double)pos);
      });

  fskInstance->fields["length"] = std::make_shared<NativeFunction>(
      1, [](Interpreter &interp, std::vector<Value> args) {
         if (std::holds_alternative<std::string>(args[0])) {
             return Value((double)std::get<std::string>(args[0]).length());
         }
         return Value(0.0);
      });

  fskInstance->fields["substr"] = std::make_shared<NativeFunction>(
      3, [](Interpreter &interp, std::vector<Value> args) {
         if (!std::holds_alternative<std::string>(args[0]) ||
             !std::holds_alternative<double>(args[1]) ||
             !std::holds_alternative<double>(args[2])) {
           throw std::runtime_error("substr: (str, start, len) required.");
         }
         std::string s = std::get<std::string>(args[0]);
         int start = (int)std::get<double>(args[1]);
         int len = (int)std::get<double>(args[2]);
         if (start < 0 || start >= s.length()) return Value(std::string(""));
         return Value(s.substr(start, len));
      });

  globals->define("FSK", fskInstance);

  std::vector<std::string> searchPaths = {"std/prelude.fsk", "../std/prelude.fsk", "/usr/local/lib/fsk/std/prelude.fsk"};
  for (const auto &path : searchPaths) {
    std::ifstream file(path);
    if (file.is_open()) {
      std::stringstream buffer;
      buffer << file.rdbuf();
      std::string source = buffer.str();
      Lexer lexer(source);
      std::vector<Token> tokens = lexer.scanTokens();
      Parser parser(tokens);
      std::vector<std::shared_ptr<Stmt>> statements = parser.parse();
      try {
        interpret(statements);
      } catch (...) {
      }
      break;
    }
  }
}

void Interpreter::interpret(std::vector<std::shared_ptr<Stmt>> statements) {
  try {
    for (const auto &stmt : statements) {
      execute(stmt);
    }
  } catch (const std::runtime_error &error) {
    std::cerr << "Erreur d'exécution : " << error.what() << std::endl;
  }
}

void Interpreter::execute(std::shared_ptr<Stmt> stmt) {
  if (stmt)
    stmt->accept(*this);
}

Value Interpreter::evaluate(std::shared_ptr<Expr> expr) {
  if (expr) {
    expr->accept(*this);
    return lastValue;
  }
  return std::monostate{};
}

void Interpreter::visitExpressionStmt(Expression &stmt) {
  evaluate(stmt.expression);
}

void Interpreter::visitPrintStmt(Print &stmt) {
  Value value = evaluate(stmt.expression);
  std::cout << stringify(value) << std::endl;
}

void Interpreter::visitLetStmt(Let &stmt) {
  Value value = std::monostate{};
  if (stmt.initializer != nullptr) {
    value = evaluate(stmt.initializer);
  }

  if (!stmt.typeHint.empty()) {
    if (stmt.typeHint == "int" || stmt.typeHint == "number") {
      if (!std::holds_alternative<double>(value))
        throw std::runtime_error("Type invalide : attendu " + stmt.typeHint);
    } else if (stmt.typeHint == "string") {
      if (!std::holds_alternative<std::string>(value))
        throw std::runtime_error("Type invalide : attendu string.");
    } else if (stmt.typeHint == "bool") {
      if (!std::holds_alternative<bool>(value))
        throw std::runtime_error("Type invalide : attendu bool.");
    }
  }

  environment->define(stmt.name.lexeme, value);
}

void Interpreter::visitConstStmt(Const &stmt) {
  Value value = evaluate(stmt.initializer);
  environment->define(stmt.name.lexeme, value);
}

void Interpreter::visitBlockStmt(Block &stmt) {
  executeBlock(stmt.statements, std::make_shared<Environment>(environment));
}

void Interpreter::executeBlock(
    const std::vector<std::shared_ptr<Stmt>> &statements,
    std::shared_ptr<Environment> env) {
  std::shared_ptr<Environment> previous = this->environment;
  try {
    this->environment = env;
    for (const auto &statement : statements) {
      execute(statement);
    }
  } catch (...) {
    this->environment = previous;
    throw;
  }
  this->environment = previous;
}

void Interpreter::visitIfStmt(If &stmt) {
  if (isTruthy(evaluate(stmt.condition))) {
    execute(stmt.thenBranch);
  } else if (stmt.elseBranch != nullptr) {
    execute(stmt.elseBranch);
  }
}

void Interpreter::visitWhileStmt(While &stmt) {
  while (isTruthy(evaluate(stmt.condition))) {
    execute(stmt.body);
  }
}

void Interpreter::visitFunctionStmt(Function &stmt) {
  auto function = std::make_shared<FunctionCallable>(
      std::make_shared<Function>(stmt), environment);
  environment->define(stmt.name.lexeme, function);
}

void Interpreter::visitReturnStmt(Return &stmt) {
  Value value = std::monostate{};
  if (stmt.value != nullptr)
    value = evaluate(stmt.value);

  throw value; 
}

void Interpreter::visitTryStmt(Try &stmt) {
  try {
    execute(stmt.tryBranch);
  } catch (Value &error) {
    std::shared_ptr<Environment> env =
        std::make_shared<Environment>(this->environment);
    env->define(stmt.catchName.lexeme, error);
    executeBlock({stmt.catchBranch}, env);
  } catch (const std::runtime_error &error) {
    std::shared_ptr<Environment> env =
        std::make_shared<Environment>(this->environment);
    env->define(stmt.catchName.lexeme, Value(std::string(error.what())));
    executeBlock({stmt.catchBranch}, env);
  }
}

void Interpreter::visitThrowStmt(Throw &stmt) {
  Value value = evaluate(stmt.value);
  throw value;
}

void Interpreter::visitClassStmt(Class &stmt) {
  std::shared_ptr<FSKClass> superclass = nullptr;
  if (stmt.superclass != nullptr) {
    Value value = evaluate(stmt.superclass);
    if (!std::holds_alternative<std::shared_ptr<Callable>>(value)) {
      throw std::runtime_error("La superclasse doit être une classe.");
    }
    auto callable = std::get<std::shared_ptr<Callable>>(value);
    superclass = std::dynamic_pointer_cast<FSKClass>(callable);
    if (superclass == nullptr) {
      throw std::runtime_error("La superclasse doit être une classe.");
    }
  }

  environment->define(stmt.name.lexeme, std::monostate{});

  if (superclass != nullptr) {
    environment = std::make_shared<Environment>(environment);
    environment->define("super",
                        std::static_pointer_cast<Callable>(superclass));
  }

  std::map<std::string, std::shared_ptr<FunctionCallable>> methods;
  for (const auto &method : stmt.methods) {
    auto function = std::make_shared<FunctionCallable>(method, environment);
    methods[method->name.lexeme] = function;
  }

  auto klass =
      std::make_shared<FSKClass>(stmt.name.lexeme, superclass, methods);

  if (superclass != nullptr) {
    environment = environment->getEnclosing();
  }

  environment->assign(stmt.name, std::static_pointer_cast<Callable>(klass));
}

void Interpreter::visitLiteralExpr(Literal &expr) { lastValue = expr.value; }

void Interpreter::visitGroupingExpr(Grouping &expr) {
  lastValue = evaluate(expr.expression);
}

void Interpreter::visitUnaryExpr(Unary &expr) {
  Value right = evaluate(expr.right);
  switch (expr.op.type) {
  case TokenType::MINUS:
    lastValue = -std::get<double>(right);
    break;
  case TokenType::BANG:
    lastValue = !isTruthy(right);
    break;
  default:
    break;
  }
}

void Interpreter::visitBinaryExpr(Binary &expr) {
  Value left = evaluate(expr.left);
  Value right = evaluate(expr.right);

  switch (expr.op.type) {
  case TokenType::GREATER:
    lastValue = std::get<double>(left) > std::get<double>(right);
    break;
  case TokenType::GREATER_EQUAL:
    lastValue = std::get<double>(left) >= std::get<double>(right);
    break;
  case TokenType::LESS:
    lastValue = std::get<double>(left) < std::get<double>(right);
    break;
  case TokenType::LESS_EQUAL:
    lastValue = std::get<double>(left) <= std::get<double>(right);
    break;
  case TokenType::BANG_EQUAL:
    lastValue = !isEqual(left, right);
    break;
  case TokenType::EQUAL_EQUAL:
    lastValue = isEqual(left, right);
    break;
  case TokenType::MINUS:
    lastValue = std::get<double>(left) - std::get<double>(right);
    break;
  case TokenType::PLUS:
    if (std::holds_alternative<double>(left) &&
        std::holds_alternative<double>(right)) {
      lastValue = std::get<double>(left) + std::get<double>(right);
    } else if (std::holds_alternative<std::string>(left) ||
               std::holds_alternative<std::string>(right)) {
      lastValue = stringify(left) + stringify(right);
    }
    break;
  case TokenType::SLASH:
    lastValue = std::get<double>(left) / std::get<double>(right);
    break;
  case TokenType::STAR:
    lastValue = std::get<double>(left) * std::get<double>(right);
    break;
  default:
    break;
  }
}

void Interpreter::visitVariableExpr(Variable &expr) {
  lastValue = environment->get(expr.name);
}

void Interpreter::visitAssignExpr(Assign &expr) {
  Value value = evaluate(expr.value);
  environment->assign(expr.name, value);
  lastValue = value;
}

void Interpreter::visitLogicalExpr(Logical &expr) {
  Value left = evaluate(expr.left);
  if (expr.op.type == TokenType::OR) {
    if (isTruthy(left)) {
      lastValue = left;
      return;
    }
  } else {
    if (!isTruthy(left)) {
      lastValue = left;
      return;
    }
  }
  lastValue = evaluate(expr.right);
}

void Interpreter::visitCallExpr(Call &expr) {
  Value callee = evaluate(expr.callee);

  std::vector<Value> arguments;
  for (const auto &arg : expr.arguments) {
    arguments.push_back(evaluate(arg));
  }

  if (std::holds_alternative<std::shared_ptr<Callable>>(callee)) {
    auto function = std::get<std::shared_ptr<Callable>>(callee);
    if (arguments.size() != (size_t)function->arity() &&
        function->arity() != -1) {
      throw std::runtime_error("Expected " + std::to_string(function->arity()) +
                               " arguments but got " +
                               std::to_string(arguments.size()) + ".");
    }
    lastValue = function->call(*this, arguments);
  } else {
    throw std::runtime_error("Can only call functions and classes.");
  }
}

void Interpreter::visitGetExpr(Get &expr) {
  Value object = evaluate(expr.object);
  if (std::holds_alternative<std::shared_ptr<FSKInstance>>(object)) {
    lastValue = std::get<std::shared_ptr<FSKInstance>>(object)->get(expr.name);
    return;
  }

  throw std::runtime_error("Seules les instances ont des propriétés.");
}

void Interpreter::visitSetExpr(Set &expr) {
  Value object = evaluate(expr.object);

  if (std::holds_alternative<std::shared_ptr<FSKInstance>>(object)) {
    Value value = evaluate(expr.value);
    std::get<std::shared_ptr<FSKInstance>>(object)->set(expr.name, value);
    lastValue = value;
    return;
  }

  throw std::runtime_error("Only instances have fields.");
}

void Interpreter::visitThisExpr(This &expr) {
  lastValue = environment->get(expr.keyword);
}

void Interpreter::visitSuperExpr(Super &expr) {
  Value superValue = environment->get("super");
  std::shared_ptr<Callable> callable =
      std::get<std::shared_ptr<Callable>>(superValue);
  std::shared_ptr<FSKClass> superclass =
      std::dynamic_pointer_cast<FSKClass>(callable);

  Value thisValue = environment->getEnclosing()->get("this");
  std::shared_ptr<FSKInstance> object =
      std::get<std::shared_ptr<FSKInstance>>(thisValue);

  std::shared_ptr<FunctionCallable> method =
      superclass->findMethod(expr.method.lexeme);

  if (method == nullptr) {
    throw std::runtime_error("Undefined property '" + expr.method.lexeme +
                             "'.");
  }

  lastValue = method->bind(object);
}

void Interpreter::visitAwaitExpr(Await &expr) {
  lastValue = evaluate(expr.expression);
}

void Interpreter::visitFunctionExpr(FunctionExpr &expr) {
  Token name(TokenType::IDENTIFIER, "", std::monostate{}, 0);
  auto function = std::make_shared<Function>(name, expr.params, expr.body, false);
  auto callable = std::make_shared<FunctionCallable>(function, environment);
  lastValue = callable;
}

bool Interpreter::isTruthy(Value value) {
  if (std::holds_alternative<std::monostate>(value))
    return false;
  if (std::holds_alternative<bool>(value))
    return std::get<bool>(value);
  return true;
}

bool Interpreter::isEqual(Value a, Value b) { return a == b; }

std::string Interpreter::stringify(Value value) {
  if (std::holds_alternative<std::monostate>(value))
    return "nil";
  if (std::holds_alternative<double>(value)) {
    std::string text = std::to_string(std::get<double>(value));
    if (text.find(".000000") != std::string::npos) {
      text = text.substr(0, text.find(".000000"));
    }
    return text;
  }
  if (std::holds_alternative<bool>(value))
    return std::get<bool>(value) ? "true" : "false";
  if (std::holds_alternative<std::string>(value))
    return std::get<std::string>(value);
  if (std::holds_alternative<std::shared_ptr<FSKInstance>>(value))
    return std::get<std::shared_ptr<FSKInstance>>(value)->toString();
  return "Object";
}

void Interpreter::visitImportStmt(Import &stmt) {
  Value value = evaluate(stmt.file);
  if (!std::holds_alternative<std::string>(value)) {
    throw std::runtime_error("Import path must be a string.");
  }
  std::string path = std::get<std::string>(value);

  std::ifstream file(path);
  if (!file.is_open()) {
      std::string systemPath = "/usr/local/lib/fsk/" + path + ".fsk";
      file.open(systemPath);
      if (!file.is_open()) {
        file.open(path + ".fsk");
        if (!file.is_open()) {
            throw std::runtime_error("Could not open file: " + path);
        }
      }
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string source = buffer.str();

  Lexer lexer(source);
  std::vector<Token> tokens = lexer.scanTokens();
  Parser parser(tokens);
  std::vector<std::shared_ptr<Stmt>> statements = parser.parse();

  interpret(statements);
}
