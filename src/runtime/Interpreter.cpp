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
#include "HttpCallback.hpp"
#include <iostream>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#define popen _popen
#define pclose _pclose
#else
#ifndef __EMSCRIPTEN__
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/inotify.h>
#include <fcntl.h>
#else
#include <emscripten.h>
#include <unistd.h> 
#endif
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
#include <openssl/sha.h>
#include <openssl/md5.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#endif

using json = nlohmann::json;

#ifndef __EMSCRIPTEN__
extern "C" {
    char* fsk_fetch_blocking(const char* url);
    void fsk_free_string(char* s);
    
    typedef void (*FskHttpCallback)(uint64_t, const char*, const char*, const char*, void*);
    void fsk_start_server(uint16_t port, FskHttpCallback callback, void* context);
    void fsk_http_respond(uint64_t req_id, uint16_t status, const char* body);

    typedef void (*FskWsCallback)(uint32_t, const char*, void*);
    void fsk_ws_listen(uint16_t port, FskWsCallback callback, void* context);
    void fsk_ws_send(uint32_t ws_id, const char* message);

    char* fsk_crypto_sha256(const char* input);
    char* fsk_crypto_md5(const char* input);

    char* fsk_system_get_info();

    uint32_t fsk_sql_open(const char* path);
    char* fsk_sql_query(uint32_t db_id, const char* query);

    void fsk_vm_run(const uint8_t* bytecode, size_t bytecode_len, const double* constants, size_t constants_len);

    uint64_t fsk_ffi_open(const char* path);
    int32_t fsk_ffi_call_void_string(uint64_t lib_id, const char* symbol, const char* arg);
    int32_t fsk_ffi_call_void_string(uint64_t lib_id, const char* symbol, const char* arg);
    int32_t fsk_ffi_call_int_int(uint64_t lib_id, const char* symbol, int32_t arg);

    void fsk_ffi_call_void(uint64_t lib_id, const char* symbol);
    bool fsk_ffi_call_bool(uint64_t lib_id, const char* symbol);
    void fsk_ffi_call_void_int_int_string(uint64_t lib_id, const char* symbol, int32_t a1, int32_t a2, const char* a3);
    void fsk_ffi_call_void_int_int(uint64_t lib_id, const char* symbol, int32_t a1, int32_t a2);
    void fsk_ffi_call_void_4int(uint64_t lib_id, const char* symbol, int32_t a1, int32_t a2, int32_t a3, int32_t a4);
    void fsk_ffi_call_void_5int(uint64_t lib_id, const char* symbol, int32_t a1, int32_t a2, int32_t a3, int32_t a4, int32_t a5);
    void fsk_ffi_call_void_6int(uint64_t lib_id, const char* symbol, int32_t a1, int32_t a2, int32_t a3, int32_t a4, int32_t a5, int32_t a6);
    void fsk_ffi_call_void_8int(uint64_t lib_id, const char* symbol, int32_t a1, int32_t a2, int32_t a3, int32_t a4, int32_t a5, int32_t a6, int32_t a7, int32_t a8);
    void fsk_ffi_call_void_2int_float_int(uint64_t lib_id, const char* symbol, int32_t a1, int32_t a2, float a3, int32_t a4);
    void fsk_ffi_call_void_string_4int(uint64_t lib_id, const char* symbol, const char* s1, int32_t a2, int32_t a3, int32_t a4, int32_t a5);
}
#endif

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

class LibraryCallable : public Callable {
public:
    uint64_t libId;
    LibraryCallable(uint64_t id) : libId(id) {}

    int arity() override { return 0; }
    Value call(Interpreter &interpreter, std::vector<Value> arguments) override { return Value(0.0); }
    std::string toString() override { return "<Native Library>"; }
};

Interpreter::Interpreter() {
  eventLoop = std::make_shared<EventLoop>();
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
#ifdef __EMSCRIPTEN__
            emscripten_sleep(ms);
#else
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
#endif
        }
        return Value(0.0);
      }));

  globals->define("setTimeout", std::make_shared<NativeFunction>(
      2, [](Interpreter &interp, std::vector<Value> args) {
        if (!std::holds_alternative<std::shared_ptr<Callable>>(args[0]) ||
            !std::holds_alternative<double>(args[1])) {
          throw std::runtime_error("setTimeout attend (callback, delay_ms).");
        }
        auto callable = std::get<std::shared_ptr<Callable>>(args[0]);
        int ms = (int)std::get<double>(args[1]);
        int id = interp.eventLoop->setTimeout([&interp, callable]() {
            try { callable->call(interp, {}); } catch(...) {}
        }, std::chrono::milliseconds(ms));
        return Value((double)id);
      }));

  globals->define("setInterval", std::make_shared<NativeFunction>(
      2, [](Interpreter &interp, std::vector<Value> args) {
        if (!std::holds_alternative<std::shared_ptr<Callable>>(args[0]) ||
            !std::holds_alternative<double>(args[1])) {
          throw std::runtime_error("setInterval attend (callback, interval_ms).");
        }
        auto callable = std::get<std::shared_ptr<Callable>>(args[0]);
        int ms = (int)std::get<double>(args[1]);
        int id = interp.eventLoop->setInterval([&interp, callable]() {
            try { callable->call(interp, {}); } catch(...) {}
        }, std::chrono::milliseconds(ms));
        return Value((double)id);
      }));

  globals->define("clearTimeout", std::make_shared<NativeFunction>(
      1, [](Interpreter &interp, std::vector<Value> args) {
        if (!std::holds_alternative<double>(args[0])) {
          throw std::runtime_error("clearTimeout attend un ID.");
        }
        interp.eventLoop->cancelTimer((int)std::get<double>(args[0]));
        return Value(std::monostate{});
      }));

  globals->define("clearInterval", std::make_shared<NativeFunction>(
      1, [](Interpreter &interp, std::vector<Value> args) {
        if (!std::holds_alternative<double>(args[0])) {
          throw std::runtime_error("clearInterval attend un ID.");
        }
        interp.eventLoop->cancelTimer((int)std::get<double>(args[0]));
        return Value(std::monostate{});
      }));

  auto promiseClass = std::make_shared<FSKClass>("Promise", nullptr, std::map<std::string, std::shared_ptr<FunctionCallable>>());
  globals->define("Promise", promiseClass);

  fskInstance->fields["fetch"] = std::make_shared<NativeFunction>(
      1, std::function<Value(Interpreter &, std::vector<Value>)>(
             [=](Interpreter &interp, std::vector<Value> args) -> Value {
               if (!std::holds_alternative<std::string>(args[0])) {
                 throw std::runtime_error("fetch attend une URL.");
               }
               std::string url = std::get<std::string>(args[0]);

               auto pClassVal = interp.globals->get("Promise");
               auto pClass = std::static_pointer_cast<FSKClass>(
                   std::get<std::shared_ptr<Callable>>(pClassVal));
               std::shared_ptr<FSKInstance> pInst = std::make_shared<FSKInstance>(pClass);
               pInst->fields["status"] = std::string("pending");
               pInst->fields["onResolve"] =
                   std::make_shared<FSKArray>(std::vector<Value>());

               pInst->fields["then"] = std::make_shared<NativeFunction>(
                   1, std::function<Value(Interpreter &, std::vector<Value>)>(
                          [pInst](Interpreter &i, std::vector<Value> a) -> Value {
                            auto cb = a[0];
                            if (std::get<std::string>(
                                    pInst->fields["status"]) == "resolved") {
                              if (auto c = std::get_if<std::shared_ptr<Callable>>(
                                      &cb))
                                (*c)->call(i, {pInst->fields["value"]});
                            } else {
                              std::get<std::shared_ptr<FSKArray>>(
                                  pInst->fields["onResolve"])
                                  ->elements.push_back(cb);
                            }
                            return Value(pInst);
                          }));

               interp.eventLoop->incrementWorkCount();
               std::thread([this, url, pInst]() {
                 char *result = fsk_fetch_blocking(url.c_str());
                 std::string body_res(result);
                 fsk_free_string(result);

                 this->eventLoop->post([this, pInst, body_res]() {
                   pInst->fields["status"] = std::string("resolved");
                   pInst->fields["value"] = body_res;
                   auto callbacks = std::get<std::shared_ptr<FSKArray>>(
                       pInst->fields["onResolve"]);
                   for (auto &cb : callbacks->elements) {
                     if (auto c = std::get_if<std::shared_ptr<Callable>>(&cb))
                       (*c)->call(*this, {Value(body_res)});
                   }
                   this->eventLoop->decrementWorkCount();
                 });
               }).detach();

               return Value(pInst);
             }));

#ifdef __EMSCRIPTEN__
   fskInstance->fields["startServer"] = std::make_shared<NativeFunction>(
      2, [](Interpreter &interp, std::vector<Value> args) {
        if (!std::holds_alternative<double>(args[0])) throw std::runtime_error("Port required");
        auto handler = std::get<std::shared_ptr<Callable>>(args[1]);
        
        std::cout << "[WEB] Server Mock Started. Listening for 'request.tmp'..." << std::endl;
        
        while(true) {
            emscripten_sleep(100);
            
            std::ifstream reqFile("request.tmp");
            if (reqFile.is_open()) {
                std::stringstream buffer;
                buffer << reqFile.rdbuf();
                std::string request = buffer.str();
                reqFile.close();
                std::remove("request.tmp");
                
                std::cout << "[WEB] Handling Request: " << request.substr(0, 50) << "..." << std::endl;
                
                Value res = handler->call(interp, {Value(request)});
                std::string response = interp.stringify(res);
                
                std::ofstream resFile("response.tmp");
                resFile << response;
                resFile.close();
            }
        }
        return Value(true);
      });
#endif
  
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
#ifdef __EMSCRIPTEN__
        emscripten_sleep(ms);
#else
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
#endif
        return Value(true);
      });

#ifndef __EMSCRIPTEN__
  fskInstance->fields["listen"] = std::make_shared<NativeFunction>(
      2, [](Interpreter &interp, std::vector<Value> args) {
        if (!std::holds_alternative<double>(args[0])) {
          throw std::runtime_error("listen attend un port (nombre).");
        }
        int port = (int)std::get<double>(args[0]);

        if (!std::holds_alternative<std::shared_ptr<Callable>>(args[1])) {
           throw std::runtime_error("listen attend une fonction handler.");
        }
        
        interp.globals->define("onHttpRequest", args[1]);
        interp.eventLoop->incrementWorkCount();

        std::thread([port, &interp]() {
            fsk_start_server((uint16_t)port, fsk_on_http_request, &interp);
        }).detach();

        return Value(true);
      });
#endif

  fskInstance->fields["shell"] = std::make_shared<NativeFunction>(
      1, [](Interpreter &interp, std::vector<Value> args) {
        if (!std::holds_alternative<std::string>(args[0])) {
          throw std::runtime_error("shell attend une commande (string).");
        }
        std::string cmd = std::get<std::string>(args[0]);
#ifndef __EMSCRIPTEN__
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
#else
        return Value(std::string("Shell commands not supported in WASM."));
#endif
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

  fskInstance->fields["split"] = std::make_shared<NativeFunction>(
      2, [](Interpreter &interp, std::vector<Value> args) {
        if (!std::holds_alternative<std::string>(args[0]) ||
            !std::holds_alternative<std::string>(args[1])) {
          throw std::runtime_error("split: (str, delimiter) required.");
        }
        std::string s = std::get<std::string>(args[0]);
        std::string delimiter = std::get<std::string>(args[1]);
        std::vector<Value> parts;
        size_t start = 0, end;
        while ((end = s.find(delimiter, start)) != std::string::npos) {
          parts.push_back(Value(s.substr(start, end - start)));
          start = end + delimiter.length();
        }
        parts.push_back(Value(s.substr(start)));
        return Value(std::make_shared<FSKArray>(parts));
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

    fskInstance->fields["readFile"] = std::make_shared<NativeFunction>(
      1, [](Interpreter &interp, std::vector<Value> args) {
         if (!std::holds_alternative<std::string>(args[0])) return Value(std::string(""));
         std::string path = std::get<std::string>(args[0]);
         std::ifstream f(path);
         if (!f.is_open()) return Value(std::string(""));
         std::stringstream buffer;
         buffer << f.rdbuf();
         return Value(buffer.str());
      });

    fskInstance->fields["writeFile"] = std::make_shared<NativeFunction>(
      2, [](Interpreter &interp, std::vector<Value> args) {
         if (!std::holds_alternative<std::string>(args[0]) || 
             !std::holds_alternative<std::string>(args[1])) return Value(false);
         std::string path = std::get<std::string>(args[0]);
         std::string content = std::get<std::string>(args[1]);
         std::ofstream f(path);
         if (!f.is_open()) return Value(false);
         f << content;
         f.close();
         return Value(true);
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

    fskInstance->fields["trim"] = std::make_shared<NativeFunction>(
      1, [](Interpreter &interp, std::vector<Value> args) {
         if (!std::holds_alternative<std::string>(args[0])) return args[0];
         std::string s = std::get<std::string>(args[0]);
         if (s.empty()) return args[0];
         s.erase(0, s.find_first_not_of(" \t\r\n"));
         if (s.empty()) return Value(std::string(""));
         s.erase(s.find_last_not_of(" \t\r\n") + 1);
         return Value(s);
      });

    fskInstance->fields["startsWith"] = std::make_shared<NativeFunction>(
      2, [](Interpreter &interp, std::vector<Value> args) {
         if (!std::holds_alternative<std::string>(args[0]) || 
             !std::holds_alternative<std::string>(args[1])) {
              return Value(false);
         }
         std::string s = std::get<std::string>(args[0]);
         std::string prefix = std::get<std::string>(args[1]);
         if (prefix.length() > s.length()) return Value(false);
         return Value(s.compare(0, prefix.length(), prefix) == 0);
      });

    fskInstance->fields["endsWith"] = std::make_shared<NativeFunction>(
      2, [](Interpreter &interp, std::vector<Value> args) {
         if (!std::holds_alternative<std::string>(args[0]) || 
             !std::holds_alternative<std::string>(args[1])) {
              return Value(false);
         }
         std::string s = std::get<std::string>(args[0]);
         std::string suffix = std::get<std::string>(args[1]);
         if (suffix.length() > s.length()) return Value(false);
         return Value(s.compare(s.length() - suffix.length(), suffix.length(), suffix) == 0);
      });
   fskInstance->fields["E"] = Value(2.71828182845904523536);

   auto wsNInstance = std::make_shared<FSKInstance>(std::make_shared<FSKClass>("WS", nullptr, std::map<std::string, std::shared_ptr<FunctionCallable>>()));
   wsNInstance->fields["listen"] = std::make_shared<NativeFunction>(2, [](Interpreter &interp, std::vector<Value> args) {
       if (!std::holds_alternative<double>(args[0])) throw std::runtime_error("WS.listen requires port");
       int port = (int)std::get<double>(args[0]);
       interp.globals->define("onWsMessage", args[1]);
       interp.eventLoop->incrementWorkCount();
       std::thread([port, &interp]() {
           fsk_ws_listen((uint16_t)port, fsk_on_ws_message, &interp);
       }).detach();
       return Value(true);
   });
   
   globals->define("WS", wsNInstance);

  globals->define("FSK", fskInstance);

   auto ffiClass = std::make_shared<FSKClass>("FFI", nullptr, std::map<std::string, std::shared_ptr<FunctionCallable>>());
   auto ffiInstance = std::make_shared<FSKInstance>(ffiClass);

   ffiInstance->fields["open"] = std::make_shared<NativeFunction>(1, [](Interpreter &interp, std::vector<Value> args) {
       if (!std::holds_alternative<std::string>(args[0])) throw std::runtime_error("FFI.open requires path");
       std::string path = std::get<std::string>(args[0]);
       
       uint64_t id = fsk_ffi_open(path.c_str());
       if (id == 0) return Value(false);

       auto libClass = std::make_shared<FSKClass>("Library", nullptr, std::map<std::string, std::shared_ptr<FunctionCallable>>());
       auto libInst = std::make_shared<FSKInstance>(libClass);
              // lib.call("symbol", ...args)
        libInst->fields["call"] = std::make_shared<NativeFunction>(-1, [id](Interpreter &interp, std::vector<Value> args) {
            if (args.empty() || !std::holds_alternative<std::string>(args[0])) throw std::runtime_error("Lib.call requires symbol");
            std::string symbol = std::get<std::string>(args[0]);
            
            if (args.size() == 1) { // 0 Args
                fsk_ffi_call_void(id, symbol.c_str());
                return Value(std::monostate{}); 
            }

            if (args.size() == 2) { // 1 Arg
                if (std::holds_alternative<double>(args[1])) {
                    uint32_t val = (uint32_t)std::get<double>(args[1]);
                    int32_t res = fsk_ffi_call_int_int(id, symbol.c_str(), (int32_t)val);
                    return Value((double)res);
                } else if (std::holds_alternative<std::string>(args[1])) {
                    fsk_ffi_call_void_string(id, symbol.c_str(), std::get<std::string>(args[1]).c_str());
                    return Value(std::monostate{});
                }
            }

            if (args.size() == 3) { // 2 Args (int, int)
                fsk_ffi_call_void_int_int(id, symbol.c_str(), (int32_t)std::get<double>(args[1]), (int32_t)std::get<double>(args[2]));
                return Value(std::monostate{});
            }

            // 3 Args (int, int, string) or (int, int, int)
            if (args.size() == 4) {
                if (std::holds_alternative<std::string>(args[3])) {
                    fsk_ffi_call_void_int_int_string(id, symbol.c_str(), 
                        (int32_t)std::get<double>(args[1]), (int32_t)std::get<double>(args[2]), std::get<std::string>(args[3]).c_str());
                } else {
                    fsk_ffi_call_void_4int(id, symbol.c_str(), (int32_t)std::get<double>(args[1]), (int32_t)std::get<double>(args[2]), (int32_t)std::get<double>(args[3]), (int32_t)(uint32_t)std::get<double>(args[3]));
                }
                return Value(std::monostate{});
            }

            if (args.size() == 5) { // 4 Args
                 if (symbol == "DrawCircle" && std::holds_alternative<double>(args[3])) {
                     fsk_ffi_call_void_2int_float_int(id, symbol.c_str(), (int32_t)std::get<double>(args[1]), (int32_t)std::get<double>(args[2]), (float)std::get<double>(args[3]), (int32_t)(uint32_t)std::get<double>(args[4]));
                 } else {
                     fsk_ffi_call_void_4int(id, symbol.c_str(), (int32_t)std::get<double>(args[1]), (int32_t)std::get<double>(args[2]), (int32_t)std::get<double>(args[3]), (int32_t)(uint32_t)std::get<double>(args[4]));
                 }
                 return Value(std::monostate{});
            }

            if (args.size() == 6) { // 5 Args
                if (symbol == "DrawText") {
                    fsk_ffi_call_void_string_4int(id, symbol.c_str(), std::get<std::string>(args[1]).c_str(), (int32_t)std::get<double>(args[2]), (int32_t)std::get<double>(args[3]), (int32_t)std::get<double>(args[4]), (int32_t)(uint32_t)std::get<double>(args[5]));
                } else {
                    fsk_ffi_call_void_5int(id, symbol.c_str(), (int32_t)std::get<double>(args[1]), (int32_t)std::get<double>(args[2]), (int32_t)std::get<double>(args[3]), (int32_t)std::get<double>(args[4]), (int32_t)(uint32_t)std::get<double>(args[5]));
                }
                return Value(std::monostate{});
            }

            if (args.size() == 7) { // 6 Args
                fsk_ffi_call_void_6int(id, symbol.c_str(), (int32_t)std::get<double>(args[1]), (int32_t)std::get<double>(args[2]), (int32_t)std::get<double>(args[3]), (int32_t)std::get<double>(args[4]), (int32_t)std::get<double>(args[5]), (int32_t)(uint32_t)std::get<double>(args[6]));
                return Value(std::monostate{});
            }

            if (args.size() == 8) { // 7 Args -> DrawTriangle(x1, y1, x2, y2, x3, y3, color)
                fsk_ffi_call_void_8int(id, symbol.c_str(), 
                    (int32_t)std::get<double>(args[1]), (int32_t)std::get<double>(args[2]),
                    (int32_t)std::get<double>(args[3]), (int32_t)std::get<double>(args[4]),
                    (int32_t)std::get<double>(args[5]), (int32_t)std::get<double>(args[6]),
                    (int32_t)(uint32_t)std::get<double>(args[7]), 0); // 8th is padding
                return Value(std::monostate{});
            }

            return Value(0.0);
        });
        libInst->fields["callBool"] = std::make_shared<NativeFunction>(-1, [id](Interpreter &interp, std::vector<Value> args) {
            if (args.empty() || !std::holds_alternative<std::string>(args[0])) throw std::runtime_error("Lib.callBool requires symbol");
            std::string symbol = std::get<std::string>(args[0]);
            
            if (args.size() == 1) {
                return Value(fsk_ffi_call_bool(id, symbol.c_str()));
            }

            if (args.size() == 2) {
                if (std::holds_alternative<double>(args[1])) {
                    int32_t res = fsk_ffi_call_int_int(id, symbol.c_str(), (int32_t)std::get<double>(args[1]));
                    return Value(res != 0);
                }
            }

            return Value(false);
        });

       return Value(libInst);
   });

   globals->define("FFI", ffiInstance);


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

   sqlInstance->fields["open"] = std::make_shared<NativeFunction>(1, [](Interpreter &interp, std::vector<Value> args) {
      if (!std::holds_alternative<std::string>(args[0])) return Value(0.0);
      return Value((double)fsk_sql_open(std::get<std::string>(args[0]).c_str()));
   });

   sqlInstance->fields["query"] = std::make_shared<NativeFunction>(2, [](Interpreter &interp, std::vector<Value> args) {
      if (!std::holds_alternative<double>(args[0]) || !std::holds_alternative<std::string>(args[1])) return Value(std::monostate{}); 
      char* res = fsk_sql_query((uint32_t)std::get<double>(args[0]), std::get<std::string>(args[1]).c_str());
      std::string result(res);
      fsk_free_string(res);
      return interp.jsonParse(result);
   });

   globals->define("SQL", sqlInstance);

   auto systemClass = std::make_shared<FSKClass>("System", nullptr, std::map<std::string, std::shared_ptr<FunctionCallable>>());
   auto systemInstance = std::make_shared<FSKInstance>(systemClass);
   systemInstance->fields["getInfo"] = std::make_shared<NativeFunction>(0, [](Interpreter &interp, std::vector<Value> args) {
      char* res = fsk_system_get_info();
      std::string result(res);
      fsk_free_string(res);
      return interp.jsonParse(result);
   });
   globals->define("System", systemInstance);

   auto vmClass = std::make_shared<FSKClass>("VM", nullptr, std::map<std::string, std::shared_ptr<FunctionCallable>>());
   auto vmInstance = std::make_shared<FSKInstance>(vmClass);
   vmInstance->fields["run"] = std::make_shared<NativeFunction>(2, [](Interpreter &interp, std::vector<Value> args) {
       if (!std::holds_alternative<std::shared_ptr<FSKArray>>(args[0]) || !std::holds_alternative<std::shared_ptr<FSKArray>>(args[1])) {
           throw std::runtime_error("VM.run needs (bytecode_array, constants_array)");
       }
       auto bArr = std::get<std::shared_ptr<FSKArray>>(args[0]);
       auto cArr = std::get<std::shared_ptr<FSKArray>>(args[1]);

       std::vector<uint8_t> bytecode;
       for (auto& v : bArr->elements) {
           if (std::holds_alternative<double>(v)) bytecode.push_back((uint8_t)std::get<double>(v));
       }

       std::vector<double> constants;
       for (auto& v : cArr->elements) {
           if (std::holds_alternative<double>(v)) constants.push_back(std::get<double>(v));
       }

       fsk_vm_run(bytecode.data(), bytecode.size(), constants.data(), constants.size());
       return Value(true);
   });
   globals->define("VM", vmInstance);

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

  auto cryptoClass = std::make_shared<FSKClass>("Crypto", nullptr, std::map<std::string, std::shared_ptr<FunctionCallable>>());
  auto cryptoInstance = std::make_shared<FSKInstance>(cryptoClass);

   cryptoInstance->fields["sha256"] = std::make_shared<NativeFunction>(
       1, [](Interpreter &interp, std::vector<Value> args) {
         if (!std::holds_alternative<std::string>(args[0])) return Value(std::string(""));
         char* res = fsk_crypto_sha256(std::get<std::string>(args[0]).c_str());
         std::string result(res);
         fsk_free_string(res);
         return Value(result);
       });

   cryptoInstance->fields["md5"] = std::make_shared<NativeFunction>(
       1, [](Interpreter &interp, std::vector<Value> args) {
         if (!std::holds_alternative<std::string>(args[0])) return Value(std::string(""));
         char* res = fsk_crypto_md5(std::get<std::string>(args[0]).c_str());
         std::string result(res);
         fsk_free_string(res);
         return Value(result);
       });

  cryptoInstance->fields["base64Encode"] = std::make_shared<NativeFunction>(
    1, [](Interpreter &interp, std::vector<Value> args) {
        if (!std::holds_alternative<std::string>(args[0])) return Value(std::string(""));
#ifndef __EMSCRIPTEN__
        std::string input = std::get<std::string>(args[0]);
        BIO *bio, *b64;
        BUF_MEM *bufferPtr;
        b64 = BIO_new(BIO_f_base64());
        bio = BIO_new(BIO_s_mem());
        bio = BIO_push(b64, bio);
        BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL); 
        BIO_write(bio, input.c_str(), input.length());
        BIO_flush(bio);
        BIO_get_mem_ptr(bio, &bufferPtr);
        std::string result(bufferPtr->data, bufferPtr->length);
        BIO_free_all(bio);
        return Value(result);
#else
        return Value(std::string("Base64 not supported in WASM yet"));
#endif
    });

  cryptoInstance->fields["base64Decode"] = std::make_shared<NativeFunction>(
    1, [](Interpreter &interp, std::vector<Value> args) {
        if (!std::holds_alternative<std::string>(args[0])) return Value(std::string(""));
#ifndef __EMSCRIPTEN__
        std::string input = std::get<std::string>(args[0]);
        BIO *bio, *b64;
        char *buffer = (char *)malloc(input.length());
        b64 = BIO_new(BIO_f_base64());
        bio = BIO_new_mem_buf(input.c_str(), input.length());
        bio = BIO_push(b64, bio);
        BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
        int decodedLength = BIO_read(bio, buffer, input.length());
        buffer[decodedLength] = '\0'; 
        std::string result(buffer, decodedLength);
        BIO_free_all(bio);
        free(buffer);
        return Value(result);
#else
        return Value(std::string("Base64 not supported in WASM yet"));
#endif
    });

  globals->define("Crypto", cryptoInstance);

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

#ifndef __EMSCRIPTEN__
#ifndef _WIN32
#endif
#endif

  fsInstance->fields["watch"] = std::make_shared<NativeFunction>(1, [](Interpreter &interp, std::vector<Value> args) {
#if !defined(__EMSCRIPTEN__) && !defined(_WIN32)
      if (!std::holds_alternative<std::string>(args[0])) return Value(-1.0);
      std::string path = std::get<std::string>(args[0]);
      
      int fd = inotify_init1(IN_NONBLOCK); 
      if (fd < 0) return Value(-1.0);
      
      int wd = inotify_add_watch(fd, path.c_str(), IN_MODIFY | IN_CREATE | IN_DELETE);
      if (wd < 0) {
          close(fd);
          return Value(-1.0);
      }
      
      int id = interp.fsWatcherId++;
      interp.fsWatchers[id] = fd;
      return Value((double)id);
#else
      return Value(-1.0);
#endif
  });

  fsInstance->fields["poll"] = std::make_shared<NativeFunction>(1, [](Interpreter &interp, std::vector<Value> args) {
      std::vector<Value> events;
#if !defined(__EMSCRIPTEN__) && !defined(_WIN32)
      if (!std::holds_alternative<double>(args[0])) return Value(std::make_shared<FSKArray>(events));
      int id = (int)std::get<double>(args[0]);
      
      if (interp.fsWatchers.find(id) == interp.fsWatchers.end()) return Value(std::make_shared<FSKArray>(events));
      int fd = interp.fsWatchers[id];
      
      char buffer[4096] __attribute__ ((aligned(__alignof__(struct inotify_event))));
      const struct inotify_event *event;
      ssize_t len;
      
      while ((len = read(fd, buffer, sizeof(buffer))) > 0) {
          char *ptr;
          for (ptr = buffer; ptr < buffer + len; ptr += sizeof(struct inotify_event) + event->len) {
              event = (const struct inotify_event *) ptr;
              if (event->len) {
                  events.push_back(Value(std::string(event->name)));
              }
          }
      }
#endif
      return Value(std::make_shared<FSKArray>(events));
  });

  fsInstance->fields["unwatch"] = std::make_shared<NativeFunction>(1, [](Interpreter &interp, std::vector<Value> args) {
#if !defined(__EMSCRIPTEN__) && !defined(_WIN32)
      if (!std::holds_alternative<double>(args[0])) return Value(false);
      int id = (int)std::get<double>(args[0]);
      if (interp.fsWatchers.find(id) != interp.fsWatchers.end()) {
          close(interp.fsWatchers[id]);
          interp.fsWatchers.erase(id);
          return Value(true);
      }
#endif
      return Value(false);
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


  auto workerHandleClass = std::make_shared<FSKClass>("WorkerHandle", nullptr, std::map<std::string, std::shared_ptr<FunctionCallable>>());
  
  auto workerFactoryClass = std::make_shared<FSKClass>("WorkerFactory", nullptr, std::map<std::string, std::shared_ptr<FunctionCallable>>());
  auto workerFactory = std::make_shared<FSKInstance>(workerFactoryClass);

  workerFactory->fields["init"] = std::make_shared<NativeFunction>(1, [workerHandleClass](Interpreter &interp, std::vector<Value> args) {
      if (!std::holds_alternative<std::string>(args[0])) return Value(std::monostate{});
      std::string scriptPath = std::get<std::string>(args[0]);
      
      auto resource = std::make_shared<WorkerResource>();
      resource->incoming = std::make_shared<ThreadSafeQueue<std::string>>();
      resource->outgoing = std::make_shared<ThreadSafeQueue<std::string>>();
      
      resource->thread = std::make_shared<std::thread>([scriptPath, resource]() {
          std::string source;
          try {
              std::ifstream t(scriptPath);
              std::stringstream buffer;
              buffer << t.rdbuf();
              source = buffer.str();
          } catch (...) { return; }

          Lexer lexer(source);
          std::vector<Token> tokens = lexer.scanTokens();
          Parser parser(tokens);
          std::vector<std::shared_ptr<Stmt>> statements = parser.parse();
          
          Interpreter workerInterp;
          workerInterp.isWorker = true;
          workerInterp.workerIncoming = resource->incoming;
          workerInterp.workerOutgoing = resource->outgoing;
          
          try {
             workerInterp.interpret(statements);
          } catch(...) {}
      });
      resource->thread->detach(); 
      
      int id = interp.workerIdCounter++;
      interp.workers[id] = resource;
      
      auto instance = std::make_shared<FSKInstance>(workerHandleClass);
      instance->fields["id"] = Value((double)id);
      
      instance->fields["postMessage"] = std::make_shared<NativeFunction>(1, [id](Interpreter &interp, std::vector<Value> args) {
          if (interp.workers.find(id) == interp.workers.end()) return Value(false);
          std::string msg = stringify(args[0]);
          interp.workers[id]->incoming->push(msg);
          return Value(true);
      });
      
      instance->fields["poll"] = std::make_shared<NativeFunction>(0, [id](Interpreter &interp, std::vector<Value> args) {
          if (interp.workers.find(id) == interp.workers.end()) return Value(std::make_shared<FSKArray>(std::vector<Value>{}));
          std::vector<Value> msgs;
          while (auto msg = interp.workers[id]->outgoing->pop(false)) {
              msgs.push_back(Value(*msg));
          }
          return Value(std::make_shared<FSKArray>(msgs));
      });

      instance->fields["terminate"] = std::make_shared<NativeFunction>(0, [id](Interpreter &interp, std::vector<Value> args) {
          if (interp.workers.find(id) != interp.workers.end()) {
              interp.workers[id]->incoming->close();
              interp.workers.erase(id);
          }
          return Value(true);
      });

      return Value(instance);
  });

  globals->define("Worker", workerFactory);

  globals->define("workerPostMessage", std::make_shared<NativeFunction>(1, [](Interpreter &interp, std::vector<Value> args) {
      if (!interp.isWorker) return Value(false);
      std::string msg = stringify(args[0]);
      interp.workerOutgoing->push(msg);
      return Value(true);
  }));

  globals->define("workerPoll", std::make_shared<NativeFunction>(0, [](Interpreter &interp, std::vector<Value> args) {
      if (!interp.isWorker) return Value(std::make_shared<FSKArray>(std::vector<Value>{}));
      std::vector<Value> msgs;
      while (auto msg = interp.workerIncoming->pop(false)) {
          msgs.push_back(Value(*msg));
      }
      return Value(std::make_shared<FSKArray>(msgs));
      return Value(std::make_shared<FSKArray>(msgs));
  }));

  auto taskClass = std::make_shared<FSKClass>("Task", nullptr, std::map<std::string, std::shared_ptr<FunctionCallable>>());
  auto taskFactory = std::make_shared<FSKInstance>(taskClass);

  taskFactory->fields["run"] = std::make_shared<NativeFunction>(1, [](Interpreter &interp, std::vector<Value> args) {
      if (!std::holds_alternative<std::string>(args[0])) return Value(std::monostate{});
      std::string scriptPath = std::get<std::string>(args[0]);
      
      auto resource = std::make_shared<WorkerResource>();
      resource->incoming = std::make_shared<ThreadSafeQueue<std::string>>();
      resource->outgoing = std::make_shared<ThreadSafeQueue<std::string>>();
      
      resource->thread = std::make_shared<std::thread>([scriptPath, resource]() {
          std::string source;
          try {
              std::ifstream t(scriptPath);
              std::stringstream buffer;
              buffer << t.rdbuf();
              source = buffer.str();
          } catch (...) { return; }

          Lexer lexer(source);
          std::vector<Token> tokens = lexer.scanTokens();
          Parser parser(tokens);
          std::vector<std::shared_ptr<Stmt>> statements = parser.parse();
          
          Interpreter workerInterp;
          workerInterp.isWorker = true;
          workerInterp.workerIncoming = resource->incoming;
          workerInterp.workerOutgoing = resource->outgoing;
          
          try {
             workerInterp.interpret(statements);
          } catch(...) {}
      });
      resource->thread->detach(); 
      
      int id = interp.workerIdCounter++;
      interp.workers[id] = resource;
      
      auto instance = std::make_shared<FSKInstance>(std::make_shared<FSKClass>("TaskInstance", nullptr, std::map<std::string, std::shared_ptr<FunctionCallable>>()));
      instance->fields["id"] = Value((double)id);
      
      instance->fields["wait"] = std::make_shared<NativeFunction>(0, [id](Interpreter &interp, std::vector<Value> args) {
          if (interp.workers.find(id) == interp.workers.end()) return Value(std::monostate{});
          
          auto msg = interp.workers[id]->outgoing->pop(true); // Blocking pop
          Value result = std::monostate{};
          if (msg) {
              result = Value(*msg); 
          }
          
          interp.workers[id]->incoming->close();
          interp.workers.erase(id);
          
          return result;
      });

      return Value(instance);
  });

  globals->define("Task", taskFactory);

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

void Interpreter::interpret(std::vector<std::shared_ptr<Stmt>> statements, bool runEventLoop) {
  try {
    for (const auto &stmt : statements) {
      execute(stmt);
    }
    
    if (runEventLoop) {
      eventLoop->run();
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
  case TokenType::PERCENT:
    lastValue = fmod(std::get<double>(left), std::get<double>(right));
    break;
  case TokenType::PIPE:
    if (std::holds_alternative<std::shared_ptr<Callable>>(right)) {
        auto function = std::get<std::shared_ptr<Callable>>(right);
        if (function->arity() != 1 && function->arity() != -1) {
             throw std::runtime_error("Pipe operator expects a function with 1 argument.");
        }
        std::vector<Value> args;
        args.push_back(left);
        lastValue = function->call(*this, args);
    } else {
        throw std::runtime_error("Pipe operator expects a function on the right.");
    }
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
  } else if (expr.op.type == TokenType::QUESTION_QUESTION) {
     if (!std::holds_alternative<std::monostate>(left)) {
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
    int min = function->minArity();
    int max = function->maxArity();
    
    if ((min != -1 && arguments.size() < (size_t)min) || (max != -1 && arguments.size() > (size_t)max)) {
      if (min == max) {
        throw std::runtime_error("Expected " + std::to_string(min) +
                                 " arguments but got " +
                                 std::to_string(arguments.size()) + ".");
      } else {
        throw std::runtime_error("Expected " + std::to_string(min) + "-" + 
                                 std::to_string(max) +
                                 " arguments but got " +
                                 std::to_string(arguments.size()) + ".");
      }
    }
    lastValue = function->call(*this, arguments);
  } else {
    throw std::runtime_error("Can only call functions and classes.");
  }
}
void Interpreter::visitGetExpr(Get &expr) {
  Value object = evaluate(expr.object);

  if (expr.isOptional) {
     if (std::holds_alternative<std::monostate>(object)) {
         lastValue = std::monostate{};
         return;
     }
  }
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
    if (expr.name.lexeme == "pushFront") {
      lastValue = std::make_shared<NativeFunction>(
          1, [arr](Interpreter &interp, std::vector<Value> args) {
            arr->elements.insert(arr->elements.begin(), args[0]);
            return args[0];
          });
      return;
    }
    if (expr.name.lexeme == "popFront") {
      lastValue = std::make_shared<NativeFunction>(
          0, [arr](Interpreter &interp, std::vector<Value> args) {
            if (arr->elements.empty())
              return Value(std::monostate{});
            Value v = arr->elements.front();
            arr->elements.erase(arr->elements.begin());
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

  if (expr.isOptional) {
      lastValue = std::monostate{};
      return;
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
  Value value = evaluate(expr.expression);

  if (std::holds_alternative<std::shared_ptr<FSKInstance>>(value)) {
      auto instance = std::get<std::shared_ptr<FSKInstance>>(value);
      Token waitToken(TokenType::IDENTIFIER, "wait", std::monostate{}, 0);
      try {
          Value waitMethod = instance->get(waitToken); 
          if (std::holds_alternative<std::shared_ptr<Callable>>(waitMethod)) {
              auto callable = std::get<std::shared_ptr<Callable>>(waitMethod);
              lastValue = callable->call(*this, {}); 
              return;
          }
      } catch(...) {
      }
  }
  lastValue = value;
}

void Interpreter::visitFunctionExpr(FunctionExpr &expr) {
  Token name(TokenType::IDENTIFIER, "", std::monostate{}, 0);
  auto function = std::make_shared<Function>(name, expr.params, expr.body, false);
  auto callable = std::make_shared<FunctionCallable>(function, environment);
  lastValue = callable;
}

void Interpreter::visitArrowFunctionExpr(ArrowFunction &expr) {
  Token name(TokenType::IDENTIFIER, "", std::monostate{}, 0);
  auto function = std::make_shared<Function>(name, expr.params, expr.body, false);
  auto callable = std::make_shared<FunctionCallable>(function, environment);
  lastValue = callable;
}

void Interpreter::visitObjectExpr(ObjectExpr &expr) {
  static auto objClass = std::make_shared<FSKClass>("Object", nullptr, std::map<std::string, std::shared_ptr<FunctionCallable>>());
  auto instance = std::make_shared<FSKInstance>(objClass);
  for (auto const& [key, valExpr] : expr.fields) {
    instance->fields[key] = evaluate(valExpr);
  }
  lastValue = instance;
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
    Value val = evaluate(element.expr);
    if (element.isSpread) {
        if (std::holds_alternative<std::shared_ptr<FSKArray>>(val)) {
            auto arr = std::get<std::shared_ptr<FSKArray>>(val);
            elements.insert(elements.end(), arr->elements.begin(), arr->elements.end());
        } else {
            throw std::runtime_error("Spread operator expects an array.");
        }
    } else {
        elements.push_back(val);
    }
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
    
    size_t valIdx = 0;
    for (size_t i = 0; i < a->elements.size(); i++) {
      if (a->elements[i].isSpread) {
          std::vector<Value> rest;
          while (valIdx < arr->elements.size()) {
              rest.push_back(arr->elements[valIdx++]);
          }
          bindPattern(a->elements[i].expr, std::make_shared<FSKArray>(rest), isConst);
          return; 
      }

      if (valIdx < arr->elements.size()) {
        bindPattern(a->elements[i].expr, arr->elements[valIdx++], isConst);
      } else {
        bindPattern(a->elements[i].expr, std::monostate{}, isConst);
      }
    }
    return;
  }

  if (ObjectExpr *o = dynamic_cast<ObjectExpr *>(pat.get())) {
    if (!std::holds_alternative<std::shared_ptr<FSKInstance>>(value)) {
      throw std::runtime_error("Cannot destructure non-object value.");
    }
    auto inst = std::get<std::shared_ptr<FSKInstance>>(value);
    for (auto const& [key, valPat] : o->fields) {
      if (inst->fields.count(key)) {
        bindPattern(valPat, inst->fields[key], isConst);
      } else {
        bindPattern(valPat, std::monostate{}, isConst);
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
        if (!matchPattern(a->elements[i].expr, arr->elements[i])) return false;
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

extern "C" void fsk_on_http_request(uint64_t req_id, const char* method, const char* path, const char* body, void* context) {
    if (!context) return;
    auto interp = static_cast<Interpreter*>(context);
    
    std::string m(method);
    std::string p(path);
    std::string b(body);
    
    interp->eventLoop->post([interp, req_id, m, p, b]() {
        Value handler;
        try {
            handler = interp->globals->get("onHttpRequest");
        } catch (...) {
            fsk_http_respond(req_id, 404, "Not Found");
            return;
        }

        if (std::holds_alternative<std::shared_ptr<Callable>>(handler)) {
            static auto reqClass = std::make_shared<FSKClass>("Request", nullptr, std::map<std::string, std::shared_ptr<FunctionCallable>>());
            auto reqInst = std::make_shared<FSKInstance>(reqClass);
            reqInst->fields["id"] = (double)req_id;
            reqInst->fields["method"] = m;
            reqInst->fields["path"] = p;
            reqInst->fields["body"] = b;
            
            reqInst->fields["send"] = std::make_shared<NativeFunction>(1, std::function<Value(Interpreter&, std::vector<Value>)>([req_id](Interpreter& i, std::vector<Value> args) -> Value {
                if (args.size() > 0 && std::holds_alternative<std::string>(args[0])) {
                    fsk_http_respond(req_id, 200, std::get<std::string>(args[0]).c_str());
                } else {
                    fsk_http_respond(req_id, 200, "");
                }
                return Value(std::monostate{});
            }));

            std::get<std::shared_ptr<Callable>>(handler)->call(*interp, {Value(reqInst)});
        } else {
            fsk_http_respond(req_id, 500, "Internal Server Error");
        }
    });
}

extern "C" void fsk_on_ws_message(uint32_t ws_id, const char* message, void* context) {
    if (!context) return;
    auto interp = static_cast<Interpreter*>(context);
    std::string msg(message);
    
    interp->eventLoop->post([interp, ws_id, msg]() {
        Value handler;
        try {
            handler = interp->globals->get("onWsMessage");
        } catch (...) {
            return;
        }

        if (std::holds_alternative<std::shared_ptr<Callable>>(handler)) {
            static auto wsClass = std::make_shared<FSKClass>("WebSocketPointer", nullptr, std::map<std::string, std::shared_ptr<FunctionCallable>>());
            auto wsInst = std::make_shared<FSKInstance>(wsClass);
            wsInst->fields["id"] = (double)ws_id;
            
            wsInst->fields["send"] = std::make_shared<NativeFunction>(1, std::function<Value(Interpreter&, std::vector<Value>)>([ws_id](Interpreter& i, std::vector<Value> args) -> Value {
                if (args.size() > 0 && std::holds_alternative<std::string>(args[0])) {
                    fsk_ws_send(ws_id, std::get<std::string>(args[0]).c_str());
                }
                return Value(std::monostate{});
            }));

            std::get<std::shared_ptr<Callable>>(handler)->call(*interp, {Value(wsInst), Value(msg)});
        }
    });
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


Value FSKClass::call(Interpreter &interpreter, std::vector<Value> arguments) {
  auto instance = std::make_shared<FSKInstance>(shared_from_this());

  std::shared_ptr<FunctionCallable> initializer = findMethod("init");
  if (initializer != nullptr) {
    auto boundConstructor = initializer->bind(instance); 
    boundConstructor->call(interpreter, arguments);
  }

  return instance;
}
