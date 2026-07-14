#include "http/web_assets.hpp"

namespace wiresprite::web {

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
  <h1>&#9889; Wiresprite</h1>
  <span id="last-updated">loading&hellip;</span>
  <form method="post" action="/logout" class="logout-form">
    <button type="submit">Log out</button>
  </form>
</header>
<main id="devices"></main>
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
  font-size: 1.3rem;
  font-weight: 600;
}

#last-updated {
  color: var(--ink-muted);
  font-size: 0.85rem;
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

.logout-form {
  margin-left: auto;
}

.logout-form button {
  font: inherit;
  padding: 0.35rem 0.75rem;
  border: 1px solid var(--border);
  border-radius: 6px;
  background: transparent;
  color: inherit;
  cursor: pointer;
}

.logout-form button:hover {
  background: color-mix(in srgb, var(--ink) 8%, transparent);
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
  font-size: 1.2rem;
  text-align: center;
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

const char* const kAppJs = R"JS(const REFRESH_MS = 5000;

function statusLabel(code) {
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

function renderStatusPill(operStatus) {
  const pill = document.createElement("span");
  let cls, icon;
  if (operStatus === 1) {
    cls = "pill-good";
    icon = "●"; // filled circle
  } else if (operStatus === 2) {
    cls = "pill-critical";
    icon = "▲"; // triangle
  } else {
    cls = "pill-warning";
    icon = "◆"; // diamond
  }
  pill.className = "pill " + cls;
  pill.textContent = icon + " " + statusLabel(operStatus);
  return pill;
}

// A small inline SVG line chart: 2px rounded line, ~10%-opacity area
// fill, a crosshair + tooltip on hover, and a direct end-label showing
// the current value. Single series per call — In/Out are two separate
// sparklines side by side rather than one chart with a legend.
function renderSparkline(points, opts) {
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

  if (points.length < 2) {
    const empty = document.createElement("div");
    empty.className = "sparkline-empty";
    empty.textContent = "collecting…";
    wrap.appendChild(empty);
    return wrap;
  }

  const maxValue = Math.max(...points.map((p) => p.v), 1);
  const xAt = (i) => pad + (i / (points.length - 1)) * (width - pad * 2);
  const yAt = (v) => height - pad - (v / maxValue) * (height - pad * 2);

  const svg = document.createElementNS(svgNS, "svg");
  svg.setAttribute("viewBox", "0 0 " + width + " " + height);
  svg.setAttribute("class", "sparkline");
  svg.setAttribute("preserveAspectRatio", "none");

  let linePath = "";
  let areaPath = "M " + xAt(0) + " " + (height - pad) + " ";
  points.forEach((p, i) => {
    const x = xAt(i);
    const y = yAt(p.v);
    linePath += (i === 0 ? "M " : "L ") + x + " " + y + " ";
    areaPath += "L " + x + " " + y + " ";
  });
  areaPath += "L " + xAt(points.length - 1) + " " + (height - pad) + " Z";

  const area = document.createElementNS(svgNS, "path");
  area.setAttribute("d", areaPath);
  area.setAttribute("fill", opts.color);
  area.setAttribute("opacity", "0.1");
  area.setAttribute("stroke", "none");
  svg.appendChild(area);

  const line = document.createElementNS(svgNS, "path");
  line.setAttribute("d", linePath);
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

  const tooltip = document.createElement("div");
  tooltip.className = "sparkline-tooltip";
  tooltip.hidden = true;
  wrap.appendChild(tooltip);

  const endLabel = document.createElement("div");
  endLabel.className = "sparkline-value";
  endLabel.textContent = opts.formatValue(points[points.length - 1].v);
  wrap.appendChild(endLabel);

  function nearestIndex(offsetX) {
    const px = (offsetX / svg.getBoundingClientRect().width) * width;
    let nearest = 0;
    let best = Infinity;
    for (let i = 0; i < points.length; i++) {
      const d = Math.abs(xAt(i) - px);
      if (d < best) {
        best = d;
        nearest = i;
      }
    }
    return nearest;
  }

  function handleMove(evt) {
    const rect = svg.getBoundingClientRect();
    const i = nearestIndex(evt.clientX - rect.left);
    const x = xAt(i);
    crosshair.setAttribute("x1", String(x));
    crosshair.setAttribute("x2", String(x));
    crosshair.setAttribute("visibility", "visible");
    dot.setAttribute("cx", String(x));
    dot.setAttribute("cy", String(yAt(points[i].v)));
    dot.setAttribute("visibility", "visible");

    const when = new Date(points[i].t * 1000);
    tooltip.textContent = opts.formatValue(points[i].v) + " — " + when.toLocaleTimeString();
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

  return wrap;
}

function renderInterfaceCard(iface) {
  const card = document.createElement("div");
  card.className = "iface-card";

  const head = document.createElement("div");
  head.className = "iface-head";
  const name = document.createElement("span");
  name.className = "iface-name";
  name.textContent = iface.ifDescr || ("#" + iface.ifIndex);
  head.appendChild(name);
  head.appendChild(renderStatusPill(iface.ifOperStatus));
  card.appendChild(head);

  const meta = document.createElement("div");
  meta.className = "iface-meta";
  meta.textContent = formatRate(iface.ifSpeed) + " link · admin " + statusLabel(iface.ifAdminStatus).toLowerCase();
  card.appendChild(meta);

  const charts = document.createElement("div");
  charts.className = "iface-charts";
  const inPoints = iface.history.map((h) => ({ t: h.t, v: h.inBps }));
  const outPoints = iface.history.map((h) => ({ t: h.t, v: h.outBps }));
  charts.appendChild(renderSparkline(inPoints, { color: "var(--series-in)", label: "In", formatValue: formatRate }));
  charts.appendChild(
    renderSparkline(outPoints, { color: "var(--series-out)", label: "Out", formatValue: formatRate })
  );
  card.appendChild(charts);

  const errors = iface.ifInErrors + iface.ifOutErrors;
  const discards = iface.ifInDiscards + iface.ifOutDiscards;
  if (errors > 0 || discards > 0) {
    const warn = document.createElement("div");
    warn.className = "iface-warn";
    warn.textContent = errors + " errors, " + discards + " discards";
    card.appendChild(warn);
  }

  return card;
}

function renderDevice(device) {
  const el = document.createElement("section");
  el.className = "device" + (device.reachable ? "" : " unreachable");

  const header = document.createElement("div");
  header.className = "device-header";
  const title = document.createElement("h2");
  title.textContent = device.displayName;
  const host = document.createElement("span");
  host.className = "device-host";
  host.textContent = device.host;
  header.appendChild(title);
  header.appendChild(host);
  el.appendChild(header);

  if (!device.reachable) {
    const err = document.createElement("p");
    err.className = "error";
    err.textContent = device.error || "unreachable";
    el.appendChild(err);
    return el;
  }

  const meta = document.createElement("p");
  meta.className = "meta";
  meta.textContent =
    "uptime " + formatDuration(device.sysUpTimeTicks / 100) + " · " + device.interfaces.length + " interfaces";
  el.appendChild(meta);

  const grid = document.createElement("div");
  grid.className = "interfaces";
  for (const iface of device.interfaces) {
    grid.appendChild(renderInterfaceCard(iface));
  }
  el.appendChild(grid);

  return el;
}

async function refresh() {
  try {
    const res = await fetch("/api/status");
    if (!res.ok) throw new Error("HTTP " + res.status);
    const data = await res.json();
    const container = document.getElementById("devices");
    container.innerHTML = "";
    for (const device of data.devices) {
      container.appendChild(renderDevice(device));
    }
    document.getElementById("last-updated").textContent = "updated " + new Date().toLocaleTimeString();
  } catch (e) {
    document.getElementById("last-updated").textContent = "refresh failed: " + e.message;
  }
}

refresh();
setInterval(refresh, REFRESH_MS);
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
    <h1>&#9889; Wiresprite</h1>
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

} // namespace wiresprite::web
