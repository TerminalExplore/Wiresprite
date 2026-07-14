#include "http/web_assets.hpp"

namespace wiresprite::web {

// A minimal line-art spiderweb: 6 spokes from center to the viewBox
// edge plus two concentric hexagonal "rings". stroke="currentColor" so
// it inherits the surrounding text color for free in both themes.
#define WIRESPRITE_LOGO_SVG                                                                                          \
    "<svg class=\"logo-icon\" viewBox=\"0 0 24 24\" width=\"20\" height=\"20\" fill=\"none\" "                       \
    "stroke=\"currentColor\" stroke-width=\"1.3\" stroke-linecap=\"round\" aria-hidden=\"true\">"                    \
    "<line x1=\"12\" y1=\"12\" x2=\"12\" y2=\"2\"/>"                                                                 \
    "<line x1=\"12\" y1=\"12\" x2=\"20.66\" y2=\"7\"/>"                                                              \
    "<line x1=\"12\" y1=\"12\" x2=\"20.66\" y2=\"17\"/>"                                                             \
    "<line x1=\"12\" y1=\"12\" x2=\"12\" y2=\"22\"/>"                                                                \
    "<line x1=\"12\" y1=\"12\" x2=\"3.34\" y2=\"17\"/>"                                                              \
    "<line x1=\"12\" y1=\"12\" x2=\"3.34\" y2=\"7\"/>"                                                               \
    "<polygon points=\"12,9 14.6,10.5 14.6,13.5 12,15 9.4,13.5 9.4,10.5\"/>"                                         \
    "<polygon points=\"12,5.5 17.63,8.75 17.63,15.25 12,18.5 6.37,15.25 6.37,8.75\"/>"                               \
    "</svg>"

// Applies a previously chosen light/dark preference (see the toggle
// button in kAppJs/kSettingsJs) before first paint, so there's no
// flash of the wrong theme. Absent a stored choice, this is a no-op
// and the OS's prefers-color-scheme keeps driving the theme exactly
// as before this feature existed. Every page includes this, not just
// the dashboard — a toggle that only "stuck" on one page would be
// worse than not having one.
#define THEME_INIT_SCRIPT                                                                                            \
    "<script>(function(){var t=localStorage.getItem(\"wiresprite-theme\");"                                         \
    "if(t===\"light\"||t===\"dark\"){document.documentElement.setAttribute(\"data-theme\",t);}})();</script>"

// Same spiderweb mark as the header's inline logo, as a standalone SVG
// document for the browser tab's favicon. Favicons render outside any
// surrounding text color context, so this can't rely on currentColor
// like the header icon does — instead it picks its own stroke color
// via an embedded prefers-color-scheme media query (supported by
// Chrome/Firefox for SVG favicons), so the tab icon still matches
// light/dark like everything else on the page.
const char* const kFaviconSvg = R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
<style>
  .web { stroke: #0b0b0b; }
  @media (prefers-color-scheme: dark) { .web { stroke: #ffffff; } }
</style>
<g class="web" fill="none" stroke-width="1.5" stroke-linecap="round">
<line x1="12" y1="12" x2="12" y2="2"/>
<line x1="12" y1="12" x2="20.66" y2="7"/>
<line x1="12" y1="12" x2="20.66" y2="17"/>
<line x1="12" y1="12" x2="12" y2="22"/>
<line x1="12" y1="12" x2="3.34" y2="17"/>
<line x1="12" y1="12" x2="3.34" y2="7"/>
<polygon points="12,9 14.6,10.5 14.6,13.5 12,15 9.4,13.5 9.4,10.5"/>
<polygon points="12,5.5 17.63,8.75 17.63,15.25 12,18.5 6.37,15.25 6.37,8.75"/>
</g>
</svg>
)SVG";

std::string renderIndexPage(bool authEnabled) {
    std::string logoutForm = authEnabled ? R"HTML(
    <form method="post" action="/logout" class="logout-form">
      <button type="submit" class="header-button">Log out</button>
    </form>)HTML"
                                          : "";

    return std::string(R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Wiresprite</title>
<link rel="icon" type="image/svg+xml" href="/favicon.svg">
)HTML" THEME_INIT_SCRIPT R"HTML(
<link rel="stylesheet" href="/style.css">
</head>
<body>
<header>
  <h1>)HTML" WIRESPRITE_LOGO_SVG R"HTML( Wiresprite</h1>
  <span id="last-updated">loading&hellip;</span>
  <div class="header-actions">
    <button type="button" id="theme-toggle" class="header-button" aria-label="Toggle light/dark theme"></button>
    <button type="button" id="kiosk-toggle" class="header-button" aria-label="Kiosk / wallboard mode">
      <svg viewBox="0 0 24 24" width="18" height="18" fill="none" stroke="currentColor" stroke-width="1.6"
           stroke-linecap="round" stroke-linejoin="round">
        <path d="M4 9V4h5"/><path d="M20 9V4h-5"/><path d="M4 15v5h5"/><path d="M20 15v5h-5"/>
      </svg>
    </button>
    <button type="button" id="export-csv" class="header-button">Export CSV</button>)HTML") +
           logoutForm + R"HTML(
  </div>
</header>
<nav class="navbar">
  <button type="button" class="nav-btn active" data-page="ports">Ports</button>
  <button type="button" class="nav-btn" data-page="overview">Overview</button>
  <a href="/settings" class="nav-settings-link">Settings</a>
</nav>
<section id="page-ports" class="page active">
  <div id="summary" class="summary-grid"></div>
  <div id="alert-banner"></div>
  <div class="filter-bar">
    <span class="filter-label">Show:</span>
    <div class="segmented" id="port-filter">
      <button type="button" class="segmented-btn active" data-filter="all">All</button>
      <button type="button" class="segmented-btn" data-filter="up">Up</button>
      <button type="button" class="segmented-btn" data-filter="down">Down</button>
    </div>
  </div>
  <main id="devices"></main>
</section>
<section id="page-overview" class="page">
  <div class="overview-grid">
    <div class="overview-card">
      <h3>Total traffic</h3>
      <div class="iface-charts" id="overview-charts"></div>
    </div>
    <div class="overview-card">
      <h3>Errors &amp; discards</h3>
      <div id="overview-errors-wrap"></div>
    </div>
  </div>
</section>
<script src="/app.js"></script>
</body>
</html>
)HTML";
}

// Palette and mark specs are the dataviz skill's validated reference
// instance (references/palette.md), used verbatim rather than
// invented — see cpp/README.md's "Dashboard design" section.
const char* const kStyleCss = R"CSS(:root {
  color-scheme: light;
  --page: #f9f9f7;
  --surface: #fcfcfb;
  --ink: #0b0b0b;
  --ink-secondary: #52514e;
  --ink-muted: #898781;
  --gridline: #e1e0d9;
  --border: rgba(11, 11, 11, 0.10);
  --series-in: #2a78d6;
  --series-out: #1baf7a;
  --status-good: #0ca30c;
  --status-warning: #fab219;
  --status-critical: #d03b3b;
  --status-good-wash: rgba(12, 163, 12, 0.12);
  --status-warning-wash: rgba(250, 178, 25, 0.16);
  --status-critical-wash: rgba(208, 59, 59, 0.12);
  --accent: #2a78d6;
  --accent-hover: #1c5cab;
}

@media (prefers-color-scheme: dark) {
  :root {
    color-scheme: dark;
    --page: #0d0d0d;
    --surface: #1a1a19;
    --ink: #ffffff;
    --ink-secondary: #c3c2b7;
    --ink-muted: #898781;
    --gridline: #2c2c2a;
    --border: rgba(255, 255, 255, 0.10);
    --series-in: #3987e5;
    --series-out: #199e70;
    --status-good: #0ca30c;
    --status-warning: #fab219;
    --status-critical: #d03b3b;
    --status-good-wash: rgba(12, 163, 12, 0.18);
    --status-warning-wash: rgba(250, 178, 25, 0.20);
    --status-critical-wash: rgba(208, 59, 59, 0.20);
    --accent: #3987e5;
    --accent-hover: #5598e7;
  }
}

/* Manual theme toggle: an attribute selector on :root has higher
   specificity than the plain :root/media-query rules above, so these
   win whenever data-theme is actually set (by the head script or the
   toggle button in kAppJs/kSettingsJs) — and since attribute selectors
   simply don't match when the attribute is absent, the OS-driven
   media query above still governs until the user toggles once. */
:root[data-theme="light"] {
  color-scheme: light;
  --page: #f9f9f7;
  --surface: #fcfcfb;
  --ink: #0b0b0b;
  --ink-secondary: #52514e;
  --ink-muted: #898781;
  --gridline: #e1e0d9;
  --border: rgba(11, 11, 11, 0.10);
  --series-in: #2a78d6;
  --series-out: #1baf7a;
  --status-good: #0ca30c;
  --status-warning: #fab219;
  --status-critical: #d03b3b;
  --status-good-wash: rgba(12, 163, 12, 0.12);
  --status-warning-wash: rgba(250, 178, 25, 0.16);
  --status-critical-wash: rgba(208, 59, 59, 0.12);
  --accent: #2a78d6;
  --accent-hover: #1c5cab;
}

:root[data-theme="dark"] {
  color-scheme: dark;
  --page: #0d0d0d;
  --surface: #1a1a19;
  --ink: #ffffff;
  --ink-secondary: #c3c2b7;
  --ink-muted: #898781;
  --gridline: #2c2c2a;
  --border: rgba(255, 255, 255, 0.10);
  --series-in: #3987e5;
  --series-out: #199e70;
  --status-good: #0ca30c;
  --status-warning: #fab219;
  --status-critical: #d03b3b;
  --status-good-wash: rgba(12, 163, 12, 0.18);
  --status-warning-wash: rgba(250, 178, 25, 0.20);
  --status-critical-wash: rgba(208, 59, 59, 0.20);
  --accent: #3987e5;
  --accent-hover: #5598e7;
}

* {
  box-sizing: border-box;
}

body {
  font-family: system-ui, -apple-system, "Segoe UI", Roboto, sans-serif;
  margin: 0;
  padding: 0 1.5rem 3rem;
  background: var(--page);
  color: var(--ink);
}

header {
  display: flex;
  align-items: baseline;
  gap: 1rem;
  padding: 1.5rem 0 1.25rem;
  max-width: 1200px;
  margin: 0 auto;
}

header h1 {
  margin: 0;
  display: flex;
  align-items: center;
  gap: 0.4rem;
  font-size: 1.3rem;
  font-weight: 600;
}

.logo-icon {
  flex: none;
}

#last-updated {
  color: var(--ink-muted);
  font-size: 0.85rem;
}

.navbar {
  display: flex;
  gap: 0.4rem;
  max-width: 1200px;
  margin: 0 auto;
  padding-bottom: 1rem;
}

.nav-btn,
.nav-settings-link {
  font: inherit;
  display: inline-block;
  padding: 0.4rem 0.9rem;
  border: 1px solid var(--border);
  border-radius: 6px;
  background: transparent;
  color: var(--ink-secondary);
  cursor: pointer;
  font-size: 0.85rem;
  text-decoration: none;
}

.nav-btn:hover,
.nav-settings-link:hover {
  background: color-mix(in srgb, var(--ink) 6%, transparent);
}

.nav-btn.active {
  background: var(--accent);
  border-color: var(--accent);
  color: white;
}

.page {
  display: none;
}

.page.active {
  display: block;
}

.filter-bar {
  display: flex;
  align-items: center;
  gap: 0.5rem;
  max-width: 1200px;
  margin: 0 auto 0.85rem;
  font-size: 0.82rem;
  color: var(--ink-secondary);
}

.segmented {
  display: inline-flex;
  border: 1px solid var(--border);
  border-radius: 6px;
  overflow: hidden;
}

.segmented-btn {
  font: inherit;
  padding: 0.3rem 0.7rem;
  border: none;
  border-left: 1px solid var(--border);
  background: transparent;
  color: inherit;
  cursor: pointer;
  font-size: 0.8rem;
}

.segmented-btn:first-child {
  border-left: none;
}

.segmented-btn.active {
  background: var(--accent);
  color: white;
}

main {
  max-width: 1200px;
  margin: 0 auto;
  display: flex;
  flex-direction: column;
  gap: 1rem;
}

.device {
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: 10px;
  padding: 1.1rem 1.25rem;
}

.device.unreachable {
  border-color: var(--status-critical);
}

.device.hidden-by-filter,
.iface-card.hidden-by-filter {
  display: none;
}

.device-header {
  display: flex;
  align-items: baseline;
  gap: 0.6rem;
  margin-bottom: 0.15rem;
}

.device-header h2 {
  margin: 0;
  font-size: 1.05rem;
  font-weight: 600;
}

.device-host {
  color: var(--ink-muted);
  font-size: 0.85rem;
  font-variant-numeric: tabular-nums;
}

.device-descr {
  color: var(--ink-muted);
  font-size: 0.78rem;
  margin: 0 0 0.5rem;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.device .meta {
  color: var(--ink-secondary);
  font-size: 0.85rem;
  margin: 0 0 0.85rem;
}

.device .error {
  color: var(--status-critical);
  font-size: 0.9rem;
  margin: 0.25rem 0 0;
}

.interfaces {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(280px, 1fr));
  gap: 0.75rem;
}

.iface-card {
  border: 1px solid var(--border);
  border-radius: 8px;
  padding: 0.75rem 0.85rem;
}

.iface-head {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 0.5rem;
}

.iface-name {
  font-weight: 600;
  font-size: 0.92rem;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.iface-meta {
  color: var(--ink-muted);
  font-size: 0.78rem;
  margin: 0.15rem 0 0.6rem;
  font-variant-numeric: tabular-nums;
}

.iface-warn {
  font-size: 0.78rem;
  margin-top: 0.5rem;
}

.iface-warn-warning {
  color: var(--status-warning);
}

.iface-warn-critical {
  color: var(--status-critical);
  font-weight: 600;
}

.mac-details {
  margin-top: 0.5rem;
  font-size: 0.78rem;
}

.mac-details summary {
  color: var(--ink-secondary);
  cursor: pointer;
}

.mac-details ul {
  margin: 0.35rem 0 0;
  padding-left: 1.1rem;
  color: var(--ink-muted);
  font-variant-numeric: tabular-nums;
}

.down-ports {
  display: flex;
  flex-wrap: wrap;
  gap: 0.4rem;
  margin-top: 0.75rem;
}

.down-badge {
  display: inline-flex;
  align-items: center;
  gap: 0.3rem;
  font-size: 0.78rem;
  color: var(--ink-secondary);
  background: color-mix(in srgb, var(--ink) 4%, transparent);
  border: 1px solid var(--border);
  border-radius: 999px;
  padding: 0.2rem 0.6rem;
  white-space: nowrap;
}

.no-up-note {
  color: var(--ink-muted);
  font-size: 0.85rem;
  margin: 0;
}

/* Status pill: icon + text always together, never color alone. */
.pill {
  display: inline-flex;
  align-items: center;
  gap: 0.3rem;
  font-size: 0.75rem;
  font-weight: 600;
  padding: 0.15rem 0.55rem;
  border-radius: 999px;
  white-space: nowrap;
}

.pill-good {
  background: var(--status-good-wash);
  color: var(--status-good);
}

.pill-warning {
  background: var(--status-warning-wash);
  color: var(--status-warning);
}

.pill-critical {
  background: var(--status-critical-wash);
  color: var(--status-critical);
}

.iface-charts {
  display: flex;
  gap: 0.6rem;
}

.sparkline-wrap {
  flex: 1;
  min-width: 0;
  position: relative;
}

.sparkline-label {
  color: var(--ink-muted);
  font-size: 0.7rem;
  text-transform: uppercase;
  letter-spacing: 0.03em;
  margin-bottom: 0.1rem;
}

.sparkline {
  display: block;
  width: 100%;
  height: 90px;
  cursor: crosshair;
}

.sparkline-empty {
  color: var(--ink-muted);
  font-size: 0.78rem;
  height: 90px;
  display: flex;
  align-items: center;
}

.sparkline-value {
  font-size: 0.78rem;
  font-weight: 600;
  font-variant-numeric: tabular-nums;
  margin-top: 0.1rem;
}

.sparkline-range {
  font-size: 0.7rem;
  color: var(--ink-muted);
}

.sparkline-tooltip {
  position: absolute;
  top: -1.9rem;
  transform: translateX(-50%);
  background: var(--ink);
  color: var(--surface);
  font-size: 0.72rem;
  font-variant-numeric: tabular-nums;
  padding: 0.2rem 0.4rem;
  border-radius: 4px;
  white-space: nowrap;
  pointer-events: none;
  z-index: 1;
}

.header-actions {
  margin-left: auto;
  display: flex;
  align-items: center;
  gap: 0.5rem;
}

.header-button {
  font: inherit;
  padding: 0.35rem 0.75rem;
  border: 1px solid var(--border);
  border-radius: 6px;
  background: transparent;
  color: inherit;
  cursor: pointer;
}

.header-button:hover {
  background: color-mix(in srgb, var(--ink) 8%, transparent);
}

/* Wallboard mode: hide chrome that's pointless on a wallboard (nav,
   the port filter, header buttons) but keep #summary/#alert-banner —
   "what's wrong right now" is exactly what a wallboard should show. */
body.kiosk-mode .navbar,
body.kiosk-mode .filter-bar,
body.kiosk-mode .header-actions {
  display: none;
}

.summary-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(180px, 1fr));
  gap: 0.75rem;
  max-width: 1200px;
  margin: 0 auto 1rem;
}

.stat-tile {
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: 10px;
  padding: 0.85rem 1.1rem;
}

.stat-tile-label {
  color: var(--ink-muted);
  font-size: 0.72rem;
  font-weight: 600;
  text-transform: uppercase;
  letter-spacing: 0.03em;
}

.stat-tile-value {
  font-size: 1.55rem;
  font-weight: 600;
  margin-top: 0.2rem;
}

.stat-tile-sub {
  color: var(--ink-muted);
  font-size: 0.75rem;
  margin-top: 0.1rem;
}

/* Meter: fill carries severity (accent/warning/critical); the track is
   a lighter step of the same ramp so state reads across the whole bar. */
.meter {
  height: 6px;
  border-radius: 999px;
  background: color-mix(in srgb, var(--ink) 10%, transparent);
  margin-top: 0.5rem;
  overflow: hidden;
}

.meter-fill {
  height: 100%;
  border-radius: 999px;
  background: var(--accent);
}

.meter-fill.warning {
  background: var(--status-warning);
}

.meter-fill.critical {
  background: var(--status-critical);
}

/* Icon + text always together, never color alone, matching the pills. */
.alert-banner {
  display: none;
  align-items: center;
  gap: 0.5rem;
  max-width: 1200px;
  margin: 0 auto 1rem;
  padding: 0.6rem 1rem;
  border-radius: 8px;
  font-size: 0.88rem;
  font-weight: 600;
}

.alert-banner.visible {
  display: flex;
}

.alert-banner.warning {
  background: var(--status-warning-wash);
  color: var(--status-warning);
}

.alert-banner.critical {
  background: var(--status-critical-wash);
  color: var(--status-critical);
}

.overview-grid {
  max-width: 1200px;
  margin: 0 auto;
  display: flex;
  flex-direction: column;
  gap: 1rem;
}

.overview-card {
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: 10px;
  padding: 1.1rem 1.25rem;
}

.overview-card h3 {
  margin: 0 0 0.85rem;
  font-size: 0.95rem;
  font-weight: 600;
}

.errors-table {
  width: 100%;
  border-collapse: collapse;
  font-size: 0.85rem;
}

.errors-table th,
.errors-table td {
  text-align: left;
  padding: 0.4rem 0.6rem;
  border-bottom: 1px solid var(--border);
}

.errors-table th {
  color: var(--ink-muted);
  font-weight: 600;
  font-size: 0.72rem;
  text-transform: uppercase;
  letter-spacing: 0.03em;
}

.errors-empty {
  color: var(--ink-muted);
  font-size: 0.85rem;
  margin: 0;
}

.login {
  display: flex;
  align-items: center;
  justify-content: center;
  min-height: 100vh;
}

.login-card {
  display: flex;
  flex-direction: column;
  gap: 0.75rem;
  width: 100%;
  max-width: 320px;
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: 10px;
  padding: 1.5rem;
}

.login-card h1 {
  margin: 0 0 0.5rem;
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 0.4rem;
  font-size: 1.2rem;
}

.login-card label {
  display: flex;
  flex-direction: column;
  gap: 0.3rem;
  font-size: 0.9rem;
  color: var(--ink-secondary);
}

.login-remember {
  flex-direction: row !important;
  align-items: center;
  gap: 0.4rem !important;
}

.login-card input {
  font: inherit;
  padding: 0.4rem 0.5rem;
  border: 1px solid var(--border);
  border-radius: 6px;
  background: var(--page);
  color: var(--ink);
}

.login-card button {
  font: inherit;
  padding: 0.5rem;
  border: none;
  border-radius: 6px;
  background: var(--accent);
  color: white;
  cursor: pointer;
}

.login-card button:hover {
  background: var(--accent-hover);
}

.settings-welcome {
  max-width: 700px;
  margin: 1.5rem auto 1.5rem;
  padding: 1.25rem 1.5rem;
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: 10px;
}

.settings-welcome h2 {
  margin: 0 0 0.5rem;
  font-size: 1.15rem;
}

.settings-welcome p {
  margin: 0;
  color: var(--ink-secondary);
  font-size: 0.9rem;
}

.settings-form {
  max-width: 700px;
  margin: 1.5rem auto;
  display: flex;
  flex-direction: column;
  gap: 1rem;
}

.settings-card {
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: 10px;
  padding: 1.1rem 1.25rem;
  display: flex;
  flex-direction: column;
  gap: 0.75rem;
}

.settings-card h3 {
  margin: 0;
  font-size: 0.95rem;
  font-weight: 600;
}

.settings-card label {
  display: flex;
  flex-direction: column;
  gap: 0.3rem;
  font-size: 0.85rem;
  color: var(--ink-secondary);
}

.settings-card input,
.settings-card select {
  font: inherit;
  padding: 0.4rem 0.5rem;
  border: 1px solid var(--border);
  border-radius: 6px;
  background: var(--page);
  color: var(--ink);
}

.settings-hint {
  color: var(--ink-muted);
  font-weight: 400;
  font-size: 0.78rem;
}

.settings-checkbox-row {
  flex-direction: row !important;
  align-items: center;
  gap: 0.4rem !important;
}

.device-row {
  display: grid;
  grid-template-columns: 1fr 1fr 1.3fr 0.7fr 1fr 0.8fr auto;
  gap: 0.5rem;
  align-items: center;
}

.device-row input,
.device-row select {
  font: inherit;
  padding: 0.35rem 0.5rem;
  border: 1px solid var(--border);
  border-radius: 6px;
  background: var(--page);
  color: var(--ink);
  min-width: 0;
}

.settings-message {
  font-size: 0.85rem;
  padding: 0.5rem 0.75rem;
  border-radius: 6px;
  display: none;
}

.settings-message.success {
  display: block;
  background: var(--status-good-wash);
  color: var(--status-good);
}

.settings-message.error {
  display: block;
  background: var(--status-critical-wash);
  color: var(--status-critical);
}
)CSS";

const char* const kAppJs = R"JS(function statusLabel(code) {
  switch (code) {
    case 1: return "Up";
    case 2: return "Down";
    case 3: return "Testing";
    default: return "Unknown";
  }
}

// Network rates use decimal (1000-based) prefixes, not binary 1024 —
// bps/Kbps/Mbps/Gbps is the networking convention.
function formatRate(bitsPerSec) {
  const units = ["bps", "Kbps", "Mbps", "Gbps"];
  let i = 0;
  let value = bitsPerSec;
  while (value >= 1000 && i < units.length - 1) {
    value /= 1000;
    i++;
  }
  return value.toFixed(value < 10 && i > 0 ? 1 : 0) + " " + units[i];
}

function formatDuration(totalSeconds) {
  const s = Math.max(0, Math.floor(totalSeconds));
  const days = Math.floor(s / 86400);
  const hours = Math.floor((s % 86400) / 3600);
  const minutes = Math.floor((s % 3600) / 60);
  if (days > 0) return days + "d " + hours + "h";
  if (hours > 0) return hours + "h " + minutes + "m";
  return minutes + "m";
}

// ifLastChange is in the same TimeTicks epoch as the device's own
// sysUpTimeTicks (RFC2863: "the value of sysUpTime at the time this
// interface entered its current state"), so the time since is just
// the difference between the two, in centiseconds.
function formatSinceChange(sysUpTimeTicks, ifLastChangeTicks) {
  return formatDuration((sysUpTimeTicks - ifLastChangeTicks) / 100);
}

)JS" R"JS(
// ---- Sparkline: split into create (build DOM + wire listeners once)
// and update (recompute the path/label only) so a live SSE push never
// tears down and re-attaches hover listeners — that churn is what
// would otherwise cause visible flicker/jank at push cadence.
function createSparkline(opts) {
  const width = 140;
  const height = 90;
  const pad = 2;
  const svgNS = "http://www.w3.org/2000/svg";

  const wrap = document.createElement("div");
  wrap.className = "sparkline-wrap";

  const labelEl = document.createElement("div");
  labelEl.className = "sparkline-label";
  labelEl.textContent = opts.label;
  wrap.appendChild(labelEl);

  const svg = document.createElementNS(svgNS, "svg");
  svg.setAttribute("viewBox", "0 0 " + width + " " + height);
  svg.setAttribute("class", "sparkline");
  svg.setAttribute("preserveAspectRatio", "none");

  // Two recessive hairline gridlines (1/3, 2/3 height) so the chart
  // reads as a real chart with a scale reference, not a bare line on
  // an empty background. Fixed per instance since chart height never
  // changes, so no need to recompute these in updateSparkline.
  for (const frac of [1 / 3, 2 / 3]) {
    const gridline = document.createElementNS(svgNS, "line");
    const y = height * frac;
    gridline.setAttribute("x1", "0");
    gridline.setAttribute("x2", String(width));
    gridline.setAttribute("y1", String(y));
    gridline.setAttribute("y2", String(y));
    gridline.setAttribute("stroke", "var(--gridline)");
    gridline.setAttribute("stroke-width", "1");
    gridline.setAttribute("vector-effect", "non-scaling-stroke");
    svg.appendChild(gridline);
  }

  const area = document.createElementNS(svgNS, "path");
  area.setAttribute("fill", opts.color);
  area.setAttribute("opacity", "0.1");
  area.setAttribute("stroke", "none");
  svg.appendChild(area);

  // preserveAspectRatio="none" below stretches the viewBox's 140x{height}
  // coordinate space non-uniformly to fill whatever width the container
  // ends up being (a narrow Ports-page card vs. a full-width Overview
  // card, in particular) — without vector-effect="non-scaling-stroke",
  // that horizontal stretch inflates stroke widths and turns the round
  // hover dot into a fat ellipse. Every stroked mark below opts out of
  // that scaling so line/marker thickness stays constant in real pixels
  // regardless of how wide the chart is rendered.
  const line = document.createElementNS(svgNS, "path");
  line.setAttribute("fill", "none");
  line.setAttribute("stroke", opts.color);
  line.setAttribute("stroke-width", "2");
  line.setAttribute("stroke-linecap", "round");
  line.setAttribute("stroke-linejoin", "round");
  line.setAttribute("vector-effect", "non-scaling-stroke");
  svg.appendChild(line);

  const crosshair = document.createElementNS(svgNS, "line");
  crosshair.setAttribute("y1", "0");
  crosshair.setAttribute("y2", String(height));
  crosshair.setAttribute("stroke", "var(--gridline)");
  crosshair.setAttribute("stroke-width", "1");
  crosshair.setAttribute("vector-effect", "non-scaling-stroke");
  crosshair.setAttribute("visibility", "hidden");
  svg.appendChild(crosshair);

  // A near-zero-radius circle stroked with a constant (non-scaling)
  // width renders as a crisp, genuinely round dot even under the
  // non-uniform scale above — a filled circle with a plain `r` would
  // get stretched into an ellipse the same way the line's stroke would.
  const dot = document.createElementNS(svgNS, "circle");
  dot.setAttribute("r", "0.01");
  dot.setAttribute("fill", "none");
  dot.setAttribute("stroke", opts.color);
  dot.setAttribute("stroke-width", "6");
  dot.setAttribute("vector-effect", "non-scaling-stroke");
  dot.setAttribute("visibility", "hidden");
  svg.appendChild(dot);

  const hit = document.createElementNS(svgNS, "rect");
  hit.setAttribute("x", "0");
  hit.setAttribute("y", "0");
  hit.setAttribute("width", String(width));
  hit.setAttribute("height", String(height));
  hit.setAttribute("fill", "transparent");
  svg.appendChild(hit);

  wrap.appendChild(svg);

  const empty = document.createElement("div");
  empty.className = "sparkline-empty";
  empty.textContent = "No data yet";
  wrap.appendChild(empty);

  const tooltip = document.createElement("div");
  tooltip.className = "sparkline-tooltip";
  tooltip.hidden = true;
  wrap.appendChild(tooltip);

  const endLabel = document.createElement("div");
  endLabel.className = "sparkline-value";
  wrap.appendChild(endLabel);

  const range = document.createElement("div");
  range.className = "sparkline-range";
  wrap.appendChild(range);

  const handles = {
    wrap, svg, area, line, crosshair, dot, hit, tooltip, endLabel, empty, range,
    opts, points: [], xAt: null, yAt: null, width, height, pad,
  };

  function nearestIndex(offsetX) {
    const px = (offsetX / handles.svg.getBoundingClientRect().width) * width;
    let nearest = 0;
    let best = Infinity;
    for (let i = 0; i < handles.points.length; i++) {
      const d = Math.abs(handles.xAt(i) - px);
      if (d < best) {
        best = d;
        nearest = i;
      }
    }
    return nearest;
  }

  function handleMove(evt) {
    if (handles.points.length < 2) return;
    const rect = handles.svg.getBoundingClientRect();
    const i = nearestIndex(evt.clientX - rect.left);
    const x = handles.xAt(i);
    crosshair.setAttribute("x1", String(x));
    crosshair.setAttribute("x2", String(x));
    crosshair.setAttribute("visibility", "visible");
    dot.setAttribute("cx", String(x));
    dot.setAttribute("cy", String(handles.yAt(handles.points[i].v)));
    dot.setAttribute("visibility", "visible");

    const when = new Date(handles.points[i].t * 1000);
    tooltip.textContent = handles.opts.formatValue(handles.points[i].v) + " — " + when.toLocaleTimeString();
    tooltip.hidden = false;
    tooltip.style.left = (x / width) * 100 + "%";
  }

  function handleLeave() {
    crosshair.setAttribute("visibility", "hidden");
    dot.setAttribute("visibility", "hidden");
    tooltip.hidden = true;
  }

  hit.addEventListener("pointermove", handleMove);
  hit.addEventListener("pointerleave", handleLeave);

  updateSparkline(handles, []);
  return handles;
}

function updateSparkline(handles, points) {
  handles.points = points;
  const enough = points.length >= 2;
  handles.svg.style.display = enough ? "block" : "none";
  handles.empty.style.display = enough ? "none" : "flex";
  handles.endLabel.style.display = enough ? "block" : "none";
  handles.range.style.display = enough ? "block" : "none";
  if (!enough) return;

  handles.range.textContent = "last " + formatDuration(points[points.length - 1].t - points[0].t);

  const { width, height, pad } = handles;
  const maxValue = Math.max(...points.map((p) => p.v), 1);
  const xAt = (i) => pad + (i / (points.length - 1)) * (width - pad * 2);
  const yAt = (v) => height - pad - (v / maxValue) * (height - pad * 2);
  handles.xAt = xAt;
  handles.yAt = yAt;

  let linePath = "";
  let areaPath = "M " + xAt(0) + " " + (height - pad) + " ";
  points.forEach((p, i) => {
    const x = xAt(i);
    const y = yAt(p.v);
    linePath += (i === 0 ? "M " : "L ") + x + " " + y + " ";
    areaPath += "L " + x + " " + y + " ";
  });
  areaPath += "L " + xAt(points.length - 1) + " " + (height - pad) + " Z";

  handles.area.setAttribute("d", areaPath);
  handles.line.setAttribute("d", linePath);
  handles.endLabel.textContent = handles.opts.formatValue(points[points.length - 1].v);
}

)JS" R"JS(
// ---- Interface card: create once per (device, ifIndex), update in place.
function createIfaceCard() {
  const card = document.createElement("div");
  card.className = "iface-card";

  const head = document.createElement("div");
  head.className = "iface-head";
  const name = document.createElement("span");
  name.className = "iface-name";
  head.appendChild(name);
  const pill = document.createElement("span");
  head.appendChild(pill);
  card.appendChild(head);

  const meta = document.createElement("div");
  meta.className = "iface-meta";
  card.appendChild(meta);

  const charts = document.createElement("div");
  charts.className = "iface-charts";
  const inSpark = createSparkline({ color: "var(--series-in)", label: "In", formatValue: formatRate });
  const outSpark = createSparkline({ color: "var(--series-out)", label: "Out", formatValue: formatRate });
  charts.appendChild(inSpark.wrap);
  charts.appendChild(outSpark.wrap);
  card.appendChild(charts);

  const warn = document.createElement("div");
  warn.className = "iface-warn";
  warn.style.display = "none";
  card.appendChild(warn);

  const macDetails = document.createElement("details");
  macDetails.className = "mac-details";
  macDetails.style.display = "none";
  const macSummary = document.createElement("summary");
  macDetails.appendChild(macSummary);
  const macList = document.createElement("ul");
  macDetails.appendChild(macList);
  card.appendChild(macDetails);

  return { el: card, name, pill, meta, inSpark, outSpark, warn, macDetails, macSummary, macList, ifOperStatus: null };
}

function updateIfaceCard(handles, iface, sysUpTimeTicks) {
  handles.name.textContent = iface.ifAlias || iface.ifDescr || ("#" + iface.ifIndex);

  let cls, icon;
  if (iface.ifOperStatus === 1) {
    cls = "pill-good";
    icon = "●"; // filled circle
  } else if (iface.ifOperStatus === 2) {
    cls = "pill-critical";
    icon = "▲"; // triangle
  } else {
    cls = "pill-warning";
    icon = "◆"; // diamond
  }
  handles.pill.className = "pill " + cls;
  handles.pill.textContent = icon + " " + statusLabel(iface.ifOperStatus);

  handles.meta.textContent =
    formatRate(iface.ifSpeed) +
    " link · admin " +
    statusLabel(iface.ifAdminStatus).toLowerCase() +
    " · up " +
    formatSinceChange(sysUpTimeTicks, iface.ifLastChange);

  handles.macList.innerHTML = "";
  if (iface.macs && iface.macs.length > 0) {
    handles.macSummary.textContent = iface.macs.length + " device" + (iface.macs.length > 1 ? "s" : "");
    for (const mac of iface.macs) {
      const li = document.createElement("li");
      li.textContent = mac;
      handles.macList.appendChild(li);
    }
    handles.macDetails.style.display = "block";
  } else {
    handles.macDetails.style.display = "none";
  }

  const inPoints = iface.history.map((h) => ({ t: h.t, v: h.inBps }));
  const outPoints = iface.history.map((h) => ({ t: h.t, v: h.outBps }));
  updateSparkline(handles.inSpark, inPoints);
  updateSparkline(handles.outSpark, outPoints);

  // IF-MIB only gives cumulative counters, not a packet-count
  // denominator, so there's no clean traffic-relative percentage to
  // show — use magnitude tiers instead, the same kind of threshold
  // call the alert banner already makes for "down" (any amount matters,
  // just at different severities).
  const errors = iface.ifInErrors + iface.ifOutErrors;
  const discards = iface.ifInDiscards + iface.ifOutDiscards;
  const total = errors + discards;
  if (total === 0) {
    handles.warn.style.display = "none";
  } else {
    const critical = total >= 50;
    handles.warn.className = "iface-warn " + (critical ? "iface-warn-critical" : "iface-warn-warning");
    handles.warn.textContent = (critical ? "▲ " : "◆ ") + errors + " errors, " + discards + " discards";
    handles.warn.style.display = "block";
  }

  handles.ifOperStatus = iface.ifOperStatus;
  handles.ifAdminStatus = iface.ifAdminStatus;
}

)JS" R"JS(
// ---- Device section: create once per device id, update in place.
// Up ports (ifOperStatus === 1) get full cards with charts, in the
// keyed ifaceCards Map so live updates keep diffing them. Everything
// else (down, testing, unknown, admin-disabled) collapses into a
// compact badge list — no chart DOM worth preserving, so it's cheap
// to rebuild wholesale each push, same treatment as the errors table.
function createDeviceSection() {
  const el = document.createElement("section");
  el.className = "device";

  const header = document.createElement("div");
  header.className = "device-header";
  const title = document.createElement("h2");
  header.appendChild(title);
  const host = document.createElement("span");
  host.className = "device-host";
  header.appendChild(host);
  el.appendChild(header);

  const descr = document.createElement("div");
  descr.className = "device-descr";
  descr.style.display = "none";
  el.appendChild(descr);

  const err = document.createElement("p");
  err.className = "error";
  err.style.display = "none";
  el.appendChild(err);

  const meta = document.createElement("p");
  meta.className = "meta";
  meta.style.display = "none";
  el.appendChild(meta);

  const noUpNote = document.createElement("p");
  noUpNote.className = "no-up-note";
  noUpNote.textContent = "No ports currently up";
  noUpNote.style.display = "none";
  el.appendChild(noUpNote);

  const grid = document.createElement("div");
  grid.className = "interfaces";
  el.appendChild(grid);

  const downList = document.createElement("div");
  downList.className = "down-ports";
  el.appendChild(downList);

  return { el, title, host, descr, err, meta, noUpNote, grid, downList, ifaceCards: new Map() };
}

function updateDeviceSection(handles, device) {
  handles.el.className = "device" + (device.reachable ? "" : " unreachable");
  handles.title.textContent = device.displayName;
  handles.host.textContent = device.host;

  if (device.sysDescr) {
    handles.descr.textContent = device.sysDescr;
    handles.descr.style.display = "block";
  } else {
    handles.descr.style.display = "none";
  }

  if (!device.reachable) {
    handles.err.textContent = device.error || "unreachable";
    handles.err.style.display = "block";
    handles.meta.style.display = "none";
    handles.noUpNote.style.display = "none";
    handles.grid.style.display = "none";
    handles.downList.style.display = "none";
    return;
  }

  handles.err.style.display = "none";
  handles.meta.style.display = "block";
  handles.meta.textContent =
    "uptime " + formatDuration(device.sysUpTimeTicks / 100) + " · " + device.interfaces.length + " interfaces";

  const upIfaces = device.interfaces.filter((i) => i.ifOperStatus === 1);
  const otherIfaces = device.interfaces.filter((i) => i.ifOperStatus !== 1);

  const seen = new Set();
  for (const iface of upIfaces) {
    seen.add(iface.ifIndex);
    let cardHandles = handles.ifaceCards.get(iface.ifIndex);
    if (!cardHandles) {
      cardHandles = createIfaceCard();
      handles.ifaceCards.set(iface.ifIndex, cardHandles);
      handles.grid.appendChild(cardHandles.el);
    }
    updateIfaceCard(cardHandles, iface, device.sysUpTimeTicks);
  }
  for (const [ifIndex, cardHandles] of handles.ifaceCards) {
    if (!seen.has(ifIndex)) {
      cardHandles.el.remove();
      handles.ifaceCards.delete(ifIndex);
    }
  }
  handles.grid.style.display = upIfaces.length > 0 ? "grid" : "none";
  handles.noUpNote.style.display = upIfaces.length === 0 ? "block" : "none";

  handles.downList.innerHTML = "";
  handles.downList.style.display = otherIfaces.length > 0 ? "flex" : "none";
  for (const iface of otherIfaces) {
    const badge = document.createElement("span");
    badge.className = "down-badge";
    const icon = iface.ifOperStatus === 2 ? "▲" : "◆";
    badge.textContent =
      icon +
      " " +
      (iface.ifAlias || iface.ifDescr || "#" + iface.ifIndex) +
      " · " +
      statusLabel(iface.ifOperStatus) +
      " · " +
      formatSinceChange(device.sysUpTimeTicks, iface.ifLastChange);
    handles.downList.appendChild(badge);
  }
}

function buildStatTile(label, value) {
  const tile = document.createElement("div");
  tile.className = "stat-tile";
  const labelEl = document.createElement("div");
  labelEl.className = "stat-tile-label";
  labelEl.textContent = label;
  tile.appendChild(labelEl);
  const valueEl = document.createElement("div");
  valueEl.className = "stat-tile-value";
  valueEl.textContent = value;
  tile.appendChild(valueEl);
  return tile;
}

// Three tiles, cheap to rebuild wholesale each push (three small text
// nodes, no charts/listeners worth diffing). Utilization has no clean
// backend-provided metric, so it's computed here from data the payload
// already carries: current in+out rate summed over Up interfaces,
// against their summed full-duplex capacity (ifSpeed x 2).
function renderSummary(devices) {
  const container = document.getElementById("summary");
  container.innerHTML = "";

  let upCount = 0;
  let totalCount = 0;
  let minUptimeTicks = null;
  let minUptimeDevice = null;
  let capacityBps = 0;
  let currentBps = 0;

  for (const device of devices) {
    if (!device.reachable) continue;
    if (minUptimeTicks === null || device.sysUpTimeTicks < minUptimeTicks) {
      minUptimeTicks = device.sysUpTimeTicks;
      minUptimeDevice = device.displayName;
    }
    for (const iface of device.interfaces) {
      totalCount++;
      if (iface.ifOperStatus === 1) {
        upCount++;
        capacityBps += iface.ifSpeed * 2;
        const last = iface.history.length > 0 ? iface.history[iface.history.length - 1] : null;
        if (last) currentBps += last.inBps + last.outBps;
      }
    }
  }

  container.appendChild(buildStatTile("Ports up", totalCount > 0 ? upCount + "/" + totalCount : "No data"));

  const uptimeLabel = minUptimeDevice && devices.length > 1 ? "Uptime (" + minUptimeDevice + ")" : "Uptime";
  container.appendChild(
    buildStatTile(uptimeLabel, minUptimeTicks !== null ? formatDuration(minUptimeTicks / 100) : "No data")
  );

  const utilTile = document.createElement("div");
  utilTile.className = "stat-tile";
  const utilLabel = document.createElement("div");
  utilLabel.className = "stat-tile-label";
  utilLabel.textContent = "Traffic utilization";
  utilTile.appendChild(utilLabel);
  const utilValue = document.createElement("div");
  utilValue.className = "stat-tile-value";
  utilTile.appendChild(utilValue);
  if (capacityBps > 0) {
    const pct = (currentBps / capacityBps) * 100;
    utilValue.textContent = pct.toFixed(1) + "%";
    const meter = document.createElement("div");
    meter.className = "meter";
    const fill = document.createElement("div");
    fill.className = "meter-fill" + (pct >= 90 ? " critical" : pct >= 70 ? " warning" : "");
    fill.style.width = Math.min(100, pct) + "%";
    meter.appendChild(fill);
    utilTile.appendChild(meter);
  } else {
    utilValue.textContent = "No data";
  }
  container.appendChild(utilTile);
}

// Aggregates already-fetched status data into a single summary banner —
// not a replacement for Alertmanager (no delivery/rules/silencing),
// just surfacing state the dashboard already knows so the user doesn't
// have to scan every card. Admin-disabled ports (ifAdminStatus !== 1)
// are excluded so intentionally-off ports don't count as "down".
function renderAlertBanner(devices) {
  const banner = document.getElementById("alert-banner");
  let unreachableDevices = 0;
  let downPorts = 0;
  let errorPorts = 0;

  for (const device of devices) {
    if (!device.reachable) {
      unreachableDevices++;
      continue;
    }
    for (const iface of device.interfaces) {
      if (iface.ifAdminStatus === 1 && iface.ifOperStatus === 2) {
        downPorts++;
      }
      if (iface.ifInErrors + iface.ifOutErrors + iface.ifInDiscards + iface.ifOutDiscards > 0) {
        errorPorts++;
      }
    }
  }

  if (unreachableDevices === 0 && downPorts === 0 && errorPorts === 0) {
    banner.className = "alert-banner";
    banner.textContent = "";
    return;
  }

  const parts = [];
  if (unreachableDevices > 0) parts.push(unreachableDevices + " device(s) unreachable");
  if (downPorts > 0) parts.push(downPorts + " port(s) down");
  if (errorPorts > 0) parts.push(errorPorts + " port(s) with errors/discards");

  const severity = unreachableDevices > 0 || downPorts > 0 ? "critical" : "warning";
  const icon = severity === "critical" ? "▲" : "◆"; // triangle / diamond, matches the pills
  banner.className = "alert-banner visible " + severity;
  banner.textContent = icon + " " + parts.join(", ");
}

)JS" R"JS(
// ---- Port status filter (All / Up / Down). Toggles which of the two
// sections (Up cards, Down/other badges) a device shows; unreachable
// devices always show regardless (no ports to filter, and "device is
// down" matters no matter which filter is active).
let currentFilter = "all";

function applyFilter() {
  for (const [, deviceHandles] of deviceSections) {
    if (deviceHandles.el.classList.contains("unreachable")) {
      deviceHandles.el.classList.remove("hidden-by-filter");
      continue;
    }
    const hasUp = deviceHandles.ifaceCards.size > 0;
    const hasDown = deviceHandles.downList.children.length > 0;
    const showUp = currentFilter !== "down" && hasUp;
    const showDown = currentFilter !== "up" && hasDown;

    deviceHandles.grid.style.display = showUp ? "grid" : "none";
    deviceHandles.noUpNote.style.display = currentFilter !== "down" && !hasUp ? "block" : "none";
    deviceHandles.downList.style.display = showDown ? "flex" : "none";
    deviceHandles.el.classList.toggle("hidden-by-filter", !showUp && !showDown);
  }
}

)JS" R"JS(
// ---- Overview page: aggregate traffic (summed across every interface
// of every device, bucketed by exact timestamp) and an errors/discards
// summary table — both derived client-side from the same SSE payload
// the Ports page uses, no backend aggregation endpoint needed.
function computeAggregateTraffic(devices) {
  const byTime = new Map();
  for (const device of devices) {
    if (!device.reachable) continue;
    for (const iface of device.interfaces) {
      for (const point of iface.history) {
        let bucket = byTime.get(point.t);
        if (!bucket) {
          bucket = { t: point.t, inBps: 0, outBps: 0 };
          byTime.set(point.t, bucket);
        }
        bucket.inBps += point.inBps;
        bucket.outBps += point.outBps;
      }
    }
  }
  return Array.from(byTime.values()).sort((a, b) => a.t - b.t);
}

function renderErrorsTable(devices) {
  const wrap = document.getElementById("overview-errors-wrap");
  const rows = [];
  for (const device of devices) {
    if (!device.reachable) continue;
    for (const iface of device.interfaces) {
      const errors = iface.ifInErrors + iface.ifOutErrors;
      const discards = iface.ifInDiscards + iface.ifOutDiscards;
      if (errors + discards > 0) {
        rows.push({
          device: device.displayName,
          port: iface.ifAlias || iface.ifDescr || ("#" + iface.ifIndex),
          errors,
          discards,
          total: errors + discards,
        });
      }
    }
  }
  rows.sort((a, b) => b.total - a.total);

  wrap.innerHTML = ""; // one small table, cheap to rebuild wholesale each push
  if (rows.length === 0) {
    const empty = document.createElement("p");
    empty.className = "errors-empty";
    empty.textContent = "No errors or discards — all clean.";
    wrap.appendChild(empty);
    return;
  }

  const table = document.createElement("table");
  table.className = "errors-table";
  table.innerHTML = "<thead><tr><th>Device</th><th>Port</th><th>Errors</th><th>Discards</th></tr></thead>";
  const tbody = document.createElement("tbody");
  for (const row of rows) {
    const tr = document.createElement("tr");
    const tdDevice = document.createElement("td");
    tdDevice.textContent = row.device;
    const tdPort = document.createElement("td");
    tdPort.textContent = row.port;
    const tdErrors = document.createElement("td");
    tdErrors.textContent = String(row.errors);
    const tdDiscards = document.createElement("td");
    tdDiscards.textContent = String(row.discards);
    tr.append(tdDevice, tdPort, tdErrors, tdDiscards);
    tbody.appendChild(tr);
  }
  table.appendChild(tbody);
  wrap.appendChild(table);
}

function renderOverview(data) {
  const buckets = computeAggregateTraffic(data.devices);
  updateSparkline(overviewInSpark, buckets.map((b) => ({ t: b.t, v: b.inBps })));
  updateSparkline(overviewOutSpark, buckets.map((b) => ({ t: b.t, v: b.outBps })));
  renderErrorsTable(data.devices);
}

)JS" R"JS(
// ---- CSV export: client-side only, built from the last SSE payload —
// the data's already in the browser, so no new server endpoint.
function toCsvField(value) {
  const s = String(value);
  return /[",\n]/.test(s) ? '"' + s.replace(/"/g, '""') + '"' : s;
}

let lastStatusData = null;

function exportCsv() {
  if (!lastStatusData) return;
  const header = [
    "device", "host", "ifIndex", "name", "operStatus", "adminStatus",
    "speedBps", "inOctets", "outOctets", "errors", "discards", "currentInBps", "currentOutBps",
  ];
  const rows = [header];
  for (const device of lastStatusData.devices) {
    for (const iface of device.interfaces) {
      const last = iface.history.length > 0 ? iface.history[iface.history.length - 1] : null;
      rows.push([
        device.displayName,
        device.host,
        iface.ifIndex,
        iface.ifAlias || iface.ifDescr,
        statusLabel(iface.ifOperStatus),
        statusLabel(iface.ifAdminStatus),
        iface.ifSpeed,
        iface.ifInOctets,
        iface.ifOutOctets,
        iface.ifInErrors + iface.ifOutErrors,
        iface.ifInDiscards + iface.ifOutDiscards,
        last ? last.inBps : "",
        last ? last.outBps : "",
      ]);
    }
  }
  const csv = rows.map((row) => row.map(toCsvField).join(",")).join("\r\n");
  const blob = new Blob([csv], { type: "text/csv" });
  const url = URL.createObjectURL(blob);
  const link = document.createElement("a");
  link.href = url;
  link.download = "wiresprite-status-" + new Date().toISOString().replace(/[:.]/g, "-") + ".csv";
  link.click();
  URL.revokeObjectURL(url);
}

)JS" R"JS(
// ---- Top-level wiring: navbar, filter, one-time Overview chart shells,
// keyed device reconciliation driven by a persistent EventSource push
// instead of a client poll timer.
const deviceSections = new Map();

const overviewChartsContainer = document.getElementById("overview-charts");
const overviewInSpark = createSparkline({ color: "var(--series-in)", label: "Total In", formatValue: formatRate });
const overviewOutSpark = createSparkline({ color: "var(--series-out)", label: "Total Out", formatValue: formatRate });
overviewChartsContainer.appendChild(overviewInSpark.wrap);
overviewChartsContainer.appendChild(overviewOutSpark.wrap);

function renderPorts(data) {
  const container = document.getElementById("devices");
  const seen = new Set();
  for (const device of data.devices) {
    seen.add(device.id);
    let handles = deviceSections.get(device.id);
    if (!handles) {
      handles = createDeviceSection();
      deviceSections.set(device.id, handles);
      container.appendChild(handles.el);
    }
    updateDeviceSection(handles, device);
  }
  for (const [id, handles] of deviceSections) {
    if (!seen.has(id)) {
      handles.el.remove();
      deviceSections.delete(id);
    }
  }
  applyFilter();
}

function render(data) {
  lastStatusData = data;
  renderSummary(data.devices);
  renderAlertBanner(data.devices);
  renderPorts(data);
  renderOverview(data);
  document.getElementById("last-updated").textContent = "updated " + new Date().toLocaleTimeString();
}

document.querySelectorAll(".nav-btn").forEach((btn) => {
  btn.addEventListener("click", () => {
    document.querySelectorAll(".nav-btn").forEach((b) => b.classList.toggle("active", b === btn));
    document
      .querySelectorAll(".page")
      .forEach((p) => p.classList.toggle("active", p.id === "page-" + btn.dataset.page));
  });
});

document.getElementById("port-filter").addEventListener("click", (evt) => {
  const btn = evt.target.closest(".segmented-btn");
  if (!btn) return;
  currentFilter = btn.dataset.filter;
  document
    .querySelectorAll("#port-filter .segmented-btn")
    .forEach((el) => el.classList.toggle("active", el === btn));
  applyFilter();
});

document.getElementById("export-csv").addEventListener("click", exportCsv);

// ---- Manual light/dark toggle. THEME_INIT_SCRIPT (web_assets.cpp)
// already applied any stored choice before this script ever runs, so
// this only needs to read that back and handle clicks — never touches
// the CSS values themselves (see the :root[data-theme] blocks).
const SUN_ICON =
  '<svg viewBox="0 0 24 24" width="18" height="18" fill="none" stroke="currentColor" stroke-width="1.6" ' +
  'stroke-linecap="round"><circle cx="12" cy="12" r="4"/><line x1="12" y1="2" x2="12" y2="5"/>' +
  '<line x1="12" y1="19" x2="12" y2="22"/><line x1="2" y1="12" x2="5" y2="12"/>' +
  '<line x1="19" y1="12" x2="22" y2="12"/><line x1="4.6" y1="4.6" x2="6.7" y2="6.7"/>' +
  '<line x1="17.3" y1="17.3" x2="19.4" y2="19.4"/><line x1="4.6" y1="19.4" x2="6.7" y2="17.3"/>' +
  '<line x1="17.3" y1="6.7" x2="19.4" y2="4.6"/></svg>';
const MOON_ICON =
  '<svg viewBox="0 0 24 24" width="18" height="18" fill="none" stroke="currentColor" stroke-width="1.6" ' +
  'stroke-linecap="round" stroke-linejoin="round">' +
  '<path d="M20 14.5A8.5 8.5 0 1 1 9.5 4a6.5 6.5 0 0 0 10.5 10.5z"/></svg>';

function currentTheme() {
  const attr = document.documentElement.getAttribute("data-theme");
  if (attr === "light" || attr === "dark") return attr;
  return window.matchMedia("(prefers-color-scheme: dark)").matches ? "dark" : "light";
}

function applyThemeIcon() {
  // Shows the icon for the theme a click will switch *to*, not the
  // current one — a moon while light (click for dark), a sun while dark.
  document.getElementById("theme-toggle").innerHTML = currentTheme() === "dark" ? SUN_ICON : MOON_ICON;
}

function setTheme(theme) {
  document.documentElement.setAttribute("data-theme", theme);
  localStorage.setItem("wiresprite-theme", theme);
  applyThemeIcon();
}

document.getElementById("theme-toggle").addEventListener("click", () => {
  setTheme(currentTheme() === "dark" ? "light" : "dark");
});
applyThemeIcon();

// ---- Kiosk / wallboard mode: real browser fullscreen (not a CSS-only
// maximize), forces the port filter to "all" so nothing is hidden, and
// slowly auto-scrolls top<->bottom when there's more content than fits
// on one screen. fullscreenchange — not the click handler — owns
// entering/exiting state, since Escape fires that event too and should
// clean up exactly the same way as clicking the button again.
let kioskPreviousFilter = "all";
let kioskWasOnOverview = false;
let kioskScrollTimer = null;
let kioskScrollDirection = 1;

function kioskTick() {
  const maxScroll = document.documentElement.scrollHeight - window.innerHeight;
  if (maxScroll <= 0) return; // everything already fits on one screen
  const next = window.scrollY + kioskScrollDirection;
  if (kioskScrollDirection > 0 && next >= maxScroll) {
    window.scrollTo({ top: maxScroll });
    kioskScrollDirection = -1;
    clearInterval(kioskScrollTimer);
    setTimeout(() => {
      kioskScrollTimer = setInterval(kioskTick, 50);
    }, 3000);
    return;
  }
  if (kioskScrollDirection < 0 && next <= 0) {
    window.scrollTo({ top: 0 });
    kioskScrollDirection = 1;
    clearInterval(kioskScrollTimer);
    setTimeout(() => {
      kioskScrollTimer = setInterval(kioskTick, 50);
    }, 3000);
    return;
  }
  window.scrollTo({ top: next });
}

function stopKioskScroll() {
  if (kioskScrollTimer) {
    clearInterval(kioskScrollTimer);
    kioskScrollTimer = null;
  }
}

function startKioskScroll() {
  stopKioskScroll();
  kioskScrollDirection = 1;
  kioskScrollTimer = setInterval(kioskTick, 50);
}

function showNavPage(pageName) {
  document.querySelectorAll(".nav-btn").forEach((b) => b.classList.toggle("active", b.dataset.page === pageName));
  document.querySelectorAll(".page").forEach((p) => p.classList.toggle("active", p.id === "page-" + pageName));
}

function enterKiosk() {
  document.body.classList.add("kiosk-mode");
  kioskPreviousFilter = currentFilter;
  kioskWasOnOverview = document.getElementById("page-overview").classList.contains("active");

  currentFilter = "all";
  document
    .querySelectorAll("#port-filter .segmented-btn")
    .forEach((el) => el.classList.toggle("active", el.dataset.filter === "all"));
  showNavPage("ports");
  applyFilter();

  window.scrollTo({ top: 0 });
  startKioskScroll();
}

function exitKiosk() {
  document.body.classList.remove("kiosk-mode");
  stopKioskScroll();

  currentFilter = kioskPreviousFilter;
  document
    .querySelectorAll("#port-filter .segmented-btn")
    .forEach((el) => el.classList.toggle("active", el.dataset.filter === currentFilter));
  applyFilter();
  if (kioskWasOnOverview) {
    showNavPage("overview");
  }

  window.scrollTo({ top: 0 });
}

document.getElementById("kiosk-toggle").addEventListener("click", () => {
  if (!document.fullscreenElement) {
    document.documentElement.requestFullscreen().catch(() => {
      // Fullscreen can be denied (permissions policy, embedded iframe,
      // ...) — nothing useful to do beyond not crashing.
    });
  } else {
    document.exitFullscreen();
  }
});

document.addEventListener("fullscreenchange", () => {
  if (document.fullscreenElement) {
    enterKiosk();
  } else {
    exitKiosk();
  }
});

const events = new EventSource("/api/events");
events.onmessage = (evt) => {
  try {
    render(JSON.parse(evt.data));
  } catch (e) {
    document.getElementById("last-updated").textContent = "update failed: " + e.message;
  }
};
events.onerror = () => {
  document.getElementById("last-updated").textContent = "reconnecting…";
};
)JS";

std::string renderLoginPage(bool showError) {
    std::string errorBanner = showError ? "<p class=\"error\">Invalid username or password.</p>" : "";

    return std::string(R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Wiresprite &mdash; sign in</title>
<link rel="icon" type="image/svg+xml" href="/favicon.svg">
)HTML" THEME_INIT_SCRIPT R"HTML(
<link rel="stylesheet" href="/style.css">
</head>
<body>
<main class="login">
  <form class="login-card" method="post" action="/login">
    <h1>)HTML" WIRESPRITE_LOGO_SVG R"HTML( Wiresprite</h1>
    )HTML") +
           errorBanner + R"HTML(
    <label>Username<input type="text" name="username" autocomplete="username" autofocus></label>
    <label>Password<input type="password" name="password" autocomplete="current-password"></label>
    <label class="login-remember"><input type="checkbox" name="remember"> Remember me</label>
    <button type="submit">Sign in</button>
  </form>
</main>
</body>
</html>
)HTML";
}

// The settings page's own script — deliberately separate from
// kAppJs/app.js (which opens an EventSource and drives the live
// dashboard; none of that belongs on a plain settings form) and
// embedded inline in renderSettingsPage rather than served as its own
// route, since it's only ever used by that one page.
const char* const kSettingsJs = R"JS(
function versionLabel(v) {
  return v === "1" ? "SNMP v1" : "SNMP v2c";
}

function createDeviceRow(device) {
  const row = document.createElement("div");
  row.className = "device-row";

  const id = document.createElement("input");
  id.type = "text";
  id.name = "device_id";
  id.placeholder = "id (e.g. core-switch)";
  id.value = device ? device.id : "";
  row.appendChild(id);

  const displayName = document.createElement("input");
  displayName.type = "text";
  displayName.name = "device_display_name";
  displayName.placeholder = "Display name";
  displayName.value = device ? device.displayName : "";
  row.appendChild(displayName);

  const host = document.createElement("input");
  host.type = "text";
  host.name = "device_host";
  host.placeholder = "Host / IP";
  host.value = device ? device.host : "";
  row.appendChild(host);

  const port = document.createElement("input");
  port.type = "number";
  port.name = "device_port";
  port.placeholder = "161";
  port.min = "1";
  port.max = "65535";
  port.value = device ? device.port : "";
  row.appendChild(port);

  const community = document.createElement("input");
  community.type = "text";
  community.name = "device_community";
  community.placeholder = "public";
  community.value = device ? device.community : "";
  row.appendChild(community);

  const version = document.createElement("select");
  version.name = "device_version";
  for (const v of ["2c", "1"]) {
    const opt = document.createElement("option");
    opt.value = v;
    opt.textContent = versionLabel(v);
    if (device && device.version === v) {
      opt.selected = true;
    }
    version.appendChild(opt);
  }
  row.appendChild(version);

  const removeBtn = document.createElement("button");
  removeBtn.type = "button";
  removeBtn.className = "header-button";
  removeBtn.textContent = "Remove";
  removeBtn.addEventListener("click", () => row.remove());
  row.appendChild(removeBtn);

  return row;
}

async function loadSettings() {
  const res = await fetch("/api/config");
  const config = await res.json();

  document.getElementById("auth_username").value = config.auth.username;
  document.getElementById("auth_session_ttl_minutes").value = config.auth.sessionTtlMinutes;
  document.getElementById("auth_remember_me_days").value = config.auth.rememberMeDays;
  document.getElementById("password-hint").textContent = config.auth.passwordSet
    ? "(a password is currently set)"
    : "(no password set — the dashboard is open to anyone)";

  document.getElementById("polling_interval_seconds").value = config.polling.intervalSeconds;
  document.getElementById("polling_timeout_ms").value = config.polling.timeoutMs;
  document.getElementById("polling_retries").value = config.polling.retries;
  document.getElementById("polling_max_concurrent_devices").value = config.polling.maxConcurrentDevices;
  document.getElementById("polling_history_points").value = config.polling.historyPoints;

  document.getElementById("http_listen_address").value = config.http.listenAddress;
  document.getElementById("http_listen_port").value = config.http.listenPort;
  document.getElementById("http_open_browser").checked = config.http.openBrowserOnFirstRun;

  const deviceRows = document.getElementById("device-rows");
  deviceRows.innerHTML = "";
  for (const device of config.devices) {
    deviceRows.appendChild(createDeviceRow(device));
  }
  if (config.devices.length === 0) {
    deviceRows.appendChild(createDeviceRow(null));
  }
}

document.getElementById("add-device").addEventListener("click", () => {
  document.getElementById("device-rows").appendChild(createDeviceRow(null));
});

document.getElementById("settings-form").addEventListener("submit", async (evt) => {
  evt.preventDefault();
  const messageEl = document.getElementById("settings-message");
  messageEl.className = "settings-message";
  messageEl.textContent = "";

  // Unchecked checkboxes are simply absent from FormData, matching the
  // backend's "presence means checked" convention.
  const params = new URLSearchParams();
  for (const [key, value] of new FormData(evt.target).entries()) {
    params.append(key, value);
  }

  try {
    const res = await fetch("/api/config", {
      method: "POST",
      headers: { "Content-Type": "application/x-www-form-urlencoded" },
      body: params.toString(),
    });
    const result = await res.json();
    if (!res.ok) {
      messageEl.textContent = "Error: " + (result.error || "failed to save");
      messageEl.className = "settings-message error";
      return;
    }
    messageEl.textContent = "Saved. Restart wiresprite for device/polling/HTTP changes to take effect.";
    messageEl.className = "settings-message success";
  } catch (e) {
    messageEl.textContent = "Error: " + e.message;
    messageEl.className = "settings-message error";
  }
});

// ---- Manual light/dark toggle — same mechanism as the dashboard's
// (kAppJs); duplicated rather than shared since this page's script is
// already self-contained and it's only ~15 lines.
const SUN_ICON =
  '<svg viewBox="0 0 24 24" width="18" height="18" fill="none" stroke="currentColor" stroke-width="1.6" ' +
  'stroke-linecap="round"><circle cx="12" cy="12" r="4"/><line x1="12" y1="2" x2="12" y2="5"/>' +
  '<line x1="12" y1="19" x2="12" y2="22"/><line x1="2" y1="12" x2="5" y2="12"/>' +
  '<line x1="19" y1="12" x2="22" y2="12"/><line x1="4.6" y1="4.6" x2="6.7" y2="6.7"/>' +
  '<line x1="17.3" y1="17.3" x2="19.4" y2="19.4"/><line x1="4.6" y1="19.4" x2="6.7" y2="17.3"/>' +
  '<line x1="17.3" y1="6.7" x2="19.4" y2="4.6"/></svg>';
const MOON_ICON =
  '<svg viewBox="0 0 24 24" width="18" height="18" fill="none" stroke="currentColor" stroke-width="1.6" ' +
  'stroke-linecap="round" stroke-linejoin="round">' +
  '<path d="M20 14.5A8.5 8.5 0 1 1 9.5 4a6.5 6.5 0 0 0 10.5 10.5z"/></svg>';

function currentTheme() {
  const attr = document.documentElement.getAttribute("data-theme");
  if (attr === "light" || attr === "dark") return attr;
  return window.matchMedia("(prefers-color-scheme: dark)").matches ? "dark" : "light";
}

function applyThemeIcon() {
  document.getElementById("theme-toggle").innerHTML = currentTheme() === "dark" ? SUN_ICON : MOON_ICON;
}

function setTheme(theme) {
  document.documentElement.setAttribute("data-theme", theme);
  localStorage.setItem("wiresprite-theme", theme);
  applyThemeIcon();
}

document.getElementById("theme-toggle").addEventListener("click", () => {
  setTheme(currentTheme() === "dark" ? "light" : "dark");
});
applyThemeIcon();

loadSettings();
)JS";

std::string renderSettingsPage(bool isFirstRun) {
    std::string welcomeBanner = isFirstRun ? R"HTML(
  <div class="settings-welcome">
    <h2>Welcome to Wiresprite &mdash; let's get set up</h2>
    <p>Set a password to protect this dashboard, then add your first switch below. Nothing
    is being polled yet.</p>
  </div>)HTML"
                                            : "";

    return std::string(R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Wiresprite &mdash; settings</title>
<link rel="icon" type="image/svg+xml" href="/favicon.svg">
)HTML" THEME_INIT_SCRIPT R"HTML(
<link rel="stylesheet" href="/style.css">
</head>
<body>
<header>
  <h1>)HTML" WIRESPRITE_LOGO_SVG R"HTML( Wiresprite</h1>
  <div class="header-actions">
    <button type="button" id="theme-toggle" class="header-button" aria-label="Toggle light/dark theme"></button>
    <a href="/" class="header-button">Back to dashboard</a>
  </div>
</header>)HTML") +
           welcomeBanner + R"HTML(
<form class="settings-form" id="settings-form">
  <div class="settings-card">
    <h3>Account</h3>
    <label>Username<input type="text" id="auth_username" name="auth_username" autocomplete="username"></label>
    <label>New password <span class="settings-hint" id="password-hint"></span>
      <input type="password" id="auth_new_password" name="auth_new_password" autocomplete="new-password"
             placeholder="Leave blank to keep current password">
    </label>
    <label>Session length (minutes)
      <input type="number" id="auth_session_ttl_minutes" name="auth_session_ttl_minutes" min="1">
    </label>
    <label>&quot;Remember me&quot; length (days)
      <input type="number" id="auth_remember_me_days" name="auth_remember_me_days" min="1">
    </label>
  </div>

  <div class="settings-card">
    <h3>Devices</h3>
    <div id="device-rows"></div>
    <button type="button" id="add-device" class="header-button">+ Add device</button>
  </div>

  <div class="settings-card">
    <h3>Polling</h3>
    <label>Interval (seconds)<input type="number" id="polling_interval_seconds" name="polling_interval_seconds" min="1"></label>
    <label>Timeout (ms)<input type="number" id="polling_timeout_ms" name="polling_timeout_ms" min="1"></label>
    <label>Retries<input type="number" id="polling_retries" name="polling_retries" min="0"></label>
    <label>Max concurrent devices<input type="number" id="polling_max_concurrent_devices" name="polling_max_concurrent_devices" min="1"></label>
    <label>History points<input type="number" id="polling_history_points" name="polling_history_points" min="2"></label>
  </div>

  <div class="settings-card">
    <h3>HTTP server</h3>
    <label>Listen address<input type="text" id="http_listen_address" name="http_listen_address"></label>
    <label>Listen port<input type="number" id="http_listen_port" name="http_listen_port" min="1" max="65535"></label>
    <label class="settings-checkbox-row">
      <input type="checkbox" id="http_open_browser" name="http_open_browser"> Open browser on first run
    </label>
  </div>

  <div id="settings-message" class="settings-message"></div>
  <button type="submit" class="header-button">Save</button>
</form>
<script>
)HTML" + kSettingsJs + R"HTML(
</script>
</body>
</html>
)HTML";
}

#undef WIRESPRITE_LOGO_SVG
#undef THEME_INIT_SCRIPT

} // namespace wiresprite::web
