"""ESP control-plane TCP acceptor (NDJSON HELLO / role_ack)."""

from __future__ import annotations

import json
import socket
import threading
import time
from datetime import datetime, timezone
from typing import Optional

from controller.node_registry.registry import REGISTRY


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def _send(conn: socket.socket, obj: dict) -> None:
    conn.sendall((json.dumps(obj) + "\n").encode("utf-8"))


def handle_client(conn: socket.socket, addr, auto_role: Optional[str]) -> None:
    peer = f"{addr[0]}:{addr[1]}"
    print(f"[{utc_now()}] connect {peer}", flush=True)
    buf = b""
    device_id: Optional[str] = None
    try:
        conn.settimeout(1.0)
        while True:
            try:
                chunk = conn.recv(4096)
            except socket.timeout:
                continue
            if not chunk:
                break
            buf += chunk
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                line = line.strip()
                if not line:
                    continue
                try:
                    msg = json.loads(line.decode("utf-8"))
                except json.JSONDecodeError as exc:
                    print(f"[{utc_now()}] bad json from {peer}: {exc}", flush=True)
                    continue
                mtype = msg.get("msg")
                body = msg.get("body") or {}
                node_id = msg.get("node_id") or body.get("device_id")
                if mtype == "hello" and node_id:
                    device_id = str(node_id)
                    state = REGISTRY.upsert_hello(device_id, body, peer, conn=conn)
                    role = body.get("role")
                    print(
                        f"[{utc_now()}] HELLO {device_id} role={role} "
                        f"ip={body.get('ip')} usb_devices={body.get('usb_devices')} "
                        f"fw={body.get('fw')} git={body.get('git_sha')}",
                        flush=True,
                    )
                    last = body.get("last_usb_event")
                    if last:
                        print(
                            f"[{utc_now()}]   last_usb_event={json.dumps(last, ensure_ascii=False)}",
                            flush=True,
                        )
                    if auto_role and not state.assigned_role:
                        assign = {
                            "v": 1,
                            "msg": "assign_role",
                            "ts_us": int(time.time() * 1_000_000),
                            "node_id": device_id,
                            "session_id": None,
                            "body": {"role": auto_role},
                        }
                        _send(conn, assign)
                        # registry key may be device_id or device_id@ip
                        for view in REGISTRY.list_views_keyed():
                            if view["node_id"] == device_id and view["ip"] == body.get("ip"):
                                REGISTRY.set_assigned_role(view["registry_key"], auto_role)
                                break
                        else:
                            REGISTRY.set_assigned_role(device_id, auto_role)
                        print(
                            f"[{utc_now()}] assign_role {device_id} -> {auto_role}",
                            flush=True,
                        )
                elif mtype == "role_ack":
                    print(
                        f"[{utc_now()}] role_ack {node_id} role={(body or {}).get('role')}",
                        flush=True,
                    )
                elif mtype == "pong":
                    print(f"[{utc_now()}] pong {node_id}", flush=True)
                else:
                    print(f"[{utc_now()}] msg={mtype} from {node_id or peer}", flush=True)
    finally:
        conn.close()
        REGISTRY.mark_gone(peer)
        print(
            f"[{utc_now()}] disconnect {peer}"
            + (f" ({device_id})" if device_id else ""),
            flush=True,
        )


def serve_forever(host: str, port: int, auto_role: Optional[str] = None) -> None:
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind((host, port))
    sock.listen(8)
    print(
        f"[{utc_now()}] discovery listening on {host}:{port} auto_role={auto_role}",
        flush=True,
    )
    while True:
        conn, addr = sock.accept()
        threading.Thread(
            target=handle_client,
            args=(conn, addr, auto_role),
            daemon=True,
        ).start()
