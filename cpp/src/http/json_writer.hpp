#pragma once

#include <string>

// A tiny JSON *encoder* only — this project never needs to parse JSON
// (the config file is INI), so there's no value tree or parser here,
// just enough to safely embed arbitrary strings (SNMP data an agent
// returns is untrusted) into hand-built JSON responses.
namespace wiresprite::json {

// Appends `value` to `out` as a double-quoted, escaped JSON string
// literal (including the surrounding quotes).
void appendEscapedString(std::string& out, const std::string& value);

} // namespace wiresprite::json
