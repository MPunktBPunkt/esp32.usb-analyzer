# Experiments & research tracks

**Status:** Gate 0 planning  
Each experiment must produce: `FACT` / `MEASURED` / `INFERENCE` / `UNKNOWN` / `NEXT TEST`.

## Track A — Bring-up (Gates 1–4)

### A1 Host enumeration

- **Hypothesis:** ESP32-S3 Host stack enumerates a known FS MSC stick.
- **Pass:** descriptors + config visible; VID/PID logged; no claim of MITM yet.
- **Fail modes:** VBUS missing, HS-only device, descriptor > control max.

### A2 Device enumeration

- **Hypothesis:** ESP32-S3 Device MSC (or CDC) enumerates on a Linux PC.
- **Pass:** `lsusb` + SET_CONFIGURATION / mount epoch logged.
- **Note:** Address ≠ Mount (REJECTED confusion).

### A3 TCP sequenced link

- **Hypothesis:** Two nodes exchange monotonic `seq` events with zero gaps for N minutes (Relay OK).
- **Pass:** integrity summary gaps=0; document latency distribution as MEASURED.

### A4 Transparent proxy baseline

- **Hypothesis:** PC Host ↔ ESP-Device ↔ tunnel ↔ ESP-Host ↔ Stick completes enumeration + BOT READ10.
- **Observer test:** compare stick-only vs proxied on same control host (method B4 spirit).
- **Pass:** functional I/O + documented Δ latency; **not** “transparent” as FACT without metrics.
- **0.0.25 slice (MEASURED):** Device→Host DUT MSC bulk selftest byte-exact (`lab_usb_proxy_verify.py`). Optional PC blockdev path when gadget is visible. **Not claimed:** transparency / control MITM.

## Track B — Capture depth (Gates 5–6)

### B1 Class-hook vs App-callback

- Log both `bot_cbw`/`bot_csw` and `tud_msc_read10_cb` on device node.
- **Expected:** App chunks ≠ CBW boundaries (already REJECTED as equality).

### B2 Integrity under load

- Sustained Bulk; force ring pressure; confirm overflow counters increment correctly.

### B3 WiFi coex discrimination

- Follow `usb-wifi-coex-discrimination` method: Power vs Starve vs RF.
- Do not declare root cause from one symptom.

## Track C — Timeline / Diff (Gates 7–8)

### C1 Dual-node timeline UX

- Show local clocks separately; optional Debian receive time.
- **Forbidden:** silent µs alignment.

### C2 Session Diff

- Baseline vs delayed-proxy session on control host.
- Metrics: enum time, first READ, STALL/RESET counts, Δ ms table.

## Track D — Fault injection (Gate 9)

Only after A4 pass.

| Injection | Measure |
|-----------|---------|
| delay 0/1/2/5/10/20/50 ms on DATA | host reaction / recovery |
| drop | retry / reset |
| stall | CLEAR_HALT vs BOT Reset path |
| disconnect / reset | re-enum |

## Track E — D+/D− research (Gate 10)

Strictly separate three visualizations:

| Level | Meaning | Status |
|-------|---------|--------|
| 1 | USB protocol/state machine | GO (from L1 events) |
| 2 | PHY/controller status registers | UNKNOWN → experiment |
| 3 | Raw D+/D− waveform | UNKNOWN → experiment; may need external LA/PHY |

### E1 Feasibility questions

1. Can GPIO19/20 be read while OTG PHY owns them? (expect: no / destructive)
2. What status bits does the DWC/OTG controller expose?
3. Does external comparator/LA parallel tap work without breaking FS eye?
4. Is external PHY + probe pads a cleaner instrument path?

**Must not ship** a “D+/D− oscilloscope” UI that implies Level 3 without MEASURED proof.

## Track F — Application (future, BMW/NBT)

Out of Gate 0 scope. Any vehicle claim requires:

- control-host baseline first
- explicit session profile
- no transfer of PC-host MEASURED numbers as NBT FACT
