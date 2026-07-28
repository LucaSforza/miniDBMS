/**
 * @file HeapFileTests.cpp
 * @brief Regression tests for HeapFile CRUD, size checks, deletion
 *        semantics, swap-with-last, truncation, and persistence.
 *
 * Every test creates an isolated fixture under build/test-data/ and
 * cleans up with RAII so repeated runs are independent.
 */

#include "TestHarness.hpp"
#include "HeapFile.hpp"
#include "File.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ==========================================================================
// RAII test-directory helper
// ==========================================================================

struct TestDir {
    std::string path;

    TestDir() {
        static int counter = 0;
        path = "build/test-data/heap-" + std::to_string(++counter);
        fs::create_directories(path);
    }

    ~TestDir() { fs::remove_all(path); }

    std::string file(const std::string& name) const {
        return path + "/" + name;
    }
};

// ==========================================================================
// Record helpers
// ==========================================================================

/// Pack an int into a fixed 4-byte string (little-endian host layout).
static std::string intBytes(int x) {
    std::string s(sizeof(int), '\0');
    std::memcpy(s.data(), &x, sizeof(int));
    return s;
}

/// Build a 12-byte record: [4-byte int key] [8-byte name padded with spaces].
static std::string rec(int id, const std::string& name) {
    std::string data = intBytes(id);
    data += name;
    if (data.size() < 12)
        data.append(12 - data.size(), ' ');
    data.resize(12);
    return data;
}

// ==========================================================================
// Constructor validation
// ==========================================================================

TEST("HeapFile rejects zero recordSize") {
    TestDir dir;
    CHECK_THROW(HeapFile(dir.file("bad.heap"), 4, 0), std::runtime_error);
}

TEST("HeapFile rejects zero keySize") {
    TestDir dir;
    CHECK_THROW(HeapFile(dir.file("bad.heap"), 0, 8), std::runtime_error);
}

TEST("HeapFile rejects keySize larger than recordSize") {
    TestDir dir;
    CHECK_THROW(HeapFile(dir.file("bad.heap"), 8, 4), std::runtime_error);
}

TEST("HeapFile accepts equal keySize and recordSize") {
    // keySize == recordSize is allowed (degenerate case: whole record is key)
    TestDir dir;
    CHECK_NOTHROW(HeapFile(dir.file("ok.heap"), 4, 4));
}

// ==========================================================================
// Empty-file safety
// ==========================================================================

TEST("HeapFile scan on empty file returns empty vector") {
    TestDir dir;
    HeapFile hf(dir.file("empty.heap"), 4, 12);
    auto records = hf.scan();
    CHECK_TRUE(records.empty());
}

TEST("HeapFile getData on empty file returns nullopt") {
    TestDir dir;
    HeapFile hf(dir.file("empty.heap"), 4, 12);
    auto result = hf.getData(intBytes(1));
    CHECK_FALSE(result.has_value());
}

TEST("HeapFile deleteData on empty file returns nullopt") {
    TestDir dir;
    HeapFile hf(dir.file("empty.heap"), 4, 12);
    auto result = hf.deleteData(intBytes(1));
    CHECK_FALSE(result.has_value());
}

// ==========================================================================
// Insert, scan, get
// ==========================================================================

TEST("HeapFile insert then scan returns one record") {
    TestDir dir;
    HeapFile hf(dir.file("ins_scan.heap"), 4, 12);

    std::string r = rec(1, "Alice   ");
    hf.insert(r);

    auto records = hf.scan();
    CHECK_EQ(records.size(), 1);
    CHECK_EQ(records[0], r);
}

TEST("HeapFile insert multiple then scan returns all records") {
    TestDir dir;
    HeapFile hf(dir.file("ins_multi.heap"), 4, 12);

    hf.insert(rec(1, "Alice   "));
    hf.insert(rec(2, "Bob     "));

    auto records = hf.scan();
    CHECK_EQ(records.size(), 2);
}

TEST("HeapFile getData finds existing record by key") {
    TestDir dir;
    HeapFile hf(dir.file("get_ok.heap"), 4, 12);

    std::string r = rec(1, "Alice   ");
    hf.insert(r);

    auto result = hf.getData(intBytes(1));
    CHECK_TRUE(result.has_value());
    CHECK_EQ(*result, r);
}

TEST("HeapFile getData returns nullopt for missing key") {
    TestDir dir;
    HeapFile hf(dir.file("get_miss.heap"), 4, 12);
    hf.insert(rec(1, "Alice   "));

    auto result = hf.getData(intBytes(2));
    CHECK_FALSE(result.has_value());
}

TEST("HeapFile getData throws on key-size mismatch") {
    TestDir dir;
    HeapFile hf(dir.file("get_size.heap"), 4, 12);
    CHECK_THROW(hf.getData("abc"), std::runtime_error);   // only 3 bytes
}

TEST("HeapFile insert throws on record-size mismatch") {
    TestDir dir;
    HeapFile hf(dir.file("ins_bad.heap"), 4, 12);
    CHECK_THROW(hf.insert("short"),            std::runtime_error);
    CHECK_THROW(hf.insert("this_is_way_too_long_for_record_size"), std::runtime_error);
}

// ==========================================================================
// Update
// ==========================================================================

TEST("HeapFile updateData replaces record content") {
    TestDir dir;
    HeapFile hf(dir.file("upd1.heap"), 4, 12);

    hf.insert(rec(1, "Alice   "));
    hf.updateData(intBytes(1), rec(1, "Bob     "));

    auto result = hf.getData(intBytes(1));
    CHECK_TRUE(result.has_value());
    CHECK_EQ(*result, rec(1, "Bob     "));
}

TEST("HeapFile updateData throws on missing key") {
    TestDir dir;
    HeapFile hf(dir.file("upd2.heap"), 4, 12);
    hf.insert(rec(1, "Alice   "));

    CHECK_THROW(hf.updateData(intBytes(2), rec(2, "Bob     ")),
                std::runtime_error);
}

TEST("HeapFile updateData throws on wrong key size") {
    TestDir dir;
    HeapFile hf(dir.file("upd3.heap"), 4, 12);
    CHECK_THROW(hf.updateData("abc", rec(1, "Bob     ")), std::runtime_error);
}

TEST("HeapFile updateData throws on wrong record size") {
    TestDir dir;
    HeapFile hf(dir.file("upd4.heap"), 4, 12);
    hf.insert(rec(1, "Alice   "));
    CHECK_THROW(hf.updateData(intBytes(1), "short"), std::runtime_error);
}

TEST("HeapFile updateData can change the record key and the new key is findable") {
    TestDir dir;
    HeapFile hf(dir.file("upd_key.heap"), 4, 12);

    hf.insert(rec(1, "Alice   "));
    // Overwrite the record found at key 1 with data whose key prefix is 2
    hf.updateData(intBytes(1), rec(2, "Bob     "));

    // The old key no longer resolves
    CHECK_FALSE(hf.getData(intBytes(1)).has_value());
    // The new key finds the updated record
    auto result = hf.getData(intBytes(2));
    CHECK_TRUE(result.has_value());
    CHECK_EQ(*result, rec(2, "Bob     "));
}

// ==========================================================================
// Delete: basic
// ==========================================================================

TEST("HeapFile deleteData returns the deleted record data") {
    TestDir dir;
    HeapFile hf(dir.file("del1.heap"), 4, 12);

    std::string r = rec(1, "Alice   ");
    hf.insert(r);

    auto deleted = hf.deleteData(intBytes(1));
    CHECK_TRUE(deleted.has_value());
    CHECK_EQ(*deleted, r);
}

TEST("HeapFile deleteData returns nullopt for missing key") {
    TestDir dir;
    HeapFile hf(dir.file("del2.heap"), 4, 12);
    hf.insert(rec(1, "Alice   "));

    auto result = hf.deleteData(intBytes(99));
    CHECK_FALSE(result.has_value());
}

TEST("HeapFile deleteData throws on key-size mismatch") {
    TestDir dir;
    HeapFile hf(dir.file("del3.heap"), 4, 12);
    CHECK_THROW(hf.deleteData("abc"), std::runtime_error);
}

// ==========================================================================
// Delete: first / last / only
// ==========================================================================

TEST("HeapFile deleteData on only record leaves file logically empty") {
    TestDir dir;
    HeapFile hf(dir.file("del_only.heap"), 4, 12);
    hf.insert(rec(1, "Alice   "));
    hf.deleteData(intBytes(1));

    auto records = hf.scan();
    CHECK_TRUE(records.empty());
}

TEST("HeapFile deleteData on last record in multi-record file") {
    TestDir dir;
    HeapFile hf(dir.file("del_last.heap"), 4, 12);
    hf.insert(rec(1, "Alice   "));
    hf.insert(rec(2, "Bob     "));

    auto deleted = hf.deleteData(intBytes(2));
    CHECK_TRUE(deleted.has_value());
    CHECK_EQ(deleted->substr(0, 4), intBytes(2));   // returned Bob's key

    // Only Alice remains
    auto records = hf.scan();
    CHECK_EQ(records.size(), 1);
    CHECK_TRUE(hf.getData(intBytes(1)).has_value());
    CHECK_FALSE(hf.getData(intBytes(2)).has_value());
}

TEST("HeapFile deleteData on first record swaps last into its slot") {
    TestDir dir;
    HeapFile hf(dir.file("del_first.heap"), 4, 12);
    hf.insert(rec(1, "Alice   "));
    hf.insert(rec(2, "Bob     "));

    auto deleted = hf.deleteData(intBytes(1));
    CHECK_TRUE(deleted.has_value());
    CHECK_EQ(deleted->substr(0, 4), intBytes(1));   // returned Alice's key

    // After swap, Bob is the only record (may be at any position).
    // Assert by key lookup, not by scan order.
    CHECK_FALSE(hf.getData(intBytes(1)).has_value());
    CHECK_TRUE(hf.getData(intBytes(2)).has_value());
    CHECK_EQ(hf.scan().size(), 1);
}

TEST("HeapFile deleteData on middle record swaps last into slot") {
    TestDir dir;
    HeapFile hf(dir.file("del_mid.heap"), 4, 12);
    hf.insert(rec(1, "Alice   "));
    hf.insert(rec(2, "Bob     "));
    hf.insert(rec(3, "Carol   "));

    hf.deleteData(intBytes(2));   // delete middle record (Bob)

    // Carol (the last) should have been swapped into Bob's old slot
    CHECK_TRUE(hf.getData(intBytes(1)).has_value());
    CHECK_FALSE(hf.getData(intBytes(2)).has_value());
    CHECK_TRUE(hf.getData(intBytes(3)).has_value());
}

// ==========================================================================
// Immediate file-size truncation
// ==========================================================================

TEST("HeapFile file size is zero after deleting the only record") {
    TestDir dir;
    {
        HeapFile hf(dir.file("trunc_only.heap"), 4, 12);
        hf.insert(rec(1, "Alice   "));
        hf.deleteData(intBytes(1));
    }
    CHECK_EQ(fs::file_size(dir.file("trunc_only.heap")), 0);
}

TEST("HeapFile file size shrinks after deleting one record from three") {
    TestDir dir;
    HeapFile hf(dir.file("trunc_multi.heap"), 4, 12);
    hf.insert(rec(1, "Alice   "));
    hf.insert(rec(2, "Bob     "));
    hf.insert(rec(3, "Carol   "));

    hf.flush();
    CHECK_EQ(fs::file_size(dir.file("trunc_multi.heap")), 36);  // 3 * 12

    hf.deleteData(intBytes(1));
    // Size is reduced immediately inside the live HeapFile scope (ftruncate
    // is called before deleteData returns, not deferred to the destructor).
    CHECK_EQ(fs::file_size(dir.file("trunc_multi.heap")), 24);  // 2 * 12
}

// ==========================================================================
// Close/reopen persistence
// ==========================================================================

TEST("HeapFile data persists across close and reopen") {
    TestDir dir;
    {
        HeapFile hf(dir.file("persist.heap"), 4, 12);
        hf.insert(rec(1, "Alice   "));
        hf.insert(rec(2, "Bob     "));
    }   // hf destroyed, file closed

    // Reopen and verify
    HeapFile hf2(dir.file("persist.heap"), 4, 12);
    auto records = hf2.scan();
    CHECK_EQ(records.size(), 2);

    CHECK_TRUE(hf2.getData(intBytes(1)).has_value());
    CHECK_TRUE(hf2.getData(intBytes(2)).has_value());
}

TEST("HeapFile reopened after delete is consistent") {
    TestDir dir;
    {
        HeapFile hf(dir.file("persist_del.heap"), 4, 12);
        hf.insert(rec(1, "Alice   "));
        hf.insert(rec(2, "Bob     "));
        hf.deleteData(intBytes(1));
    }
    // Reopen: only Bob should remain
    HeapFile hf2(dir.file("persist_del.heap"), 4, 12);
    CHECK_EQ(hf2.scan().size(), 1);
    CHECK_FALSE(hf2.getData(intBytes(1)).has_value());
    CHECK_TRUE(hf2.getData(intBytes(2)).has_value());
}

// ==========================================================================
// Misaligned existing file rejection
// ==========================================================================

TEST("HeapFile rejects an existing file whose size is not a multiple of recordSize") {
    TestDir dir;
    // Create a file with 13 bytes (1 byte extra)
    {
        std::ofstream f(dir.file("misalign.heap"), std::ios::binary);
        std::string data = rec(1, "Alice   ");  // 12 bytes
        f.write(data.data(), static_cast<std::streamsize>(data.size()));
        f.write("x", 1);                        // 1 extra byte -> 13 total
    }
    CHECK_THROW(HeapFile(dir.file("misalign.heap"), 4, 12), std::runtime_error);
}
