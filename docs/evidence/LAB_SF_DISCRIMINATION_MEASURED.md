# Lab MEASURED — SF discrimination · 0.0.50

**Claim:** `sf_discrimination_lab`
**USB alone:** 888.0 kB/s · **TCP alone:** 168.0 kB/s · gap=720.0
**Concurrent USB/TCP:** 914.0 / 242.0 kB/s
**SF warranted (heuristic):** True

## Epistemik

- This measures the Mode-A residual vs USB Soft-Ceiling — contract for SF-0+.
- Does **not** implement STORE_FORWARD rings or prove coex root cause.
- See `docs/STORE_FORWARD.md` gates SF-0…SF-5.

JSON: `LAB_SF_DISCRIMINATION_MEASURED.json`

```json
{
  "ok": true,
  "claim": "sf_discrimination_lab",
  "timestamp_utc": "20261103T075814Z",
  "nodes": {
    "host": "0.0.50",
    "device": "0.0.50"
  },
  "usb_alone_kBs": 888.0,
  "tcp_alone_kBs": 168.0,
  "usb_concurrent_kBs": 914.0,
  "tcp_concurrent_kBs": 242.0,
  "gap_usb_minus_tcp_kBs": 720.0,
  "sf_warranted": true,
  "usb_alone": {
    "ok": true,
    "rate_kBs": 888,
    "duration_ms": 36,
    "err": null
  },
  "tcp_alone": {
    "ok": true,
    "rate_kBs": 168,
    "rate": null,
    "gaps": null,
    "err": null
  },
  "usb_concurrent": {
    "ok": true,
    "rate_kBs": 914,
    "duration_ms": 35,
    "err": null
  },
  "tcp_concurrent": {
    "ok": true,
    "rate_kBs": 242,
    "rate": null,
    "gaps": null,
    "err": null
  },
  "next": "SF-0 Dual PSRAM rings + block-ready (USB task never calls TCP) \u2014 docs/STORE_FORWARD.md",
  "not_claimed": [
    "store_forward_implemented",
    "coex_root_cause",
    "psram_rings",
    "transparent_mitm"
  ]
}
```
