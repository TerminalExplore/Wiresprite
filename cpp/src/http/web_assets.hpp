#pragma once

#include <string>

// The dashboard's HTML/CSS/JS, embedded as string constants so the
// whole app ships as a single binary with no webroot directory to
// deploy alongside it.
namespace wiresprite::web {

extern const char* const kIndexHtml;
extern const char* const kStyleCss;
extern const char* const kAppJs;

// The one page that needs to vary per-request (whether to show an
// "invalid credentials" banner), so it's a function rather than a
// plain constant like the others.
std::string renderLoginPage(bool showError);

} // namespace wiresprite::web
