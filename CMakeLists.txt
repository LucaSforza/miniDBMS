cmake_minimum_required(VERSION 3.10)

# Nome del progetto
project(MiniDBMS VERSION 1.0)

# Specifica lo standard C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Aggiungi l'eseguibile principale
add_executable(MiniDBMS src/main.cpp)

# Aggiungi le directory con i file header
include_directories(src)

# Aggiungi i file sorgente al progetto
add_library(StorageEngine src/StorageEngine.cpp)
# add_library(BTree src/BTree.cpp src/BTree.h)
# add_library(SQLParser src/SQLParser.cpp src/SQLParser.h)
# add_library(QueryExecutor src/QueryExecutor.cpp src/QueryExecutor.h)
# add_library(TransactionManager src/TransactionManager.cpp src/TransactionManager.h)

# Collega le librerie all'eseguibile
target_link_libraries(MiniDBMS StorageEngine)
# target_link_libraries(MiniDBMS BTree)
# target_link_libraries(MiniDBMS SQLParser)
# target_link_libraries(MiniDBMS QueryExecutor)
# target_link_libraries(MiniDBMS TransactionManager)

# Aggiungi i test se esistono
# enable_testing()

# Aggiungi test per StorageEngine
# add_executable(StorageEngineTest tests/StorageEngineTest.cpp)
# target_link_libraries(StorageEngineTest StorageEngine)
# add_test(NAME StorageEngineTest COMMAND StorageEngineTest)

# Aggiungi test per BTree
# add_executable(BTreeTest tests/BTreeTest.cpp)
# target_link_libraries(BTreeTest BTree)
# add_test(NAME BTreeTest COMMAND BTreeTest)

# Aggiungi test per SQLParser
# add_executable(SQLParserTest tests/SQLParserTest.cpp)
# target_link_libraries(SQLParserTest SQLParser)
# add_test(NAME SQLParserTest COMMAND SQLParserTest)

# Aggiungi test per QueryExecutor
# add_executable(QueryExecutorTest tests/QueryExecutorTest.cpp)
# target_link_libraries(QueryExecutorTest QueryExecutor)
# add_test(NAME QueryExecutorTest COMMAND QueryExecutorTest)

# Aggiungi test per TransactionManager
# add_executable(TransactionManagerTest tests/TransactionManagerTest.cpp)
# target_link_libraries(TransactionManagerTest TransactionManager)
# add_test(NAME TransactionManagerTest COMMAND TransactionManagerTest)
