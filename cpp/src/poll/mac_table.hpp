#pragma once

#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "snmp/pdu.hpp"

// Standard BRIDGE-MIB (RFC1493) polling: which MAC addresses the switch
// has learned on which port. Same "standard MIB, no vendor profiles"
// constraint as IF-MIB polling in if_table.hpp — dot1dTpFdbTable and
// dot1dBasePortTable are implemented the same way regardless of vendor.
namespace wiresprite {

struct MacEntry {
    std::array<uint8_t, 6> address{};
    uint32_t ifIndex = 0; // translated from the bridge port via dot1dBasePortTable
};

// Buckets dot1dBasePortTable's ifIndex column
// (1.3.6.1.2.1.17.1.4.1.2.<bridgePort> -> ifIndex) into a lookup map.
// RFC1493 doesn't guarantee bridge port numbers equal ifIndex, so
// dot1dTpFdbTable's port numbers must be translated through this
// rather than assumed identical.
std::unordered_map<uint32_t, uint32_t> bucketBridgePortIfIndex(const std::vector<VarBind>& varbinds);

// Buckets a WALK over dot1dTpFdbEntry (1.3.6.1.2.1.17.4.3.1, all three
// columns together — the MAC address itself is the table index, per
// RFC1493, not a value in any column). Keeps only entries whose
// dot1dTpFdbStatus is 3 ("learned") — self/mgmt/other/invalid aren't
// "what's plugged into this port" information — and whose bridge port
// has a known ifIndex mapping in `portMap`; entries that can't be
// attributed to an ifIndex are dropped rather than guessed at.
std::vector<MacEntry> bucketMacTable(const std::vector<VarBind>& varbinds,
                                      const std::unordered_map<uint32_t, uint32_t>& portMap);

} // namespace wiresprite
