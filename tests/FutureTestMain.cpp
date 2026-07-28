/**
 * @file FutureTestMain.cpp
 * @brief Main entry point for the future (intentionally red) SQL specification
 *        test suite.
 *
 * These tests describe SQL workflows (CREATE TABLE, INSERT, SELECT, UPDATE,
 * DELETE) that are not yet implemented by the SQL executor.  Every test is
 * expected to fail with a named assertion until the corresponding execution
 * path is completed.
 *
 * See SqlWorkflowTests.cpp for the individual test cases.
 *
 * Test binaries live under build/tests/; fixture data is placed under
 * build/test-data/ (covered by ignored build/).
 */

#include "TestHarness.hpp"

int main() {
    return test::runAllTests();
}
