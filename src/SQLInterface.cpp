#include <iostream>
#include <string>

#include "linenoise.h"
#include "SQLInterface.hpp"
#include "SQLInterpreter.hpp"

SQLInterface::SQLInterface() : interpreter(SQLInterpreter()) {}

void SQLInterface::run() {
    printWelcomeMessage();

    char *line;
    while ((line = linenoise("sql> ")) != nullptr) {
        std::string input(line);
        linenoiseFree(line);

        if (input == "exit" || input == "quit") {
            break;
        }

        if (!input.empty()) {
            linenoiseHistoryAdd(input.c_str());
        }

        handleInput(input);
    }
}

void SQLInterface::printWelcomeMessage() {
    std::cout << "Welcome to MiniDBMS! Type 'exit' or 'quit' to exit." << std::endl;
}

void SQLInterface::handleInput(const std::string& input) {
    try {
        interpreter.execute(input);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}