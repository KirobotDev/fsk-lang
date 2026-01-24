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

## Building the Project

Once the prerequisites are installed, you can build the project:

1.  Open a **Developer Command Prompt for VS 2022** (search for it in the Start menu).
2.  Navigate to your `fsk-lang` directory:
    ```cmd
    cd C:\fsk-lang
    ```
3.  Create a build directory and configure the project:
    ```cmd
    mkdir build
    cd build
    cmake .. -DCMAKE_TOOLCHAIN_FILE="C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake" -A x64
    ```
    *(Replace `C:/path/to/vcpkg/` with the actual path where you installed vcpkg)*

4.  Build the executable:
    ```cmd
    cmake --build . --config Release
    ```

The compiled `fsk.exe` will be located in `build\Release\`.

## Troubleshooting

### 'cmake' is not recognized
Ensure CMake is in your system PATH. You can check this by typing `cmake --version` in a new terminal window. If it fails, add the directory containing `cmake.exe` (usually `C:\Program Files\CMake\bin`) to your environment variables.

### Missing Libraries
If CMake fails to find SQLite3, OpenSSL, or CURL, double-check the `-DCMAKE_TOOLCHAIN_FILE` path in the configuration step.
