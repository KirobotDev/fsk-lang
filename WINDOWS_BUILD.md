# Building Fsk Language on Windows

This guide provides detailed instructions for setting up the environment and building `fsk-lang` on a Windows system.

## Prerequisites

### 1. Install CMake
*   Download the Windows Installer from [cmake.org](https://cmake.org/download/).
*   **IMPORTANT**: During installation, select **"Add CMake to the system PATH for all users"** (or current user).

### 2. Install a C++ Compiler
We recommend using **Visual Studio 2022**:
*   Download [Visual Studio Community 2022](https://visualstudio.microsoft.com/vs/community/).
*   Run the installer and select the **"Desktop development with C++"** workload.
*   Ensure that the **MSVC v143 - VS 2022 C++ x64/x86 build tools** and **Windows 10/11 SDK** are selected.

### 3. Install Dependencies (Using vcpkg)
The easiest way to manage dependencies on Windows is using `vcpkg`.

1.  Open a terminal (PowerShell or Command Prompt).
2.  Install `vcpkg`:
    ```powershell
    git clone https://github.com/microsoft/vcpkg.git
    cd vcpkg
    .\bootstrap-vcpkg.bat
    ```
3.  Install the required libraries:
    ```powershell
    .\vcpkg install sqlite3:x64-windows openssl:x64-windows curl:x64-windows
    ```

### 4. Install Rust (Required)
Fsk "Perfect Edition" requires Rust for the core modules.
1. Download **rustup-init.exe** from [rustup.rs](https://rustup.rs/).
2. Run the installer and follow the default instructions.
3. Open a new terminal and verify with `cargo --version`.

## Building the Project

Once the prerequisites are installed, you can build the project:

1.  Open a **Developer Command Prompt for VS 2022**.
2.  Navigate to your `fsk-lang` directory:
    ```cmd
    cd C:\fsk-lang
    ```

3.  Build the Rust Core:
    ```cmd
    cd fsk-core
    cargo build --release
    cd ..
    ```

4.  Configure CMake:
    ```cmd
    mkdir build
    cd build
    cmake .. -DCMAKE_TOOLCHAIN_FILE="C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake" -A x64
    ```
    *(Replace `C:/path/to/vcpkg/` with the actual path where you installed vcpkg)*

5.  Build the executable:
    ```cmd
    cmake --build . --config Release
    ```

The compiled `fsk.exe` will be located in `build\Release\`.
