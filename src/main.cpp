/*
* all rights reserved by xql.dev :)
*/
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "AstPrinter.hpp"
#include "Lexer.hpp"
#include "Parser.hpp"
#include "Interpreter.hpp"

namespace fs = std::filesystem;

void run(std::string source) {
  Lexer lexer(source);
  std::vector<Token> tokens = lexer.scanTokens();

  Parser parser(tokens);
  std::vector<std::shared_ptr<Stmt>> statements = parser.parse();

  Interpreter interpreter;
  interpreter.interpret(statements);
}

int runfile(const char *path) { 
  std::ifstream file(path);
  if (!file.is_open()) {
    std::cerr << "Could not open file: " << path << std::endl;
    return -1;
  }
  std::stringstream buffer;
  buffer << file.rdbuf();
  run(buffer.str());
  return 0;
}

void runFile(int argc, char *argv[]) {
  const char *path = argv[1];
  std::ifstream file(path);
  if (!file.is_open()) {
    std::cerr << "Could not open file: " << path << std::endl;
    exit(1);
  }
  std::stringstream buffer;
  buffer << file.rdbuf();
  
  Interpreter interpreter;
  interpreter.setArgs(argc, argv);
  
  Lexer lexer(buffer.str());
  std::vector<Token> tokens = lexer.scanTokens();
  Parser parser(tokens);
  std::vector<std::shared_ptr<Stmt>> statements = parser.parse();
  interpreter.interpret(statements);
}


const char* MINIMAL_HTML_TEMPLATE = R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Fsk App</title>
    <style>
        body { margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; background: #f0f0f0; color: #333; }
        #root { width: 100vw; height: 100vh; overflow-y: auto; }
        .loading { display: flex; justify-content: center; align-items: center; height: 100vh; font-size: 1.5rem; color: #666; }
    </style>
    <script type="module" src="ui.js" defer></script>
</head>
<body>
    <div id="root">
        <div class="loading">Loading Fsk Runtime...</div>
    </div>
</body>
</html>)";

const char* UI_JS_TEMPLATE = R"(
(function() {
    // --- Fsk React-like Router Engine ---
    
    function loadScript(src) {
        return new Promise((resolve, reject) => {
            const s = document.createElement('script');
            s.src = src;
            s.onload = resolve;
            s.onerror = reject;
            document.head.appendChild(s);
        });
    }

    // State
    const root = document.getElementById('root');
    let isRequestPending = false;

    // --- Communication Bridge ---
    function sendRequest(path) {
        if (!window.FS) return;
        if (isRequestPending) return;

        isRequestPending = true;
        // console.log("[Router] Navigating:", path);
        
        try {
            // Write request
            const req = "GET " + path + " HTTP/1.1";
            FS.writeFile('request.tmp', req);
        } catch(e) { console.error(e); isRequestPending = false; }
    }

    function updateDOM(html) {
        // Simple Virtual DOM diffing replacement (InnerHTML for now)
        root.innerHTML = html;
        
        // Re-attach intercepts
        const links = root.querySelectorAll('a');
        links.forEach(a => {
            a.addEventListener('click', (e) => {
                const href = a.getAttribute('href');
                if (href && href.startsWith('/')) {
                    e.preventDefault();
                    history.pushState({}, "", href);
                    sendRequest(href);
                }
            });
        });
        
        isRequestPending = false;
    }

    function startPolling() {
        setInterval(() => {
            if (!window.FS) return;
            try {
                if (FS.analyzePath('response.tmp').exists) {
                    const content = FS.readFile('response.tmp', {encoding: 'utf8'});
                    FS.unlink('response.tmp');
                    updateDOM(content);
                }
            } catch(e) {}
        }, 50); // Fast polling
    }

    // --- Initialization ---
    window.Module = {
        print: (t) => console.log("[FSK]", t),
        printErr: (t) => console.error("[FSK ERR]", t),
        onRuntimeInitialized: () => {
            console.log("Fsk Ready.");
            
            // Ensure Global FS
            if (!window.FS && Module.FS) window.FS = Module.FS;
            
            Module.callMain(['web/main.fsk']);
            
            // Initial Route
            const path = window.location.pathname === '/index.html' ? '/' : window.location.pathname;
            sendRequest(path);
            startPolling();
        }
    };
    
    // History support
    window.onpopstate = () => sendRequest(window.location.pathname);

    // Bootstrap
    loadScript('fsk.js');
})();
)";

const char* MAIN_FSK_TEMPLATE = R"(// Fsk Web Application
// Acts like a Backend Server for the Frontend!

fn renderHome() {
    return "
        <div style='text-align:center; padding: 50px;'>
            <h1 style='color: #5555FF;'>Welcome to Fsk Web</h1>
            <p>This is a compiled WebAssembly application.</p>
            <div style='margin-top:20px;'>
                <a href='/about' style='margin:10px; color:#333;'>About</a>
                <a href='/contact' style='margin:10px; color:#333;'>Contact</a>
            </div>
            <p style='margin-top:50px; color:#999;'>Server Time: " + clock() + "s</p>
        </div>
    ";
}

fn renderAbout() {
    return "
        <div style='padding: 50px;'>
            <h1>About</h1>
            <p>Fsk brings server-side logic to the browser.</p>
            <a href='/'>Back Home</a>
        </div>
    ";
}

fn handle(req) {
    if (FSK.indexOf(req, "GET /about") != -1) return renderAbout();
    if (FSK.indexOf(req, "GET /contact") != -1) return "<h1>Contact Us</h1><a href='/'>Home</a>";
    return renderHome();
}

print "Starting Fsk Router...";
FSK.startServer(80, handle);
)";

void handleWebInit() {
    if (fs::exists("web")) {
        std::cout << "Error: 'web' directory already exists." << std::endl;
        return;
    }
    try {
        fs::create_directory("web");
        
        std::ofstream html("web/index.html");
        html << MINIMAL_HTML_TEMPLATE;
        html.close();

        std::ofstream js("web/ui.js");
        js << UI_JS_TEMPLATE;
        js.close();
        
        std::ofstream fsk("web/main.fsk");
        fsk << MAIN_FSK_TEMPLATE;
        fsk.close();
        
        std::cout << "\033[32m[SUCCESS]\033[0m Initialized Fsk Web project." << std::endl;
        std::cout << "  - web/index.html (Minimal HTML loader)" << std::endl;
        std::cout << "  - web/ui.js      (3D Engine & UI Logic)" << std::endl;
        std::cout << "  - web/main.fsk   (Your Code)" << std::endl;
        std::cout << "\nRun 'fsk build' to compile." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error creating web project: " << e.what() << std::endl;
    }
}

void handleWebBuild() {
    bool insideWeb = false;
    std::string webPath = "web";
    
    if (fs::exists("main.fsk") && fs::exists("index.html")) {
        insideWeb = true;
        webPath = ".";
    } else if (!fs::exists("web/main.fsk")) {
        std::cerr << "Error: Fsk web project not found. Run 'fsk webinit' or cd into project root." << std::endl;
        return;
    }

    std::string buildDir = insideWeb ? "build" : "web/build";
    if (!fs::exists(buildDir)) {
        fs::create_directory(buildDir);
    }
    
    std::cout << "\033[36m[BUILD]\033[0m Compiling Fsk Runtime + App to WebAssembly..." << std::endl;
    
    std::string srcPrefix = "";
    std::string includePrefix = "-I include";
    
    if (insideWeb) {
        if (fs::exists("../src/main.cpp")) {
             srcPrefix = "../";
             includePrefix = "-I ../include";
        } else if (fs::exists("../../src/main.cpp")) {
             srcPrefix = "../../"; 
             includePrefix = "-I ../../include";
        }
    } else {
        if (fs::exists("src/main.cpp")) {
            srcPrefix = ""; 
        } else if (fs::exists("../src/main.cpp")) {
            srcPrefix = "../"; 
            includePrefix = "-I ../include";
        }
    }
    
    std::string embedArg = insideWeb ? "--embed-file main.fsk@web/main.fsk" : "--embed-file web/main.fsk";
    std::string outputArg = "-o " + buildDir + "/fsk.js";

    std::string emsdkEnv = insideWeb ? "../emsdk/emsdk_env.sh" : "emsdk/emsdk_env.sh";
    
    bool emccGlobal = (std::system("which emcc > /dev/null 2>&1") == 0);
    std::string cmdPrefix = "";

    if (!emccGlobal) {
        if (fs::exists(emsdkEnv)) {
             std::cout << "[INFO] Found local emsdk. Using environment from: " << emsdkEnv << std::endl;
             cmdPrefix = "source " + emsdkEnv + " > /dev/null && ";
        } else {
             std::cout << "[INFO] Global 'emcc' not found. Checking local emsdk..." << std::endl;
        }
    }

    std::string cmd = cmdPrefix + "emcc " + srcPrefix + "src/main.cpp " + srcPrefix + "src/lexer/Lexer.cpp " + srcPrefix + "src/parser/Parser.cpp " + srcPrefix + "src/runtime/Interpreter.cpp " + srcPrefix + "src/runtime/Callable.cpp " +
                      includePrefix + " -std=c++20 -O3 -w "
                      "-s WASM=1 "
                      "-s EXIT_RUNTIME=1 "
                      "-s \"EXPORTED_RUNTIME_METHODS=['callMain', 'FS']\" "
                      "-s ALLOW_MEMORY_GROWTH=1 "
                      "-s DISABLE_EXCEPTION_CATCHING=0 "
                      "-s USES_SQLITE3=1 " 
                      "-s ASYNCIFY=1 " 
                      "-s ASSERTIONS=1 " 
                      + embedArg + " "
                      + outputArg;
    
    std::cout << "Executing build..." << std::endl;
    
    try {
        int result = std::system(("bash -c '" + cmd + "'").c_str());
        
        if (result == 0) {
            std::string srcUI = insideWeb ? "ui.js" : "web/ui.js";
            std::string dstUI = buildDir + "/ui.js";
            std::string srcIndex = insideWeb ? "index.html" : "web/index.html";
            std::string dstIndex = buildDir + "/index.html";

            if (fs::exists(srcUI)) fs::copy_file(srcUI, dstUI, fs::copy_options::overwrite_existing);
            if (fs::exists(srcIndex)) fs::copy_file(srcIndex, dstIndex, fs::copy_options::overwrite_existing);

            std::cout << "\033[32m[SUCCESS]\033[0m Build complete." << std::endl;
            std::cout << "Artifacts in '" << buildDir << "'" << std::endl;
        } else {
            std::cerr << "\033[31m[FAILED]\033[0m Build failed." << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Build error: " << e.what() << std::endl;
    }
}

// --- Native Web Server ---
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

void sendResponse(int clientSocket, const std::string& status, const std::string& contentType, const std::string& body) {
    std::string response = "HTTP/1.1 " + status + "\r\n"
                           "Content-Type: " + contentType + "\r\n"
                           "Content-Length: " + std::to_string(body.size()) + "\r\n"
                           "Connection: close\r\n\r\n" + body;
    send(clientSocket, response.c_str(), response.size(), 0);
}

std::string getMimeType(const std::string& path) {
    if (path.ends_with(".html")) return "text/html";
    if (path.ends_with(".js")) return "application/javascript";
    if (path.ends_with(".wasm")) return "application/wasm";
    if (path.ends_with(".css")) return "text/css";
    if (path.ends_with(".png")) return "image/png";
    if (path.ends_with(".jpg")) return "image/jpeg";
    if (path.ends_with(".ico")) return "image/x-icon";
    return "text/plain";
}


void handleWebStart(int port = 8080) {
    std::string serveDir = "web/build";
    
    if (fs::exists("build") && fs::exists("build/fsk.js")) {
        serveDir = "build";
    } 
    else if (fs::exists("web/build") && fs::exists("web/build/fsk.js")) {
        serveDir = "web/build";
    }
    else {
        std::cerr << "Error: Build artifacts (fsk.js) not found in 'build/' or 'web/build/'." << std::endl;
        std::cerr << "Run 'fsk build' first." << std::endl;
        return;
    }

    if (!fs::exists(serveDir)) {
          std::cerr << "Error: Directory '" << serveDir << "' does not exist." << std::endl;
          return;
    }

    // int port = 8080; // Replaced by argument
    
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

#ifndef _WIN32
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
#endif

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    std::cout << "\033[36m[SERVER]\033[0m Starting local web server..." << std::endl;
    std::cout << "Serving: ./" << serveDir << std::endl;
    std::cout << "Open \033[36mhttp://localhost:" << port << "\033[0m" << std::endl;
    
    while (true) {
        int new_socket;
#ifdef _WIN32
        new_socket = accept(server_fd, NULL, NULL);
#else
        new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
#endif
        if (new_socket < 0) continue;

        char buffer[4096] = {0};
        recv(new_socket, buffer, 4096, 0);
        
        std::string request(buffer);
        std::string method, path, protocol;
        std::stringstream ss(request);
        ss >> method >> path >> protocol;

        if (method == "GET") {
            if (path == "/") path = "/index.html";
            if (path.find("..") != std::string::npos) {
                sendResponse(new_socket, "403 Forbidden", "text/plain", "Access Denied");
#ifdef _WIN32
                closesocket(new_socket);
#else
                close(new_socket);
#endif
                continue;
            }

            std::string fullPath = serveDir + path;
            
            if (fs::exists(fullPath) && !fs::is_directory(fullPath)) {
                std::ifstream file(fullPath, std::ios::binary);
                std::stringstream fileBuffer;
                fileBuffer << file.rdbuf();
                sendResponse(new_socket, "200 OK", getMimeType(fullPath), fileBuffer.str());
                std::cout << "Served: " << path << std::endl;
            } else {
                // SPA Fallback: Serve index.html for unknown paths (if not looking for a file extension)
                if (path.find('.') == std::string::npos) {
                     std::string indexPath = serveDir + "/index.html";
                     if (fs::exists(indexPath)) {
                        std::ifstream file(indexPath, std::ios::binary);
                        std::stringstream fileBuffer;
                        fileBuffer << file.rdbuf();
                        sendResponse(new_socket, "200 OK", "text/html", fileBuffer.str());
                        std::cout << "Served (SPA): " << path << " -> /index.html" << std::endl;
                     } else {
                        sendResponse(new_socket, "404 Not Found", "text/plain", "Not Found");
                     }
                } else {
                    sendResponse(new_socket, "404 Not Found", "text/plain", "Not Found");
                }
            }
        }
#ifdef _WIN32
        closesocket(new_socket);
#else
        close(new_socket);
#endif
    }
}

int main(int argc, char *argv[]) {
  if (argc >= 2) {
    std::string arg = argv[1];
    if (arg == "--version" || arg == "-v") {
      std::cout << "Fsk Language v1.0.0" << std::endl;
      return 0;
    }
    if (arg == "--help" || arg == "-h") {
      std::cout << "Usage: fsk [command] [options]" << std::endl;
      // ... help ...
      return 0;
    }
    
    if (arg == "webinit") { handleWebInit(); return 0; }
    if (arg == "build") { handleWebBuild(); return 0; }
    if (arg == "start") { 
        int port = 8080;
        if (argc >= 3) {
            try {
                port = std::stoi(argv[2]);
            } catch(...) {
                std::cerr << "Invalid port number. Using 8080." << std::endl;
            }
        }
        handleWebStart(port); 
        return 0; 
    }
    
    runFile(argc, argv);
  } else {
#ifndef __EMSCRIPTEN__
    std::cout << "FuckSociety (FSK) v1.0" << std::endl;
    std::cout << "Tapez 'exit' pour quitter." << std::endl;
    std::string line;
    Interpreter interpreter;
    interpreter.setArgs(argc, argv);
    while (true) {
      std::cout << "fsk> ";
      if (!std::getline(std::cin, line) || line == "exit")
        break;
      if (line.empty())
        continue;
      try {
        Lexer lexer(line);
        std::vector<Token> tokens = lexer.scanTokens();
        Parser parser(tokens);
        std::vector<std::shared_ptr<Stmt>> statements = parser.parse();
        interpreter.interpret(statements);
      } catch (const std::exception &e) {
        std::cerr << "Erreur : " << e.what() << std::endl;
      }
    }
#else
    std::cout << "Fsk Web Runtime" << std::endl;
#endif
  }
  return 0;
}
