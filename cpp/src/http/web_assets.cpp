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
  <form method="post" action="/logout" class="logout-form">
    <button type="submit">Log out</button>
  </form>
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

.logout-form {
  margin-left: auto;
}

.logout-form button {
  font: inherit;
  padding: 0.35rem 0.75rem;
  border: 1px solid color-mix(in srgb, CanvasText 30%, transparent);
  border-radius: 6px;
  background: transparent;
  color: inherit;
  cursor: pointer;
}

.logout-form button:hover {
  background: color-mix(in srgb, CanvasText 8%, transparent);
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
  border: 1px solid color-mix(in srgb, CanvasText 20%, transparent);
  border-radius: 8px;
  padding: 1.5rem;
}

.login-card h1 {
  margin: 0 0 0.5rem;
  font-size: 1.3rem;
  text-align: center;
}

.login-card label {
  display: flex;
  flex-direction: column;
  gap: 0.3rem;
  font-size: 0.9rem;
}

.login-card input {
  font: inherit;
  padding: 0.4rem 0.5rem;
  border: 1px solid color-mix(in srgb, CanvasText 25%, transparent);
  border-radius: 6px;
  background: Canvas;
  color: CanvasText;
}

.login-card button {
  font: inherit;
  padding: 0.5rem;
  border: none;
  border-radius: 6px;
  background: #2a6df4;
  color: white;
  cursor: pointer;
}

.login-card button:hover {
  background: #1d55c9;
}
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

std::string renderLoginPage(bool showError) {
    std::string errorBanner = showError ? "<p class=\"error\">Invalid username or password.</p>" : "";

    return std::string(R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>snmpmon &mdash; sign in</title>
<link rel="stylesheet" href="/style.css">
</head>
<body>
<main class="login">
  <form class="login-card" method="post" action="/login">
    <h1>snmpmon</h1>
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

} // namespace snmpmon::web
