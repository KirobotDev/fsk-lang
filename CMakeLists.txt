cmake_minimum_required(VERSION 3.10)
project(FuckSociety)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

set(SOURCES 
    src/main.cpp 
    src/lexer/Lexer.cpp
    src/parser/Parser.cpp
    src/runtime/Interpreter.cpp
    src/runtime/Callable.cpp
)

add_executable(fsk ${SOURCES})

# Future: Add dependencies like LLVM, libcurl, etc.
