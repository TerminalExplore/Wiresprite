#pragma once

// The dashboard's HTML/CSS/JS, embedded as string constants so the
// whole app ships as a single binary with no webroot directory to
// deploy alongside it.
namespace snmpmon::web {

extern const char* const kIndexHtml;
extern const char* const kStyleCss;
extern const char* const kAppJs;

} // namespace snmpmon::web
