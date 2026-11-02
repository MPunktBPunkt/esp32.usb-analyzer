const logEl = document.getElementById("log");
const nodesEl = document.getElementById("nodes");
const healthEl = document.getElementById("health");
const capHost = document.getElementById("cap-host");
const capDevice = document.getElementById("cap-device");
const capMetaHost = document.getElementById("cap-meta-host");
const capMetaDevice = document.getElementById("cap-meta-device");
const capStats = document.getElementById("cap-stats");

let capPaused = false;
let sinceHost = 0;
let sinceDevice = 0;
const seenHost = new Set();
const seenDevice = new Set();
const allHost = [];
const allDevice = [];
const MAX_ROWS = 120;
const MAX_STORE = 400;

const filters = {
  attach: true,
  enum: true,
  msc: true,
  bot: true,
  proxy: true,
  tcp: true,
  sync: true,
  other: true,
};
let lastSync = null;

function log(line) {
  const t = new Date().toISOString().slice(11, 19);
  logEl.textContent = `[${t}] ${line}\n` + logEl.textContent;
}

function roleOf(n) {
  return n.assigned_role || n.role || "unassigned";
}

function fillLane(role, node) {
  const slot = document.querySelector(`.node-slot[data-role="${role}"]`);
  if (!slot) return;
  if (!node) {
    slot.classList.remove("online");
    slot.innerHTML = "kein Node";
    return;
  }
  slot.classList.add("online");
  slot.innerHTML =
    `<div class="id">${node.node_id}</div>` +
    `<div class="kv">${node.ip || "?"} · fw ${node.fw || "?"} · rssi ${node.wifi_rssi ?? "?"} · usb_devices ${node.usb_devices ?? 0}</div>` +
    `<div class="kv">seen ${node.last_seen_ms ?? "?"} ms ago · ${node.git_sha || ""}</div>`;
}

function renderNodes(nodes) {
  const host = nodes.find((n) => roleOf(n) === "host");
  const device = nodes.find((n) => roleOf(n) === "device");
  fillLane("host", host);
  fillLane("device", device);

  nodesEl.innerHTML = "";
  if (!nodes.length) {
    nodesEl.textContent = "warte auf HELLO…";
    return;
  }
  for (const n of nodes) {
    const id = n.registry_key || n.node_id;
    const card = document.createElement("div");
    card.className = "card";
    card.innerHTML =
      `<div><div class="title">${n.node_id}</div>` +
      `<div class="sub">${n.ip || "?"} · ${n.mac || "?"} · role=${roleOf(n)} · usb=${n.usb_devices ?? 0} · fw ${n.fw || "?"}</div></div>` +
      `<div class="actions">` +
      `<button data-act="ping" data-id="${id}">ping</button>` +
      `<button data-act="role" data-id="${id}" data-role="host">→ host</button>` +
      `<button data-act="role" data-id="${id}" data-role="device">→ device</button>` +
      `</div>`;
    nodesEl.appendChild(card);
  }
}

nodesEl.addEventListener("click", async (ev) => {
  const btn = ev.target.closest("button");
  if (!btn) return;
  const id = btn.dataset.id;
  const act = btn.dataset.act;
  try {
    if (act === "ping") {
      const r = await fetch(`/api/v1/nodes/${encodeURIComponent(id)}/ping`, { method: "POST" });
      log(`ping ${id} → ${await r.text()}`);
    } else if (act === "role") {
      const role = btn.dataset.role;
      const r = await fetch(`/api/v1/nodes/${encodeURIComponent(id)}/role`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ role }),
      });
      log(`role ${id}→${role} → ${await r.text()}`);
      refresh();
    }
  } catch (e) {
    log(`cmd error ${e}`);
  }
});

function filterBucket(et) {
  const t = et || "";
  if (t === "attach" || t === "detach") return "attach";
  if (t === "enum_info") return "enum";
  if (t === "sync_marker") return "sync";
  if (t.startsWith("msc_") && !t.includes("proxy")) return "msc";
  if (t.startsWith("bot_")) return "bot";
  if (t.includes("proxy")) return "proxy";
  if (t === "tcp_bulk") return "tcp";
  return "other";
}

function updateSyncBadge(sync) {
  const badge = document.getElementById("cap-sync-badge");
  if (!badge) return;
  if (!sync || sync.offset_us == null) {
    badge.textContent = "clocks not synchronized";
    return;
  }
  badge.textContent =
    `offset_us≈${sync.offset_us} ±${sync.residual_us ?? "?"} (estimate, not hard µs lock)`;
}

function passesFilter(e) {
  return !!filters[filterBucket(e.event_type)];
}

function fmtEvent(e, lane) {
  const dec = e.decoded || {};
  const cmd = dec.command || "";
  const lba = dec.lba != null ? ` lba=${dec.lba}` : "";
  const n = dec.transfer_length != null ? ` n=${dec.transfer_length}` : "";
  const prod = dec.product ? ` ${dec.product}` : "";
  const note = e.note ? ` ${e.note}` : "";
  let tus = e.timestamp_us || 0;
  let domain = "local";
  // Display-only session align: device lane shifted by last sync offset
  if (lane === "device" && lastSync && lastSync.offset_us != null) {
    tus = tus + Number(lastSync.offset_us);
    domain = "session";
  }
  const t = (tus / 1000).toFixed(1);
  return {
    cls: e.status === "error" ? "err" : e.event_type || "",
    html:
      `<span class="t" title="${domain}">${t}ms</span> ` +
      `<span class="et">${e.event_type || "?"}</span> ` +
      `<span class="cmd">${cmd}</span>${lba}${n}${prod}` +
      `<span class="note">${note}</span>`,
  };
}

const inspectEl = document.getElementById("cap-inspect");

function showInspect(e) {
  if (!inspectEl) return;
  inspectEl.textContent = JSON.stringify(e, null, 2);
}

function recount(list) {
  const c = {};
  for (const e of list) {
    const b = filterBucket(e.event_type);
    c[b] = (c[b] || 0) + 1;
  }
  return c;
}

function renderStats() {
  if (!capStats) return;
  const h = recount(allHost);
  const d = recount(allDevice);
  const fmt = (c) =>
    `a${c.attach || 0} e${c.enum || 0} m${c.msc || 0} b${c.bot || 0} p${c.proxy || 0} t${c.tcp || 0}`;
  capStats.textContent =
    `host n=${allHost.length} [${fmt(h)}] · device n=${allDevice.length} [${fmt(d)}] · filter live`;
}

function rebuildLane(listEl, store, lane) {
  if (!listEl) return;
  listEl.innerHTML = "";
  const shown = store.filter(passesFilter).slice(-MAX_ROWS).reverse();
  for (const e of shown) {
    const row = document.createElement("div");
    const f = fmtEvent(e, lane);
    row.className = `cap-row ${f.cls}`;
    row.innerHTML = f.html;
    row.addEventListener("click", () => {
      document.querySelectorAll(".cap-row.selected").forEach((n) => n.classList.remove("selected"));
      row.classList.add("selected");
      showInspect(e);
    });
    listEl.appendChild(row);
  }
}

function ingest(store, seen, events) {
  let added = false;
  for (const e of events || []) {
    const key = `${e.node_id || ""}:${e.seq}`;
    if (seen.has(key)) continue;
    seen.add(key);
    store.push(e);
    added = true;
  }
  while (store.length > MAX_STORE) store.shift();
  return added;
}

function setMeta(el, batch, role) {
  if (!el) return;
  const px = (batch && batch._proxy) || {};
  const err = batch && batch.error;
  if (err) {
    el.textContent = `${role} ${px.ip || "?"} OFFLINE · ${err}`;
    return;
  }
  el.textContent =
    `${px.ip || "?"} · seq→${batch.next_seq ?? "?"} · dropped ${batch.dropped ?? 0}` +
    (batch.overflow ? " · overflow" : "") +
    ` · ${batch.layer || "L1_lab"}`;
}

async function pollCapture() {
  if (capPaused) return;
  try {
    const q = new URLSearchParams({
      since_host: String(sinceHost),
      since_device: String(sinceDevice),
      limit: "48",
    });
    const j = await (await fetch(`/api/v1/capture/dual?${q}`)).json();
    const host = j.host || {};
    const device = j.device || {};
    if (j.sync) {
      lastSync = j.sync;
      updateSyncBadge(lastSync);
    }
    setMeta(capMetaHost, host, "host");
    setMeta(capMetaDevice, device, "device");
    const ah = ingest(allHost, seenHost, host.events || []);
    const ad = ingest(allDevice, seenDevice, device.events || []);
    if (ah) rebuildLane(capHost, allHost, "host");
    if (ad) rebuildLane(capDevice, allDevice, "device");
    if (ah || ad) renderStats();
    for (const e of host.events || []) {
      if (e.seq > sinceHost) sinceHost = e.seq;
    }
    for (const e of device.events || []) {
      if (e.seq > sinceDevice) sinceDevice = e.seq;
    }
  } catch (e) {
    if (capMetaHost) capMetaHost.textContent = `poll error ${e}`;
  }
}

document.getElementById("cap-filters")?.addEventListener("change", (ev) => {
  const cb = ev.target.closest("input[data-f]");
  if (!cb) return;
  filters[cb.dataset.f] = cb.checked;
  rebuildLane(capHost, allHost, "host");
  rebuildLane(capDevice, allDevice, "device");
  renderStats();
});

document.getElementById("btn-cap-pause")?.addEventListener("click", (ev) => {
  capPaused = !capPaused;
  ev.target.textContent = capPaused ? "Resume" : "Pause";
});

document.getElementById("btn-cap-clear")?.addEventListener("click", () => {
  if (capHost) capHost.innerHTML = "";
  if (capDevice) capDevice.innerHTML = "";
  seenHost.clear();
  seenDevice.clear();
  allHost.length = 0;
  allDevice.length = 0;
  renderStats();
});

document.getElementById("btn-cap-sync")?.addEventListener("click", async () => {
  try {
    const r = await fetch("/api/v1/sync", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ samples: 7 }),
    });
    const j = await r.json();
    lastSync = j;
    updateSyncBadge(j);
    rebuildLane(capHost, allHost, "host");
    rebuildLane(capDevice, allDevice, "device");
    log(
      `sync offset_us≈${j.offset_us} residual=${j.residual_us} samples=${j.samples}`
    );
  } catch (e) {
    log(`sync error ${e}`);
  }
});

async function refresh() {
  try {
    const h = await (await fetch("/api/v1/health")).json();
    healthEl.textContent = `uptime ${h.uptime_s}s · nodes ${h.nodes} · v${h.version}`;
    const j = await (await fetch("/api/v1/nodes")).json();
    renderNodes(j.nodes || []);
  } catch (e) {
    healthEl.textContent = "API offline";
  }
}

function connectSSE() {
  const es = new EventSource("/api/v1/stream");
  es.addEventListener("snapshot", (ev) => {
    const data = JSON.parse(ev.data);
    renderNodes(data.nodes || []);
    log(`snapshot ${(data.nodes || []).length} nodes`);
  });
  es.addEventListener("hello", (ev) => {
    const n = JSON.parse(ev.data);
    log(`HELLO ${n.node_id} role=${roleOf(n)} usb=${n.usb_devices ?? 0}`);
    refresh();
  });
  es.addEventListener("node_gone", (ev) => {
    log(`node_gone ${ev.data}`);
    refresh();
  });
  es.addEventListener("cmd_ack", (ev) => {
    log(`cmd_ack ${ev.data}`);
  });
  es.onerror = () => {
    healthEl.textContent = "SSE reconnect…";
  };
}

const labOut = document.getElementById("lab-action-out");
const labStatus = document.getElementById("lab-node-status");

function showLab(obj) {
  if (labOut) labOut.textContent = typeof obj === "string" ? obj : JSON.stringify(obj, null, 2);
}

async function refreshLabStatus() {
  try {
    const j = await (await fetch("/api/v1/lab/status")).json();
    const hv = j.host?.body?.version ?? j.host?.err ?? "?";
    const hn = j.host?.body?.name ?? "";
    const dv = j.device?.body?.version ?? j.device?.err ?? "?";
    const dn = j.device?.body?.name ?? j.device?.body?.fwType ?? "";
    if (labStatus) {
      labStatus.textContent =
        `host ${j.host?.ip || "?"} ${hn} v${hv} · device ${j.device?.ip || "?"} ${dn} v${dv}`;
    }
    return j;
  } catch (e) {
    if (labStatus) labStatus.textContent = `lab status error ${e}`;
    return null;
  }
}

document.getElementById("btn-lab-refresh")?.addEventListener("click", async () => {
  const j = await refreshLabStatus();
  showLab(j || { ok: false });
  log("lab status refresh");
});

document.getElementById("btn-lab-stimulus")?.addEventListener("click", async () => {
  try {
    showLab("running bmw-hu-stimulus…");
    const r = await fetch("/api/v1/lab/bmw-hu-stimulus", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ profile: "bmw_mount_scan", tur: 32, scan_blocks: 80, file_lba0: 81 }),
    });
    const j = await r.json();
    showLab(j);
    log(`bmw stimulus → ${r.status}`);
    refreshLabStatus();
  } catch (e) {
    showLab(String(e));
    log(`stimulus error ${e}`);
  }
});

async function softCycle(role) {
  try {
    showLab(`soft-cycle ${role}…`);
    const r = await fetch("/api/v1/lab/usb-cycle", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ role, holdMs: 600 }),
    });
    const j = await r.json();
    showLab(j);
    log(`usb-cycle ${role} → ${r.status}`);
  } catch (e) {
    showLab(String(e));
    log(`usb-cycle error ${e}`);
  }
}

document.getElementById("btn-lab-cycle-dev")?.addEventListener("click", () => softCycle("device"));
document.getElementById("btn-lab-cycle-host")?.addEventListener("click", () => softCycle("host"));

refresh();
refreshLabStatus();
connectSSE();
setInterval(refresh, 5000);
setInterval(refreshLabStatus, 8000);
pollCapture();
setInterval(pollCapture, 1500);
renderStats();
