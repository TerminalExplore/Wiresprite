#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "snmp/client.hpp"
#include "snmp/pdu.hpp"

// Polls the standard IF-MIB ifTable (1.3.6.1.2.1.2.2.1), the table
// every SNMP-speaking switch/router implements the same way regardless
// of vendor — this is what makes multi-vendor support "just work"
// without per-device OID profiles.
namespace snmpmon {

struct IfEntry {
    uint32_t ifIndex = 0;
    std::string ifDescr;
    int32_t ifType = 0;
    uint32_t ifSpeed = 0;
    int32_t ifAdminStatus = 0; // RFC1213: 1=up, 2=down, 3=testing
    int32_t ifOperStatus = 0;  // RFC1213: 1=up, 2=down, 3=testing, ...
    uint64_t ifInOctets = 0;
    uint64_t ifOutOctets = 0;
    uint64_t ifInErrors = 0;
    uint64_t ifOutErrors = 0;
    uint64_t ifInDiscards = 0;
    uint64_t ifOutDiscards = 0;
};

struct DevicePollResult {
    bool reachable = false;
    std::string error; // set when reachable == false
    uint32_t sysUpTimeTicks = 0;
    std::vector<IfEntry> interfaces; // ascending ifIndex order
};

// Pure bucketing logic: groups ifTable varbinds (as returned by a WALK
// over 1.3.6.1.2.1.2.2.1, in the column-major order a real agent
// returns them in — all of column N across every row before column
// N+1) by trailing ifIndex, keeping only the columns this project
// tracks. Split out from pollIfTable so it can be unit-tested without
// a network round trip.
std::vector<IfEntry> bucketIfTableVarBinds(const std::vector<VarBind>& varbinds);

// Walks ifTable and fetches sysUpTime.0 for one device. Never throws:
// network/protocol failures come back as DevicePollResult::reachable
// == false with `error` set, so one unreachable device can't abort a
// polling cycle covering several (see Phase 4's poller).
DevicePollResult pollIfTable(SnmpClient& client);

} // namespace snmpmon
