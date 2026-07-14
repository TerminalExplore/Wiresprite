#include "doctest.h"
#include "poll/if_table.hpp"

using namespace snmpmon;

namespace {
Oid col(uint32_t column, uint32_t ifIndex) {
    return Oid::parse("1.3.6.1.2.1.2.2.1").withSuffix({column, ifIndex});
}
} // namespace

TEST_CASE("bucketIfTableVarBinds groups column-major varbinds by ifIndex") {
    std::vector<VarBind> varbinds = {
        // column-major order, as a real WALK returns it: all of column 2
        // across both rows, then all of column 8, etc.
        {col(2, 1), ber::Value::octetString("eth0")},
        {col(2, 2), ber::Value::octetString("eth1")},
        {col(3, 1), ber::Value::integer(6)},
        {col(3, 2), ber::Value::integer(6)},
        {col(5, 1), ber::Value::gauge32(100000000)},
        {col(5, 2), ber::Value::gauge32(1000000000)},
        {col(7, 1), ber::Value::integer(1)},
        {col(7, 2), ber::Value::integer(1)},
        {col(8, 1), ber::Value::integer(1)},
        {col(8, 2), ber::Value::integer(2)},
        {col(10, 1), ber::Value::counter32(1000)},
        {col(10, 2), ber::Value::counter32(2000)},
        {col(13, 1), ber::Value::counter32(0)},
        {col(13, 2), ber::Value::counter32(1)},
        {col(14, 1), ber::Value::counter32(0)},
        {col(14, 2), ber::Value::counter32(3)},
        {col(16, 1), ber::Value::counter32(500)},
        {col(16, 2), ber::Value::counter32(2500)},
        {col(19, 1), ber::Value::counter32(0)},
        {col(19, 2), ber::Value::counter32(2)},
        {col(20, 1), ber::Value::counter32(0)},
        {col(20, 2), ber::Value::counter32(4)},
    };

    std::vector<IfEntry> entries = bucketIfTableVarBinds(varbinds);

    REQUIRE(entries.size() == 2);

    const IfEntry& e1 = entries[0];
    CHECK(e1.ifIndex == 1);
    CHECK(e1.ifDescr == "eth0");
    CHECK(e1.ifType == 6);
    CHECK(e1.ifSpeed == 100000000);
    CHECK(e1.ifAdminStatus == 1);
    CHECK(e1.ifOperStatus == 1);
    CHECK(e1.ifInOctets == 1000);
    CHECK(e1.ifInDiscards == 0);
    CHECK(e1.ifInErrors == 0);
    CHECK(e1.ifOutOctets == 500);
    CHECK(e1.ifOutDiscards == 0);
    CHECK(e1.ifOutErrors == 0);

    const IfEntry& e2 = entries[1];
    CHECK(e2.ifIndex == 2);
    CHECK(e2.ifDescr == "eth1");
    CHECK(e2.ifSpeed == 1000000000);
    CHECK(e2.ifOperStatus == 2); // down
    CHECK(e2.ifInOctets == 2000);
    CHECK(e2.ifInErrors == 3);
    CHECK(e2.ifOutDiscards == 2);
    CHECK(e2.ifOutErrors == 4);
}

TEST_CASE("bucketIfTableVarBinds ignores columns it doesn't track and malformed OIDs") {
    std::vector<VarBind> varbinds = {
        {col(2, 1), ber::Value::octetString("eth0")},
        {col(4, 1), ber::Value::integer(1500)},                 // ifMtu: not tracked, must not crash or leak in
        {col(6, 1), ber::Value::octetString("\x11\x22\x33\x44\x55")}, // ifPhysAddress: not tracked
        {Oid::parse("1.3.6.1.2.1.2.2.1.2"), ber::Value::octetString("too-short")}, // missing ifIndex component
    };

    std::vector<IfEntry> entries = bucketIfTableVarBinds(varbinds);

    REQUIRE(entries.size() == 1);
    CHECK(entries[0].ifIndex == 1);
    CHECK(entries[0].ifDescr == "eth0");
    CHECK(entries[0].ifType == 0);  // untouched by the ignored ifMtu varbind
    CHECK(entries[0].ifSpeed == 0);
}

TEST_CASE("bucketIfTableVarBinds returns entries in ascending ifIndex order regardless of input order") {
    std::vector<VarBind> varbinds = {
        {col(2, 5), ber::Value::octetString("eth4")},
        {col(2, 1), ber::Value::octetString("eth0")},
        {col(2, 3), ber::Value::octetString("eth2")},
    };

    std::vector<IfEntry> entries = bucketIfTableVarBinds(varbinds);
    REQUIRE(entries.size() == 3);
    CHECK(entries[0].ifIndex == 1);
    CHECK(entries[1].ifIndex == 3);
    CHECK(entries[2].ifIndex == 5);
}
