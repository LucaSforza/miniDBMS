#ifndef SQLINTERFACE_HPP
#define SQLINTERFACE_HPP
    
#include <string>
#include "SQLInterpreter.hpp"
    
class SQLInterface {
public:
    SQLInterface();
    
    void run();

private:
    SQLInterpreter interpreter;
    
    void printWelcomeMessage();
    void handleInput(const std::string& input);
};
    
#endif // SQLINTERFACE_HPP