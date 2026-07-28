/**
 * @file TablesTests.cpp
 * @brief Regression tests for VirtualTable and PhysicalTable CRUD,
 *        schema normalisation, duplicate-key rejection, and safe updates.
 *
 * All records are fixed-width raw byte strings.  No SQL codec assumptions.
 * Heap-file fixtures are isolated under build/test-data/ with RAII cleanup.
 */

#include "TestHarness.hpp"

// StorageEngine.hpp must appear before Tables.hpp because of the current
// header cycle (StorageEngine.hpp includes Tables.hpp internally at the
// point where Database needs PhysicalTable).  This is the same discipline
// used in src/Tables.cpp.
#include "StorageEngine.hpp"
#include "Tables.hpp"

#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ==========================================================================
// Helpers
// ==========================================================================

static std::shared_ptr<IntegerDomain> intDom() {
    return std::make_shared<IntegerDomain>();
}

static std::shared_ptr<StringDomain> strDom(size_t n) {
    return std::make_shared<StringDomain>(n);
}

/// Pack an int into a fixed 4-byte string (little-endian host layout).
static std::string intBytes(int x) {
    std::string s(sizeof(int), '\0');
    std::memcpy(s.data(), &x, sizeof(int));
    return s;
}

/// Build a 12-byte record: [4-byte int key] [8-byte name padded with spaces].
static std::string personRec(int id, const std::string& name) {
    std::string data = intBytes(id);
    data += name;
    if (data.size() < 12)
        data.append(12 - data.size(), ' ');
    data.resize(12);
    return data;
}

/// Shortcut for a two-field person relation (id:int key, name:string(8)).
static std::shared_ptr<Relation> makePersonRel() {
    return std::make_shared<Relation>(std::vector<Field>({
        Field("id",   intDom(), true),
        Field("name", strDom(8), false)
    }));
}

/// Enum domain for "yes"/"no " (3-byte slot).
static std::shared_ptr<EnumDomain> enumYN() {
    return std::make_shared<EnumDomain>(
        std::vector<std::string>{"yes", "no "});
}

/// Three-field relation: id(int,key) + name(str8) + flag(enumYN,3).
/// recordSize == 15 (4 + 8 + 3).
static std::shared_ptr<Relation> makeFlagRel() {
    return std::make_shared<Relation>(std::vector<Field>({
        Field("id",   intDom(), true),
        Field("name", strDom(8), false),
        Field("flag", enumYN(), false)
    }));
}

/// Build a 15-byte record for makeFlagRel: [4-byte key][8-byte name][3-byte flag].
static std::string flagRec(int id, const std::string& name,
                           const std::string& flag) {
    std::string data = intBytes(id);
    data += name;
    if (data.size() < 12) data.append(12 - data.size(), ' ');
    data.resize(12);
    data += flag;
    if (data.size() < 15) data.append(15 - data.size(), ' ');
    data.resize(15);
    return data;
}

// ==========================================================================
// RAII test-directory helper (for PhysicalTable tests)
// ==========================================================================

struct TestDir {
    std::string path;
    TestDir() {
        static int counter = 0;
        path = "build/test-data/tables-" + std::to_string(++counter);
        fs::create_directories(path);
    }
    ~TestDir() { fs::remove_all(path); }
    std::string file(const std::string& name) const {
        return path + "/" + name;
    }
};

// ======================================================================
// VirtualTable
// ======================================================================

TEST("VirtualTable construction with a valid relation") {
    auto rel = makePersonRel();
    VirtualTable vt(rel);
    CHECK_TRUE(true);   // reached if no exception
}

TEST("VirtualTable rejects null relation") {
    CHECK_THROW(VirtualTable(nullptr), std::invalid_argument);
}

TEST("VirtualTable addRecord with raw-string overload") {
    VirtualTable vt(makePersonRel());
    CHECK_NOTHROW(vt.addRecord(personRec(1, "Alice   ")));
}

TEST("VirtualTable addRecord then getRecord returns the record") {
    VirtualTable vt(makePersonRel());
    vt.addRecord(personRec(1, "Alice   "));

    auto rec = vt.getRecord(intBytes(1));
    CHECK_TRUE(rec.has_value());
    CHECK_EQ(rec->getData(), personRec(1, "Alice   "));
}

TEST("VirtualTable getRecord returns nullopt for missing key") {
    VirtualTable vt(makePersonRel());
    vt.addRecord(personRec(1, "Alice   "));

    auto rec = vt.getRecord(intBytes(99));
    CHECK_FALSE(rec.has_value());
}

TEST("VirtualTable getRecord throws on wrong key size") {
    VirtualTable vt(makePersonRel());
    CHECK_THROW(vt.getRecord("abc"), std::invalid_argument);
}

TEST("VirtualTable rejects duplicate key on insert") {
    VirtualTable vt(makePersonRel());
    vt.addRecord(personRec(1, "Alice   "));
    CHECK_THROW(vt.addRecord(personRec(1, "Bob     ")), std::invalid_argument);
}

TEST("VirtualTable scan returns all inserted records") {
    VirtualTable vt(makePersonRel());
    vt.addRecord(personRec(1, "Alice   "));
    vt.addRecord(personRec(2, "Bob     "));

    auto records = vt.scan();
    CHECK_EQ(records.size(), 2);
}

TEST("VirtualTable deleteRecord removes the record and returns its data") {
    VirtualTable vt(makePersonRel());
    vt.addRecord(personRec(1, "Alice   "));
    vt.addRecord(personRec(2, "Bob     "));

    auto deleted = vt.deleteRecord(intBytes(1));
    CHECK_TRUE(deleted.has_value());
    CHECK_EQ(deleted->getKeyData(), intBytes(1));

    // Record 1 is gone; record 2 remains
    CHECK_FALSE(vt.getRecord(intBytes(1)).has_value());
    CHECK_TRUE(vt.getRecord(intBytes(2)).has_value());
}

TEST("VirtualTable deleteRecord returns nullopt for missing key") {
    VirtualTable vt(makePersonRel());
    vt.addRecord(personRec(1, "Alice   "));

    auto result = vt.deleteRecord(intBytes(99));
    CHECK_FALSE(result.has_value());
}

TEST("VirtualTable updateRecordByKey updates non-key fields") {
    VirtualTable vt(makePersonRel());
    vt.addRecord(personRec(1, "Alice   "));

    Field nameField("name", strDom(8), false);
    bool ok = vt.updateRecordByKey(
        intBytes(1),
        {Value(nameField, std::string_view("Bob     ", 8))}
    );
    CHECK_TRUE(ok);

    auto rec = vt.getRecord(intBytes(1));
    CHECK_TRUE(rec.has_value());
    CHECK_EQ(rec->valueAt(nameField), "Bob     ");
}

TEST("VirtualTable updateRecordByKey rejects duplicate new key") {
    VirtualTable vt(makePersonRel());
    vt.addRecord(personRec(1, "Alice   "));
    vt.addRecord(personRec(2, "Bob     "));

    // Try to change record-2's key from 2 to 1 (already used by Alice)
    Field idField("id", intDom(), true);
    bool ok = vt.updateRecordByKey(
        intBytes(2),
        {Value(idField, intBytes(1))}
    );
    CHECK_FALSE(ok);

    // Record 2 must be unchanged
    auto rec2 = vt.getRecord(intBytes(2));
    CHECK_TRUE(rec2.has_value());
    CHECK_EQ(rec2->getKeyData(), intBytes(2));
}

TEST("VirtualTable updateRecordByKey preserves original on failed validation") {
    VirtualTable vt(makePersonRel());
    vt.addRecord(personRec(1, "Alice   "));

    Field nameField("name", strDom(8), false);
    // Value exceeds field size (9 bytes for 8-byte slot) -> throws
    CHECK_THROW(vt.updateRecordByKey(
        intBytes(1),
        {Value(nameField, std::string_view("123456789", 9))}
    ), std::invalid_argument);

    // Original data must be unchanged
    auto rec = vt.getRecord(intBytes(1));
    CHECK_TRUE(rec.has_value());
    CHECK_EQ(rec->valueAt(nameField), "Alice   ");
}

TEST("VirtualTable updateRecordByKey returns false for missing key") {
    VirtualTable vt(makePersonRel());
    vt.addRecord(personRec(1, "Alice   "));

    Field nameField("name", strDom(8), false);
    bool ok = vt.updateRecordByKey(
        intBytes(99),
        {Value(nameField, std::string_view("Bob     ", 8))}
    );
    CHECK_FALSE(ok);
}

TEST("VirtualTable updateRecordByKey can change the key") {
    VirtualTable vt(makePersonRel());
    vt.addRecord(personRec(1, "Alice   "));

    Field idField("id", intDom(), true);
    bool ok = vt.updateRecordByKey(
        intBytes(1), {Value(idField, intBytes(2))}
    );
    CHECK_TRUE(ok);

    // Old key must be gone; new key finds the record
    CHECK_FALSE(vt.getRecord(intBytes(1)).has_value());

    auto rec = vt.getRecord(intBytes(2));
    CHECK_TRUE(rec.has_value());
    CHECK_EQ(rec->getKeyData(), intBytes(2));
}

TEST("VirtualTable multi-value update: earlier valid + later invalid preserves original") {
    VirtualTable vt(makeFlagRel());
    vt.addRecord(flagRec(1, "Alice   ", "yes"));

    Field nameField("name", strDom(8), false);
    Field flagField("flag", enumYN(), false);

    // First value is valid (name change), second is invalid (flag not in enum)
    CHECK_THROW(vt.updateRecordByKey(
        intBytes(1),
        {Value(nameField, std::string_view("Bob     ", 8)),
         Value(flagField, std::string_view("bad", 3))}
    ), std::invalid_argument);

    // Both fields must be unchanged; the candidate was never committed
    auto rec = vt.getRecord(intBytes(1));
    CHECK_TRUE(rec.has_value());
    CHECK_EQ(rec->valueAt(nameField), "Alice   ");
    CHECK_EQ(rec->valueAt(flagField), "yes");
}

TEST("VirtualTable normalises record schema on insert") {
    auto rel = makePersonRel();
    // A distinct-but-equivalent relation object
    auto otherRel = std::make_shared<Relation>(std::vector<Field>({
        Field("id",   intDom(), true),
        Field("name", strDom(8), false)
    }));
    CHECK_TRUE(*rel == *otherRel);   // same schema

    VirtualTable vt(rel);
    // Record constructed with a different relation object
    Record rec(otherRel, personRec(1, "Alice   "));
    CHECK_NOTHROW(vt.addRecord(rec));

    auto result = vt.getRecord(intBytes(1));
    CHECK_TRUE(result.has_value());
    CHECK_EQ(result->getData(), personRec(1, "Alice   "));
}

TEST("VirtualTable rejects foreign-schema record whose bytes are invalid under table schema") {
    // Two relations with same width (4 + 3 == 7) but different enum domains
    auto enumYN  = std::make_shared<EnumDomain>(
        std::vector<std::string>{"yes", "no "});
    auto enumBAD = std::make_shared<EnumDomain>(
        std::vector<std::string>{"bad", "ok "});   // same 3-byte slot

    auto relYN = std::make_shared<Relation>(std::vector<Field>({
        Field("id", intDom(), true), Field("f", enumYN, false)
    }));
    auto relBAD = std::make_shared<Relation>(std::vector<Field>({
        Field("id", intDom(), true), Field("f", enumBAD, false)
    }));

    // Record "bad" is valid under relBAD but not under relYN
    Record foreign(relBAD, intBytes(1) + "bad");   // 7 bytes

    VirtualTable vt(relYN);
    // Normalisation: Record(relYN, foreign.getData()) -> throws
    CHECK_THROW(vt.addRecord(foreign), std::invalid_argument);
}

// ======================================================================
// PhysicalTable
// ======================================================================

TEST("PhysicalTable construction") {
    TestDir dir;
    auto rel = makePersonRel();
    auto file = std::make_unique<HeapFile>(
        dir.file("pt.heap"), rel->getKeySize(), rel->getRecordSize());
    CHECK_NOTHROW(PhysicalTable(rel, "test", std::move(file)));
}

TEST("PhysicalTable rejects null relation") {
    TestDir dir;
    FilePtr f = std::make_unique<HeapFile>(
        dir.file("pt_null_rel.heap"), 4, 12);
    CHECK_THROW(PhysicalTable(nullptr, "test", std::move(f)),
                std::invalid_argument);
}

TEST("PhysicalTable rejects null file pointer") {
    auto rel = makePersonRel();
    CHECK_THROW(PhysicalTable(rel, "test", nullptr), std::invalid_argument);
}

TEST("PhysicalTable addRecord then getRecord") {
    TestDir dir;
    auto rel = makePersonRel();
    auto file = std::make_unique<HeapFile>(
        dir.file("pt2.heap"), rel->getKeySize(), rel->getRecordSize());
    PhysicalTable pt(rel, "test", std::move(file));

    pt.addRecord(personRec(1, "Alice   "));
    auto rec = pt.getRecord(intBytes(1));
    CHECK_TRUE(rec.has_value());
    CHECK_EQ(rec->getData(), personRec(1, "Alice   "));
}

TEST("PhysicalTable addRecord with raw-string overload") {
    TestDir dir;
    auto rel = makePersonRel();
    auto file = std::make_unique<HeapFile>(
        dir.file("pt3.heap"), rel->getKeySize(), rel->getRecordSize());
    PhysicalTable pt(rel, "test", std::move(file));

    pt.addRecord(personRec(1, "Alice   "));
    CHECK_TRUE(pt.getRecord(intBytes(1)).has_value());
}

TEST("PhysicalTable rejects duplicate key") {
    TestDir dir;
    auto rel = makePersonRel();
    auto file = std::make_unique<HeapFile>(
        dir.file("pt4.heap"), rel->getKeySize(), rel->getRecordSize());
    PhysicalTable pt(rel, "test", std::move(file));

    pt.addRecord(personRec(1, "Alice   "));
    CHECK_THROW(pt.addRecord(personRec(1, "Bob     ")), std::invalid_argument);
}

TEST("PhysicalTable scan returns all records") {
    TestDir dir;
    auto rel = makePersonRel();
    auto file = std::make_unique<HeapFile>(
        dir.file("pt5.heap"), rel->getKeySize(), rel->getRecordSize());
    PhysicalTable pt(rel, "test", std::move(file));

    pt.addRecord(personRec(1, "Alice   "));
    pt.addRecord(personRec(2, "Bob     "));

    CHECK_EQ(pt.scan().size(), 2);
}

TEST("PhysicalTable getRecord returns nullopt for missing key") {
    TestDir dir;
    auto rel = makePersonRel();
    auto file = std::make_unique<HeapFile>(
        dir.file("pt6.heap"), rel->getKeySize(), rel->getRecordSize());
    PhysicalTable pt(rel, "test", std::move(file));

    pt.addRecord(personRec(1, "Alice   "));
    CHECK_FALSE(pt.getRecord(intBytes(99)).has_value());
}

TEST("PhysicalTable getRecord throws on wrong key size") {
    TestDir dir;
    auto rel = makePersonRel();
    auto file = std::make_unique<HeapFile>(
        dir.file("pt7.heap"), rel->getKeySize(), rel->getRecordSize());
    PhysicalTable pt(rel, "test", std::move(file));

    CHECK_THROW(pt.getRecord("abc"), std::invalid_argument);
}

TEST("PhysicalTable deleteRecord removes the record") {
    TestDir dir;
    auto rel = makePersonRel();
    auto file = std::make_unique<HeapFile>(
        dir.file("pt8.heap"), rel->getKeySize(), rel->getRecordSize());
    PhysicalTable pt(rel, "test", std::move(file));

    pt.addRecord(personRec(1, "Alice   "));
    pt.addRecord(personRec(2, "Bob     "));

    auto deleted = pt.deleteRecord(intBytes(1));
    CHECK_TRUE(deleted.has_value());
    CHECK_EQ(deleted->getKeyData(), intBytes(1));

    CHECK_FALSE(pt.getRecord(intBytes(1)).has_value());
    CHECK_TRUE(pt.getRecord(intBytes(2)).has_value());
}

TEST("PhysicalTable deleteRecord returns nullopt for missing key") {
    TestDir dir;
    auto rel = makePersonRel();
    auto file = std::make_unique<HeapFile>(
        dir.file("pt9.heap"), rel->getKeySize(), rel->getRecordSize());
    PhysicalTable pt(rel, "test", std::move(file));

    pt.addRecord(personRec(1, "Alice   "));
    CHECK_FALSE(pt.deleteRecord(intBytes(99)).has_value());
}

TEST("PhysicalTable updateRecordByKey updates non-key fields") {
    TestDir dir;
    auto rel = makePersonRel();
    auto file = std::make_unique<HeapFile>(
        dir.file("pt10.heap"), rel->getKeySize(), rel->getRecordSize());
    PhysicalTable pt(rel, "test", std::move(file));

    pt.addRecord(personRec(1, "Alice   "));

    Field nameField("name", strDom(8), false);
    bool ok = pt.updateRecordByKey(
        intBytes(1),
        {Value(nameField, std::string_view("Bob     ", 8))}
    );
    CHECK_TRUE(ok);

    auto rec = pt.getRecord(intBytes(1));
    CHECK_TRUE(rec.has_value());
    CHECK_EQ(rec->valueAt(nameField), "Bob     ");
}

TEST("PhysicalTable updateRecordByKey rejects duplicate new key") {
    TestDir dir;
    auto rel = makePersonRel();
    auto file = std::make_unique<HeapFile>(
        dir.file("pt11.heap"), rel->getKeySize(), rel->getRecordSize());
    PhysicalTable pt(rel, "test", std::move(file));

    pt.addRecord(personRec(1, "Alice   "));
    pt.addRecord(personRec(2, "Bob     "));

    // Try to change record-2's key to 1 (already used)
    Field idField("id", intDom(), true);
    bool ok = pt.updateRecordByKey(
        intBytes(2),
        {Value(idField, intBytes(1))}
    );
    CHECK_FALSE(ok);

    // Record 2 must be unchanged
    auto rec2 = pt.getRecord(intBytes(2));
    CHECK_TRUE(rec2.has_value());
    CHECK_EQ(rec2->getKeyData(), intBytes(2));
}

TEST("PhysicalTable updateRecordByKey preserves original on failed validation") {
    TestDir dir;
    auto rel = makePersonRel();
    auto file = std::make_unique<HeapFile>(
        dir.file("pt12.heap"), rel->getKeySize(), rel->getRecordSize());
    PhysicalTable pt(rel, "test", std::move(file));

    pt.addRecord(personRec(1, "Alice   "));

    Field nameField("name", strDom(8), false);
    // Value too large for 8-byte slot -> throws
    CHECK_THROW(pt.updateRecordByKey(
        intBytes(1),
        {Value(nameField, std::string_view("123456789", 9))}
    ), std::invalid_argument);

    // Original must be unchanged
    auto rec = pt.getRecord(intBytes(1));
    CHECK_TRUE(rec.has_value());
    CHECK_EQ(rec->valueAt(nameField), "Alice   ");
}

TEST("PhysicalTable updateRecordByKey returns false for missing key") {
    TestDir dir;
    auto rel = makePersonRel();
    auto file = std::make_unique<HeapFile>(
        dir.file("pt13.heap"), rel->getKeySize(), rel->getRecordSize());
    PhysicalTable pt(rel, "test", std::move(file));

    pt.addRecord(personRec(1, "Alice   "));

    Field nameField("name", strDom(8), false);
    bool ok = pt.updateRecordByKey(
        intBytes(99),
        {Value(nameField, std::string_view("Bob     ", 8))}
    );
    CHECK_FALSE(ok);
}

TEST("PhysicalTable updateRecordByKey can change the key") {
    TestDir dir;
    auto rel = makePersonRel();
    auto file = std::make_unique<HeapFile>(
        dir.file("pt14.heap"), rel->getKeySize(), rel->getRecordSize());
    PhysicalTable pt(rel, "test", std::move(file));
    pt.addRecord(personRec(1, "Alice   "));

    Field idField("id", intDom(), true);
    bool ok = pt.updateRecordByKey(
        intBytes(1), {Value(idField, intBytes(2))}
    );
    CHECK_TRUE(ok);

    CHECK_FALSE(pt.getRecord(intBytes(1)).has_value());
    auto rec = pt.getRecord(intBytes(2));
    CHECK_TRUE(rec.has_value());
    CHECK_EQ(rec->getKeyData(), intBytes(2));
}

TEST("PhysicalTable multi-value update: earlier valid + later invalid preserves original") {
    TestDir dir;
    auto rel = makeFlagRel();
    auto file = std::make_unique<HeapFile>(
        dir.file("pt15.heap"), rel->getKeySize(), rel->getRecordSize());
    PhysicalTable pt(rel, "test", std::move(file));
    pt.addRecord(flagRec(1, "Alice   ", "yes"));

    Field nameField("name", strDom(8), false);
    Field flagField("flag", enumYN(), false);

    // First change is valid (name), second is invalid (flag not in {"yes","no "})
    CHECK_THROW(pt.updateRecordByKey(
        intBytes(1),
        {Value(nameField, std::string_view("Bob     ", 8)),
         Value(flagField, std::string_view("bad", 3))}
    ), std::invalid_argument);

    // Both fields must be unchanged — the candidate was discarded before any write
    auto rec = pt.getRecord(intBytes(1));
    CHECK_TRUE(rec.has_value());
    CHECK_EQ(rec->valueAt(nameField), "Alice   ");
    CHECK_EQ(rec->valueAt(flagField), "yes");
}

TEST("PhysicalTable data persists after close and reopen") {
    TestDir dir;
    {
        auto rel = makePersonRel();
        auto file = std::make_unique<HeapFile>(
            dir.file("pt_persist.heap"), rel->getKeySize(), rel->getRecordSize());
        PhysicalTable pt(rel, "test", std::move(file));
        pt.addRecord(personRec(1, "Alice   "));
        pt.addRecord(personRec(2, "Bob     "));
    }
    // Reopen with a new table instance backed by the same file
    auto rel = makePersonRel();
    auto file = std::make_unique<HeapFile>(
        dir.file("pt_persist.heap"), rel->getKeySize(), rel->getRecordSize());
    PhysicalTable pt(rel, "test", std::move(file));

    CHECK_EQ(pt.scan().size(), 2);
    CHECK_TRUE(pt.getRecord(intBytes(1)).has_value());
    CHECK_TRUE(pt.getRecord(intBytes(2)).has_value());
}

TEST("PhysicalTable normalises record schema on insert") {
    TestDir dir;
    auto rel = makePersonRel();
    auto otherRel = std::make_shared<Relation>(std::vector<Field>({
        Field("id",   intDom(), true),
        Field("name", strDom(8), false)
    }));

    auto file = std::make_unique<HeapFile>(
        dir.file("pt_schema.heap"), rel->getKeySize(), rel->getRecordSize());
    PhysicalTable pt(rel, "test", std::move(file));

    // Record constructed with a different (but compatible) relation
    Record rec(otherRel, personRec(1, "Alice   "));
    CHECK_NOTHROW(pt.addRecord(rec));

    auto result = pt.getRecord(intBytes(1));
    CHECK_TRUE(result.has_value());
    CHECK_EQ(result->getData(), personRec(1, "Alice   "));
}

TEST("PhysicalTable rejects foreign-schema record whose bytes are invalid under table schema") {
    TestDir dir;
    auto enumYN  = std::make_shared<EnumDomain>(
        std::vector<std::string>{"yes", "no "});
    auto enumBAD = std::make_shared<EnumDomain>(
        std::vector<std::string>{"bad", "ok "});

    auto relYN = std::make_shared<Relation>(std::vector<Field>({
        Field("id", intDom(), true), Field("f", enumYN, false)
    }));
    auto relBAD = std::make_shared<Relation>(std::vector<Field>({
        Field("id", intDom(), true), Field("f", enumBAD, false)
    }));

    Record foreign(relBAD, intBytes(1) + "bad");

    auto file = std::make_unique<HeapFile>(
        dir.file("pt_schema_reject.heap"),
        relYN->getKeySize(), relYN->getRecordSize());
    PhysicalTable pt(relYN, "test", std::move(file));

    // Normalisation fails because "bad" is not in {"yes", "no "}
    CHECK_THROW(pt.addRecord(foreign), std::invalid_argument);

    // Verify the file is still empty (no partial insert)
    CHECK_TRUE(pt.scan().empty());
}
