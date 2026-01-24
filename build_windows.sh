#!/bin/bash
VCPKG_ROOT="$HOME/vcpkg"
TRIPLET="x64-mingw-static"
BUILD_DIR="build_windows"

echo "########################################"
echo "# Fsk Language - Windows Build (Linux) #"
echo "########################################"

if ! command -v x86_64-w64-mingw32-g++ &> /dev/null; then
    echo "[!] MinGW (x86_64-w64-mingw32-g++) not found."
    echo "[!] Please install it: sudo apt install g++-mingw-w64-x86-64"
    exit 1
fi

if [ ! -f "$VCPKG_ROOT/vcpkg" ]; then
    echo "[!] vcpkg not found at $VCPKG_ROOT."
    echo "[!] Please set VCPKG_ROOT in this script or install vcpkg."
    exit 1
fi

echo "[*] Ensuring dependencies for $TRIPLET are installed..."
"$VCPKG_ROOT/vcpkg" install sqlite3:$TRIPLET openssl:$TRIPLET curl:$TRIPLET

echo "[*] Configuring project for Windows..."
cmake -B $BUILD_DIR \
    -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
    -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
    -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
    -DVCPKG_TARGET_TRIPLET=$TRIPLET \
    -DVCPKG_APPLOCAL_DEPS=OFF \
    -DCMAKE_BUILD_TYPE=Release .

if [ $? -ne 0 ]; then
    echo "[!] CMake configuration failed."
    exit 1
fi

echo "[*] Building fsk.exe..."
cmake --build $BUILD_DIR --config Release

if [ $? -eq 0 ]; then
    echo "[x] Build successful!"
    echo "[x] Executable: $BUILD_DIR/fsk.exe"
    
    if command -v zip &> /dev/null; then
        echo "[*] Creating distribution zip..."
        mkdir -p dist
        cp $BUILD_DIR/fsk.exe dist/
        cp -r std dist/
        zip -r fsk_windows.zip dist/
        echo "[x] Distribution file: fsk_windows.zip"
    fi
else
    echo "[!] Build failed."
    exit 1
fi
