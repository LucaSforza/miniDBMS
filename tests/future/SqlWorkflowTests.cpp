/**
 * @file SqlWorkflowTests.cpp
 * @brief Intentionally red specification tests for the minimal SQL dialect.
 *
 * These tests describe the expected behaviour of five SQL workflows:
 *   1. CREATE TABLE with integer primary key and VARCHAR field
 *   2. INSERT then SELECT
 *   3. Persisted SELECT after reopening the database
 *   4. Key-filtered UPDATE then SELECT
 *   5. Key-filtered DELETE then SELECT
 *
 * All tests are expected to **fail** until the SQL executor implements the
 * corresponding execution paths.  The failure is a named assertion that
 * clearly identifies the missing behaviour.
 *
 * The SQL statements use syntax accepted by the pinned Hyrise SQL parser;
 * any failure is due to incomplete execution, not parser rejection.
 */

#include "TestHarness.hpp"
#include "SQLInterpreter.hpp"
#include "StorageEngine.hpp"

#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>

// -----------------------------------------------------------------------
// RAII helpers
// -----------------------------------------------------------------------
namespace {

/**
 * @brief Redirect std::cout to an internal stringstream for the lifetime
 *        of this object.
 *
 * The original stream buffer is restored in the destructor, making this
 * safe to use even when an exception unwinds past the guard.
 */
struct CoutCapture {
    std::stringstream buffer_;
    std::streambuf*   old_;

    CoutCapture()
        : old_(std::cout.rdbuf())
    {
        std::cout.rdbuf(buffer_.rdbuf());
    }

    ~CoutCapture() {
        std::cout.rdbuf(old_);
    }

    std::string str() const { return buffer_.str(); }

    CoutCapture(const CoutCapture&)            = delete;
    CoutCapture& operator=(const CoutCapture&) = delete;
};

/**
 * @brief RAII fixture for an isolated test database directory.
 *
 * On construction, removes any stale state at the target path (so that
 * a previous interrupted run does not pollute the test) and creates a
 * fresh empty directory.  On destruction the entire tree is removed.
 *
 * The persistence test holds one fixture across two Database lifetimes
 * so the directory survives the close/reopen cycle.
 */
struct TestDbDir {
    std::string path_;

    TestDbDir(const char* subdir) {
        namespace fs = std::filesystem;
        path_ = std::string("build/test-data/") + subdir;

        // Ensure the parent exists (build/test-data/ is git-ignored).
        fs::path parent("build/test-data");
        if (!fs::exists(parent)) {
            fs::create_directories(parent);
        }

        // Remove any leftover state from a prior run.
        if (fs::exists(path_)) {
            fs::remove_all(path_);
        }
        fs::create_directory(path_);
    }

    ~TestDbDir() {
        namespace fs = std::filesystem;
        std::error_code ec;
        fs::remove_all(path_, ec);      // best-effort cleanup
    }

    const std::string& path() const { return path_; }

    TestDbDir(const TestDbDir&)            = delete;
    TestDbDir& operator=(const TestDbDir&) = delete;
};

} // anonymous namespace

// =======================================================================
// Test 1 — CREATE TABLE
// =======================================================================

TEST("Future: CREATE TABLE with integer primary key and VARCHAR field") {
    TestDbDir      dir("create_table_test");
    Database       db("create_db", dir.path());
    SQLInterpreter interp(db);

    std::string setupOutput;
    {
        CoutCapture cap;
        interp.execute(
            "CREATE TABLE users ("
            "  id   INTEGER PRIMARY KEY,"
            "  name VARCHAR(25)"
            ")"
        );
        setupOutput = cap.str();
    }

    // The parser must accept the statement.
    CHECK_EQ(setupOutput.find("SQL_PARSER_ERROR"), std::string::npos);

    // When CREATE TABLE execution is implemented the output should indicate
    // success.  Currently the statement falls through to “unsupported query”.
    CHECK_EQ(setupOutput.find("unsupported query"), std::string::npos);
}

// =======================================================================
// Test 2 — INSERT then SELECT
// =======================================================================

TEST("Future: INSERT then SELECT shows inserted row") {
    TestDbDir      dir("insert_select_test");
    Database       db("is_db", dir.path());
    SQLInterpreter interp(db);

    // --- setup: CREATE TABLE + INSERT ---
    std::string setupOutput;
    {
        CoutCapture cap;
        interp.execute(
            "CREATE TABLE users ("
            "  id   INTEGER PRIMARY KEY,"
            "  name VARCHAR(25)"
            ")"
        );
        interp.execute("INSERT INTO users VALUES (1, 'Alice')");
        setupOutput = cap.str();
    }

    // Setup must parse and execute without error.
    CHECK_EQ(setupOutput.find("SQL_PARSER_ERROR"), std::string::npos);
    CHECK_EQ(setupOutput.find("unsupported query"), std::string::npos);

    // --- SELECT output captured separately ---
    // This prevents the test from passing merely because INSERT
    // echoes the value while SELECT itself remains broken.
    std::string selectOutput;
    {
        CoutCapture cap;
        interp.execute("SELECT * FROM users");
        selectOutput = cap.str();
    }

    // SELECT itself must execute without parser or unsupported error,
    // otherwise a partially implemented SELECT that prints a value
    // alongside an error marker would incorrectly pass.
    CHECK_EQ(selectOutput.find("SQL_PARSER_ERROR"), std::string::npos);
    CHECK_EQ(selectOutput.find("unsupported query"), std::string::npos);

    CHECK_NE(selectOutput.find("Alice"), std::string::npos);
}

// =======================================================================
// Test 3 — Persistence across reopen
// =======================================================================

TEST("Future: SELECT persists after reopening database") {
    TestDbDir dir("reopen_test");

    // --- Phase 1: create table and insert a row ---
    std::string setupOutput;
    {
        CoutCapture     cap;
        Database        db1("re_db", dir.path());
        SQLInterpreter  interp1(db1);

        interp1.execute(
            "CREATE TABLE users ("
            "  id   INTEGER PRIMARY KEY,"
            "  name VARCHAR(25)"
            ")"
        );
        interp1.execute("INSERT INTO users VALUES (1, 'Alice')");
        setupOutput = cap.str();
        // db1 destroyed here — file descriptors are closed.
    }

    // Phase 1 must complete without parser or execution error before
    // persistence can be tested.
    CHECK_EQ(setupOutput.find("SQL_PARSER_ERROR"), std::string::npos);
    CHECK_EQ(setupOutput.find("unsupported query"), std::string::npos);

    // --- Phase 2: reopen the same directory and SELECT ---
    std::string selectOutput;
    {
        CoutCapture     cap;
        Database        db2("re_db", dir.path());
        SQLInterpreter  interp2(db2);

        interp2.execute("SELECT * FROM users");
        selectOutput = cap.str();
    }

    // SELECT itself must execute without parser or unsupported error.
    CHECK_EQ(selectOutput.find("SQL_PARSER_ERROR"), std::string::npos);
    CHECK_EQ(selectOutput.find("unsupported query"), std::string::npos);

    // When persistence is implemented the re-opened table should still
    // contain the row inserted in phase 1.
    CHECK_NE(selectOutput.find("Alice"), std::string::npos);
}

// =======================================================================
// Test 4 — UPDATE by key
// =======================================================================

TEST("Future: UPDATE by key changes value from Alice to Bob") {
    TestDbDir      dir("update_test");
    Database       db("up_db", dir.path());
    SQLInterpreter interp(db);

    // --- setup: CREATE TABLE + INSERT + UPDATE ---
    std::string setupOutput;
    {
        CoutCapture cap;
        interp.execute(
            "CREATE TABLE users ("
            "  id   INTEGER PRIMARY KEY,"
            "  name VARCHAR(25)"
            ")"
        );
        interp.execute("INSERT INTO users VALUES (1, 'Alice')");
        interp.execute("UPDATE users SET name = 'Bob' WHERE id = 1");
        setupOutput = cap.str();
    }

    CHECK_EQ(setupOutput.find("SQL_PARSER_ERROR"), std::string::npos);
    CHECK_EQ(setupOutput.find("unsupported query"), std::string::npos);

    // --- SELECT output ---
    std::string selectOutput;
    {
        CoutCapture cap;
        interp.execute("SELECT * FROM users");
        selectOutput = cap.str();
    }

    // SELECT itself must execute without parser or unsupported error.
    CHECK_EQ(selectOutput.find("SQL_PARSER_ERROR"), std::string::npos);
    CHECK_EQ(selectOutput.find("unsupported query"), std::string::npos);

    // After UPDATE the new value (Bob) must appear in SELECT.
    CHECK_NE(selectOutput.find("Bob"), std::string::npos);

    // The old value should no longer be visible under the updated key.
    CHECK_EQ(selectOutput.find("Alice"), std::string::npos);
}

// =======================================================================
// Test 5 — DELETE by key
// =======================================================================

TEST("Future: DELETE by key removes row leaving other rows intact") {
    TestDbDir      dir("delete_test");
    Database       db("del_db", dir.path());
    SQLInterpreter interp(db);

    // --- setup: CREATE TABLE + two INSERTs + DELETE ---
    std::string setupOutput;
    {
        CoutCapture cap;
        interp.execute(
            "CREATE TABLE users ("
            "  id   INTEGER PRIMARY KEY,"
            "  name VARCHAR(25)"
            ")"
        );
        interp.execute("INSERT INTO users VALUES (1, 'Alice')");
        interp.execute("INSERT INTO users VALUES (2, 'Bob')");
        interp.execute("DELETE FROM users WHERE id = 1");
        setupOutput = cap.str();
    }

    CHECK_EQ(setupOutput.find("SQL_PARSER_ERROR"), std::string::npos);
    CHECK_EQ(setupOutput.find("unsupported query"), std::string::npos);

    // --- SELECT output ---
    std::string selectOutput;
    {
        CoutCapture cap;
        interp.execute("SELECT * FROM users");
        selectOutput = cap.str();
    }

    // SELECT itself must execute without parser or unsupported error.
    CHECK_EQ(selectOutput.find("SQL_PARSER_ERROR"), std::string::npos);
    CHECK_EQ(selectOutput.find("unsupported query"), std::string::npos);

    // The surviving row (Bob) must appear in SELECT.
    CHECK_NE(selectOutput.find("Bob"), std::string::npos);

    // The deleted row must no longer appear.
    CHECK_EQ(selectOutput.find("Alice"), std::string::npos);
}
