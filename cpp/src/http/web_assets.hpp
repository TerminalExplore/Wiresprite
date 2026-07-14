#pragma once

#include <string>

// The dashboard's HTML/CSS/JS, embedded as string constants so the
// whole app ships as a single binary with no webroot directory to
// deploy alongside it.
namespace wiresprite::web {

// The dashboard shell. Takes whether session auth is enabled so the
// "Log out" button isn't shown when there's nothing to log out of
// (logging out with auth disabled used to just bounce you straight
// back from /login to / — see server.cpp's / handler).
std::string renderIndexPage(bool authEnabled);
extern const char* const kStyleCss;
extern const char* const kAppJs;
extern const char* const kFaviconSvg; // same spiderweb mark as the header's inline logo

// The one page that needs to vary per-request (whether to show an
// "invalid credentials" banner), so it's a function rather than a
// plain constant like the others.
std::string renderLoginPage(bool showError);

} // namespace wiresprite::web
