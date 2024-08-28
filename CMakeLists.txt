cmake_minimum_required(VERSION 3.10)

# Nome del progetto
project(MiniDBMS VERSION 1.0)

# Specifica lo standard C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include(ExternalProject)

set(SQL_PARSER_DIR ${CMAKE_SOURCE_DIR}/libs/sql-parser)

# Aggiungi le directory con i file header
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${SQL_PARSER_DIR}/src)

ExternalProject_Add(
    sqlparser
    SOURCE_DIR ${SQL_PARSER_DIR}
    CONFIGURE_COMMAND ""
    BUILD_COMMAND make
    INSTALL_COMMAND ""
    BUILD_IN_SOURCE 1
)

# Aggiungi l'eseguibile principale
add_executable(MiniDBMS src/main.cpp)

# Aggiungi i file sorgente al progetto
add_library(StorageEngine src/StorageEngine.cpp src/Files.cpp src/Domains.cpp)

# Collega le librerie all'eseguibile
add_dependencies(MiniDBMS sqlparser)
target_link_libraries(MiniDBMS StorageEngine)
target_link_libraries(MiniDBMS ${SQL_PARSER_DIR}/libsqlparser.so)