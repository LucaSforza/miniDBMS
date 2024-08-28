cmake_minimum_required(VERSION 3.10)

# Nome del progetto
project(MiniDBMS VERSION 1.0)

# Specifica lo standard C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Aggiungi le directory con i file header
include_directories(${CMAKE_SOURCE_DIR}/include)

# Aggiungi l'eseguibile principale
add_executable(MiniDBMS src/main.cpp)

# Aggiungi i file sorgente al progetto
add_library(StorageEngine src/StorageEngine.cpp src/Files.cpp src/Domains.cpp)

# Collega le librerie all'eseguibile
target_link_libraries(MiniDBMS StorageEngine)