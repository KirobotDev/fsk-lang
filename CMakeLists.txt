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
    src/compiler/TypeChecker.cpp
    src/compiler/Compiler.cpp
    src/modules/easywsclient.cpp
)

find_package(SQLite3 REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)

set(FSK_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/fsk-core)
set(FSK_CORE_LIB ${FSK_CORE_DIR}/target/release/libfsk_core.a)

find_program(CARGO_EXECUTABLE cargo)
if(NOT CARGO_EXECUTABLE)
    message(FATAL_ERROR "cargo not found in PATH - install Rust to build fsk-core")
endif()

file(GLOB_RECURSE FSK_CORE_SOURCES CONFIGURE_DEPENDS ${FSK_CORE_DIR}/src/*.rs)

add_custom_command(
    OUTPUT ${FSK_CORE_LIB}
    COMMAND ${CMAKE_COMMAND} -E env ${FSK_CORE_LIB} ${FSK_CORE_DIR}/.cargo-lock
    COMMAND ${CARGO_EXECUTABLE} build --release --manifest-path ${FSK_CORE_DIR}/Cargo.toml
    DEPENDS ${FSK_CORE_SOURCES} ${FSK_CORE_DIR}/Cargo.toml
    WORKING_DIRECTORY ${FSK_CORE_DIR}
    COMMENT "Building fsk-core (Rust staticlib)"
    VERBATIM
)

add_custom_target(fsk-core ALL DEPENDS ${FSK_CORE_LIB})

add_executable(fsk ${SOURCES})
add_dependencies(fsk fsk-core)

if(TARGET SQLite3::SQLite3)
    target_link_libraries(fsk PRIVATE SQLite3::SQLite3)
else()
    target_link_libraries(fsk PRIVATE ${SQLite3_LIBRARIES})
    target_include_directories(fsk PRIVATE ${SQLite3_INCLUDE_DIRS})
endif()

target_link_libraries(fsk PRIVATE OpenSSL::SSL OpenSSL::Crypto)

target_link_libraries(fsk PRIVATE CURL::libcurl)

if(WIN32)
    target_link_libraries(fsk PRIVATE ws2_32)
endif()

target_link_libraries(fsk PRIVATE ${FSK_CORE_LIB})
target_link_libraries(fsk PRIVATE raylib)
target_link_libraries(fsk PRIVATE pthread dl m)



