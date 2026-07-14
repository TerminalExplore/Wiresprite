#include "http/web_assets.hpp"

namespace snmpmon::web {

const char* const kIndexHtml = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>snmpmon</title>
<link rel="stylesheet" href="/style.css">
</head>
<body>
<header>
  <h1>snmpmon</h1>
  <span id="last-updated">loading&hellip;</span>
</header>
<main id="devices"></main>
<script src="/app.js"></script>
</body>
</html>
)HTML";

const char* const kStyleCss = R"CSS(:root {
  color-scheme: light dark;
}

body {
  font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
  margin: 0;
  padding: 0 1.5rem 2rem;
  background: Canvas;
  color: CanvasText;
}

header {
  display: flex;
  align-items: baseline;
  gap: 1rem;
  padding: 1.5rem 0 1rem;
}

header h1 {
  margin: 0;
  font-size: 1.4rem;
}

#last-updated {
  color: GrayText;
  font-size: 0.85rem;
}

.device {
  border: 1px solid color-mix(in srgb, CanvasText 20%, transparent);
  border-radius: 8px;
  padding: 1rem 1.25rem;
  margin-bottom: 1rem;
}

.device.unreachable {
  border-color: #d33;
}

.device h2 {
  margin: 0 0 0.5rem;
  font-size: 1.1rem;
}

.device .meta {
  color: GrayText;
  font-size: 0.85rem;
  margin: 0 0 0.75rem;
}

.device .error {
  color: #d33;
}

table {
  width: 100%;
  border-collapse: collapse;
  font-size: 0.9rem;
}

th, td {
  text-align: left;
  padding: 0.35rem 0.5rem;
  border-bottom: 1px solid color-mix(in srgb, CanvasText 12%, transparent);
}

.status-up { color: #2a2; font-weight: 600; }
.status-down { color: #d33; font-weight: 600; }
.status-testing, .status-unknown { color: #b80; font-weight: 600; }
)CSS";

const char* const kAppJs = R"JS(const REFRESH_MS = 5000;

function statusLabel(code) {
  switch (code) {
    case 1: return "up";
    case 2: return "down";
    case 3: return "testing";
    default: return "unknown";
  }
}

function formatBytes(n) {
  const units = ["B", "KB", "MB", "GB", "TB"];
  let i = 0;
  let value = n;
  while (value >= 1024 && i < units.length - 1) {
    value /= 1024;
    i++;
  }
  return value.toFixed(value < 10 && i > 0 ? 1 : 0) + " " + units[i];
}

function escapeHtml(s) {
  const div = document.createElement("div");
  div.textContent = s;
  return div.innerHTML;
}

function renderDevice(device) {
  const el = document.createElement("section");
  el.className = "device" + (device.reachable ? "" : " unreachable");

  const header = document.createElement("h2");
  header.textContent = device.displayName + " (" + device.host + ")";
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
  meta.textContent = "uptime: " + Math.floor(device.sysUpTimeTicks / 100) + "s";
  el.appendChild(meta);

  const table = document.createElement("table");
  const thead = document.createElement("thead");
  thead.innerHTML = "<tr><th>Port</th><th>Status</th><th>Speed</th><th>In</th><th>Out</th><th>Errors</th></tr>";
  table.appendChild(thead);

  const tbody = document.createElement("tbody");
  for (const iface of device.interfaces) {
    const row = document.createElement("tr");
    row.innerHTML =
      "<td>" + escapeHtml(iface.ifDescr || ("#" + iface.ifIndex)) + "</td>" +
      "<td class=\"status-" + statusLabel(iface.ifOperStatus) + "\">" + statusLabel(iface.ifOperStatus) + "</td>" +
      "<td>" + formatBytes(iface.ifSpeed / 8) + "/s</td>" +
      "<td>" + formatBytes(iface.ifInOctets) + "</td>" +
      "<td>" + formatBytes(iface.ifOutOctets) + "</td>" +
      "<td>" + (iface.ifInErrors + iface.ifOutErrors) + "</td>";
    tbody.appendChild(row);
  }
  table.appendChild(tbody);
  el.appendChild(table);

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

} // namespace snmpmon::web
