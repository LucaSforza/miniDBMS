/**
 * @file TestMain.cpp
 * @brief Main entry point for the green (passing) regression test suite.
 *
 * Tests are registered through the TEST() macro and run automatically
 * by test::runAllTests().  The process exits with EXIT_SUCCESS when
 * every test passes, or EXIT_FAILURE when any assertion fails.
 *
 * Test objects and binaries live under build/tests/; fixture data
 * should be placed under build/test-data/ by each test file.
 */

#include "TestHarness.hpp"

int main() {
    return test::runAllTests();
}
