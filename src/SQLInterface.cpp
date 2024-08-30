#include <iostream>

#include "SQLInterface.hpp"
#include "SQLInterpreter.hpp"

SQLInterface::SQLInterface() : interpreter(SQLInterpreter()) {}

void SQLInterface::run() {
    printWelcomeMessage();

    std::string input;
    while (true) {
        std::cout << "sql> ";
        std::getline(std::cin, input);

        if (input == "exit" || input == "quit") {
            break;
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