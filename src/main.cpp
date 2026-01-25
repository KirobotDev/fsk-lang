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
    <title>FSK</title>
    <style>body{margin:0;overflow:hidden;background:#000;}</style>
    <script type="module" src="ui.js" defer></script>
</head>
<body>
</body>
</html>)";

const char* UI_JS_TEMPLATE = R"(
(function() {
    function loadScript(src) {
        return new Promise((resolve, reject) => {
            const s = document.createElement('script');
            s.src = src;
            s.onload = resolve;
            s.onerror = reject;
            document.head.appendChild(s);
        });
    }

    function create(tag, style, parent) {
        const el = document.createElement(tag);
        if (style) Object.assign(el.style, style);
        if (parent) parent.appendChild(el);
        return el;
    }

    async function init() {
        await loadScript('https://cdnjs.cloudflare.com/ajax/libs/three.js/r128/three.min.js');
        await loadScript('https://cdn.jsdelivr.net/npm/three@0.128.0/examples/js/controls/OrbitControls.js');

        const container = create('div', {width:'100vw', height:'100vh', position:'absolute', top:'0', left:'0', zIndex:'1'}, document.body);
        container.id = 'canvas-container';

        const uiLayer = create('div', {
            position:'absolute', top:'20px', right:'20px', zIndex:'100', 
            fontFamily:'monospace', display:'flex', flexDirection:'column', alignItems:'flex-end', pointerEvents:'none'
        }, document.body);

        const wrapper = create('div', {
            width:'450px', height:'300px', backgroundColor:'rgba(10,10,10,0.9)', 
            border:'1px solid #333', borderRadius:'4px', overflow:'hidden', 
            pointerEvents:'auto', display:'flex', flexDirection:'column'
        }, uiLayer);
        
        const header = create('div', {
            background:'#111', padding:'5px 10px', borderBottom:'1px solid #333',
            color:'#666', fontSize:'10px', display:'flex', justifyContent:'space-between'
        }, wrapper);
        header.textContent = 'FSK TERMINAL';

        const consoleDiv = create('div', {
            flex:'1', padding:'10px', color:'#0f0', fontSize:'12px', overflowY:'auto', fontFamily:'monospace'
        }, wrapper);
        consoleDiv.id = 'console';

        const log = (msg, color='#0f0') => {
            const line = create('div', {marginBottom:'4px', color:color}, consoleDiv);
            const ts = create('span', {color:'#444', marginRight:'8px', fontSize:'10px'}, line);
            ts.textContent = new Date().toLocaleTimeString('en-US',{hour12:false});
            const txt = create('span', {}, line);
            txt.textContent = '>> ' + msg;
            consoleDiv.scrollTop = consoleDiv.scrollHeight;
        };
        
        window.Module = {
            print: (t) => { if(t) { console.log(t); log(t); } },
            printErr: (t) => { if(t) { console.error(t); log(t, '#f33'); } },
            onRuntimeInitialized: () => { log('System Ready.', '#fff'); Module.callMain(['web/main.fsk']); }
        };

        const scene = new THREE.Scene();
        scene.background = new THREE.Color(0x000000);
        const camera = new THREE.PerspectiveCamera(45, window.innerWidth/window.innerHeight, 0.1, 1000);
        camera.position.set(0,2,5);
        
        const renderer = new THREE.WebGLRenderer({antialias:true});
        renderer.setSize(window.innerWidth, window.innerHeight);
        renderer.toneMapping = THREE.ACESFilmicToneMapping;
        container.appendChild(renderer.domElement);
        
        const controls = new THREE.OrbitControls(camera, renderer.domElement);
        controls.enableDamping = true;
        controls.minDistance = 2.5;

        const light = new THREE.DirectionalLight(0xffffff, 1.2);
        light.position.set(50,20,30);
        scene.add(light);
        scene.add(new THREE.AmbientLight(0x404040, 0.2));

        const starsGeo = new THREE.BufferGeometry();
        const pos = [];
        for(let i=0;i<9000;i++) pos.push((Math.random()-0.5)*200);
        starsGeo.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3));
        const stars = new THREE.Points(starsGeo, new THREE.PointsMaterial({color:0xffffff, size:0.1, transparent:true}));
        scene.add(stars);

        const grp = new THREE.Group();
        scene.add(grp);
        const l = new THREE.TextureLoader();
        const earth = new THREE.Mesh(
            new THREE.SphereGeometry(1,64,64),
            new THREE.MeshPhongMaterial({
                map: l.load('https://unpkg.com/three-globe/example/img/earth-blue-marble.jpg'),
                specularMap: l.load('https://unpkg.com/three-globe/example/img/earth-water.png'),
                bumpMap: l.load('https://unpkg.com/three-globe/example/img/earth-topology.png'),
                specular: new THREE.Color('grey')
            })
        );
        grp.add(earth);
        
        const clouds = new THREE.Mesh(
            new THREE.SphereGeometry(1.01,64,64),
            new THREE.MeshPhongMaterial({
                map: l.load('https://unpkg.com/three-globe/example/img/earth-clouds.png'),
                transparent:true, opacity:0.8, side:THREE.DoubleSide, blending:THREE.AdditiveBlending
            })
        );
        grp.add(clouds);

        const animate = () => {
            requestAnimationFrame(animate);
            clouds.rotation.y += 0.0002;
            earth.rotation.y += 0.00005;
            controls.update();
            renderer.render(scene, camera);
        };
        animate();
        
        window.onresize = () => {
            camera.aspect = window.innerWidth/window.innerHeight;
            camera.updateProjectionMatrix();
            renderer.setSize(window.innerWidth, window.innerHeight);
        };

        loadScript('fsk.js');
    }
    init();
})();
)";

const char* MAIN_FSK_TEMPLATE = R"(// Fsk Web Entry Point
print "Connection established.";
print "Secure environment active.";
// Your code here
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
                      "-s \"EXPORTED_RUNTIME_METHODS=['callMain']\" "
                      "-s ALLOW_MEMORY_GROWTH=1 "
                      "-s DISABLE_EXCEPTION_CATCHING=0 "
                      "-s USE_SQLITE3=1 " 
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

void handleWebStart() {
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

    int port = 8080;
    
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
                sendResponse(new_socket, "404 Not Found", "text/plain", "Not Found");
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
      std::cout << "\nCommands:" << std::endl;
      std::cout << "  webinit         Initialize a new Fsk Web project (WASM)" << std::endl;
      std::cout << "  build           Compile project to WebAssembly" << std::endl;
      std::cout << "  start           Serve the web project locally" << std::endl;
      std::cout << "  [file.fsk]      Run a Fsk script file" << std::endl;
      std::cout << "\nOptions:" << std::endl;
      std::cout << "  --version, -v   Display version information" << std::endl;
      std::cout << "  --help, -h      Display this help message" << std::endl;
      return 0;
    }
    
    if (arg == "webinit") {
        handleWebInit();
        return 0;
    }
    if (arg == "build") {
        handleWebBuild();
        return 0;
    }
    if (arg == "start") {
        handleWebStart();
        return 0;
    }
    
    runFile(argc, argv);
  } else {
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
  }
  return 0;
}
