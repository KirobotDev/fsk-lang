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
#include "TypeChecker.hpp"

namespace fs = std::filesystem;

void run(std::string source) {
  Lexer lexer(source);
  std::vector<Token> tokens = lexer.scanTokens();

  Parser parser(tokens);
  std::vector<std::shared_ptr<Stmt>> statements = parser.parse();

  TypeChecker typeChecker;
  typeChecker.check(statements);

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

  TypeChecker typeChecker;
  typeChecker.check(statements);

  interpreter.interpret(statements);
}


const char* MINIMAL_HTML_TEMPLATE = R"(<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Fsk Earth</title>
    <style>
        body { margin: 0; padding: 0; overflow: hidden; background: #000; }
        #canvas-ui { position: fixed; top: 0; left: 0; width: 100%; height: 100%; z-index: 1; }
        #root { position: fixed; top: 0; left: 0; width: 100%; height: 100%; z-index: 2; pointer-events: none; }
    </style>
    <script type="importmap">
    {
      "imports": {
        "three": "https://unpkg.com/three@0.160.0/build/three.module.js",
        "three/addons/": "https://unpkg.com/three@0.160.0/examples/jsm/"
      }
    }
    </script>
    <script type="module" src="ui.js" defer></script>
</head>
<body>
    <div id="canvas-ui"></div>
    <div id="root"></div>
</body>
</html>)";

const char* UI_JS_TEMPLATE = R"(
import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';

(function() {
    const container = document.getElementById('canvas-ui');
    const scene = new THREE.Scene();
    const camera = new THREE.PerspectiveCamera(45, window.innerWidth / window.innerHeight, 0.1, 2000);
    const renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true });
    
    renderer.setSize(window.innerWidth, window.innerHeight);
    renderer.setPixelRatio(window.devicePixelRatio);
    container.appendChild(renderer.domElement);

    camera.position.z = 4;

    const controls = new OrbitControls(camera, renderer.domElement);
    controls.enableDamping = true;
    controls.dampingFactor = 0.05;
    controls.rotateSpeed = 0.8;
    controls.enableZoom = true;
    controls.minDistance = 2.5;
    controls.maxDistance = 10;

    const loader = new THREE.TextureLoader();
    const earthMap = loader.load('https://unpkg.com/three-globe/example/img/earth-blue-marble.jpg');
    const earthBump = loader.load('https://unpkg.com/three-globe/example/img/earth-topology.png');
    const earthSpec = loader.load('https://unpkg.com/three-globe/example/img/earth-waterbodies.png');

    const earthGeometry = new THREE.SphereGeometry(1.6, 64, 64);
    const earthMaterial = new THREE.MeshPhongMaterial({
        map: earthMap,
        bumpMap: earthBump,
        bumpScale: 0.05,
        specularMap: earthSpec,
        specular: new THREE.Color('grey'),
        shininess: 10
    });
    
    const earth = new THREE.Mesh(earthGeometry, earthMaterial);
    scene.add(earth);

    const cloudGeometry = new THREE.SphereGeometry(1.62, 64, 64);
    const cloudTexture = loader.load('https://raw.githubusercontent.com/mrdoob/three.js/master/examples/textures/planets/earth_clouds_1024.png');
    const cloudMaterial = new THREE.MeshPhongMaterial({
        map: cloudTexture,
        transparent: true,
        opacity: 0.4,
        depthWrite: false
    });
    const clouds = new THREE.Mesh(cloudGeometry, cloudMaterial);
    scene.add(clouds);

    const starGeometry = new THREE.BufferGeometry();
    const starMaterial = new THREE.PointsMaterial({ color: 0xffffff, size: 0.7 });
    const starVertices = [];
    for (let i = 0; i < 15000; i++) {
        const x = (Math.random() - 0.5) * 2000;
        const y = (Math.random() - 0.5) * 2000;
        const z = (Math.random() - 0.5) * 2000;
        starVertices.push(x, y, z);
    }
    starGeometry.setAttribute('position', new THREE.Float32BufferAttribute(starVertices, 3));
    const stars = new THREE.Points(starGeometry, starMaterial);
    scene.add(stars);

    const ambientLight = new THREE.AmbientLight(0xffffff, 0.4);
    scene.add(ambientLight);

    const sunLight = new THREE.DirectionalLight(0xffffff, 1.5);
    sunLight.position.set(5, 3, 5);
    scene.add(sunLight);

    function animate() {
        requestAnimationFrame(animate);
        earth.rotation.y += 0.001;
        clouds.rotation.y += 0.0015;
        controls.update();
        renderer.render(scene, camera);
    }

    window.addEventListener('resize', () => {
        camera.aspect = window.innerWidth / window.innerHeight;
        camera.updateProjectionMatrix();
        renderer.setSize(window.innerWidth, window.innerHeight);
    });

    animate();

    function loadScript(src) {
        return new Promise((resolve, reject) => {
            const s = document.createElement('script');
            s.src = src;
            s.onload = resolve;
            s.onerror = reject;
            document.head.appendChild(s);
        });
    }

    const root = document.getElementById('root');
    let isRequestPending = false;

    function sendRequest(path) {
        if (!window.FS) return;
        if (isRequestPending) return;
        isRequestPending = true;
        try {
            const req = "GET " + path + " HTTP/1.1";
            FS.writeFile('request.tmp', req);
        } catch(e) { console.error(e); isRequestPending = false; }
    }

    function updateDOM(html) {
        root.innerHTML = html;
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
        }, 50);
    }

    window.Module = {
        print: (t) => {},
        printErr: (t) => {},
        onRuntimeInitialized: () => {
            if (!window.FS && Module.FS) window.FS = Module.FS;
            Module.callMain(['web/main.fsk']);
            const path = window.location.pathname === '/index.html' ? '/' : window.location.pathname;
            sendRequest(path);
            startPolling();
        }
    };
    
    window.onpopstate = () => sendRequest(window.location.pathname);
    loadScript('fsk.js');
})();
)";

const char* MAIN_FSK_TEMPLATE = R"(fn handle(req) {
    return "";
}
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

#ifndef __EMSCRIPTEN__
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
                      "-s SINGLE_FILE=1 "
                      "-s ENVIRONMENT=web "
                      "-s EXIT_RUNTIME=1 "
                      "-s EXPORTED_RUNTIME_METHODS=callMain,FS,UTF8ToString,stringToUTF8 "
                      "-s ALLOW_MEMORY_GROWTH=1 "
                      "-s FORCE_FILESYSTEM=1 "
                      "-s DISABLE_EXCEPTION_CATCHING=0 "
                      "-s USE_SQLITE3=1 " 
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
#endif

#ifndef __EMSCRIPTEN__
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
#endif

#ifndef __EMSCRIPTEN__
void sendResponse(int clientSocket, const std::string& status, const std::string& contentType, const std::string& body) {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << status << "\r\n"
        << "Content-Type: " << contentType << "\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Connection: close\r\n\r\n";
    std::string headers = oss.str();
    send(clientSocket, headers.c_str(), headers.size(), 0);
    send(clientSocket, body.data(), body.size(), 0);
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
                std::string body((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                sendResponse(new_socket, "200 OK", getMimeType(fullPath), body);
            } else {
                if (path.find('.') == std::string::npos) {
                     std::string indexPath = serveDir + "/index.html";
                     if (fs::exists(indexPath)) {
                        std::ifstream file(indexPath, std::ios::binary);
                        std::string body((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                        sendResponse(new_socket, "200 OK", "text/html", body);
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
#endif

#ifndef __EMSCRIPTEN__
#include "Callable.hpp"

void handleInstall() {
    if (!fs::exists("fsk.json")) {
        std::cerr << "Error: fsk.json not found in current directory." << std::endl;
        return;
    }

    std::cout << "\033[36m[FPM]\033[0m Reading fsk.json..." << std::endl;
    std::ifstream f("fsk.json");
    std::stringstream buffer;
    buffer << f.rdbuf();

    Interpreter interp;

    
    try {
        Value config = interp.jsonParse(buffer.str());
        
        if (std::holds_alternative<std::shared_ptr<FSKInstance>>(config)) {
             auto inst = std::get<std::shared_ptr<FSKInstance>>(config);
             
             if (inst->fields.count("dependencies")) {
                 Value deps = inst->fields["dependencies"];
                 if (std::holds_alternative<std::shared_ptr<FSKInstance>>(deps)) {
                     auto depsInst = std::get<std::shared_ptr<FSKInstance>>(deps);
                     
                     if (depsInst->fields.empty()) {
                         std::cout << "  No dependencies found." << std::endl;
                         return;
                     }

                     if (!fs::exists("fsk_modules")) {
                         fs::create_directory("fsk_modules");
                     }

                     for (auto const& [name, val] : depsInst->fields) {
                         if (std::holds_alternative<std::string>(val)) {
                             std::string url = std::get<std::string>(val);
                             std::cout << "  Installing \033[33m" << name << "\033[0m from " << url << "..." << std::endl;
                             
                             std::string targetDir = "fsk_modules/" + name;
                             if (fs::exists(targetDir)) {
                                 std::cout << "    Already installed." << std::endl;
                             } else {
                                 std::string cmd = "git clone --depth 1 " + url + " " + targetDir;
                                 int res = std::system(cmd.c_str());
                                 if (res != 0) {
                                     std::cerr << "    \033[31mFailed to install " << name << "\033[0m" << std::endl;
                                 } else {
                                     std::cout << "    \033[32mSuccess\033[0m" << std::endl;
                                 }
                             }
                         }
                     }
                 } else {
                     std::cerr << "Error: 'dependencies' should be an object." << std::endl;
                 }
             } else {
                 std::cout << "  No 'dependencies' field in fsk.json." << std::endl;
             }
        } else {
            std::cerr << "Error: fsk.json root must be an object." << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error parsing fsk.json: " << e.what() << std::endl;
    }
}
#endif

int main(int argc, char *argv[]) {
  if (argc >= 2) {
    std::string arg = argv[1];
    if (arg == "--version" || arg == "-v") {
#ifndef __EMSCRIPTEN__
      std::cout << "Fsk Language v1.1.0 (Async/HTTP/FPM)" << std::endl;
#else
      std::cout << "Fsk Web v1.0" << std::endl;
#endif
      return 0;
    }
    if (arg == "--help" || arg == "-h") {
      std::cout << "Usage: fsk [command] [options]" << std::endl;
      std::cout << "Commands:" << std::endl;
      std::cout << "  install    Install dependencies from fsk.json" << std::endl;
      std::cout << "  webinit    Initialize a web project" << std::endl;
      std::cout << "  build      Build web project" << std::endl;
      std::cout << "  start      Start web server" << std::endl;
      std::cout << "  <file>     Run Fsk script" << std::endl;
      return 0;
    }
    
#ifndef __EMSCRIPTEN__
    if (arg == "install") { handleInstall(); return 0; }
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
#endif
    
    runFile(argc, argv);
  } else {
#ifndef __EMSCRIPTEN__
    std::cout << "Fsk Language v1.1.0" << std::endl;
    std::cout << "Tapez 'exit' pour quitter." << std::endl;
    std::string line;
    Interpreter interpreter;
    interpreter.setArgs(argc, argv);
    
    TypeChecker typeChecker;

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
        
        typeChecker.check(statements, true); // Keep state

        interpreter.interpret(statements, false, true); // No Event Loop, REPL Mode
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
