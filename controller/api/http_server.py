"""HTTP API + SSE + static Web UI (Slice A + Gate 7 capture proxy)."""

from __future__ import annotations

import json
import queue
import threading
import time
import urllib.error
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any, Dict, Optional
from urllib.parse import parse_qs, urlparse

from controller.node_registry.registry import REGISTRY

WEB_ROOT = Path(__file__).resolve().parents[2] / "web"
STARTED = time.time()

# Lab fallbacks when HELLO role is missing / unassigned
LAB_HOST_IP = "192.168.178.88"
LAB_DEVICE_IP = "192.168.178.89"

# Last Gate-8 session offset estimate (controller-local, not hard µs lock)
_LAST_SYNC: Dict[str, Any] = {}


def _post_json(url: str, timeout: float = 5.0) -> Dict[str, Any]:
    req = urllib.request.Request(url, method="POST", data=b"")
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        return json.loads(resp.read().decode("utf-8"))


def run_sync_sample(host_ip: str, device_ip: str, nonce: int) -> Dict[str, Any]:
    """Fire sync-marker on both ESPs; return offset estimate."""
    t0 = time.time()
    h = _post_json(f"http://{host_ip}/api/lab/sync-marker?nonce={nonce}")
    d = _post_json(f"http://{device_ip}/api/lab/sync-marker?nonce={nonce}")
    wall_ms = int((time.time() - t0) * 1000)
    host_ts = int(h.get("timestamp_us") or 0)
    device_ts = int(d.get("timestamp_us") or 0)
    offset = host_ts - device_ts
    return {
        "ok": bool(h.get("ok") and d.get("ok")),
        "nonce": nonce,
        "host_ts_us": host_ts,
        "device_ts_us": device_ts,
        "offset_us": offset,
        "wall_rtt_ms": wall_ms,
        "host": h,
        "device": d,
        "timestamp_domain": "session_offset",
        "claim": "cross_node_offset_estimate",
        "note": "not_hard_us_lock",
    }


def _role_of(view: Dict[str, Any]) -> str:
    return str(view.get("assigned_role") or view.get("role") or "unassigned")


def _node_ip_for_role(role: str) -> Optional[str]:
    role = role.lower().strip()
    for n in REGISTRY.list_views_keyed():
        if _role_of(n) == role and n.get("ip"):
            return str(n["ip"])
    if role == "host":
        return LAB_HOST_IP
    if role == "device":
        return LAB_DEVICE_IP
    return None


def fetch_node_capture(ip: str, since: int = 0, limit: int = 64) -> Dict[str, Any]:
    url = f"http://{ip}/api/capture?since={int(since)}&limit={int(limit)}"
    req = urllib.request.Request(url, method="GET")
    try:
        with urllib.request.urlopen(req, timeout=4) as resp:
            raw = resp.read().decode("utf-8")
            data = json.loads(raw)
            data["_proxy"] = {"ip": ip, "ok": True}
            return data
    except (urllib.error.URLError, TimeoutError, json.JSONDecodeError, OSError) as exc:
        return {
            "version": 1,
            "msg": "capture_batch",
            "events": [],
            "dropped": 0,
            "overflow": False,
            "next_seq": since,
            "layer": "L1_lab",
            "error": str(exc),
            "_proxy": {"ip": ip, "ok": False},
        }


class EventBus:
    def __init__(self) -> None:
        self._subs: list[queue.Queue] = []
        self._lock = threading.Lock()

    def subscribe(self) -> queue.Queue:
        q: queue.Queue = queue.Queue(maxsize=256)
        with self._lock:
            self._subs.append(q)
        return q

    def unsubscribe(self, q: queue.Queue) -> None:
        with self._lock:
            if q in self._subs:
                self._subs.remove(q)

    def publish(self, event_type: str, payload: Dict[str, Any]) -> None:
        with self._lock:
            subs = list(self._subs)
        for q in subs:
            try:
                q.put_nowait((event_type, payload))
            except queue.Full:
                pass


BUS = EventBus()


def _on_registry(event_type: str, payload: Dict[str, Any]) -> None:
    BUS.publish(event_type, payload)


REGISTRY.subscribe(_on_registry)


def send_node_command(node_id: str, msg: str, body: Optional[dict] = None) -> Dict[str, Any]:
    n = REGISTRY.get(node_id)
    if not n or not n.conn:
        return {"ok": False, "err": "node_offline_or_no_socket"}
    envelope = {
        "v": 1,
        "msg": msg,
        "ts_us": int(time.time() * 1_000_000),
        "node_id": n.device_id,
        "session_id": None,
        "body": body or {},
    }
    try:
        n.conn.sendall((json.dumps(envelope) + "\n").encode("utf-8"))
    except OSError as exc:
        return {"ok": False, "err": str(exc)}
    BUS.publish("cmd_ack", {"node_id": n.device_id, "msg": msg, "ok": True})
    return {"ok": True, "sent": envelope}


class Handler(BaseHTTPRequestHandler):
    server_version = "usb-analyzer-controller/0.0.1"

    def log_message(self, fmt: str, *args) -> None:
        print("[http] " + (fmt % args), flush=True)

    def _cors(self) -> None:
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET,POST,OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")

    def _json(self, code: int, obj: Any) -> None:
        data = json.dumps(obj, ensure_ascii=False).encode("utf-8")
        self.send_response(code)
        self._cors()
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def _read_json(self) -> Dict[str, Any]:
        length = int(self.headers.get("Content-Length") or 0)
        if length <= 0:
            return {}
        raw = self.rfile.read(length)
        try:
            return json.loads(raw.decode("utf-8"))
        except json.JSONDecodeError:
            return {}

    def do_OPTIONS(self) -> None:  # noqa: N802
        self.send_response(204)
        self._cors()
        self.end_headers()

    def do_GET(self) -> None:  # noqa: N802
        parsed = urlparse(self.path)
        path = parsed.path.rstrip("/") or "/"

        if path == "/api/v1/health":
            self._json(
                200,
                {
                    "ok": True,
                    "uptime_s": int(time.time() - STARTED),
                    "nodes": len(REGISTRY.list_views_keyed()),
                    "version": "0.0.22",
                },
            )
            return

        if path == "/api/v1/nodes":
            self._json(200, {"nodes": REGISTRY.list_views_keyed()})
            return

        if path.startswith("/api/v1/nodes/"):
            node_id = path[len("/api/v1/nodes/") :]
            n = REGISTRY.get(node_id)
            if not n:
                self._json(404, {"ok": False, "err": "not_found"})
                return
            view = REGISTRY.to_view(n)
            view["last_seen_ms"] = int((time.time() - n.updated_at) * 1000)
            self._json(200, view)
            return

        if path == "/api/v1/stream":
            self._sse()
            return

        if path == "/api/v1/capture":
            qs = parse_qs(parsed.query)
            role = (qs.get("role") or ["host"])[0]
            since = int((qs.get("since") or ["0"])[0] or 0)
            limit = int((qs.get("limit") or ["64"])[0] or 64)
            ip = (qs.get("ip") or [None])[0] or _node_ip_for_role(role)
            if not ip:
                self._json(404, {"ok": False, "err": "no_node_for_role", "role": role})
                return
            batch = fetch_node_capture(ip, since=since, limit=limit)
            batch["role"] = role
            self._json(200, batch)
            return

        if path == "/api/v1/capture/dual":
            qs = parse_qs(parsed.query)
            since_h = int((qs.get("since_host") or ["0"])[0] or 0)
            since_d = int((qs.get("since_device") or ["0"])[0] or 0)
            limit = int((qs.get("limit") or ["48"])[0] or 48)
            host = fetch_node_capture(_node_ip_for_role("host") or LAB_HOST_IP, since_h, limit)
            device = fetch_node_capture(
                _node_ip_for_role("device") or LAB_DEVICE_IP, since_d, limit
            )
            body: Dict[str, Any] = {
                "ok": True,
                "timestamp_domain": "local_per_node",
                "note": "Dual L1_lab lanes — clocks not synchronized",
                "host": host,
                "device": device,
            }
            if _LAST_SYNC:
                body["sync"] = {
                    "offset_us": _LAST_SYNC.get("offset_us"),
                    "residual_us": _LAST_SYNC.get("residual_us"),
                    "samples": _LAST_SYNC.get("samples"),
                    "nonce": _LAST_SYNC.get("nonce"),
                    "timestamp_domain": "session_offset",
                    "claim": "cross_node_offset_estimate",
                }
                body["note"] = (
                    "Dual L1_lab lanes — session offset available "
                    f"(offset_us={_LAST_SYNC.get('offset_us')}); not hard µs lock"
                )
            self._json(200, body)
            return

        # static
        rel = "index.html" if path == "/" else path.lstrip("/")
        file_path = (WEB_ROOT / rel).resolve()
        if not str(file_path).startswith(str(WEB_ROOT.resolve())) or not file_path.is_file():
            self._json(404, {"ok": False, "err": "not_found"})
            return
        data = file_path.read_bytes()
        ctype = "text/html; charset=utf-8"
        if file_path.suffix == ".js":
            ctype = "application/javascript; charset=utf-8"
        elif file_path.suffix == ".css":
            ctype = "text/css; charset=utf-8"
        self.send_response(200)
        self._cors()
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_POST(self) -> None:  # noqa: N802
        parsed = urlparse(self.path)
        path = parsed.path.rstrip("/")
        body = self._read_json()

        if path.endswith("/ping") and "/api/v1/nodes/" in path:
            node_id = path.split("/api/v1/nodes/")[1].split("/")[0]
            self._json(200, send_node_command(node_id, "ping"))
            return

        if path.endswith("/role") and "/api/v1/nodes/" in path:
            node_id = path.split("/api/v1/nodes/")[1].split("/")[0]
            role = body.get("role") or "unassigned"
            result = send_node_command(node_id, "assign_role", {"role": role})
            if result.get("ok"):
                n = REGISTRY.get(node_id)
                if n:
                    for v in REGISTRY.list_views_keyed():
                        if v["node_id"] == n.device_id:
                            REGISTRY.set_assigned_role(v["registry_key"], role)
                            break
            self._json(200 if result.get("ok") else 502, result)
            return

        if path == "/api/v1/sync":
            samples = int(body.get("samples") or 5)
            samples = max(1, min(samples, 21))
            host_ip = _node_ip_for_role("host") or LAB_HOST_IP
            device_ip = _node_ip_for_role("device") or LAB_DEVICE_IP
            base_nonce = int(body.get("nonce") or (time.time_ns() & 0x7FFFFFFF))
            offsets: list[int] = []
            last: Dict[str, Any] = {}
            try:
                for i in range(samples):
                    last = run_sync_sample(host_ip, device_ip, base_nonce + i)
                    if not last.get("ok"):
                        self._json(502, {"ok": False, "err": "sync_marker_fail", **last})
                        return
                    offsets.append(int(last["offset_us"]))
                    time.sleep(0.05)
            except (urllib.error.URLError, TimeoutError, json.JSONDecodeError, OSError) as exc:
                self._json(502, {"ok": False, "err": str(exc)})
                return
            offsets.sort()
            mid = offsets[len(offsets) // 2]
            residual = max(offsets) - min(offsets) if len(offsets) > 1 else 0
            out = {
                "ok": True,
                "nonce": base_nonce,
                "samples": samples,
                "offsets_us": offsets,
                "offset_us": mid,
                "residual_us": residual,
                "host_ts_us": last.get("host_ts_us"),
                "device_ts_us": last.get("device_ts_us"),
                "timestamp_domain": "session_offset",
                "claim": "cross_node_offset_estimate",
                "note": "not_hard_us_lock",
                "last": last,
            }
            _LAST_SYNC.clear()
            _LAST_SYNC.update(out)
            self._json(200, out)
            return

        self._json(404, {"ok": False, "err": "not_found"})

    def _sse(self) -> None:
        self.send_response(200)
        self._cors()
        self.send_header("Content-Type", "text/event-stream; charset=utf-8")
        self.send_header("Cache-Control", "no-cache")
        self.send_header("Connection", "keep-alive")
        self.end_headers()

        q = BUS.subscribe()
        # initial snapshot
        try:
            snap = {"nodes": REGISTRY.list_views_keyed()}
            self.wfile.write(
                f"event: snapshot\ndata: {json.dumps(snap, ensure_ascii=False)}\n\n".encode()
            )
            self.wfile.flush()
            while True:
                try:
                    et, payload = q.get(timeout=15)
                    line = f"event: {et}\ndata: {json.dumps(payload, ensure_ascii=False)}\n\n"
                    self.wfile.write(line.encode("utf-8"))
                    self.wfile.flush()
                except queue.Empty:
                    self.wfile.write(b": keepalive\n\n")
                    self.wfile.flush()
        except (BrokenPipeError, ConnectionResetError):
            pass
        finally:
            BUS.unsubscribe(q)


def serve_forever(host: str, port: int) -> None:
    httpd = ThreadingHTTPServer((host, port), Handler)
    print(f"[http] listening on http://{host}:{port}/  web={WEB_ROOT}", flush=True)
    httpd.serve_forever()
