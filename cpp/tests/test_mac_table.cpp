#include "doctest.h"
#include "poll/mac_table.hpp"

using namespace wiresprite;

namespace {

Oid basePortIfIndexOid(uint32_t bridgePort) {
    return Oid::parse("1.3.6.1.2.1.17.1.4.1.2").withSuffix({bridgePort});
}

Oid fdbOid(uint32_t column, const std::array<uint8_t, 6>& mac) {
    Oid oid = Oid::parse("1.3.6.1.2.1.17.4.3.1").withSuffix({column});
    for (uint8_t b : mac) {
        oid = oid.withSuffix(static_cast<uint32_t>(b));
    }
    return oid;
}

} // namespace

TEST_CASE("bucketBridgePortIfIndex maps bridge port to ifIndex") {
    std::vector<VarBind> varbinds = {
        {basePortIfIndexOid(1), ber::Value::integer(1)},
        {basePortIfIndexOid(2), ber::Value::integer(2)},
    };

    auto portMap = bucketBridgePortIfIndex(varbinds);

    REQUIRE(portMap.size() == 2);
    CHECK(portMap.at(1) == 1);
    CHECK(portMap.at(2) == 2);
}

TEST_CASE("bucketBridgePortIfIndex ignores malformed OIDs") {
    std::vector<VarBind> varbinds = {
        {Oid::parse("1.3.6.1.2.1.17.1.4.1.2"), ber::Value::integer(1)}, // missing bridge port component
    };

    auto portMap = bucketBridgePortIfIndex(varbinds);
    CHECK(portMap.empty());
}

TEST_CASE("bucketMacTable keeps only learned entries, translated to ifIndex") {
    const std::array<uint8_t, 6> mac1 = {0x00, 0x11, 0x0a, 0x10, 0x0a, 0x00};
    const std::array<uint8_t, 6> mac2 = {0x38, 0xf7, 0xcd, 0xcc, 0x49, 0x0e};

    std::vector<VarBind> varbinds = {
        {fdbOid(2, mac1), ber::Value::integer(0)}, // port 0: "self", not a real bridge port
        {fdbOid(3, mac1), ber::Value::integer(4)}, // status 4 = self
        {fdbOid(2, mac2), ber::Value::integer(1)}, // port 1
        {fdbOid(3, mac2), ber::Value::integer(3)}, // status 3 = learned
    };
    std::unordered_map<uint32_t, uint32_t> portMap = {{1, 1}};

    auto entries = bucketMacTable(varbinds, portMap);

    REQUIRE(entries.size() == 1);
    CHECK(entries[0].address == mac2);
    CHECK(entries[0].ifIndex == 1);
}

TEST_CASE("bucketMacTable drops learned entries whose bridge port has no known ifIndex") {
    const std::array<uint8_t, 6> mac = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
    std::vector<VarBind> varbinds = {
        {fdbOid(2, mac), ber::Value::integer(7)}, // bridge port 7, unmapped
        {fdbOid(3, mac), ber::Value::integer(3)}, // learned
    };
    std::unordered_map<uint32_t, uint32_t> portMap = {{1, 1}}; // no entry for port 7

    auto entries = bucketMacTable(varbinds, portMap);
    CHECK(entries.empty());
}

TEST_CASE("bucketMacTable ignores malformed OIDs") {
    std::vector<VarBind> varbinds = {
        {Oid::parse("1.3.6.1.2.1.17.4.3.1.2"), ber::Value::integer(1)}, // missing MAC index components
    };
    std::unordered_map<uint32_t, uint32_t> portMap = {{1, 1}};

    auto entries = bucketMacTable(varbinds, portMap);
    CHECK(entries.empty());
}
