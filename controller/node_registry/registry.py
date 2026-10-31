"""Shared in-memory node registry for discovery + HTTP API.

Nodes are keyed by stable MAC (preferred), then IP, then device_id.
Hub renames must not create duplicate cards.
"""

from __future__ import annotations

import threading
import time
from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional

# Drop entries not refreshed within this window (HELLO ~5s).
STALE_MS = 45_000


def _norm_mac(mac: Any) -> str:
    if not mac:
        return ""
    return str(mac).replace(":", "").replace("-", "").upper()


@dataclass
class NodeState:
    device_id: str
    last_hello: Dict[str, Any]
    peer: str
    updated_at: float = field(default_factory=time.time)
    assigned_role: Optional[str] = None
    conn: Any = None  # socket, optional — for outbound commands
    registry_key: str = ""


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

    def _key_for(self, device_id: str, body: Dict[str, Any]) -> str:
        mac = _norm_mac(body.get("mac"))
        if len(mac) >= 12:
            return f"mac:{mac}"
        ip = str(body.get("ip") or "").strip()
        if ip:
            return f"ip:{ip}"
        return f"id:{device_id}"

    def _purge_aliases(
        self, keep_key: str, device_id: str, body: Dict[str, Any]
    ) -> List[str]:
        """Remove other entries that are clearly the same physical node."""
        mac = _norm_mac(body.get("mac"))
        ip = str(body.get("ip") or "").strip()
        removed: List[str] = []
        for k, n in list(self._nodes.items()):
            if k == keep_key:
                continue
            b = n.last_hello or {}
            same_mac = mac and _norm_mac(b.get("mac")) == mac
            same_ip = ip and str(b.get("ip") or "") == ip
            same_id = n.device_id == device_id
            if same_mac or same_ip or same_id:
                del self._nodes[k]
                removed.append(k)
        return removed

    def upsert_hello(
        self,
        device_id: str,
        body: Dict[str, Any],
        peer: str,
        conn=None,
    ) -> NodeState:
        with self._lock:
            key = self._key_for(device_id, body)
            removed = self._purge_aliases(key, device_id, body)
            prev = self._nodes.get(key)
            state = NodeState(
                device_id=device_id,
                last_hello=body,
                peer=peer,
                assigned_role=(prev.assigned_role if prev else None),
                conn=conn if conn is not None else (prev.conn if prev else None),
                registry_key=key,
                updated_at=time.time(),
            )
            self._nodes[key] = state
        for k in removed:
            self._emit("node_gone", {"node_id": k, "reason": "alias_merged"})
        self._emit("hello", self.to_view(state, key))
        return state

    def set_assigned_role(self, key: str, role: str) -> None:
        with self._lock:
            n = self._resolve_locked(key)
            if n:
                n.assigned_role = role

    def mark_gone(self, peer: str) -> None:
        gone: List[str] = []
        with self._lock:
            for k, n in list(self._nodes.items()):
                if n.peer == peer:
                    gone.append(k)
                    del self._nodes[k]
        for k in gone:
            self._emit("node_gone", {"node_id": k})

    def _resolve_locked(self, node_id: str) -> Optional[NodeState]:
        if node_id in self._nodes:
            return self._nodes[node_id]
        mac = _norm_mac(node_id)
        if mac:
            mk = f"mac:{mac}"
            if mk in self._nodes:
                return self._nodes[mk]
        for n in self._nodes.values():
            if n.device_id == node_id:
                return n
            if n.registry_key == node_id:
                return n
            if _norm_mac((n.last_hello or {}).get("mac")) == mac and mac:
                return n
            if str((n.last_hello or {}).get("ip") or "") == node_id:
                return n
        return None

    def get(self, node_id: str) -> Optional[NodeState]:
        with self._lock:
            return self._resolve_locked(node_id)

    def prune_stale(self, max_age_ms: int = STALE_MS) -> List[str]:
        now = time.time()
        gone: List[str] = []
        with self._lock:
            for k, n in list(self._nodes.items()):
                age_ms = int((now - n.updated_at) * 1000)
                if age_ms > max_age_ms:
                    gone.append(k)
                    del self._nodes[k]
        for k in gone:
            self._emit("node_gone", {"node_id": k, "reason": "stale"})
        return gone

    def list_views(self) -> List[Dict[str, Any]]:
        self.prune_stale()
        with self._lock:
            return [self.to_view(n, k) for k, n in self._nodes.items()]

    @staticmethod
    def to_view(n: NodeState, key: Optional[str] = None) -> Dict[str, Any]:
        body = n.last_hello or {}
        rk = key or n.registry_key or n.device_id
        return {
            "node_id": n.device_id,
            "registry_key": rk,
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
        return self.list_views()


REGISTRY = NodeRegistry()
