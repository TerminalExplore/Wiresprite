// Placeholder translation unit so snmpmon_core links before Phase 1+ modules land.
// Remove once config/, snmp/, poll/, http/, auth/ sources are wired into CMakeLists.txt.
namespace snmpmon {
int core_version() { return 0; }
}
