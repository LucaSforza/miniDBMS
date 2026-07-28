/**
 * @file RecordsTests.cpp
 * @brief Regression tests for Relation construction/layout and Record safety.
 *
 * All records are fixed-width raw byte strings.  No SQL codec assumptions.
 */

#include "TestHarness.hpp"
#include "StorageEngine.hpp"

#include <cstring>
#include <memory>
#include <string>
#include <vector>

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

// ==========================================================================
// Relation: duplicate-name rejection
// ==========================================================================

TEST("Relation rejects duplicate field names") {
    CHECK_THROW(Relation({
        Field("id",   intDom(), true),
        Field("id",   strDom(8), false)   // same name, different domain
    }), std::invalid_argument);
}

TEST("Relation rejects duplicate names among key fields") {
    CHECK_THROW(Relation({
        Field("x", intDom(), true),
        Field("x", intDom(), true)
    }), std::invalid_argument);
}

// ==========================================================================
// Relation: missing-key rejection
// ==========================================================================

TEST("Relation rejects construction with no key field") {
    CHECK_THROW(Relation({
        Field("a", intDom(), false),
        Field("b", strDom(8), false)
    }), std::invalid_argument);
}

TEST("Relation with exactly one key field is accepted") {
    CHECK_NOTHROW(Relation({
        Field("id",   intDom(), true),
        Field("name", strDom(8), false)
    }));
}

// ==========================================================================
// Relation: key-first offsets and sizes
// ==========================================================================

TEST("Relation places key fields first, then non-key fields") {
    // Layout: [id:4][name:8]
    Relation rel({
        Field("id",   intDom(), true),
        Field("name", strDom(8), false)
    });
    CHECK_EQ(rel.startPointOf(Field("id",   intDom(), true)),  0);
    CHECK_EQ(rel.startPointOf(Field("name", strDom(8), false)), 4);
}

TEST("Relation with multiple keys places all keys before non-keys") {
    // Keys: a(4), b(8); non-key: c(4)
    // Layout: [a:4][b:8][c:4]
    Relation rel({
        Field("a", intDom(), true),
        Field("c", intDom(), false),
        Field("b", strDom(8), true)
    });
    CHECK_EQ(rel.startPointOf(Field("a", intDom(), true)),   0);
    CHECK_EQ(rel.startPointOf(Field("b", strDom(8), true)),  4);
    CHECK_EQ(rel.startPointOf(Field("c", intDom(), false)), 12);
}

TEST("Relation::getRecordSize returns sum of all fields") {
    Relation rel({
        Field("id",   intDom(), true),
        Field("name", strDom(8), false)
    });
    CHECK_EQ(rel.getRecordSize(), 12);   // 4 + 8
}

TEST("Relation::getKeySize returns sum of key fields") {
    Relation rel({
        Field("a", intDom(), true),
        Field("b", strDom(4), true),
        Field("c", intDom(), false)
    });
    CHECK_EQ(rel.getKeySize(), 8);       // 4 + 4
}

TEST("Relation with zero non-key fields works") {
    Relation rel({ Field("id", intDom(), true) });
    CHECK_EQ(rel.getRecordSize(), 4);
    CHECK_EQ(rel.getKeySize(), 4);
}

// ==========================================================================
// Relation: equality
// ==========================================================================

TEST("Relation equality checks full field list") {
    Relation a({
        Field("id",   intDom(), true),
        Field("name", strDom(8), false)
    });
    Relation b({
        Field("id",   intDom(), true),
        Field("name", strDom(8), false)
    });
    CHECK_TRUE(a == b);
}

TEST("Relation inequality on different fields") {
    Relation a({
        Field("id",  intDom(), true),
        Field("val", intDom(), false)
    });
    Relation b({
        Field("key", intDom(), true),
        Field("val", intDom(), false)
    });
    CHECK_FALSE(a == b);
}

TEST("Relation inequality when key role differs") {
    // Same field names and domains, but roles are swapped
    Relation a({Field("a", intDom(), true),  Field("b", strDom(4), false)});
    Relation b({Field("a", intDom(), false), Field("b", strDom(4), true)});
    CHECK_FALSE(a == b);
}

// ==========================================================================
// Relation::isValid – byte-length enforcement
// ==========================================================================

TEST("Relation::isValid rejects short buffers") {
    Relation rel({
        Field("id", intDom(), true), Field("name", strDom(8), false)
    });
    CHECK_FALSE(rel.isValid(""));               // empty
    CHECK_FALSE(rel.isValid("short"));          // 5 bytes, not 12
}

TEST("Relation::isValid rejects long buffers") {
    Relation rel({
        Field("id", intDom(), true), Field("name", strDom(8), false)
    });
    CHECK_FALSE(rel.isValid("this buffer is far longer than record size"));
}

TEST("Relation::isValid accepts exact-size valid data") {
    Relation rel({
        Field("id", intDom(), true), Field("name", strDom(8), false)
    });
    CHECK_TRUE(rel.isValid(personRec(1, "Alice   ")));
}

// ==========================================================================
// Same-name but different-domain / different-key-role rejection
// ==========================================================================

TEST("Relation::startPointOf rejects field with same name but different domain") {
    Relation rel({Field("id", intDom(), true)});
    Field sameNameWrongDomain("id", strDom(4), true);
    CHECK_THROW(rel.startPointOf(sameNameWrongDomain), std::invalid_argument);
}

TEST("Relation::startPointOf rejects field with same name but different key role") {
    Relation rel({Field("id", intDom(), true)});
    Field sameNameWrongRole("id", intDom(), false);
    CHECK_THROW(rel.startPointOf(sameNameWrongRole), std::invalid_argument);
}

TEST("Record::setValue rejects field with same name but different domain") {
    auto rel = std::make_shared<Relation>(
        std::vector<Field>({Field("id", intDom(), true)}));
    Record rec(rel, intBytes(1));
    Field sameNameWrongDomain("id", strDom(4), true);
    CHECK_THROW(rec.setValue(Value(sameNameWrongDomain, intBytes(2))),
                std::invalid_argument);
}

// ==========================================================================
// Record: exact-size enforcement
// ==========================================================================

TEST("Record constructor rejects data shorter than record size") {
    auto rel = std::make_shared<Relation>(std::vector<Field>({
        Field("id",   intDom(), true),
        Field("name", strDom(8), false)
    }));
    // Only 8 bytes instead of required 12
    CHECK_THROW(Record(rel, std::string("12345678")), std::invalid_argument);
}

TEST("Record constructor rejects data longer than record size") {
    auto rel = std::make_shared<Relation>(std::vector<Field>({
        Field("id",   intDom(), true),
        Field("name", strDom(8), false)
    }));
    // 14 bytes instead of required 12
    CHECK_THROW(Record(rel, std::string("12345678901234")), std::invalid_argument);
}

TEST("Record constructor accepts exact-size valid data") {
    auto rel = std::make_shared<Relation>(std::vector<Field>({
        Field("id",   intDom(), true),
        Field("name", strDom(8), false)
    }));
    CHECK_NOTHROW(Record(rel, personRec(1, "Alice   ")));
}

TEST("Record rejects null relation") {
    std::shared_ptr<Relation> nullRel;
    CHECK_THROW(Record(nullRel, personRec(1, "Alice   ")),
                std::invalid_argument);
}

// ==========================================================================
// Record: foreign-field rejection
// ==========================================================================

TEST("Relation::startPointOf throws for a field not in the relation") {
    Relation rel({
        Field("id",   intDom(), true),
        Field("name", strDom(8), false)
    });
    Field foreign("other", intDom(), false);
    CHECK_THROW(rel.startPointOf(foreign), std::invalid_argument);
}

TEST("Record::valueAt throws for a field not belonging to the relation") {
    auto rel = std::make_shared<Relation>(std::vector<Field>({
        Field("id",   intDom(), true),
        Field("name", strDom(8), false)
    }));
    Record rec(rel, personRec(1, "Alice   "));
    Field foreign("other", intDom(), false);
    CHECK_THROW(rec.valueAt(foreign), std::invalid_argument);
}

TEST("Record::setValue throws for a field not belonging to the relation") {
    auto rel = std::make_shared<Relation>(std::vector<Field>({
        Field("id",   intDom(), true),
        Field("name", strDom(8), false)
    }));
    Record rec(rel, personRec(1, "Alice   "));
    Field foreign("other", intDom(), false);
    Value v(foreign, intBytes(2));
    CHECK_THROW(rec.setValue(v), std::invalid_argument);
}

// ==========================================================================
// Record: value access by field
// ==========================================================================

TEST("Record::valueAt returns the correct bytes for each field") {
    auto rel = std::make_shared<Relation>(std::vector<Field>({
        Field("id",   intDom(), true),
        Field("name", strDom(8), false)
    }));
    Record rec(rel, personRec(42, "Bob     "));

    Field idField("id", intDom(), true);
    auto idVal = rec.valueAt(idField);
    CHECK_EQ(idVal.size(), 4);
    // int 42 in little-endian host order
    CHECK_EQ(static_cast<unsigned char>(idVal[0]), 42);

    Field nameField("name", strDom(8), false);
    auto nameVal = rec.valueAt(nameField);
    CHECK_EQ(nameVal.size(), 8);
    CHECK_TRUE(nameVal.substr(0, 3) == "Bob");
}

TEST("Record::getKeyData returns the key prefix bytes") {
    auto rel = std::make_shared<Relation>(std::vector<Field>({
        Field("id",   intDom(), true),
        Field("name", strDom(8), false)
    }));
    Record rec(rel, personRec(7, "Carol   "));
    auto keyData = rec.getKeyData();
    CHECK_EQ(keyData.size(), 4);
    CHECK_EQ(static_cast<unsigned char>(keyData[0]), 7);
}

TEST("Record::getKey returns one value tuple per key field") {
    auto rel = std::make_shared<Relation>(std::vector<Field>({
        Field("id",   intDom(), true),
        Field("name", strDom(8), false)
    }));
    Record rec(rel, personRec(99, "Dave    "));

    auto keys = rec.getKey();
    CHECK_EQ(keys.size(), 1);
    CHECK_EQ(std::get<0>(keys[0]).getName(), "id");
    CHECK_EQ(std::get<1>(keys[0]), intBytes(99));
}

TEST("Record::getKey returns Value tuples referencing Relation-owned fields") {
    // Two key fields: 4-byte int "id" and 4-byte string "tag".
    auto rel = std::make_shared<Relation>(std::vector<Field>({
        Field("id",   intDom(), true),
        Field("tag",  strDom(4), true),
        Field("name", strDom(8), false)
    }));
    Record rec(rel, intBytes(1) + "xyzz" + std::string(8, ' '));

    auto keys = rec.getKey();
    CHECK_EQ(keys.size(), 2);

    // Field references must point into Relation's own key-field storage.
    const auto& relKeys = rel->getKey();
    CHECK_EQ(&std::get<0>(keys[0]), &relKeys[0]);
    CHECK_EQ(&std::get<0>(keys[1]), &relKeys[1]);

    // Byte views carry the correct key data.
    CHECK_EQ(std::get<1>(keys[0]), intBytes(1));
    CHECK_EQ(std::get<1>(keys[1]), "xyzz");
}

TEST("Record::valuesInside returns true when all given values match") {
    auto rel = std::make_shared<Relation>(std::vector<Field>({
        Field("id",   intDom(), true),
        Field("name", strDom(8), false)
    }));
    Record rec(rel, personRec(3, "Eve     "));

    Field idField("id", intDom(), true);
    CHECK_TRUE(rec.valuesInside({Value(idField, intBytes(3))}));
    CHECK_FALSE(rec.valuesInside({Value(idField, intBytes(4))}));
}

// ==========================================================================
// Record: candidate-safe setValue failures
// ==========================================================================

TEST("Record::setValue rejects value whose byte size does not match field") {
    auto rel = std::make_shared<Relation>(std::vector<Field>({
        Field("id",   intDom(), true),
        Field("name", strDom(8), false)
    }));
    Record rec(rel, personRec(1, "Alice   "));

    Field nameField("name", strDom(8), false);
    // Only 4 bytes for an 8-byte slot
    Value v(nameField, std::string_view("xxxx", 4));
    CHECK_THROW(rec.setValue(v), std::invalid_argument);

    // Record data must be unchanged after the failed setValue
    CHECK_EQ(rec.valueAt(nameField), "Alice   ");
}

TEST("Record::setValue rejects same-size enum value not in domain") {
    auto enumYN = std::make_shared<EnumDomain>(
        std::vector<std::string>{"yes", "no "});
    auto rel = std::make_shared<Relation>(std::vector<Field>({
        Field("id",   intDom(), true),
        Field("flag", enumYN, false)        // recordSize == 7 (4 + 3)
    }));
    Record rec(rel, intBytes(1) + "yes");    // 7 bytes: key=1, flag="yes"

    Field flagField("flag", enumYN, false);
    // "bad" is exactly the field size (3 bytes) but not in {"yes", "no "}
    Value v(flagField, std::string_view("bad", 3));
    CHECK_THROW(rec.setValue(v), std::invalid_argument);

    // Original data must be completely unchanged
    CHECK_EQ(rec.valueAt(flagField), "yes");
}

TEST("Record::setValue succeeds with valid data and updates the record") {
    auto rel = std::make_shared<Relation>(std::vector<Field>({
        Field("id",   intDom(), true),
        Field("name", strDom(8), false)
    }));
    Record rec(rel, personRec(1, "Alice   "));

    Field nameField("name", strDom(8), false);
    Value v(nameField, std::string_view("Bob     ", 8));
    CHECK_NOTHROW(rec.setValue(v));

    // Verify the data was modified correctly
    CHECK_EQ(rec.valueAt(nameField), "Bob     ");
    // Key should be unchanged
    CHECK_EQ(rec.getKeyData(), intBytes(1));
}

TEST("Record::setValue can update the key field") {
    auto rel = std::make_shared<Relation>(std::vector<Field>({
        Field("id",   intDom(), true),
        Field("name", strDom(8), false)
    }));
    Record rec(rel, personRec(1, "Alice   "));

    Field idField("id", intDom(), true);
    Value v(idField, intBytes(2));
    CHECK_NOTHROW(rec.setValue(v));

    CHECK_EQ(rec.getKeyData(), intBytes(2));
}
