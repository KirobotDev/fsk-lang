# Fsk Language (Perfect Edition)

# Website docs https://fsk.kirosb.fr

# FPM Gestionary package Website https://fpm.kirosb.fr

A high-performance hybrid **C++/Rust** interpreted programming language.

## 💎 Beta Édition Features

- **Hybrid Core**: Critical modules (Crypto, SQL, System, Http) are written in **Rust** for maximum performance and safety.
- **Bytecode VM**: Now runs on a register-based Virtual Machine (Rust) instead of slow AST interpretation.
- **Production Web Server**: Built-in HTTP server powered by **Axum** & **Tokio**. Supports WebSockets out of the box.
- **Modern Syntax**: Arrow functions `=>`, Async/Await, Destructuring, Optional Chaining `?.`.

## Build

Requirements:
- CMake 3.10+
- C++ Compiler (C++17)
- Rust (Cargo)
- Dependencies: `libsqlite3-dev`, `libssl-dev`, `libcurl4-openssl-dev`, `libstdc++`, `libgcc`

To install dependencies on Debian/Ubuntu:
```bash
sudo apt install -y git cmake g++ make libsqlite3-dev libssl-dev libcurl4-openssl-dev cargo
```

```bash
mkdir build
cd build
cmake ..
make
```

### Windows Build
For detailed instructions on building Fsk on Windows (including Rust setup), please see [WINDOWS_BUILD.md](./WINDOWS_BUILD.md).

## Global Installation (System-wide)

### Linux
```bash
sudo mkdir -p /usr/local/lib/fsk/
sudo cp -r std /usr/local/lib/fsk/
sudo cp build/fsk /usr/local/bin/
```

### Windows (One-Click Installer)
The recommended way for users.
1. Build the project using `WINDOWS_BUILD.md`.
2. Run `package.bat` (requires Inno Setup).
3. The installer `fsk_setup.exe` will be in the `Output` folder.

## Usage

Run a script:
```bash
fsk script.fsk
```

Interactive mode (REPL):
```bash
fsk
```

## Production Web Server (std/http)

Fsk includes a high-level web framework inspired by Express.js:

```javascript
import "http";

const app = new http.Server();

app.get("/", (req, res) => {
    res.send("<h1>Hello from Fsk Perfect Edition!</h1>");
});

app.get("/json", (req, res) => {
    res.json(["Apple", "Banana", "Cherry"]);
});

print "Server running on port 8080";
app.listen(8080);
```

## Standard Library (Rust-Powered)

- **Crypto**: SHA256, MD5, Base64 (via `ring` crate).
- **SQL**: SQLite3 engine (via `rusqlite`).
- **System**: Device information (via `sysinfo`).
- **VM**: Register-based bytecode execution.

## Documentation

Run the built-in documentation server:
```bash
fsk docs/server.fsk
```
Then open `http://localhost:8092`.
