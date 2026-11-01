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

See [`GATE10_STATUS.md`](GATE10_STATUS.md). Strictly separate:

| Level | Meaning | Status |
|-------|---------|--------|
| 1 | USB protocol/state machine | GO (L1 events) |
| 2 | PHY/controller status registers | MEASURED via `/api/lab/l0-feasibility` (10A) |
| 3 | Raw D+/D− waveform / packets | offline decoder MEASURED; live tap **not yet** |

### E1 Feasibility (10A)

1. Can GPIO19/20 be a sniffer while OTG PHY owns them? → **falsified** as Path A (see evidence)
2. What status bits does DWC HPRT expose? → MEASURED in feasibility JSON
3. External comparator/LA parallel tap without breaking FS eye? → open (Path B PoC)
4. External PHY + probe pads cleaner? → **preferred** after A falsify

### E1.1 GPIO sample under live bus (10A hardening)

**Hypothesis:** CPU GPIO poll on GPIO19/20 while OTG owns the PHY is too slow and/or destructive for FS L0.

**Protocol (dual-ESP OTG loop):**

1. Baseline: Host↔Device MSC, L1 events present
2. Peek-only poll GPIO19/20 (APP CPU) concurrent with MSC (PRO CPU) — **no pin remux**
3. Document: bus stable? meaningful edges? reachable ksps? poll jitter?

**Tool:** `tools/lab_e11_gpio_sample.py` → `/api/lab/e11-gpio-sample`  
**Evidence:** `docs/evidence/LAB_GATE10A_E11_GPIO_SAMPLE.md`  
**Claim:** `gate10a_e11_gpio_sample` — not live FS packets.

### E1.2 Path B GPIO lab substitute (packages 1–4 software)

**Hypothesis:** Free GPIOs can emit known FS line-state sequences for ingest/decoder tests without a soldered tap.

**Protocol:** Host `path-b-gpio` mode `txrx_l0st` → Debian ingest → decode/correlate.

**Tool:** `tools/lab_path_b_gpio.py` · Evidence: `LAB_PATH_B_GPIO_LAB_MEASURED.md`  
**Claim:** `path_b_gpio_lab` — not live OTG PHY.

### E2 Offline decoder (10B–D)

Host unit: `tools/usb_fs_l0` + `tests/host/run_usb_fs_l0_tests.sh` — NRZI, stuff, PID, CRC, packets.

### E3 Correlation (10F)

`tools/lab_l0_correlate.py` — synthetic L0 ↔ L1; live wire correlation after Path B samples exist.

**Must not ship** a “D+/D− oscilloscope” UI that implies Level 3 without MEASURED live packets.

## Track F — Application (future, BMW/NBT)

Out of Gate 0 scope. Any vehicle claim requires:

- control-host baseline first
- explicit session profile
- no transfer of PC-host MEASURED numbers as NBT FACT
