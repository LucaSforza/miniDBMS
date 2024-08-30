#include <iostream>
#include <ncurses.h>

#include "SQLInterface.hpp"

int main(void) {

    auto sqlInterface = SQLInterface();

    sqlInterface.run();

    return 0;
}