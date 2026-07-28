#ifndef TESTHARNESS_HPP
#define TESTHARNESS_HPP

/**
 * @file TestHarness.hpp
 * @brief Minimal C++17 test framework for miniDBMS.
 *
 * Features:
 *   - Static named-test registration via the TEST() macro.
 *   - CHECK_TRUE / CHECK_FALSE / CHECK_EQ / CHECK_NE boolean assertions.
 *   - CHECK_THROW / CHECK_NOTHROW exception assertions.
 *   - FAIL for unconditional failure.
 *   - Every failure reports test name, source file, and line number.
 *   - All tests in a translation unit run even after a failure (no abort).
 *   - Summary printed to stdout; process exits non-zero if any test failed.
 *
 * No external dependencies are required beyond the C++17 standard library.
 */

#include <cstdlib>
#include <exception>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace test {

// -----------------------------------------------------------------------
// Failure record — one per assertion that does not pass.
// -----------------------------------------------------------------------
struct Failure {
    std::string testName;   ///< Name of the test that failed.
    std::string file;       ///< Source file where the failure occurred.
    int         line;       ///< Line number within that file.
    std::string message;    ///< Human-readable description.
};

// -----------------------------------------------------------------------
// TestRegistry — singleton that collects all tests, runs them, and
// produces a summary.  Each test is a named void function.
// -----------------------------------------------------------------------
class TestRegistry {
public:
    static TestRegistry& instance() {
        static TestRegistry inst;
        return inst;
    }

    /// Register a test case.  Called during static initialisation.
    void add(std::string name, std::function<void()> body) {
        tests_.push_back({std::move(name), std::move(body)});
    }

    /// Record a failure in the currently executing test.
    void recordFailure(const std::string& file, int line,
                       const std::string& message) {
        failures_.push_back({current_, file, line, message});
    }

    /// Run every registered test and return 0 on success, 1 on failure.
    int runAll() {
        int passed = 0;
        int failed = 0;

        for (auto& t : tests_) {
            current_ = t.name;
            failures_.clear();

            try {
                t.body();
            } catch (const std::exception& e) {
                recordFailure(__FILE__, __LINE__,
                    std::string("Unhandled std::exception in test \"")
                    + t.name + "\": " + e.what());
            } catch (...) {
                recordFailure(__FILE__, __LINE__,
                    std::string("Unhandled non-standard exception in test \"")
                    + t.name + "\"");
            }

            if (failures_.empty()) {
                ++passed;
                std::cout << "  PASS  " << t.name << "\n";
            } else {
                ++failed;
                std::cout << "  FAIL  " << t.name << "\n";
                for (const auto& f : failures_) {
                    std::cout << "    " << f.file << ":" << f.line
                              << "  " << f.message << "\n";
                }
            }
        }

        // Summary line.
        int total = passed + failed;
        std::cout << "\n====================\n"
                  << "Total: " << total
                  << "  Passed: " << passed
                  << "  Failed: " << failed << "\n"
                  << "====================\n";

        return (failed > 0) ? EXIT_FAILURE : EXIT_SUCCESS;
    }

private:
    struct TestCase {
        std::string             name;
        std::function<void()>   body;
    };

    std::vector<TestCase>  tests_;
    std::string            current_;       ///< Name of the running test.
    std::vector<Failure>   failures_;      ///< Failures in the current test.
};

// -----------------------------------------------------------------------
// Registrar — a small helper whose constructor calls TestRegistry::add().
// Every TEST() macro creates a static Registrar object so that tests
// are self-registering before main() runs.
// -----------------------------------------------------------------------
struct Registrar {
    Registrar(const std::string& name, std::function<void()> body) {
        TestRegistry::instance().add(name, std::move(body));
    }
};

/// Convenience: forwarder so assertion macros don't need to spell out
/// TestRegistry::instance() every time.
inline void recordFailure(const std::string& file, int line,
                          const std::string& message) {
    TestRegistry::instance().recordFailure(file, line, message);
}

/// Run all registered tests.  Call this from your main().
inline int runAllTests() {
    return TestRegistry::instance().runAll();
}

}   // namespace test

// -----------------------------------------------------------------------
// Macro helpers
// -----------------------------------------------------------------------

/// Internal token pasting (two levels to force expansion of __LINE__).
#define TEST_HARNESS_CONCAT_(a, b) a##b
#define TEST_HARNESS_CONCAT(a, b)  TEST_HARNESS_CONCAT_(a, b)

/**
 * @def TEST(name)
 *
 * Register a named test case.  Usage:
 *
 *     TEST("Arithmetic works") {
 *         CHECK_EQ(1 + 1, 2);
 *     }
 *
 * The argument must be a string literal.  The test body is a block that
 * follows the macro invocation (no semicolon after the closing brace).
 */
#define TEST(name)                                                          \
    static void TEST_HARNESS_CONCAT(test_fn_, __LINE__)();                  \
    static ::test::Registrar TEST_HARNESS_CONCAT(test_reg_, __LINE__)(      \
        (name), TEST_HARNESS_CONCAT(test_fn_, __LINE__));                   \
    static void TEST_HARNESS_CONCAT(test_fn_, __LINE__)()

// -----------------------------------------------------------------------
// Assertion macros
// -----------------------------------------------------------------------

/**
 * @def CHECK_TRUE(expr)
 * Pass if @p expr evaluates to true.
 */
#define CHECK_TRUE(expr)                                                    \
    do {                                                                    \
        if (!(expr)) {                                                      \
            ::test::TestRegistry::instance().recordFailure(                 \
                __FILE__, __LINE__,                                         \
                "CHECK_TRUE(" #expr ") failed");                            \
        }                                                                   \
    } while (false)

/**
 * @def CHECK_FALSE(expr)
 * Pass if @p expr evaluates to false.
 */
#define CHECK_FALSE(expr)                                                   \
    do {                                                                    \
        if (static_cast<bool>(expr)) {                                      \
            ::test::TestRegistry::instance().recordFailure(                 \
                __FILE__, __LINE__,                                         \
                "CHECK_FALSE(" #expr ") failed");                           \
        }                                                                   \
    } while (false)

/**
 * @def CHECK_EQ(lhs, rhs)
 * Pass if @p lhs == @p rhs.
 */
#define CHECK_EQ(lhs, rhs)                                                  \
    do {                                                                    \
        if (!((lhs) == (rhs))) {                                            \
            ::test::TestRegistry::instance().recordFailure(                 \
                __FILE__, __LINE__,                                         \
                "CHECK_EQ(" #lhs ", " #rhs ") failed");                     \
        }                                                                   \
    } while (false)

/**
 * @def CHECK_NE(lhs, rhs)
 * Pass if @p lhs != @p rhs.
 */
#define CHECK_NE(lhs, rhs)                                                  \
    do {                                                                    \
        if ((lhs) == (rhs)) {                                               \
            ::test::TestRegistry::instance().recordFailure(                 \
                __FILE__, __LINE__,                                         \
                "CHECK_NE(" #lhs ", " #rhs ") failed");                     \
        }                                                                   \
    } while (false)

/**
 * @def CHECK_THROW(expr, ex_type)
 * Pass if @p expr throws an exception of type @p ex_type (or a derived
 * type that a handler of @p ex_type would catch).
 */
#define CHECK_THROW(expr, ex_type)                                          \
    do {                                                                    \
        bool TEST_HARNESS_CONCAT(__test_caught_, __LINE__) = false;         \
        try { expr; }                                                       \
        catch (const ex_type&) {                                            \
            TEST_HARNESS_CONCAT(__test_caught_, __LINE__) = true;           \
        }                                                                   \
        catch (...) { }                                                     \
        if (!TEST_HARNESS_CONCAT(__test_caught_, __LINE__)) {               \
            ::test::TestRegistry::instance().recordFailure(                 \
                __FILE__, __LINE__,                                         \
                "CHECK_THROW(" #expr ", " #ex_type ") failed: "             \
                "no exception thrown");                                     \
        }                                                                   \
    } while (false)

/**
 * @def CHECK_NOTHROW(expr)
 * Pass if @p expr completes without throwing any exception.
 */
#define CHECK_NOTHROW(expr)                                                 \
    do {                                                                    \
        try { expr; }                                                       \
        catch (const std::exception& TEST_HARNESS_CONCAT(__e_, __LINE__)) { \
            ::test::TestRegistry::instance().recordFailure(                 \
                __FILE__, __LINE__,                                         \
                "CHECK_NOTHROW(" #expr ") failed: "                         \
                + std::string(TEST_HARNESS_CONCAT(__e_, __LINE__).what())); \
        }                                                                   \
        catch (...) {                                                       \
            ::test::TestRegistry::instance().recordFailure(                 \
                __FILE__, __LINE__,                                         \
                "CHECK_NOTHROW(" #expr ") failed: "                         \
                "non-standard exception");                                  \
        }                                                                   \
    } while (false)

/**
 * @def FAIL(msg)
 * Unconditionally record a failure with the given message string literal.
 * Useful for marking unimplemented behaviour in future specifications.
 */
#define FAIL(msg)                                                           \
    ::test::TestRegistry::instance().recordFailure(                         \
        __FILE__, __LINE__, "FAIL: " msg)

#endif  // TESTHARNESS_HPP
