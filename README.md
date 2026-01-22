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

```bash
mkdir build
cd build
cmake ..
make
```

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

### Windows Installation (PowerShell)
Run as Administrator:
```powershell
New-Item -ItemType Directory -Force -Path "C:\Fsk"

Copy-Item -Recurse -Force "std" "C:\Fsk\"

[System.Environment]::SetEnvironmentVariable("Path", $env:Path + ";C:\Fsk", "User")

Copy-Item -Recurse -Force "fpm" "C:\Fsk\"
Set-Content -Path "C:\Fsk\fpm.bat" -Value '@fsk C:\Fsk\fpm\fpm.fsk %*'
```
> Note: You will need to manually compile or download `fsk.exe` and place it in `C:\Fsk\`.

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
