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

find_package(SQLite3 REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)

add_executable(fsk ${SOURCES})

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

target_link_libraries(fsk PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/fsk-core/target/release/libfsk_core.a)
target_link_libraries(fsk PRIVATE pthread dl m)



