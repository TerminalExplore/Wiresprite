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

const char* const kIndexHtml = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Wiresprite</title>
<link rel="stylesheet" href="/style.css">
</head>
<body>
<header>
  <h1>)HTML" WIRESPRITE_LOGO_SVG R"HTML( Wiresprite</h1>
  <span id="last-updated">loading&hellip;</span>
  <div class="header-actions">
    <button type="button" id="export-csv" class="header-button">Export CSV</button>
    <form method="post" action="/logout" class="logout-form">
      <button type="submit" class="header-button">Log out</button>
    </form>
  </div>
</header>
<nav class="navbar">
  <button type="button" class="nav-btn active" data-page="ports">Ports</button>
  <button type="button" class="nav-btn" data-page="overview">Overview</button>
</nav>
<section id="page-ports" class="page active">
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

.nav-btn {
  font: inherit;
  padding: 0.4rem 0.9rem;
  border: 1px solid var(--border);
  border-radius: 6px;
  background: transparent;
  color: var(--ink-secondary);
  cursor: pointer;
  font-size: 0.85rem;
}

.nav-btn:hover {
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
  color: var(--status-warning);
  font-size: 0.78rem;
  margin-top: 0.5rem;
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
  height: 36px;
  cursor: crosshair;
}

.overview-card .sparkline {
  height: 90px;
}

.sparkline-empty {
  color: var(--ink-muted);
  font-size: 0.78rem;
  height: 36px;
  display: flex;
  align-items: center;
}

.sparkline-value {
  font-size: 0.78rem;
  font-weight: 600;
  font-variant-numeric: tabular-nums;
  margin-top: 0.1rem;
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
  justify-content: center;
  padding-top: 4rem;
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

)JS" R"JS(
// ---- Sparkline: split into create (build DOM + wire listeners once)
// and update (recompute the path/label only) so a live SSE push never
// tears down and re-attaches hover listeners — that churn is what
// would otherwise cause visible flicker/jank at push cadence.
function createSparkline(opts) {
  const width = 140;
  const height = 36;
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

  const area = document.createElementNS(svgNS, "path");
  area.setAttribute("fill", opts.color);
  area.setAttribute("opacity", "0.1");
  area.setAttribute("stroke", "none");
  svg.appendChild(area);

  const line = document.createElementNS(svgNS, "path");
  line.setAttribute("fill", "none");
  line.setAttribute("stroke", opts.color);
  line.setAttribute("stroke-width", "2");
  line.setAttribute("stroke-linecap", "round");
  line.setAttribute("stroke-linejoin", "round");
  svg.appendChild(line);

  const crosshair = document.createElementNS(svgNS, "line");
  crosshair.setAttribute("y1", "0");
  crosshair.setAttribute("y2", String(height));
  crosshair.setAttribute("stroke", "var(--gridline)");
  crosshair.setAttribute("stroke-width", "1");
  crosshair.setAttribute("visibility", "hidden");
  svg.appendChild(crosshair);

  const dot = document.createElementNS(svgNS, "circle");
  dot.setAttribute("r", "3");
  dot.setAttribute("fill", opts.color);
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
  empty.textContent = "collecting…";
  wrap.appendChild(empty);

  const tooltip = document.createElement("div");
  tooltip.className = "sparkline-tooltip";
  tooltip.hidden = true;
  wrap.appendChild(tooltip);

  const endLabel = document.createElement("div");
  endLabel.className = "sparkline-value";
  wrap.appendChild(endLabel);

  const handles = {
    wrap, svg, area, line, crosshair, dot, hit, tooltip, endLabel, empty,
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
  if (!enough) return;

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

  return { el: card, name, pill, meta, inSpark, outSpark, warn, ifOperStatus: null };
}

function updateIfaceCard(handles, iface) {
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
    formatRate(iface.ifSpeed) + " link · admin " + statusLabel(iface.ifAdminStatus).toLowerCase();

  const inPoints = iface.history.map((h) => ({ t: h.t, v: h.inBps }));
  const outPoints = iface.history.map((h) => ({ t: h.t, v: h.outBps }));
  updateSparkline(handles.inSpark, inPoints);
  updateSparkline(handles.outSpark, outPoints);

  const errors = iface.ifInErrors + iface.ifOutErrors;
  const discards = iface.ifInDiscards + iface.ifOutDiscards;
  if (errors > 0 || discards > 0) {
    handles.warn.textContent = errors + " errors, " + discards + " discards";
    handles.warn.style.display = "block";
  } else {
    handles.warn.style.display = "none";
  }

  handles.ifOperStatus = iface.ifOperStatus;
  handles.ifAdminStatus = iface.ifAdminStatus;
}

)JS" R"JS(
// ---- Device section: create once per device id, update in place.
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

  const err = document.createElement("p");
  err.className = "error";
  err.style.display = "none";
  el.appendChild(err);

  const meta = document.createElement("p");
  meta.className = "meta";
  meta.style.display = "none";
  el.appendChild(meta);

  const grid = document.createElement("div");
  grid.className = "interfaces";
  el.appendChild(grid);

  return { el, title, host, err, meta, grid, ifaceCards: new Map() };
}

function updateDeviceSection(handles, device) {
  handles.el.className = "device" + (device.reachable ? "" : " unreachable");
  handles.title.textContent = device.displayName;
  handles.host.textContent = device.host;

  if (!device.reachable) {
    handles.err.textContent = device.error || "unreachable";
    handles.err.style.display = "block";
    handles.meta.style.display = "none";
    handles.grid.style.display = "none";
    return;
  }

  handles.err.style.display = "none";
  handles.meta.style.display = "block";
  handles.meta.textContent =
    "uptime " + formatDuration(device.sysUpTimeTicks / 100) + " · " + device.interfaces.length + " interfaces";
  handles.grid.style.display = "grid";

  const seen = new Set();
  for (const iface of device.interfaces) {
    seen.add(iface.ifIndex);
    let cardHandles = handles.ifaceCards.get(iface.ifIndex);
    if (!cardHandles) {
      cardHandles = createIfaceCard();
      handles.ifaceCards.set(iface.ifIndex, cardHandles);
      handles.grid.appendChild(cardHandles.el);
    }
    updateIfaceCard(cardHandles, iface);
  }
  for (const [ifIndex, cardHandles] of handles.ifaceCards) {
    if (!seen.has(ifIndex)) {
      cardHandles.el.remove();
      handles.ifaceCards.delete(ifIndex);
    }
  }
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
// ---- Port status filter (All / Up / Down). Unreachable devices always
// show (no ports to filter, and "device is down" matters regardless).
let currentFilter = "all";

function applyFilter() {
  for (const [, deviceHandles] of deviceSections) {
    if (deviceHandles.el.classList.contains("unreachable")) {
      deviceHandles.el.classList.remove("hidden-by-filter");
      continue;
    }
    let anyVisible = false;
    for (const [, cardHandles] of deviceHandles.ifaceCards) {
      const matches =
        currentFilter === "all" ||
        (currentFilter === "up" && cardHandles.ifOperStatus === 1) ||
        (currentFilter === "down" && cardHandles.ifOperStatus === 2);
      cardHandles.el.classList.toggle("hidden-by-filter", !matches);
      if (matches) anyVisible = true;
    }
    deviceHandles.el.classList.toggle("hidden-by-filter", !anyVisible);
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
    <button type="submit">Sign in</button>
  </form>
</main>
</body>
</html>
)HTML";
}

#undef WIRESPRITE_LOGO_SVG

} // namespace wiresprite::web
