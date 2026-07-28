/**
 * @file DomainsTests.cpp
 * @brief Regression tests for Domain and Field equality, size, and validation.
 *
 * Every test uses exact fixed-width raw strings – no SQL codec assumptions.
 */

#include "TestHarness.hpp"
#include "Domains.hpp"
#include "StorageEngine.hpp"   // for Field

#include <cstring>
#include <memory>
#include <string>

// ==========================================================================
// IntegerDomain – size, validation, equality
// ==========================================================================

TEST("IntegerDomain::size returns sizeof(int)") {
    IntegerDomain d;
    CHECK_EQ(d.size(), sizeof(int));
}

TEST("IntegerDomain::isValid accepts exactly sizeof(int) bytes") {
    IntegerDomain d;
    // 4-byte binary values are accepted regardless of content
    CHECK_TRUE(d.isValid(std::string_view("\x00\x00\x00\x00", 4)));
    CHECK_TRUE(d.isValid(std::string_view("\x01\x00\x00\x00", 4)));
    CHECK_TRUE(d.isValid(std::string_view("\xff\xff\xff\xff", 4)));
    // Shorter values are rejected
    CHECK_FALSE(d.isValid(std::string_view("\x00\x00\x00", 3)));
    CHECK_FALSE(d.isValid(std::string_view("", 0)));
    // Longer values are rejected
    CHECK_FALSE(d.isValid(std::string_view("\x00\x00\x00\x00\x00", 5)));
}

TEST("IntegerDomain equality compares runtime type") {
    IntegerDomain a, b;
    CHECK_TRUE(a == b);
}

// ==========================================================================
// StringDomain – size, validation, equality
// ==========================================================================

TEST("StringDomain::size returns the max_len passed to the constructor") {
    StringDomain d8(8);
    CHECK_EQ(d8.size(), 8);

    StringDomain d42(42);
    CHECK_EQ(d42.size(), 42);
}

TEST("StringDomain::isValid accepts strings whose length <= max_len") {
    StringDomain d5(5);
    CHECK_TRUE(d5.isValid(""));         // empty is always valid
    CHECK_TRUE(d5.isValid("a"));        // short
    CHECK_TRUE(d5.isValid("abcde"));    // exactly max_len
    CHECK_FALSE(d5.isValid("abcdef"));  // one past max_len
}

TEST("StringDomain equality depends on max_len") {
    StringDomain a(8), b(8), c(16);
    CHECK_TRUE(a == b);
    CHECK_FALSE(a == c);
}

// ==========================================================================
// EnumDomain – size, validation, equality
// ==========================================================================

TEST("EnumDomain::size returns the length of the longest valid value") {
    EnumDomain e({"a", "bb", "ccc"});
    CHECK_EQ(e.size(), 3);
}

TEST("EnumDomain::isValid checks set membership") {
    EnumDomain e({"red", "green", "blue"});
    CHECK_TRUE(e.isValid("red"));
    CHECK_TRUE(e.isValid("blue"));
    CHECK_FALSE(e.isValid("yellow"));
    CHECK_FALSE(e.isValid(""));   // empty not in the set
}

TEST("EnumDomain equality compares both max_len and value set") {
    EnumDomain a({"x", "y"}), b({"x", "y"}), c({"x"});
    CHECK_TRUE(a == b);
    CHECK_FALSE(a == c);
}

// ==========================================================================
// Cross-type domain equality
// ==========================================================================

TEST("Domains of different types are never equal") {
    IntegerDomain id;
    StringDomain sd(4);
    EnumDomain ed({"a"});
    CHECK_FALSE(id == sd);
    CHECK_FALSE(sd == id);
    CHECK_FALSE(id == ed);
    CHECK_FALSE(sd == ed);
}

// ==========================================================================
// Field construction, delegation, and equality
// ==========================================================================

TEST("Field constructed without explicit key flag defaults to non-key") {
    auto d = std::make_shared<IntegerDomain>();
    Field f("val", d);
    CHECK_FALSE(f.isKey());
}

TEST("Field::isValid delegates to its domain") {
    auto d = std::make_shared<IntegerDomain>();
    Field f("val", d);
    CHECK_TRUE(f.isValid(std::string_view("\x00\x00\x00\x01", 4)));
    CHECK_FALSE(f.isValid(std::string_view("\x00\x00\x00", 3)));
}

TEST("Field::size delegates to its domain") {
    auto d = std::make_shared<StringDomain>(12);
    Field f("name", d);
    CHECK_EQ(f.size(), 12);
}

TEST("Field equality requires same name, domain, and key role") {
    auto intDom  = std::make_shared<IntegerDomain>();
    auto strDom8 = std::make_shared<StringDomain>(8);

    Field a("id", intDom, true);
    Field b("id", intDom, true);
    CHECK_TRUE(a == b);                        // identical

    Field c("id", intDom, false);
    CHECK_FALSE(a == c);                       // different key role

    Field d("pk", intDom, true);
    CHECK_FALSE(a == d);                       // different name

    Field e("id", strDom8, true);
    CHECK_FALSE(a == e);                       // different domain
}

TEST("Field equality detects domain-size differences") {
    auto str8  = std::make_shared<StringDomain>(8);
    auto str16 = std::make_shared<StringDomain>(16);

    Field a("name", str8, false);
    Field b("name", str16, false);
    CHECK_FALSE(a == b);
}

// ==========================================================================
// Field null-domain rejection
// ==========================================================================

TEST("Field rejects null domain") {
    SharedDomain nullDom;
    CHECK_THROW(Field("x", nullDom),             std::invalid_argument);
    CHECK_THROW(Field("x", nullDom, true),       std::invalid_argument);
    CHECK_THROW(Field("x", nullDom, false),      std::invalid_argument);
}
