#include "snmp/oid.hpp"

#include <stdexcept>

namespace wiresprite {

Oid Oid::parse(const std::string& dotted) {
    if (dotted.empty()) {
        throw std::invalid_argument("Oid::parse: empty string");
    }

    std::vector<uint32_t> components;
    size_t start = 0;
    while (start <= dotted.size()) {
        size_t dot = dotted.find('.', start);
        std::string token = dotted.substr(start, dot == std::string::npos ? std::string::npos : dot - start);
        if (token.empty()) {
            throw std::invalid_argument("Oid::parse: empty segment in \"" + dotted + "\"");
        }
        for (char c : token) {
            if (c < '0' || c > '9') {
                throw std::invalid_argument("Oid::parse: non-numeric segment \"" + token + "\" in \"" + dotted + "\"");
            }
        }
        unsigned long value = std::stoul(token);
        if (value > UINT32_MAX) {
            throw std::invalid_argument("Oid::parse: segment \"" + token + "\" out of range in \"" + dotted + "\"");
        }
        components.push_back(static_cast<uint32_t>(value));

        if (dot == std::string::npos) {
            break;
        }
        start = dot + 1;
    }

    return Oid(std::move(components));
}

std::string Oid::toString() const {
    std::string result;
    for (size_t i = 0; i < components_.size(); ++i) {
        if (i > 0) {
            result += '.';
        }
        result += std::to_string(components_[i]);
    }
    return result;
}

bool Oid::isSubtreeOf(const Oid& base) const {
    if (components_.size() < base.components_.size()) {
        return false;
    }
    for (size_t i = 0; i < base.components_.size(); ++i) {
        if (components_[i] != base.components_[i]) {
            return false;
        }
    }
    return true;
}

Oid Oid::withSuffix(const std::vector<uint32_t>& suffix) const {
    std::vector<uint32_t> combined = components_;
    combined.insert(combined.end(), suffix.begin(), suffix.end());
    return Oid(std::move(combined));
}

Oid Oid::withSuffix(uint32_t component) const {
    return withSuffix(std::vector<uint32_t>{component});
}

} // namespace wiresprite
