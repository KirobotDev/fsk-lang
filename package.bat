@echo off
setlocal enabledelayedexpansion

echo ########################################
echo # Fsk Language Packaging Script        #
echo ########################################

set VCPKG_PATH=C:\vcpkg
set INNO_SETUP_PATH="C:\Program Files (x86)\Inno Setup 6\ISCC.exe"

echo [*] Building project...
if not exist build mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE="%VCPKG_PATH%\scripts\buildsystems\vcpkg.cmake" -A x64
if %ERRORLEVEL% neq 0 (
    echo [!] CMake configuration failed.
    exit /b %ERRORLEVEL%
)

cmake --build . --config Release
if %ERRORLEVEL% neq 0 (
    echo [!] Build failed.
    exit /b %ERRORLEVEL%
)
cd ..

echo [*] Checking for dependencies...

echo [*] Creating installer with Inno Setup...
if exist %INNO_SETUP_PATH% (
    %INNO_SETUP_PATH% setup.iss
    if %ERRORLEVEL% neq 0 (
        echo [!] Inno Setup failed.
        exit /b %ERRORLEVEL%
      )
    echo [x] Installer created successfully in Output folder!
) else (
    echo [!] Inno Setup (ISCC.exe) not found at %INNO_SETUP_PATH%.
    echo [!] Please install Inno Setup or update the path in this script.
    exit /b 1
)

echo [x] Done!
pause
