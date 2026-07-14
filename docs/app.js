// ============================================================================
// Sections below down to "TOP-LEVEL WIRING" are copied verbatim from the real
// dashboard's app.js (cpp/src/http/web_assets.cpp's kAppJs) as of this
// snapshot — every rendering function here just consumes the same JSON shape
// /api/status and /api/events produce, with no assumption baked in about how
// that JSON arrived. Not auto-synced; a manual re-copy is needed if the real
// dashboard's rendering code changes.
// ============================================================================

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

// ifLastChange is in the same TimeTicks epoch as the device's own
// sysUpTimeTicks (RFC2863: "the value of sysUpTime at the time this
// interface entered its current state"), so the time since is just
// the difference between the two, in centiseconds.
function formatSinceChange(sysUpTimeTicks, ifLastChangeTicks) {
  return formatDuration((sysUpTimeTicks - ifLastChangeTicks) / 100);
}

// ---- Sparkline: split into create (build DOM + wire listeners once)
// and update (recompute the path/label only) so a live push never
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

// ---- Overview page: aggregate traffic (summed across every interface
// of every device, bucketed by exact timestamp) and an errors/discards
// summary table — both derived client-side from the same payload the
// Ports page uses, no backend aggregation endpoint needed.
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

// ---- CSV export: client-side only, built from the last rendered
// snapshot — the data's already in the browser, no server round trip.
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
  link.download = "wiresprite-demo-" + new Date().toISOString().replace(/[:.]/g, "-") + ".csv";
  link.click();
  URL.revokeObjectURL(url);
}

// ---- Top-level wiring: navbar, filter, one-time Overview chart shells,
// keyed device reconciliation.
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

// ---- Manual light/dark toggle.
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

// ---- Kiosk / wallboard mode.
let kioskPreviousFilter = "all";
let kioskWasOnOverview = false;
let kioskScrollTimer = null;
let kioskScrollDirection = 1;

function kioskTick() {
  const maxScroll = document.documentElement.scrollHeight - window.innerHeight;
  if (maxScroll <= 0) return;
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
    document.documentElement.requestFullscreen().catch(() => {});
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

// ============================================================================
// DEMO-ONLY: everything below fabricates data in the same shape a real
// wiresprite instance's /api/status and /api/events produce, and calls
// render() on an interval — replacing the real dashboard's
// `new EventSource("/api/events")` wiring, which needs an actual backend
// this static page doesn't have. Simulated traffic is a bounded random walk
// (not independent noise each frame) so the charts show a believable moving
// trend, seeded with continuous, healthy-looking homelab traffic per the
// "cool homelab" brief rather than idle/flat numbers.
// ============================================================================

const TICK_MS = 2000;
const HISTORY_CAP = 40;

function mbps(x) {
  return Math.round(x * 1e6);
}

function clamp(v, lo, hi) {
  return Math.max(lo, Math.min(hi, v));
}

// Deterministic-looking but clearly-fake MAC: the "locally administered"
// bit (the second-least-significant bit of the first octet) is set, which
// is the standard signal that this isn't a real vendor-assigned address —
// appropriate for fabricated demo data rather than borrowing a real OUI.
function fakeMac(seed) {
  const bytes = [0x02];
  let x = seed * 2654435761 + 12345;
  for (let i = 0; i < 5; i++) {
    x = (x * 1103515245 + 12345) & 0xffffffff;
    bytes.push((x >>> 24) & 0xff);
  }
  return bytes.map((b) => b.toString(16).padStart(2, "0")).join(":");
}

let nextIfIndex = 1;

function makePort(alias, speedMbps, baseInMbps, baseOutMbps, opts) {
  opts = opts || {};
  const ifIndex = nextIfIndex++;
  const down = !!opts.down;
  return {
    ifIndex,
    ifDescr: String(ifIndex),
    ifAlias: alias || "",
    ifType: 6,
    ifSpeed: mbps(speedMbps),
    ifAdminStatus: 1,
    ifOperStatus: down ? 2 : 1,
    // Fixed at creation, like a real device's ifLastChange — stays put
    // while the device's sysUpTimeTicks keeps climbing each tick, so the
    // displayed "up Xh"/"Down · Xm" duration grows naturally over time
    // instead of being recomputed from scratch every frame.
    ifLastChange: 0, // patched in by buildDevices() once sysUpTimeTicks is known
    ifInOctets: Math.floor(Math.random() * 5e9),
    ifOutOctets: Math.floor(Math.random() * 5e9),
    ifInErrors: 0,
    ifOutErrors: 0,
    ifInDiscards: 0,
    ifOutDiscards: 0,
    macs: down ? [] : [fakeMac(ifIndex)],
    history: [],
    _baseIn: mbps(baseInMbps),
    _baseOut: mbps(baseOutMbps),
    _curIn: mbps(baseInMbps),
    _curOut: mbps(baseOutMbps),
    _errorProne: !!opts.errorProne,
    _downSinceSec: opts.downSinceSec || 0,
    _upSinceSec: opts.upSinceSec || 0,
  };
}

function makeSpares(count, speedMbps, prefix) {
  const spares = [];
  for (let i = 1; i <= count; i++) {
    spares.push(makePort("", speedMbps, 0.02, 0.02, { upSinceSec: 3600 * (5 + i) }));
  }
  return spares;
}

function makeDevice(id, displayName, host, sysDescr, uptimeDays, ports) {
  const sysUpTimeTicks = Math.round(uptimeDays * 86400 * 100);
  for (const port of ports) {
    const sinceSec = port.ifOperStatus === 2 ? port._downSinceSec || 300 : port._upSinceSec || 3600;
    port.ifLastChange = Math.max(0, sysUpTimeTicks - sinceSec * 100);
  }
  return {
    id,
    displayName,
    host,
    reachable: true,
    error: "",
    sysUpTimeTicks,
    sysDescr,
    interfaces: ports,
  };
}

function buildDevices() {
  const core = makeDevice(
    "core-01",
    "Core • UniFi USW-Pro-Aggregation",
    "192.168.1.2",
    "UniFi USW-Pro-Aggregation, 8x25G SFP28+, FW 6.6.65",
    41.3,
    [
      makePort("Proxmox-Node-1", 25000, 300, 200, { upSinceSec: 86400 * 12 }),
      makePort("Proxmox-Node-2", 25000, 250, 180, { upSinceSec: 86400 * 12 }),
      makePort("TrueNAS-Uplink", 25000, 80, 600, { upSinceSec: 86400 * 30 }),
      makePort("Uplink-to-Rack", 25000, 400, 400, { upSinceSec: 86400 * 30 }),
      makePort("Uplink-to-IoT", 10000, 40, 40, { upSinceSec: 86400 * 30 }),
      makePort("Backup-Target", 10000, 150, 50, { upSinceSec: 86400 * 6 }),
      ...makeSpares(2, 25000),
    ]
  );

  const rack = makeDevice(
    "rack-2.5g",
    "Rack Switch • MikroTik CRS326-24S+2Q+",
    "192.168.1.3",
    "MikroTik CRS326-24S+2Q+, RouterOS 7.15",
    18.7,
    [
      makePort("Uplink-to-Core", 10000, 400, 400, { upSinceSec: 86400 * 18 }),
      makePort("Gaming-Rig", 2500, 15, 8, { upSinceSec: 86400 * 3 }),
      makePort("Plex-Transcode", 2500, 15, 180, { upSinceSec: 86400 * 18 }),
      makePort("Workstation-1", 1000, 10, 10, { upSinceSec: 86400 * 9 }),
      makePort("Workstation-2", 1000, 8, 8, { upSinceSec: 86400 * 9 }),
      makePort("HomeAssistant", 1000, 1, 1, { upSinceSec: 86400 * 18 }),
      makePort("PS5", 1000, 20, 5, { upSinceSec: 86400 * 2 }),
      makePort("Xbox-Series-X", 1000, 18, 4, { upSinceSec: 86400 * 2 }),
      // Deliberately error-prone, to demo the warning/critical error
      // coloring and the Overview page's errors table.
      makePort("NVR-Recorder", 1000, 40, 5, { upSinceSec: 86400 * 18, errorProne: true }),
      makePort("Label-Printer", 1000, 0.05, 0.05, { upSinceSec: 86400 * 18 }),
      ...makeSpares(14, 1000),
    ]
  );

  const iot = makeDevice(
    "iot-vlan",
    "IoT/Guest VLAN • TP-Link TL-SG108E",
    "192.168.1.4",
    "TP-Link TL-SG108E, HW 5.0 FW 1.0.0",
    9.4,
    [
      makePort("Uplink-to-Core", 1000, 40, 40, { upSinceSec: 86400 * 9 }),
      makePort("AP-LivingRoom", 1000, 30, 25, { upSinceSec: 86400 * 9 }),
      makePort("AP-Office", 1000, 20, 15, { upSinceSec: 86400 * 9 }),
      makePort("Camera-Frontdoor", 100, 3, 0.5, { upSinceSec: 86400 * 9 }),
      makePort("Camera-Backyard", 100, 3, 0.5, { upSinceSec: 86400 * 9 }),
      makePort("Smart-Plugs", 100, 0.1, 0.1, { upSinceSec: 86400 * 9 }),
      makePort("Guest-Wifi-AP", 1000, 5, 5, { upSinceSec: 86400 * 4 }),
      // Demonstrates the down-badge state.
      makePort("Spare-Port", 1000, 0, 0, { down: true, downSinceSec: 1800 }),
    ]
  );

  // Demonstrates the unreachable-device state (alert banner, error
  // display) — nothing polled, matching the real dashboard's shape for
  // "device didn't respond."
  const closet = {
    id: "closet-poe",
    displayName: "Closet PoE • Cisco Catalyst 9200L",
    host: "192.168.1.5",
    reachable: false,
    error: "SNMP request to 192.168.1.5:161 timed out after 3 attempt(s)",
    sysUpTimeTicks: 0,
    sysDescr: "",
    interfaces: [],
  };

  return [core, rack, iot, closet];
}

const DEVICES = buildDevices();

// A virtual clock (advanced by tickAll(), not real elapsed time) rather
// than Date.now() for history-point timestamps. The priming loop below
// calls tickAll() several times synchronously — with a real clock, those
// calls would all land in the same wall-clock second, so
// computeAggregateTraffic's bucket-by-exact-timestamp on the Overview
// page would collapse them into too few distinct buckets to draw a line
// (<2 points) even though each individual port already has enough
// points. A virtual clock guarantees distinct, evenly-spaced timestamps
// regardless of how fast ticks actually happen.
let simClockSec = Math.floor(Date.now() / 1000);

function tickTraffic(port) {
  if (port.ifOperStatus !== 1) return; // down ports stay flat
  const jitterIn = Math.max(port._baseIn * 0.18, mbps(0.05));
  const jitterOut = Math.max(port._baseOut * 0.18, mbps(0.05));
  port._curIn = clamp(port._curIn + (Math.random() - 0.5) * jitterIn, port._baseIn * 0.4, Math.min(port.ifSpeed, port._baseIn * 1.8));
  port._curOut = clamp(port._curOut + (Math.random() - 0.5) * jitterOut, port._baseOut * 0.4, Math.min(port.ifSpeed, port._baseOut * 1.8));

  // Keeps the cumulative counters internally consistent with the
  // simulated rate — not read by any renderer directly, but keeps the
  // CSV export's numbers sane if someone inspects them.
  port.ifInOctets += Math.floor((port._curIn / 8) * (TICK_MS / 1000));
  port.ifOutOctets += Math.floor((port._curOut / 8) * (TICK_MS / 1000));

  if (port._errorProne) {
    if (Math.random() < 0.35) port.ifInErrors += 1;
    if (Math.random() < 0.2) port.ifOutErrors += 1;
    if (Math.random() < 0.15) port.ifOutDiscards += 1;
  }

  port.history.push({ t: simClockSec, inBps: Math.round(port._curIn), outBps: Math.round(port._curOut) });
  if (port.history.length > HISTORY_CAP) port.history.shift();
}

function tickAll() {
  simClockSec += TICK_MS / 1000;
  for (const device of DEVICES) {
    if (!device.reachable) continue;
    device.sysUpTimeTicks += TICK_MS / 10; // TimeTicks are centiseconds
    for (const port of device.interfaces) {
      tickTraffic(port);
    }
  }
}

function buildSnapshot() {
  return { devices: DEVICES };
}

// Prime a few ticks synchronously so the very first paint already has a
// moving chart (>=2 history points per port) instead of "No data yet."
for (let i = 0; i < 4; i++) tickAll();
render(buildSnapshot());

setInterval(() => {
  tickAll();
  render(buildSnapshot());
}, TICK_MS);
