const logEl = document.getElementById("log");
const nodesEl = document.getElementById("nodes");
const healthEl = document.getElementById("health");
const streamHost = document.getElementById("stream-host");
const streamDevice = document.getElementById("stream-device");

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

function pushStream(role, text) {
  const el = role === "device" ? streamDevice : streamHost;
  el.textContent = text + "\n" + el.textContent;
}

function connectSSE() {
  const es = new EventSource("/api/v1/stream");
  es.addEventListener("snapshot", (ev) => {
    const data = JSON.parse(ev.data);
    renderNodes(data.nodes || []);
    log(`snapshot ${ (data.nodes || []).length } nodes`);
  });
  es.addEventListener("hello", (ev) => {
    const n = JSON.parse(ev.data);
    log(`HELLO ${n.node_id} role=${roleOf(n)} usb=${n.usb_devices ?? 0}`);
    const role = roleOf(n);
    if (role === "host" || role === "device") {
      pushStream(role, `HELLO usb_devices=${n.usb_devices ?? 0} rssi=${n.wifi_rssi}`);
    }
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

refresh();
connectSSE();
setInterval(refresh, 5000);
