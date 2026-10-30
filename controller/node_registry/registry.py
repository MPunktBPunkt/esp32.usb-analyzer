"""Shared in-memory node registry for discovery + HTTP API."""

from __future__ import annotations

import threading
import time
from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional


@dataclass
class NodeState:
    device_id: str
    last_hello: Dict[str, Any]
    peer: str
    updated_at: float = field(default_factory=time.time)
    assigned_role: Optional[str] = None
    conn: Any = None  # socket, optional — for outbound commands


class NodeRegistry:
    def __init__(self) -> None:
        self._lock = threading.RLock()
        self._nodes: Dict[str, NodeState] = {}
        self._listeners: List[Any] = []  # callables(event_type, payload)

    def subscribe(self, fn) -> None:
        with self._lock:
            self._listeners.append(fn)

    def _emit(self, event_type: str, payload: Dict[str, Any]) -> None:
        for fn in list(self._listeners):
            try:
                fn(event_type, payload)
            except Exception:
                pass

    def upsert_hello(
        self,
        device_id: str,
        body: Dict[str, Any],
        peer: str,
        conn=None,
    ) -> NodeState:
        ip = str(body.get("ip") or "")
        with self._lock:
            key = device_id
            if (
                key in self._nodes
                and self._nodes[key].last_hello.get("ip") not in (None, "", ip)
                and self._nodes[key].last_hello.get("ip") != ip
            ):
                key = f"{device_id}@{ip}"
            prev = self._nodes.get(key)
            state = NodeState(
                device_id=device_id,
                last_hello=body,
                peer=peer,
                assigned_role=(prev.assigned_role if prev else None),
                conn=conn if conn is not None else (prev.conn if prev else None),
            )
            self._nodes[key] = state
        self._emit("hello", self.to_view(state))
        return state

    def set_assigned_role(self, key: str, role: str) -> None:
        with self._lock:
            if key in self._nodes:
                self._nodes[key].assigned_role = role

    def mark_gone(self, peer: str) -> None:
        gone: List[str] = []
        with self._lock:
            for k, n in list(self._nodes.items()):
                if n.peer == peer:
                    gone.append(k)
                    del self._nodes[k]
        for k in gone:
            self._emit("node_gone", {"node_id": k})

    def get(self, node_id: str) -> Optional[NodeState]:
        with self._lock:
            if node_id in self._nodes:
                return self._nodes[node_id]
            for k, n in self._nodes.items():
                if n.device_id == node_id or k.startswith(node_id + "@"):
                    return n
            return None

    def list_views(self) -> List[Dict[str, Any]]:
        with self._lock:
            return [self.to_view(n) for n in self._nodes.values()]

    @staticmethod
    def to_view(n: NodeState) -> Dict[str, Any]:
        body = n.last_hello or {}
        return {
            "node_id": n.device_id,
            "registry_key": None,  # filled below when listing
            "mac": body.get("mac"),
            "ip": body.get("ip"),
            "role": body.get("role"),
            "assigned_role": n.assigned_role,
            "fw": body.get("fw"),
            "git_sha": body.get("git_sha"),
            "wifi_rssi": body.get("wifi_rssi"),
            "free_heap": body.get("free_heap"),
            "usb_devices": body.get("usb_devices"),
            "last_usb_event": body.get("last_usb_event"),
            "tcp_state": body.get("tcp_state"),
            "peer": n.peer,
            "last_seen_ms": int((time.time() - n.updated_at) * 1000),
            "updated_at": n.updated_at,
        }

    def list_views_keyed(self) -> List[Dict[str, Any]]:
        with self._lock:
            out = []
            for k, n in self._nodes.items():
                v = self.to_view(n)
                v["registry_key"] = k
                v["last_seen_ms"] = int((time.time() - n.updated_at) * 1000)
                out.append(v)
            return out


REGISTRY = NodeRegistry()
