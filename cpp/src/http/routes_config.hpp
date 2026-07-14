#pragma once

#include <string>

#include "config/config.hpp"
#include "httplib.h"

namespace wiresprite {

// Builds the GET /api/config JSON body the settings page's form reads
// to pre-fill itself. Omits passwordHash — the form only ever shows
// the username and a blank "new password" field, never the hash
// itself. Pure function, no I/O — unit-testable without a running
// server.
std::string buildConfigJson(const AppConfig& config);

// Parses a form-encoded POST /api/config body (the same
// httplib::Params/get_param_value mechanism POST /login already uses,
// deliberately not a JSON body — see routes_config.cpp) into a
// candidate AppConfig. `current` supplies the baseline: leaving the
// "new password" field blank keeps `current.auth.passwordHash`
// unchanged, since the form never has the real hash to echo back.
// Devices come from parallel repeated fields (device_id, device_host,
// ...), zipped by submission order. Throws ConfigError (reusing
// parseConfig's own messages, via a round-trip through writeConfig)
// if the result wouldn't be a valid config.
AppConfig parseConfigForm(const httplib::Request& req, const AppConfig& current);

} // namespace wiresprite
