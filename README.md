# Fsk Language

# Website docs https://fsk.kirosb.fr

# FPM Gestionary package Website https://fpm.kirosb.fr

A custom interpreted programming language written in C++.

## Features

- **Dynamic Typing**: Variables can hold any type of value.
- **Functions**: First-class functions with closures.
- **Classes**: Object-oriented programming with classes and inheritance.
- **Standard Library**: Built-in `FSK` object for system utilities.
- **Import System**: Modularize code with `import`.

## Build

Requirements:
- CMake 3.10+
- C++ Compiler (C++17 recommended)
- Dependencies: `libsqlite3-dev`, `libssl-dev`, `libcurl4-openssl-dev`

To install dependencies on Debian/Ubuntu:
```bash
sudo apt install -y git cmake g++ make libsqlite3-dev libssl-dev libcurl4-openssl-dev
```

```bash
mkdir build
cd build
cmake ..
make
```

### Windows Build
For detailed instructions on building Fsk on Windows, please see [WINDOWS_BUILD.md](./WINDOWS_BUILD.md).

#### Building for Windows on Linux (Cross-Compilation)
If you are on Linux and want to generate a `.exe` for Windows users:
1. Ensure you have `mingw-w64` and `vcpkg` installed.
2. Run `./build_windows.sh`.
3. The resulting `fsk.exe` and a distribution `fsk_windows.zip` will be generated.

## Global Installation (System-wide)

After building, you can install Fsk and FPM system-wide to run `fsk` and `fpm` from any directory.

### Linux Installation
```bash 
sudo mkdir -p /usr/local/lib/fsk/
sudo cp -r std /usr/local/lib/fsk/
sudo cp build/fsk /usr/local/bin/

sudo cp -r fpm /usr/local/lib/fsk/
echo '#!/bin/bash' | sudo tee /usr/local/bin/fpm
echo 'fsk /usr/local/lib/fsk/fpm/fpm.fsk "$@"' | sudo tee -a /usr/local/bin/fpm
sudo chmod +x /usr/local/bin/fpm
```

### Windows Installation (One-Click Installer)
The easiest way to install Fsk on Windows is to build the installer:
1. Ensure you have [Inno Setup](https://jrsoftware.org/isinfo.php) installed.
2. Run `package.bat` as Administrator.
3. Once finished, run `fsk_setup.exe` from the `Output` folder.

This will automatically:
- Install Fsk and its standard library.
- Add Fsk to your system `PATH`.
- Associate `.fsk` files with the interpreter (double-click to run!).

### Manual Windows Installation (PowerShell)

## Pterodactyl Installation (Hosting)

Fsk provides an official "Egg" for Pterodactyl panels, allowing you to host Fsk applications easily with automated installation.

1. Download the [egg-fsk-language.json](egg-fsk-language.json) file from this repository.
2. Go to your Pterodactyl Admin Panel > **Nests** > **Import Egg**.
3. Select the `egg-fsk-language.json` file and import it.
4. Create a new server using this Egg.
   - The Egg will automatically compile Fsk and set up the environment.
   - You can specify your startup file (default: `docs/server.fsk`).

## Usage

Run a script:
```bash
./fsk script.fsk
```

Interactive mode (REPL):
```bash
./fsk
```

## Syntax Example

```javascript
import "lib.fsk";

class Greeter {
  fn init(name) {
    this.name = name;
  }

  fn greet() {
    print "Hello, " + this.name + "!";
  }
}

let g = Greeter("World");
g.greet();

print "Random: " + FSK.random(1, 100);
```

## Standard Library (FSK)

- `FSK.random(min, max)`: Returns a random number between min and max.
- `FSK.exec(command)`: Executes a shell command.
- `FSK.version()`: Returns the current version.
- `FSK.sleep(ms)`: Pauses execution for `ms` milliseconds.

## Self-Hosted Standard Library (std/)

Fsk comes with a standard library written in Fsk itself, located in the `std/` directory. It is automatically loaded on startup.

### List
A linked list implementation.
```javascript
let list = List();
list.push(10);
list.push(20);
list.map(fn(x) { return x * 2; }).forEach(fn(x) { print x; });
```

### Math
Math utilities.
```javascript
print Math.max(10, 20); // 20
print Math.pow(2, 3); // 8
```

## Web Server

Fsk includes a native HTTP server! You can run the self-hosted documentation:

```bash
./build/fsk docs/server.fsk
```

Visit `https://fsk.kirosb.fr` to see the documentation served by Fsk itself.

### Example Server

```javascript
fn handler(req) {
  return "<h1>Hello from Fsk!</h1>";
}

FSK.startServer(3000, handler);
```





## Anonymous Functions (Lambdas)

Fsk supports anonymous functions, useful for callbacks and functional programming.

```javascript
let double = fn(x) { return x * 2; };
print double(5); // 10
```
