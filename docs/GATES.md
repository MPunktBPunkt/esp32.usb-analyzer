# Development gates

Work proceeds gate-by-gate. No large unproven implementation blocks.

Each gate exit needs:

| Field | Content |
|-------|---------|
| FACT | Established truths used |
| MEASURED | Numbers from this gate's tests |
| INFERENCE | Careful conclusions |
| UNKNOWN | Remaining gaps |
| NEXT TEST | Immediate follow-up |

Forbidden exit phrasing: “USB Analyzer works” when only a narrower claim was proven.

---

## Gate 0 — Architecture / Evidence / Interfaces ✅ (this commit)

**Exit criteria**

- [x] Public repo `esp32.usb-analyzer`
- [x] Architecture with Mode A/B and control vs data plane
- [x] technology-audit evidence digest
- [x] Protocol + capture schemas
- [x] Hardware risk register
- [x] Experiment / D+/D− research track
- [x] Hub baseline notes (reuse without hard dependency)

**Proven:** documentation and interfaces only.  
**Not proven:** any USB or TCP behavior.

---

## Gate 1 — ESP Host node 🔧 code ready / lab pending

**Prove:** ESP-A in role `host` detects/enumerates a Full-Speed USB device.

**Minimum artifacts:** logs with VID/PID/config; VBUS notes; health HELLO to Debian optional.

**Implementation:** `firmware/analyzer-node` (`analyzer-s3-host`) + `controller/discovery/server.py`  
**Status doc:** [`GATE1_STATUS.md`](GATE1_STATUS.md)

| | |
|-|-|
| FACT | EspUsbHost 2.x + Arduino-ESP32 ≥ 3.2; S3 Host FS-only; VBUS often missing |
| MEASURED | pending lab flash |
| UNKNOWN | field enum success on our boards |
| NEXT TEST | flash N8R2, plug FS keyboard/stick, record VID:PID |

## Gate 2 — ESP Device node

**Prove:** ESP-B in role `device` enumerates to a PC host as a simple gadget (MSC or CDC).

**Minimum artifacts:** enum epoch (SET_ADDRESS vs SET_CONFIGURATION) correctly labeled.

## Gate 3 — TCP link

**Prove:** Sequenced events ESP⇄ESP (Relay Mode B acceptable) with integrity counters.

**Minimum artifacts:** `gaps`, `tcp_errors`, latency histogram.

## Gate 4 — Proxy

**Prove:** USB Host ⇄ ESP1 ⇄ TCP ⇄ ESP2 ⇄ USB Device forwards enumeration + basic Bulk/Control.

**Minimum artifacts:** observer-effect baseline on control host.  
**Not yet:** fault injection.

## Gate 5 — Capture

**Prove:** Full session with L1 events + integrity summary (`overflow`/`dropped`/`gaps`).

## Gate 6 — Decoder

**Prove:** MSC BOT/SCSI decode from class hooks (not App-callback fiction).

## Gate 7 — Web timeline

**Prove:** Live dual-node timeline + packet inspector + stats. Timestamp domains labeled.

## Gate 8 — Timing / Session Diff

**Prove:** Diff table baseline vs experiment (timing, order, status, errors).

## Gate 9 — Fault injection

**Prove:** delay/drop/duplicate/stall/reset/disconnect as explicit experimental ops with measured host/device reaction.

## Gate 10 — D+/D− PHY research

**Prove or falsify:** Level 2/3 visualization feasibility. Protocol-state UI may exist earlier without claiming wire capture.

---

## Current pointer

**Active gate:** Gate 1 code ready → **lab MEASURED** → then Gate 2.
