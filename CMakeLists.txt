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
    src/modules/easywsclient.cpp
)

add_executable(fsk ${SOURCES})
target_link_libraries(fsk sqlite3 ssl crypto curl)
if(WIN32)
    target_link_libraries(fsk ws2_32)
endif()



