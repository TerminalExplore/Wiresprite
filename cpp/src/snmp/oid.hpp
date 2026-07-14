#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace snmpmon {

// A dotted-decimal object identifier, e.g. 1.3.6.1.2.1.2.2.1.10.1.
// Comparisons follow plain lexicographic ordering of the numeric
// components, which matches the ordering SNMP agents use when walking
// a MIB (a prefix sorts before any of its children).
class Oid {
public:
    Oid() = default;
    explicit Oid(std::vector<uint32_t> components) : components_(std::move(components)) {}

    // Throws std::invalid_argument if `dotted` is empty, has an empty
    // segment (e.g. "1..3"), or contains a non-numeric segment.
    static Oid parse(const std::string& dotted);

    std::string toString() const;

    const std::vector<uint32_t>& components() const { return components_; }
    bool empty() const { return components_.empty(); }
    size_t size() const { return components_.size(); }
    uint32_t operator[](size_t index) const { return components_[index]; }

    // True if `this` is `base` itself or a descendant of it in the MIB
    // tree — the check a subtree WALK uses to know when to stop.
    bool isSubtreeOf(const Oid& base) const;

    // Returns a new Oid with `suffix` appended, e.g. the ifTable column
    // OID plus an ifIndex to address a specific table cell.
    Oid withSuffix(const std::vector<uint32_t>& suffix) const;
    Oid withSuffix(uint32_t component) const;

    bool operator==(const Oid& other) const { return components_ == other.components_; }
    bool operator!=(const Oid& other) const { return !(*this == other); }
    bool operator<(const Oid& other) const { return components_ < other.components_; }

private:
    std::vector<uint32_t> components_;
};

} // namespace snmpmon
