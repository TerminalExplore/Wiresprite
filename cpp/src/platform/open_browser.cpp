#include "platform/open_browser.hpp"

#ifdef _WIN32
#include <windows.h>

#include <shellapi.h>
#pragma comment(lib, "shell32.lib")
#else
#include <cstdlib>
#endif

namespace wiresprite {

void openBrowser(const std::string& url) {
#ifdef _WIN32
    // The URL is one we constructed ourselves (http://127.0.0.1:<port>),
    // so it's guaranteed pure ASCII — a byte-for-byte widen is safe
    // without pulling in full UTF-8 decoding for this one call site.
    std::wstring wideUrl(url.begin(), url.end());
    ShellExecuteW(nullptr, L"open", wideUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
    // Trailing "&" backgrounds xdg-open so this never blocks startup;
    // stdout/stderr redirected since there's nowhere useful for a
    // browser launcher's own chatter to go. Return value intentionally
    // ignored — this is best-effort (no browser, no $DISPLAY on a
    // headless box, sandboxed environment, ...) and never fatal.
    std::string command = "xdg-open '" + url + "' >/dev/null 2>&1 &";
    if (std::system(command.c_str()) != 0) {
        // Nothing to do — best-effort, see comment above.
    }
#endif
}

} // namespace wiresprite
