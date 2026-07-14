#pragma once

#include <string>

namespace wiresprite {

// Best-effort: asks the OS to open `url` in the user's default browser.
// Only ever called on a genuine first run (see main.cpp) — never on an
// ordinary restart, since that would be actively wrong for the
// systemd-managed headless deployment documented in packaging/. Never
// throws and never blocks startup; a failure (no browser, no display,
// sandboxed environment, ...) is silently ignored.
void openBrowser(const std::string& url);

} // namespace wiresprite
