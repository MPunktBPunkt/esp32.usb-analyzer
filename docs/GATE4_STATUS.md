# Gate 4 status — USB proxy (slice started in 0.0.15)

**Goal:** USB Host (ESP-A) ⇄ TCP ⇄ USB Device (ESP-B) forwards enumeration + basic Bulk/Control.

## 0.0.15 slice (lab)

| Piece | Status |
|-------|--------|
| Windowed TCP bulk (`?window=N` on `/api/lab/integrity`) | done — raises WiFi rate vs ACK-each |
| MSC READ proxy Host←Device over `:9752` (`kCmdMscReq/Rsp`) | done |
| MSC WRITE proxy Device→Host DUT (`kCmdMscWriteReq/Rsp`) | done (0.0.21) |
| Persistent TCP proxy session + chunk≤16 | done (0.0.23) |
| DUT descriptor snapshot forward (`kCmdDutInfo*`) | done (0.0.23) — **not** USB control MITM |
| Device enable | `POST /api/lab/usb-proxy?enable=1&peer=.88` · `tools/lab_usb_proxy_arm.py` |
| MITM wiring | **required**: Device OTG→PC, Host OTG→**FS DUT** — **not** OTG↔OTG loop |
| Phone as DUT on S3 Host | often **FAIL** (HS-only) — use FS stick |
| USB control / enum re-enumerate as DUT VID/PID | **not yet** |
| Transparency claim | **not claimed** |

## Lab precursor (0.0.12) — OTG loop, not proxy

With Host OTG ↔ Device OTG cable:

| Check | API / tool |
|-------|------------|
| Enum DUT on Host | `/api/status` → `dut` + `otg.devices≥1` |
| MSC READ Host→Device over cable | `POST /api/lab/usb-host-probe` · `tools/lab_usb_loop_probe.py` |
| Pattern integrity on USB | `pattern_ok` (lab_sector_v1) |

**Claim:** direct OTG cable MSC I/O. **Not claimed:** WiFi USB tunnel / MITM.

## Blockers (lab MEASURED)

| Item | Status |
|------|--------|
| Gate 3 TCP integrity | OK (0.0.5+) |
| `.89` MSC on PC | OK when OTG→PC (`303a:1001`) |
| `.88` Host DUT | OK in OTG↔OTG loop (`devices=1`, 0.0.12 probe) |
| USB tunnel framing | design in `PROTOCOL.md` — not wired to Host/Device stacks |

## Prerequisites before coding proxy

1. Plug a Full-Speed DUT into **Host** OTG (`.88`); confirm HELLO `usb_devices≥1`
2. Keep Device (`.89`) enumerated on observer PC **or** use Mode A peer-only first
3. Freeze Gate 3 rate baseline (`tools/lab_perf_sweep.py`)

## Claim discipline

Do **not** claim MITM transparency until observer-effect baseline exists.
