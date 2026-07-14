#include "doctest.h"
#include "snmp/oid.hpp"

#include <stdexcept>

using wiresprite::Oid;

TEST_CASE("Oid::parse and toString round-trip") {
    Oid oid = Oid::parse("1.3.6.1.2.1.2.2.1.10.1");
    CHECK(oid.components() == std::vector<uint32_t>{1, 3, 6, 1, 2, 1, 2, 2, 1, 10, 1});
    CHECK(oid.toString() == "1.3.6.1.2.1.2.2.1.10.1");
}

TEST_CASE("Oid::parse rejects malformed input") {
    CHECK_THROWS_AS(Oid::parse(""), std::invalid_argument);
    CHECK_THROWS_AS(Oid::parse("1..3"), std::invalid_argument);
    CHECK_THROWS_AS(Oid::parse("1.3."), std::invalid_argument);
    CHECK_THROWS_AS(Oid::parse(".1.3"), std::invalid_argument);
    CHECK_THROWS_AS(Oid::parse("1.3.x"), std::invalid_argument);
    CHECK_THROWS_AS(Oid::parse("1.-3"), std::invalid_argument);
}

TEST_CASE("Oid::isSubtreeOf") {
    Oid base = Oid::parse("1.3.6.1.2.1.2.2.1");
    Oid inSubtree = Oid::parse("1.3.6.1.2.1.2.2.1.10.1");
    Oid sameAsBase = base;
    Oid outsideSubtree = Oid::parse("1.3.6.1.2.1.2.3.1");
    Oid tooShort = Oid::parse("1.3.6.1.2.1.2.2");

    CHECK(inSubtree.isSubtreeOf(base));
    CHECK(sameAsBase.isSubtreeOf(base));
    CHECK_FALSE(outsideSubtree.isSubtreeOf(base));
    CHECK_FALSE(tooShort.isSubtreeOf(base));
}

TEST_CASE("Oid::withSuffix appends components") {
    Oid base = Oid::parse("1.3.6.1.2.1.2.2.1.10");
    CHECK(base.withSuffix(1) == Oid::parse("1.3.6.1.2.1.2.2.1.10.1"));
    CHECK(base.withSuffix(std::vector<uint32_t>{5, 1}) == Oid::parse("1.3.6.1.2.1.2.2.1.10.5.1"));
}

TEST_CASE("Oid ordering matches numeric MIB walk order") {
    // A prefix must sort before any of its children, and this must hold
    // numerically (2 < 10), not lexicographically as strings ("10" < "2").
    Oid parent = Oid::parse("1.3.6.1.2.1.2.2.1.10");
    Oid child = Oid::parse("1.3.6.1.2.1.2.2.1.10.1");
    Oid nextColumn = Oid::parse("1.3.6.1.2.1.2.2.1.2");

    CHECK(parent < child);
    CHECK(nextColumn < parent);
}
