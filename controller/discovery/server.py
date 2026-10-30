#!/usr/bin/env python3
"""Gate 1 Debian discovery acceptor.

Listens for NDJSON control messages (HELLO) from analyzer nodes.
Optionally sends assign_role. Does not relay USB data.

Usage:
  python3 controller/discovery/server.py [--host 0.0.0.0] [--port 9750]
"""

from __future__ import annotations

import argparse
import json
import socket
import threading
import time
from dataclasses import dataclass, field
from datetime import datetime, timezone
from typing import Any, Dict, Optional


@dataclass
class NodeState:
    device_id: str
    last_hello: Dict[str, Any]
    peer: str
    updated_at: float = field(default_factory=time.time)
    assigned_role: Optional[str] = None


REGISTRY: Dict[str, NodeState] = {}
REGISTRY_LOCK = threading.Lock()


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def handle_client(conn: socket.socket, addr: tuple[str, int], auto_role: Optional[str]) -> None:
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
                    # Key by device_id; if colliding ids (bug), fall back to ip.
                    reg_key = str(device_id)
                    ip = str(body.get("ip") or addr[0])
                    with REGISTRY_LOCK:
                        if (
                            reg_key in REGISTRY
                            and REGISTRY[reg_key].last_hello.get("ip") not in (None, "", ip)
                            and REGISTRY[reg_key].last_hello.get("ip") != ip
                        ):
                            reg_key = f"{device_id}@{ip}"
                        prev = REGISTRY.get(reg_key)
                        already_assigned = bool(prev and prev.assigned_role)
                        REGISTRY[reg_key] = NodeState(
                            device_id=device_id,
                            last_hello=body,
                            peer=peer,
                            assigned_role=(prev.assigned_role if prev else None),
                        )
                    role = body.get("role")
                    usb_devices = body.get("usb_devices")
                    print(
                        f"[{utc_now()}] HELLO {device_id} role={role} "
                        f"ip={body.get('ip')} usb_devices={usb_devices} "
                        f"fw={body.get('fw')} git={body.get('git_sha')}",
                        flush=True,
                    )
                    last = body.get("last_usb_event")
                    if last:
                        print(
                            f"[{utc_now()}]   last_usb_event={json.dumps(last, ensure_ascii=False)}",
                            flush=True,
                        )
                    if auto_role and not already_assigned:
                        assign = {
                            "v": 1,
                            "msg": "assign_role",
                            "ts_us": int(time.time() * 1_000_000),
                            "node_id": device_id,
                            "session_id": None,
                            "body": {"role": auto_role},
                        }
                        conn.sendall((json.dumps(assign) + "\n").encode("utf-8"))
                        with REGISTRY_LOCK:
                            if device_id in REGISTRY:
                                REGISTRY[device_id].assigned_role = auto_role
                        print(
                            f"[{utc_now()}] assign_role {device_id} -> {auto_role}",
                            flush=True,
                        )
                elif mtype == "role_ack":
                    print(
                        f"[{utc_now()}] role_ack {node_id} "
                        f"role={(body or {}).get('role')}",
                        flush=True,
                    )
                elif mtype == "pong":
                    print(f"[{utc_now()}] pong {node_id}", flush=True)
                else:
                    print(
                        f"[{utc_now()}] msg={mtype} from {node_id or peer}",
                        flush=True,
                    )
    finally:
        conn.close()
        print(
            f"[{utc_now()}] disconnect {peer}"
            + (f" ({device_id})" if device_id else ""),
            flush=True,
        )


def registry_printer(interval_s: float) -> None:
    while True:
        time.sleep(interval_s)
        with REGISTRY_LOCK:
            snap = {
                k: {
                    "device_id": v.device_id,
                    "peer": v.peer,
                    "assigned_role": v.assigned_role,
                    "role": (v.last_hello or {}).get("role"),
                    "ip": (v.last_hello or {}).get("ip"),
                    "usb_devices": (v.last_hello or {}).get("usb_devices"),
                    "fw": (v.last_hello or {}).get("fw"),
                    "updated_at": v.updated_at,
                }
                for k, v in REGISTRY.items()
            }
        print(
            f"[{utc_now()}] registry {json.dumps(snap, ensure_ascii=False)}",
            flush=True,
        )


def main() -> None:
    ap = argparse.ArgumentParser(description="USB Analyzer discovery (Gate 1)")
    ap.add_argument("--host", default="0.0.0.0")
    ap.add_argument("--port", type=int, default=9750)
    ap.add_argument(
        "--auto-role",
        default=None,
        help="If set (host|device), send assign_role after first HELLO",
    )
    ap.add_argument("--registry-interval", type=float, default=30.0)
    args = ap.parse_args()

    threading.Thread(
        target=registry_printer, args=(args.registry_interval,), daemon=True
    ).start()

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind((args.host, args.port))
    sock.listen(8)
    print(
        f"[{utc_now()}] discovery listening on {args.host}:{args.port} "
        f"auto_role={args.auto_role}",
        flush=True,
    )
    while True:
        conn, addr = sock.accept()
        threading.Thread(
            target=handle_client,
            args=(conn, addr, args.auto_role),
            daemon=True,
        ).start()


if __name__ == "__main__":
    main()
