"""HTTP API + SSE + static Web UI (Slice A)."""

from __future__ import annotations

import json
import queue
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any, Dict, Optional
from urllib.parse import parse_qs, urlparse

from controller.node_registry.registry import REGISTRY

WEB_ROOT = Path(__file__).resolve().parents[2] / "web"
STARTED = time.time()


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
                    "version": "0.0.1",
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
