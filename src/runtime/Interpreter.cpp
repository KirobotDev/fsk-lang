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
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#define popen _popen
#define pclose _pclose
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/ioctl.h>
#endif
#include <sstream> 
#include <sqlite3.h>
#include <cstring>
#include <vector>
#include <array>
#include <memory>
#include <cstdio>
#include <iomanip>
#include <ctime>
#include <filesystem>

#include <regex>

#ifndef __EMSCRIPTEN__
#include "easywsclient.hpp"
#endif

#include <sqlite3.h>
#include "json.hpp"

#ifndef __EMSCRIPTEN__
#include <curl/curl.h>
#include <openssl/ssl.h>
#endif

using json = nlohmann::json;

#ifndef __EMSCRIPTEN__
static size_t WriteCallback(void *contents, size_t size, size_t nmemb,
                            void *userp) {
  ((std::string *)userp)->append((char *)contents, size * nmemb);
  return size * nmemb;
}
#endif

Value jsonToValue(json j) {
    if (j.is_null()) return Value(std::monostate{});
    if (j.is_boolean()) return Value(j.get<bool>());
    if (j.is_number()) return Value(j.get<double>());
    if (j.is_string()) return Value(j.get<std::string>());
    if (j.is_array()) {
        std::vector<Value> elements;
        for (auto& element : j) {
            elements.push_back(jsonToValue(element));
        }
        return Value(std::make_shared<FSKArray>(elements));
    }
    if (j.is_object()) {
        static auto objClass = std::make_shared<FSKClass>("Object", nullptr, std::map<std::string, std::shared_ptr<FunctionCallable>>());
        auto instance = std::make_shared<FSKInstance>(objClass);
        for (auto& [key, val] : j.items()) {
             instance->fields[key] = jsonToValue(val);
        }
        return Value(instance);
    }
    return Value(std::monostate{});
}

json valueToJson(Value v) {
    if (std::holds_alternative<std::monostate>(v)) return nullptr;
    if (std::holds_alternative<bool>(v)) return std::get<bool>(v);
    if (std::holds_alternative<double>(v)) return std::get<double>(v);
    if (std::holds_alternative<std::string>(v)) return std::get<std::string>(v);
    if (std::holds_alternative<std::shared_ptr<FSKArray>>(v)) {
        auto arr = std::get<std::shared_ptr<FSKArray>>(v);
        json j = json::array();
        for (const auto& elem : arr->elements) {
            j.push_back(valueToJson(elem));
        }
        return j;
    }
    if (std::holds_alternative<std::shared_ptr<FSKInstance>>(v)) {
        auto inst = std::get<std::shared_ptr<FSKInstance>>(v);
        json j = json::object();
        for (auto const& [key, val] : inst->fields) {
            j[key] = valueToJson(val);
        }
        return j;
    }
    return nullptr;
}

Interpreter::Interpreter() {
  globals = std::make_shared<Environment>();
  environment = globals;

  std::vector<Value> argList;


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

  globals->define("exit", std::make_shared<NativeFunction>(
      0, [](Interpreter &interp, std::vector<Value> args) {
        exit(0);
        return Value(0.0);
      }));

  globals->define("sleep", std::make_shared<NativeFunction>(
      1, [](Interpreter &interp, std::vector<Value> args) {
        if (std::holds_alternative<double>(args[0])) {
            int ms = (int)std::get<double>(args[0]);
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        }
        return Value(0.0);
      }));
  
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
#ifdef _WIN32
        throw std::runtime_error("startServer n'est pas encore supporté sur Windows.");
#else
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

        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt,
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
             int valread = recv(new_socket, buffer, sizeof(buffer), 0);
             if (valread <= 0) break;
             request.append(buffer, valread);
          }
          
          size_t cl_pos = request.find("Content-Length: ");
          if (cl_pos != std::string::npos) {
              size_t cl_end = request.find("\r\n", cl_pos);
              if (cl_end != std::string::npos) {
                  std::string cl_str = request.substr(cl_pos + 16, cl_end - (cl_pos + 16));
                  try {
                      content_length = std::stoi(cl_str);
                  } catch (...) { content_length = 0; }
              }
          }
          
          size_t header_end_pos = request.find("\r\n\r\n");
          if (header_end_pos != std::string::npos) {
              size_t body_start = header_end_pos + 4;
              size_t current_body_len = request.length() - body_start;
              
              while (current_body_len < content_length) {
                  int valread = recv(new_socket, buffer, sizeof(buffer), 0);
                  if (valread <= 0) break;
                  request.append(buffer, valread);
                  current_body_len += valread;
              }
          }

          std::vector<Value> handlerArgs;
          handlerArgs.push_back(Value(request));
          std::string responseBody = "";

          try {
             Value responseVal = handler->call(interp, handlerArgs);

             if (std::holds_alternative<std::string>(responseVal)) {
                responseBody = std::get<std::string>(responseVal);
             }
          } catch (const Value &returnValue) {

             if (std::holds_alternative<std::string>(returnValue)) {
                responseBody = std::get<std::string>(returnValue);
             }
          } catch (const std::exception &e) {

          } catch (...) {

          }

          std::string httpResponse;
          if (responseBody.substr(0, 8) == "HTTP/1.1" || responseBody.substr(0, 8) == "HTTP/1.0") {
              httpResponse = responseBody;
          } else {
              httpResponse = "HTTP/1.1 200 OK\nContent-Type: text/html\nContent-Length: " + std::to_string(responseBody.length()) + "\n\n" + responseBody;
          }
          
          send(new_socket, httpResponse.c_str(), httpResponse.length(), 0);
          close(new_socket);
        }
#endif
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
         if (std::holds_alternative<std::shared_ptr<FSKArray>>(args[0])) {
             return Value((double)std::get<std::shared_ptr<FSKArray>>(args[0])->elements.size());
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

  fskInstance->fields["args"] = std::make_shared<NativeFunction>(
      0, [this](Interpreter &interp, std::vector<Value> args) {

         return Value(std::monostate{}); 
      });

   fskInstance->fields["argCount"] = std::make_shared<NativeFunction>(
      0, [this](Interpreter &interp, std::vector<Value> args) {
         return Value((double)this->scriptArgs.size());
      });

   fskInstance->fields["arg"] = std::make_shared<NativeFunction>(
      1, [this](Interpreter &interp, std::vector<Value> args) {
         if (!std::holds_alternative<double>(args[0])) return Value(std::string(""));
         int index = (int)std::get<double>(args[0]);
         if (index < 0 || index >= this->scriptArgs.size()) return Value(std::string(""));
         return Value(this->scriptArgs[index]);
      });

   fskInstance->fields["mkdir"] = std::make_shared<NativeFunction>(
     1, [](Interpreter &interp, std::vector<Value> args) {
        if (!std::holds_alternative<std::string>(args[0])) return Value(false);
        std::string path = std::get<std::string>(args[0]);
        std::string cmd = "mkdir -p \"" + path + "\""; 
        int res = system(cmd.c_str());
        return Value(res == 0);
     });

   fskInstance->fields["exists"] = std::make_shared<NativeFunction>(
     1, [](Interpreter &interp, std::vector<Value> args) {
        if (!std::holds_alternative<std::string>(args[0])) return Value(false);
        std::string path = std::get<std::string>(args[0]);
        std::ifstream f(path);
        return Value(f.good());
     });

   fskInstance->fields["fetch"] = std::make_shared<NativeFunction>(
     1, [](Interpreter &interp, std::vector<Value> args) {
        if (!std::holds_alternative<std::string>(args[0])) throw std::runtime_error("fetch attend une URL.");
#ifndef __EMSCRIPTEN__
        std::string url = std::get<std::string>(args[0]);
        std::string cmd = "curl -s \"" + url + "\"";
        std::array<char, 128> buffer;
        std::string result;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
        if (!pipe) return Value(std::string("Error: fetch failed"));
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
           result += buffer.data();
        }
        return Value(result);
#else
        return Value(std::string("Fetch not supported in WASM yet."));
#endif
     });

   fskInstance->fields["sin"] = std::make_shared<NativeFunction>(
      1, [](Interpreter &interp, std::vector<Value> args) {
         if (!std::holds_alternative<double>(args[0])) return Value(0.0);
         return Value(std::sin(std::get<double>(args[0])));
      });
   fskInstance->fields["cos"] = std::make_shared<NativeFunction>(
      1, [](Interpreter &interp, std::vector<Value> args) {
         if (!std::holds_alternative<double>(args[0])) return Value(0.0);
         return Value(std::cos(std::get<double>(args[0])));
      });
   fskInstance->fields["tan"] = std::make_shared<NativeFunction>(
      1, [](Interpreter &interp, std::vector<Value> args) {
         if (!std::holds_alternative<double>(args[0])) return Value(0.0);
         return Value(std::tan(std::get<double>(args[0])));
      });
   fskInstance->fields["sqrt"] = std::make_shared<NativeFunction>(
      1, [](Interpreter &interp, std::vector<Value> args) {
         if (!std::holds_alternative<double>(args[0])) return Value(0.0);
         return Value(std::sqrt(std::get<double>(args[0])));
      });
   fskInstance->fields["PI"] = Value(3.14159265358979323846);
   fskInstance->fields["E"] = Value(2.71828182845904523536);

  globals->define("FSK", fskInstance);


  auto consoleClass = std::make_shared<FSKClass>("Console", nullptr, std::map<std::string, std::shared_ptr<FunctionCallable>>());
  auto consoleInstance = std::make_shared<FSKInstance>(consoleClass);
  
  consoleInstance->fields["clear"] = std::make_shared<NativeFunction>(0, [](Interpreter &interp, std::vector<Value> args) {
      std::cout << "\033[2J\033[1;1H"; 
      return Value(true);
  });

  consoleInstance->fields["setColor"] = std::make_shared<NativeFunction>(1, [](Interpreter &interp, std::vector<Value> args) {
      if (!std::holds_alternative<std::string>(args[0])) return Value(false);
      std::string color = std::get<std::string>(args[0]);
      std::string code = "\033[0m";
      
      if (color == "red") code = "\033[31m";
      else if (color == "green") code = "\033[32m";
      else if (color == "blue") code = "\033[34m";
      else if (color == "yellow") code = "\033[33m";
      else if (color == "magenta") code = "\033[35m";
      else if (color == "cyan") code = "\033[36m";
      else if (color == "white") code = "\033[37m";
      else if (color == "reset") code = "\033[0m";
      
      std::cout << code;
      return Value(true);
  });

  consoleInstance->fields["moveTo"] = std::make_shared<NativeFunction>(2, [](Interpreter &interp, std::vector<Value> args) {
      if (!std::holds_alternative<double>(args[0]) || !std::holds_alternative<double>(args[1])) return Value(false);
      int x = (int)std::get<double>(args[0]);
      int y = (int)std::get<double>(args[1]);
      std::cout << "\033[" << y << ";" << x << "H";
      return Value(true);
  });

  globals->define("Console", consoleInstance);

  auto sqlClass = std::make_shared<FSKClass>("SQL", nullptr, std::map<std::string, std::shared_ptr<FunctionCallable>>());
  auto sqlInstance = std::make_shared<FSKInstance>(sqlClass);

  static int dbIdCounter = 1;
  static std::map<int, sqlite3*> databases;

  sqlInstance->fields["open"] = std::make_shared<NativeFunction>(1, [](Interpreter &interp, std::vector<Value> args) {
      if (!std::holds_alternative<std::string>(args[0])) return Value(-1.0);
      std::string path = std::get<std::string>(args[0]);
      
      sqlite3 *db;
      int rc = sqlite3_open(path.c_str(), &db);
      if (rc) {
          sqlite3_close(db);
          return Value(-1.0);
      }
      
      int id = dbIdCounter++;
      databases[id] = db;
      return Value((double)id);
  });

  sqlInstance->fields["exec"] = std::make_shared<NativeFunction>(2, [](Interpreter &interp, std::vector<Value> args) {
      if (!std::holds_alternative<double>(args[0]) || !std::holds_alternative<std::string>(args[1])) return Value(false);
      int id = (int)std::get<double>(args[0]);
      std::string sql = std::get<std::string>(args[1]);

      if (databases.find(id) == databases.end()) return Value(false);
      
      char *zErrMsg = 0;
      int rc = sqlite3_exec(databases[id], sql.c_str(), 0, 0, &zErrMsg);
      if (rc != SQLITE_OK) {
           sqlite3_free(zErrMsg);
           return Value(false);
      }
      return Value(true);
  });

   sqlInstance->fields["query"] = std::make_shared<NativeFunction>(2, [](Interpreter &interp, std::vector<Value> args) {
      if (!std::holds_alternative<double>(args[0]) || !std::holds_alternative<std::string>(args[1])) return Value(std::monostate{}); 
      int id = (int)std::get<double>(args[0]);
      std::string sql = std::get<std::string>(args[1]);

      if (databases.find(id) == databases.end()) return Value(std::monostate{});

      sqlite3_stmt *stmt;
      int rc = sqlite3_prepare_v2(databases[id], sql.c_str(), -1, &stmt, 0);
      if (rc != SQLITE_OK) return Value(std::monostate{});

      std::vector<Value> rows;

      while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
          
          auto rowClass = std::make_shared<FSKClass>("Row", nullptr, std::map<std::string, std::shared_ptr<FunctionCallable>>());
          auto rowInstance = std::make_shared<FSKInstance>(rowClass);

          int cols = sqlite3_column_count(stmt);
          for (int i = 0; i < cols; i++) {
              std::string colName = sqlite3_column_name(stmt, i);
              int type = sqlite3_column_type(stmt, i);
              
              if (type == SQLITE_INTEGER) {
                  rowInstance->fields[colName] = Value((double)sqlite3_column_int(stmt, i));
              } else if (type == SQLITE_FLOAT) {
                  rowInstance->fields[colName] = Value(sqlite3_column_double(stmt, i));
              } else if (type == SQLITE_TEXT) {
                  const unsigned char *val = sqlite3_column_text(stmt, i);
                  rowInstance->fields[colName] = Value(std::string(reinterpret_cast<const char*>(val)));
              } else {
                  rowInstance->fields[colName] = Value(std::monostate{});
              }
          }
          rows.push_back(Value(rowInstance));
      }
      sqlite3_finalize(stmt);
      return Value(std::make_shared<FSKArray>(rows));
  });

  globals->define("SQL", sqlInstance);

  auto wsClass = std::make_shared<FSKClass>("WS", nullptr, std::map<std::string, std::shared_ptr<FunctionCallable>>());
  auto wsInstance = std::make_shared<FSKInstance>(wsClass);


#ifndef __EMSCRIPTEN__
  static int wsIdCounter = 1;
  static std::map<int, std::shared_ptr<void>> sockets; 
  static std::map<int, std::shared_ptr<Callable>> wsCallbacks;
#endif

  wsInstance->fields["connect"] = std::make_shared<NativeFunction>(1, [](Interpreter &interp, std::vector<Value> args) {
#ifndef __EMSCRIPTEN__
      if (!std::holds_alternative<std::string>(args[0])) return Value(-1.0);
      std::string url = std::get<std::string>(args[0]);
      
      easywsclient::WebSocket::pointer ws = easywsclient::WebSocket::from_url(url);
      if (!ws) return Value(-1.0);
      
      int id = wsIdCounter++;
      sockets[id] = std::shared_ptr<void>(ws); 
      return Value((double)id);
#else
      return Value(-1.0);
#endif
  });

  wsInstance->fields["send"] = std::make_shared<NativeFunction>(2, [](Interpreter &interp, std::vector<Value> args) {
#ifndef __EMSCRIPTEN__
      if (!std::holds_alternative<double>(args[0]) || !std::holds_alternative<std::string>(args[1])) return Value(false);
      int id = (int)std::get<double>(args[0]);
      std::string msg = std::get<std::string>(args[1]);
      
      if (sockets.find(id) == sockets.end()) return Value(false);
      auto ws = static_cast<easywsclient::WebSocket*>(sockets[id].get());
      if (ws) {
          ws->send(msg);
          return Value(true);
      }
      return Value(false);
#else
      return Value(false);
#endif
  });

  wsInstance->fields["poll"] = std::make_shared<NativeFunction>(1, [](Interpreter &interp, std::vector<Value> args) {
#ifndef __EMSCRIPTEN__
      if (!std::holds_alternative<double>(args[0])) return Value(std::make_shared<FSKArray>(std::vector<Value>{}));
      int id = (int)std::get<double>(args[0]);
      
      if (sockets.find(id) == sockets.end()) return Value(std::make_shared<FSKArray>(std::vector<Value>{}));
      auto ws = static_cast<easywsclient::WebSocket*>(sockets[id].get());
      
      std::vector<Value> messages;
      if (ws) {
        ws->poll();
        ws->dispatch([&messages](const std::string & message) {
            messages.push_back(Value(message));
        });
      }
      return Value(std::make_shared<FSKArray>(messages));
#else
      return Value(std::make_shared<FSKArray>(std::vector<Value>{}));
#endif
  });

  wsInstance->fields["readyState"] = std::make_shared<NativeFunction>(1, [](Interpreter &interp, std::vector<Value> args) {
#ifndef __EMSCRIPTEN__
     if (!std::holds_alternative<double>(args[0])) return Value(-1.0);
     int id = (int)std::get<double>(args[0]);
     if (sockets.find(id) == sockets.end()) return Value(-1.0);
     auto ws = static_cast<easywsclient::WebSocket*>(sockets[id].get());
     if(ws) return Value((double)ws->getReadyState());
     return Value(-1.0);
#else
     return Value(-1.0);
#endif
  });

  wsInstance->fields["close"] = std::make_shared<NativeFunction>(1, [](Interpreter &interp, std::vector<Value> args) {
#ifndef __EMSCRIPTEN__
     if (!std::holds_alternative<double>(args[0])) return Value(false);
     int id = (int)std::get<double>(args[0]);
     if (sockets.find(id) == sockets.end()) return Value(false);
     auto ws = static_cast<easywsclient::WebSocket*>(sockets[id].get());
     if(ws) {
         ws->close();
         sockets.erase(id);
     }
     return Value(true);
#else
     return Value(false);
#endif
  });
  
  globals->define("WS", wsInstance);

  auto jsonClass = std::make_shared<FSKClass>("JSON", nullptr, std::map<std::string, std::shared_ptr<FunctionCallable>>());
  auto jsonInstance = std::make_shared<FSKInstance>(jsonClass);

  jsonInstance->fields["parse"] = std::make_shared<NativeFunction>(1, [](Interpreter &interp, std::vector<Value> args) {
      if (!std::holds_alternative<std::string>(args[0])) return Value(std::monostate{});
      try {
          return interp.jsonParse(std::get<std::string>(args[0]));
      } catch (...) {
          return Value(std::monostate{});
      }
  });

  jsonInstance->fields["stringify"] = std::make_shared<NativeFunction>(1, [](Interpreter &interp, std::vector<Value> args) {
      try {
          return Value(interp.jsonStringify(args[0]));
      } catch (...) {
         return Value("");
      }
  });

  globals->define("JSON", jsonInstance);

  auto dateClass = std::make_shared<FSKClass>("Date", nullptr, std::map<std::string, std::shared_ptr<FunctionCallable>>());
  auto dateInstance = std::make_shared<FSKInstance>(dateClass);

  dateInstance->fields["now"] = std::make_shared<NativeFunction>(0, [](Interpreter &interp, std::vector<Value> args) {
      auto now = std::chrono::system_clock::now().time_since_epoch();
      return Value((double)std::chrono::duration_cast<std::chrono::seconds>(now).count());
  });

  dateInstance->fields["timestamp"] = std::make_shared<NativeFunction>(0, [](Interpreter &interp, std::vector<Value> args) {
      auto now = std::chrono::system_clock::now().time_since_epoch();
      return Value((double)std::chrono::duration_cast<std::chrono::seconds>(now).count());
  });

  dateInstance->fields["format"] = std::make_shared<NativeFunction>(2, [](Interpreter &interp, std::vector<Value> args) {
      if (!std::holds_alternative<double>(args[0]) || !std::holds_alternative<std::string>(args[1])) return Value(std::string(""));
      time_t time = (time_t)std::get<double>(args[0]);
      std::string format = std::get<std::string>(args[1]);
      std::tm tm = *std::localtime(&time);
      std::stringstream ss;
      ss << std::put_time(&tm, format.c_str());
      return Value(ss.str());
  });

  globals->define("Date", dateInstance);

  auto fsClass = std::make_shared<FSKClass>("FS", nullptr, std::map<std::string, std::shared_ptr<FunctionCallable>>());
  auto fsInstance = std::make_shared<FSKInstance>(fsClass);

  fsInstance->fields["exists"] = std::make_shared<NativeFunction>(1, [](Interpreter &interp, std::vector<Value> args) {
      if (!std::holds_alternative<std::string>(args[0])) return Value(false);
      std::string path = std::get<std::string>(args[0]);
      try { return Value(std::filesystem::exists(path)); } catch(...) { return Value(false); }
  });

  fsInstance->fields["read"] = std::make_shared<NativeFunction>(1, [](Interpreter &interp, std::vector<Value> args) {
      if (!std::holds_alternative<std::string>(args[0])) return Value(std::string(""));
      std::string path = std::get<std::string>(args[0]);
      std::ifstream t(path);
      std::stringstream buffer;
      buffer << t.rdbuf();
      return Value(buffer.str());
  });

  fsInstance->fields["write"] = std::make_shared<NativeFunction>(2, [](Interpreter &interp, std::vector<Value> args) {
      if (!std::holds_alternative<std::string>(args[0]) || !std::holds_alternative<std::string>(args[1])) return Value(false);
      std::string path = std::get<std::string>(args[0]);
      std::string content = std::get<std::string>(args[1]);
      std::ofstream t(path);
      t << content;
      t.close();
      return Value(true);
  });

  fsInstance->fields["append"] = std::make_shared<NativeFunction>(2, [](Interpreter &interp, std::vector<Value> args) {
      if (!std::holds_alternative<std::string>(args[0]) || !std::holds_alternative<std::string>(args[1])) return Value(false);
      std::string path = std::get<std::string>(args[0]);
      std::string content = std::get<std::string>(args[1]);
      std::ofstream t(path, std::ios::app);
      t << content;
      t.close();
      return Value(true);
  });

  fsInstance->fields["delete"] = std::make_shared<NativeFunction>(1, [](Interpreter &interp, std::vector<Value> args) {
      if (!std::holds_alternative<std::string>(args[0])) return Value(false);
      std::string path = std::get<std::string>(args[0]);
      try {
        return Value(std::filesystem::remove(path));
      } catch(...) { return Value(false); }
  });

  fsInstance->fields["mkdir"] = std::make_shared<NativeFunction>(1, [](Interpreter &interp, std::vector<Value> args) {
       if (!std::holds_alternative<std::string>(args[0])) return Value(false);
       std::string path = std::get<std::string>(args[0]);
       try {
         return Value(std::filesystem::create_directories(path));
       } catch(...) { return Value(false); }
  });

  fsInstance->fields["list"] = std::make_shared<NativeFunction>(1, [](Interpreter &interp, std::vector<Value> args) {
       if (!std::holds_alternative<std::string>(args[0])) return Value(std::make_shared<FSKArray>(std::vector<Value>{}));
       std::string path = std::get<std::string>(args[0]);
       std::vector<Value> files;
       try {
           if (std::filesystem::exists(path) && std::filesystem::is_directory(path)) {
               for (const auto & entry : std::filesystem::directory_iterator(path)) {
                   files.push_back(Value(entry.path().filename().string()));
               }
           }
       } catch(...){}
       return Value(std::make_shared<FSKArray>(files));
  });

  fsInstance->fields["copy"] = std::make_shared<NativeFunction>(2, [](Interpreter &interp, std::vector<Value> args) {
       if (!std::holds_alternative<std::string>(args[0]) || !std::holds_alternative<std::string>(args[1])) return Value(false);
       try {
           std::filesystem::copy(std::get<std::string>(args[0]), std::get<std::string>(args[1]), std::filesystem::copy_options::recursive);
           return Value(true);
       } catch(...) { return Value(false); }
  });

  fsInstance->fields["copy"] = std::make_shared<NativeFunction>(2, [](Interpreter &interp, std::vector<Value> args) {
       if (!std::holds_alternative<std::string>(args[0]) || !std::holds_alternative<std::string>(args[1])) return Value(false);
       try {
           std::filesystem::copy(std::get<std::string>(args[0]), std::get<std::string>(args[1]), std::filesystem::copy_options::recursive);
           return Value(true);
       } catch(...) { return Value(false); }
  });

  fsInstance->fields["move"] = std::make_shared<NativeFunction>(2, [](Interpreter &interp, std::vector<Value> args) {
       if (!std::holds_alternative<std::string>(args[0]) || !std::holds_alternative<std::string>(args[1])) return Value(false);
       try {
           std::filesystem::rename(std::get<std::string>(args[0]), std::get<std::string>(args[1]));
           return Value(true);
       } catch(...) { return Value(false); }
  });

  fsInstance->fields["walk"] = std::make_shared<NativeFunction>(1, [](Interpreter &interp, std::vector<Value> args) {
       if (!std::holds_alternative<std::string>(args[0])) return Value(std::make_shared<FSKArray>(std::vector<Value>{}));
       std::string path = std::get<std::string>(args[0]);
       std::vector<Value> files;
       try {
           if (std::filesystem::exists(path) && std::filesystem::is_directory(path)) {
               for (const auto & entry : std::filesystem::recursive_directory_iterator(path)) {
                   files.push_back(Value(entry.path().string()));
               }
           }
       } catch(...){}
       return Value(std::make_shared<FSKArray>(files));
  });

  globals->define("FS", fsInstance);

  auto regexClass = std::make_shared<FSKClass>("Regex", nullptr, std::map<std::string, std::shared_ptr<FunctionCallable>>());
  auto regexInstance = std::make_shared<FSKInstance>(regexClass);

  regexInstance->fields["match"] = std::make_shared<NativeFunction>(2, [](Interpreter &interp, std::vector<Value> args) {
      if (!std::holds_alternative<std::string>(args[0]) || !std::holds_alternative<std::string>(args[1])) return Value(false);
      std::string pattern = std::get<std::string>(args[0]);
      std::string text = std::get<std::string>(args[1]);
      try {
          std::regex re(pattern);
          return Value(std::regex_search(text, re));
      } catch(...) { return Value(false); }
  });

  regexInstance->fields["extract"] = std::make_shared<NativeFunction>(2, [](Interpreter &interp, std::vector<Value> args) {
       if (!std::holds_alternative<std::string>(args[0]) || !std::holds_alternative<std::string>(args[1])) return Value(std::make_shared<FSKArray>(std::vector<Value>{}));
       std::string pattern = std::get<std::string>(args[0]);
       std::string text = std::get<std::string>(args[1]);
       std::vector<Value> matches;
       try {
           std::regex re(pattern);
           std::smatch sm;
           std::string::const_iterator searchStart(text.cbegin());
           while (std::regex_search(searchStart, text.cend(), sm, re)) {
               matches.push_back(Value(sm[0].str()));
               searchStart = sm.suffix().first;
           }
       } catch(...) {}
       return Value(std::make_shared<FSKArray>(matches));
  });

  regexInstance->fields["replace"] = std::make_shared<NativeFunction>(3, [](Interpreter &interp, std::vector<Value> args) {
      if (!std::holds_alternative<std::string>(args[0]) || !std::holds_alternative<std::string>(args[1]) || !std::holds_alternative<std::string>(args[2])) return Value(std::string(""));
      std::string text = std::get<std::string>(args[0]);
      std::string pattern = std::get<std::string>(args[1]);
      std::string replacement = std::get<std::string>(args[2]);
      try {
          std::regex re(pattern);
          return Value(std::regex_replace(text, re, replacement));
      } catch(...) { return Value(text); }
  });

  globals->define("Regex", regexInstance);

  auto httpClass = std::make_shared<FSKClass>("HTTP", nullptr, std::map<std::string, std::shared_ptr<FunctionCallable>>());
  auto httpInstance = std::make_shared<FSKInstance>(httpClass);

   httpInstance->fields["httpGet"] = std::make_shared<NativeFunction>(
      1, [](Interpreter &interp, std::vector<Value> args) {
         if (!std::holds_alternative<std::string>(args[0])) return Value(std::monostate{}); 
         std::string url = std::get<std::string>(args[0]);

#ifndef __EMSCRIPTEN__
         CURL *curl;
         CURLcode res;
         std::string readBuffer;

         curl = curl_easy_init();
         if(curl) {
           curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
           curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](void *contents, size_t size, size_t nmemb, void *userp) -> size_t {
               ((std::string*)userp)->append((char*)contents, size * nmemb);
               return size * nmemb;
           });
           curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
           // Timeout basic setup
           curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
           curl_easy_setopt(curl, CURLOPT_USERAGENT, "FSK-Language/1.0");

           res = curl_easy_perform(curl);
           long response_code = 0;
           curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
           
           curl_easy_cleanup(curl);

           if (res == CURLE_OK) {
                // Return object with status and body
                auto respClass = std::make_shared<FSKClass>("HttpResponse", nullptr, std::map<std::string, std::shared_ptr<FunctionCallable>>());
                auto respInst = std::make_shared<FSKInstance>(respClass);
                respInst->fields["status"] = Value((double)response_code);
                respInst->fields["body"] = Value(readBuffer);
                return Value(respInst);
           }
         }
         return Value(std::monostate{});
#else
         return Value(std::string("httpGet not supported in WASM"));
#endif
      });

   httpInstance->fields["httpPost"] = std::make_shared<NativeFunction>(
      2, [](Interpreter &interp, std::vector<Value> args) {
         if (!std::holds_alternative<std::string>(args[0])) return Value(std::monostate{}); 
         std::string url = std::get<std::string>(args[0]);
         
         std::string postData = "";
         if (std::holds_alternative<std::string>(args[1])) {
             postData = std::get<std::string>(args[1]);
         } else if (std::holds_alternative<std::shared_ptr<FSKInstance>>(args[1])) {
              // serialize logic basic
         }

#ifndef __EMSCRIPTEN__
         CURL *curl;
         CURLcode res;
         std::string readBuffer;

         curl = curl_easy_init();
         if(curl) {
           curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
           curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());

           struct curl_slist *headers = NULL;
           headers = curl_slist_append(headers, "Content-Type: application/json");
           curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

           curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](void *contents, size_t size, size_t nmemb, void *userp) -> size_t {
               ((std::string*)userp)->append((char*)contents, size * nmemb);
               return size * nmemb;
           });
           curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

           res = curl_easy_perform(curl);
           
           long response_code = 0;
           curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

           curl_slist_free_all(headers);
           curl_easy_cleanup(curl);
           
           if (res == CURLE_OK) {
                auto respClass = std::make_shared<FSKClass>("HttpResponse", nullptr, std::map<std::string, std::shared_ptr<FunctionCallable>>());
                auto respInst = std::make_shared<FSKInstance>(respClass);
                respInst->fields["status"] = Value((double)response_code);
                respInst->fields["body"] = Value(readBuffer);
                return Value(respInst);
           }
         }
         return Value(std::monostate{});
#else
         return Value(std::string("httpPost not supported in WASM"));
#endif
      });

  globals->define("HTTP", httpInstance);




  

  std::vector<std::string> searchPaths = {
      "std/prelude.fsk",
      "../std/prelude.fsk",
      "/usr/local/lib/fsk/std/prelude.fsk",
      "C:/Fsk/std/prelude.fsk" 
  };
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
  if (stmt) {
    stmt->accept(*this);
  }
}


bool Interpreter::isEqual(Value a, Value b) {
  if (a.index() != b.index()) {
      return false;
  }

  bool res = false;
  if (std::holds_alternative<double>(a)) {
    res = (std::get<double>(a) == std::get<double>(b));
  } else if (std::holds_alternative<std::string>(a)) {
    res = (std::get<std::string>(a) == std::get<std::string>(b));
  } else if (std::holds_alternative<bool>(a)) {
    res = (std::get<bool>(a) == std::get<bool>(b));
  } else if (std::holds_alternative<std::monostate>(a)) {
    res = true;
  } else if (std::holds_alternative<std::shared_ptr<FSKArray>>(a)) {
    auto arrA = std::get<std::shared_ptr<FSKArray>>(a);
    auto arrB = std::get<std::shared_ptr<FSKArray>>(b);
    if (arrA->elements.size() != arrB->elements.size()) return false;
    for (size_t i = 0; i < arrA->elements.size(); i++) {
        if (!isEqual(arrA->elements[i], arrB->elements[i])) return false;
    }
    res = true;
  } else {
    res = (a == b);
  }
  return res;
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

  bindPattern(stmt.pattern, value, false);
}

void Interpreter::visitConstStmt(Const &stmt) {
  Value value = evaluate(stmt.initializer);
  bindPattern(stmt.pattern, value, true);
}

void Interpreter::visitMatchStmt(Match &stmt) {
  Value value = evaluate(stmt.expression);

  for (auto &arm : stmt.arms) {
    if (matchPattern(arm.first, value)) {
      std::shared_ptr<Environment> previousEnv = this->environment;
      this->environment = std::make_shared<Environment>(this->environment);
      try {
        bindPattern(arm.first, value, false);
        execute(arm.second);
        this->environment = previousEnv;
      } catch (...) {
        this->environment = previousEnv;
        throw;
      }
      return; 
    }
  }
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

void Interpreter::visitForStmt(For &stmt) {
  if (stmt.initializer != nullptr) execute(stmt.initializer);
  while (isTruthy(evaluate(stmt.condition))) {
    execute(stmt.body);
    if (stmt.increment != nullptr) evaluate(stmt.increment);
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
  this->lastValue = this->environment->get(expr.name);
}

void Interpreter::visitAssignExpr(Assign &expr) {
  Value value = evaluate(expr.value);
  this->environment->assign(expr.name, value);
  this->lastValue = value;
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
  if (std::holds_alternative<std::shared_ptr<FSKArray>>(object)) {
    auto arr = std::get<std::shared_ptr<FSKArray>>(object);
    if (expr.name.lexeme == "length") {
      lastValue = (double)arr->elements.size();
      return;
    }
    if (expr.name.lexeme == "push") {
      lastValue = std::make_shared<NativeFunction>(
          1, [arr](Interpreter &interp, std::vector<Value> args) {
            arr->elements.push_back(args[0]);
            return args[0];
          });
      return;
    }
    if (expr.name.lexeme == "pop") {
      lastValue = std::make_shared<NativeFunction>(
          0, [arr](Interpreter &interp, std::vector<Value> args) {
            if (arr->elements.empty())
              return Value(std::monostate{});
            Value v = arr->elements.back();
            arr->elements.pop_back();
            return v;
          });
      return;
    }
  }

  if (std::holds_alternative<std::string>(object)) {
    std::string s = std::get<std::string>(object);
    if (expr.name.lexeme == "length") {
      lastValue = (double)s.length();
      return;
    }
    if (expr.name.lexeme == "split") {
      lastValue = std::make_shared<NativeFunction>(
          1, [s](Interpreter &interp, std::vector<Value> args) {
            if (!std::holds_alternative<std::string>(args[0]))
              throw std::runtime_error("split attend une chaîne.");
            std::string delim = std::get<std::string>(args[0]);
            std::vector<Value> parts;
            size_t start = 0, end = 0;
            while ((end = s.find(delim, start)) != std::string::npos) {
              parts.push_back(s.substr(start, end - start));
              start = end + delim.length();
            }
            parts.push_back(s.substr(start));
            return std::make_shared<FSKArray>(parts);
          });
      return;
    }
    if (expr.name.lexeme == "trim") {
      lastValue = std::make_shared<NativeFunction>(
          0, [s](Interpreter &interp, std::vector<Value> args) {
            std::string res = s;
            res.erase(0, res.find_first_not_of(" \n\r\t"));
            res.erase(res.find_last_not_of(" \n\r\t") + 1);
            return res;
          });
      return;
    }
    if (expr.name.lexeme == "substr") {
      lastValue = std::make_shared<NativeFunction>(
          2, [s](Interpreter &interp, std::vector<Value> args) {
              if (!std::holds_alternative<double>(args[0]) || !std::holds_alternative<double>(args[1]))
                  throw std::runtime_error("substr attend (start, len).");
              int start = (int)std::get<double>(args[0]);
              int len = (int)std::get<double>(args[1]);
              if (start < 0 || (size_t)start >= s.length()) return Value(std::string(""));
              return Value(s.substr(start, len));
          });
      return;
    }
    if (expr.name.lexeme == "startsWith") {
        lastValue = std::make_shared<NativeFunction>(
          1, [s](Interpreter &interp, std::vector<Value> args) {
             if (!std::holds_alternative<std::string>(args[0])) return Value(false);
             std::string prefix = std::get<std::string>(args[0]);
             if (s.length() < prefix.length()) return Value(false);
             return Value(s.substr(0, prefix.length()) == prefix);
          });
        return;
    }
    if (expr.name.lexeme == "endsWith") {
        lastValue = std::make_shared<NativeFunction>(
          1, [s](Interpreter &interp, std::vector<Value> args) {
             if (!std::holds_alternative<std::string>(args[0])) return Value(false);
             std::string suffix = std::get<std::string>(args[0]);
             if (s.length() < suffix.length()) return Value(false);
             return Value(s.substr(s.length() - suffix.length()) == suffix);
          });
        return;
    }
    if (expr.name.lexeme == "toUpperCase") { 
        lastValue = std::make_shared<NativeFunction>(
          0, [s](Interpreter &interp, std::vector<Value> args) {
             std::string res = s;
             std::transform(res.begin(), res.end(), res.begin(), ::toupper);
             return Value(res);
          });
        return;
    }
    if (expr.name.lexeme == "toLowerCase") {
        lastValue = std::make_shared<NativeFunction>(
          0, [s](Interpreter &interp, std::vector<Value> args) {
             std::string res = s;
             std::transform(res.begin(), res.end(), res.begin(), ::tolower);
             return Value(res);
          });
        return;
    }
    if (expr.name.lexeme == "replace") {
        lastValue = std::make_shared<NativeFunction>(
          2, [s](Interpreter &interp, std::vector<Value> args) {
             if (!std::holds_alternative<std::string>(args[0]) || !std::holds_alternative<std::string>(args[1]))
                 return Value(s);
             std::string target = std::get<std::string>(args[0]);
             std::string replacement = std::get<std::string>(args[1]);
             if (target.empty()) return Value(s);
             std::string res = s;
             size_t pos = 0;
             while ((pos = res.find(target, pos)) != std::string::npos) {
                 res.replace(pos, target.length(), replacement);
                 pos += replacement.length();
             }
             return Value(res);
          });
        return;
    }
  }

  throw std::runtime_error("Seules les instances, tableaux et chaînes ont des propriétés.");
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

void Interpreter::visitTemplateLiteralExpr(TemplateLiteral &expr) {
  std::string result = "";
  for (size_t i = 0; i < expr.strings.size(); i++) {
    result += expr.strings[i];
    if (i < expr.expressions.size()) {
      result += stringify(evaluate(expr.expressions[i]));
    }
  }
  lastValue = result;
}

void Interpreter::visitArrayExpr(Array &expr) {
  std::vector<Value> elements;
  for (const auto &element : expr.elements) {
    elements.push_back(evaluate(element));
  }
  lastValue = std::make_shared<FSKArray>(elements);
}

void Interpreter::visitIndexExpr(IndexExpr &expr) {
  Value callee = evaluate(expr.callee);
  Value index = evaluate(expr.index);

  if (std::holds_alternative<std::shared_ptr<FSKArray>>(callee)) {
    if (!std::holds_alternative<double>(index)) {
      throw std::runtime_error("Index must be a number.");
    }
    auto arr = std::get<std::shared_ptr<FSKArray>>(callee);
    int idx = (int)std::get<double>(index);
    if (idx < 0 || (size_t)idx >= arr->elements.size()) {
       throw std::runtime_error("Index out of bounds.");
    }
    lastValue = arr->elements[idx];
    return;
  }
  
  if (std::holds_alternative<std::string>(callee)) {
    if (!std::holds_alternative<double>(index)) {
        throw std::runtime_error("Index must be a number.");
    }
    std::string s = std::get<std::string>(callee);
    int idx = (int)std::get<double>(index);
    if (idx < 0 || (size_t)idx >= s.length()) {
        throw std::runtime_error("Index out of bounds.");
    }
    lastValue = std::string(1, s[idx]);
    return;
  }

  if (std::holds_alternative<std::shared_ptr<FSKInstance>>(callee)) {
      if (!std::holds_alternative<std::string>(index)) {
          throw std::runtime_error("Index must be a string for objects.");
      }
      std::string key = std::get<std::string>(index);
      auto inst = std::get<std::shared_ptr<FSKInstance>>(callee);
      if (inst->fields.count(key)) {
          lastValue = inst->fields[key];
      } else {
          lastValue = Value(std::monostate{}); 
      }
      return;
  }

  throw std::runtime_error("Only arrays, objects and strings can be indexed.");
}

void Interpreter::visitIndexSetExpr(IndexSet &expr) {
  Value callee = evaluate(expr.callee);
  Value index = evaluate(expr.index);
  Value value = evaluate(expr.value);

  if (std::holds_alternative<std::shared_ptr<FSKArray>>(callee)) {
    if (!std::holds_alternative<double>(index)) {
      throw std::runtime_error("Index must be a number.");
    }
    auto arr = std::get<std::shared_ptr<FSKArray>>(callee);
    int idx = (int)std::get<double>(index);
    if (idx < 0 || (size_t)idx >= arr->elements.size()) {
       throw std::runtime_error("Index out of bounds.");
    }
    arr->elements[idx] = value;
    lastValue = value;
    return;
  }

  if (std::holds_alternative<std::shared_ptr<FSKInstance>>(callee)) {
      if (!std::holds_alternative<std::string>(index)) {
          throw std::runtime_error("Index must be a string for objects.");
      }
      std::string key = std::get<std::string>(index);
      auto inst = std::get<std::shared_ptr<FSKInstance>>(callee);
      inst->fields[key] = value;
      lastValue = value;
      return;
  }

  throw std::runtime_error("Only arrays and objects can have indexed assignments.");
}

bool Interpreter::isTruthy(Value value) {
  if (std::holds_alternative<std::monostate>(value))
    return false;
  if (std::holds_alternative<bool>(value))
    return std::get<bool>(value);
  return true;
}




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
  if (std::holds_alternative<std::shared_ptr<FSKArray>>(value)) {
    auto arr = std::get<std::shared_ptr<FSKArray>>(value);
    std::string res = "[";
    for (size_t i = 0; i < arr->elements.size(); i++) {
      res += stringify(arr->elements[i]);
      if (i < arr->elements.size() - 1) res += ", ";
    }
    res += "]";
    return res;
  }
  return "Object";
}

void Interpreter::bindPattern(std::shared_ptr<Expr> pat, Value value, bool isConst) {
  if (Variable *v = dynamic_cast<Variable *>(pat.get())) {
    this->environment->define(v->name.lexeme, value);
    return;
  }

  if (Array *a = dynamic_cast<Array *>(pat.get())) {
    if (!std::holds_alternative<std::shared_ptr<FSKArray>>(value)) {
      throw std::runtime_error("Cannot destructure non-array value.");
    }
    auto arr = std::get<std::shared_ptr<FSKArray>>(value);
    for (size_t i = 0; i < a->elements.size(); i++) {
      if (i < arr->elements.size()) {
        bindPattern(a->elements[i], arr->elements[i], isConst);
      } else {
        bindPattern(a->elements[i], std::monostate{}, isConst);
      }
    }
    return;
  }
}

bool Interpreter::matchPattern(std::shared_ptr<Expr> pat, Value value) {
  if (Literal *l = dynamic_cast<Literal *>(pat.get())) {
    return isEqual(l->value, value);
  }

  if (Array *a = dynamic_cast<Array *>(pat.get())) {
    if (!std::holds_alternative<std::shared_ptr<FSKArray>>(value)) return false;
    auto arr = std::get<std::shared_ptr<FSKArray>>(value);
    if (a->elements.size() != arr->elements.size()) return false;
    for (size_t i = 0; i < a->elements.size(); i++) {
        if (!matchPattern(a->elements[i], arr->elements[i])) return false;
    }
    return true;
  }

  if (Variable *v = dynamic_cast<Variable *>(pat.get())) {
     return true;
  }

  return false;
}






void Interpreter::visitImportStmt(Import &stmt) {
  Value value = evaluate(stmt.file);
  if (!std::holds_alternative<std::string>(value)) {
    throw std::runtime_error("Import path must be a string.");
  }
  std::string path = std::get<std::string>(value);

  std::ifstream file(path);
  if (!file.is_open()) {
      std::string rawPath = path;
      std::string attempt = path;
      if (attempt.find(".fsk") == std::string::npos) attempt += ".fsk";

      std::vector<std::string> searchPaths = {
          attempt,
          "fsk_modules/" + attempt,
          "fsk_modules/" + rawPath + "/index.fsk",
          ".system/fsk_modules/" + attempt,
          ".system/fsk_modules/" + rawPath + "/index.fsk",
          "std/" + attempt,
          ".system/std/" + attempt,
          "../" + attempt,
          "../std/" + attempt,
          "/usr/local/lib/fsk/" + attempt,
          "/usr/local/lib/fsk/std/" + attempt,
          "/usr/local/lib/fsk/fsk_modules/" + rawPath + "/index.fsk",
          "C:/Fsk/" + attempt,
          "C:/Fsk/std/" + attempt,
          "C:/Fsk/fsk_modules/" + rawPath + "/index.fsk"
      };

      for (const auto& s : searchPaths) {
          file.open(s);
          if (file.is_open()) break;
      }

      if (!file.is_open()) {
          throw std::runtime_error("Could not open file: " + path);
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

void Interpreter::setArgs(int argc, char *argv[]) {
    scriptArgs.clear();
    for(int i = 0; i < argc; i++) {
        scriptArgs.push_back(std::string(argv[i]));
    }
}


std::string Interpreter::jsonStringify(Value value) {
    return valueToJson(value).dump();
}

Value Interpreter::jsonParse(std::string source) {
    auto j = json::parse(source);
    return jsonToValue(j);
}
