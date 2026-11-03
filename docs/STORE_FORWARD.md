# Store-and-Forward USB Tunnel (planned)

**Status:** architecture building block — **not implemented** (SF-0…SF-5 below)  
**Depends on:** Gate 4 transparent proxy baseline + Gate 5 capture integrity — **present**  
**Lab precursor:** `tools/lab_sf_discrimination.py` → `LAB_SF_DISCRIMINATION_MEASURED` (Mode-A vs USB gap; does not ship rings)  
**Motivation:** USB/WiFi coexistence under Bulk (technology-audit: HYPOTHESIS set; not desk FACT). Multicore scheduling alone does not remove WLAN latency, PSRAM/cache effects, or USB stack deadlines.

### PSRAM detection ≠ PSRAM architecture (v0.0.1 / current `main`)

| Present today | Missing until SF-0+ |
|---------------|---------------------|
| `board_build.arduino.memory_type = qio_qspi` + `BOARD_HAS_PSRAM` (N8R2) | Dual Capture/TX/RX/Playback rings |
| `ESP.getPsramSize()` / `getFreePsram()` in boot log + HELLO | `ps_malloc` / heap_caps ring buffers |
| capability `"psram"` when size &gt; 0 | “block ready” API; USB task never calls TCP |

`ARDUINO_USB_MODE=0` + `ARDUINO_USB_CDC_ON_BOOT=0` is the OTG/UART lab choice — unrelated to whether rings exist. Do **not** treat HELLO `psram_size` as proof of store-and-forward.

## 1. Problem with naïve realtime path

```text
USB event → TCP send → next USB event
```

USB and WiFi share pressure in the same critical path. Under load the node can starve, drop, or freeze (related MEASURED symptoms elsewhere; root cause still UNKNOWN).

## 2. Target idea

Decouple USB from WLAN with PSRAM rings and **block** transport:

```text
ESP-A (host)                         ESP-B (device)
USB ──► Capture Ring (PSRAM)
          │
          ▼
        TX Ring ──► TCP/UDP blocks ──► RX Ring
                                        │
                                        ▼
                                   Playback Ring ──► USB
```

The USB task must not “know” WiFi. It only marks: **block N is ready**.

## 3. Three-stage pipeline

| Stage | Owner | Duty |
|-------|-------|------|
| **1 Capture** | USB / ISR-near task | USB → SRAM descriptor → PSRAM capture ring; no TCP |
| **2 Transport** | Net task | 64–128 KiB framed blocks + windowed ACK + water level |
| **3 Playback** | USB task (peer) | RX/playback ring → USB stack |

### Transport framing (above TCP)

TCP stays for reliability / retransmit / connection state. Application framing still carries:

```text
SESSION
BLOCK seq
  usb_timestamp_us
  payload_len
  crc32
  flags          # DATA | CTRL | MARKER | END
  payload[]
```

Receiver ACKs **blocks**, not USB packets. Prefer sliding window (e.g. `window=8`) over stop-and-wait.

### Water-level feedback

ACK carries buffer pressure, not only `highest_seq`:

```json
{
  "msg": "block_ack",
  "highest_seq": 473,
  "rx_buffer_pct": 72,
  "rx_buffer_free": 187392
}
```

| RX fill | Sender policy |
|---------|---------------|
| &lt; 70 % | NORMAL |
| 70–85 % | SLOW |
| 85–95 % | PAUSE |
| &gt; 95 % | EMERGENCY |

## 4. Dual rings per node (full duplex)

```text
A → B:  Capture_A → TX_A → RX_B → Playback_B
B → A:  Capture_B → TX_B → RX_A → Playback_A
```

Four logical rings (two directions × capture/playback). Physical layout may share one PSRAM heap with quotas.

## 5. Pipeline stage timestamps (analyzer value)

Separate **transport ACK** from **USB completion**:

```text
CAPTURED → TRANSMITTED → RECEIVED → QUEUED → USB_ACCEPTED → USB_COMPLETED
```

Each transition gets a local `timestamp_us` + `timestamp_domain`. UI/timeline can then attribute latency to USB vs WLAN vs peer USB — that is the coex discrimination instrument.

Extend `capture_event` / tunnel meta later with `pipeline_stage` — do not overload App MSC callbacks as wire truth (existing REJECTED claim).

## 6. Operating modes (same hardware)

| Mode | USB | WLAN | Purpose |
|------|-----|------|---------|
| `LIVE` | direct-ish | TCP small buffers | latency / min path |
| `BUFFERED` | PSRAM | TCP | default robust |
| `STORE` | PSRAM | deferred TCP | capture-only measurement |
| `REPLAY` | from PSRAM | optional off | deterministic playback |
| `STORE_FORWARD` | PSRAM ↔ PSRAM | TCP blocks | **preferred experiment mode** |
| `BURST` | timed capture then transfer | phased | isolate USB vs WLAN vs both |
| `FAULT` | PSRAM | TCP | delay/drop/corrupt on blocks |
| `ANALYZE` | capture | TCP to Debian | protocol UI only |

**Burst** phases (example): capture 500 ms → USB quiet → drain PSRAM over TCP → peer load → playback. Enables tests A–D below.

## 7. Comparative test matrix (after implement)

| Test | Setup | Question |
|------|-------|----------|
| A | USB path without WLAN load | USB-only stability |
| B | USB ↔ TCP ↔ USB, tiny buffers | realtime coupling fails how? |
| C | STORE_FORWARD, modest rings | does decoupling help? |
| D | STORE_FORWARD, large rings + core split | residual failures? |

Passing C/D is **INFERENCE**/MEASURED evidence that USB↔WiFi coupling was mitigated — not proof that “Bulk kills WiFi” was the sole cause.

## 8. Fit with current architecture

| Existing piece | Relationship |
|----------------|--------------|
| Mode A direct ESP⇄ESP | Required substrate for STORE_FORWARD data plane |
| Mode B relay | Useful for bring-up; **not** the STORE_FORWARD target |
| Capture ring + batch TCP (Gate 0/5) | Precursor; grow into dual rings + block framing |
| `capture_event.v1` | Keep for Debian observe; add pipeline stages / block refs |
| Debian control / Web API | Selects mode, arms FAULT, shows water levels + stage latency |
| Fault injection (Gate 9) | Apply on **blocks** or stage transitions, after baseline |

**Verdict:** The current design can absorb STORE_FORWARD cleanly as a **data-plane mode** on Mode A, without changing discovery/session control. Do **not** implement before Gate 4 baseline + Gate 5 integrity exist — otherwise we cannot tell buffer architecture from broken proxy.

## 9. Implementation gates (future)

| Gate+ | Prove |
|-------|-------|
| SF-0 | Dual PSRAM rings + “block ready” API; USB task never calls TCP |
| SF-1 | Block framing + sliding window ACK over TCP (one direction) |
| SF-2 | Water-level backpressure MEASURED under synthetic load |
| SF-3 | Full-duplex A⇄B |
| SF-4 | Pipeline stage timestamps in session export / UI |
| SF-5 | BURST / STORE / REPLAY modes + test matrix A–D |

Until then: treat this document as the contract. No claim that STORE_FORWARD “fixes” coexistence.

## 10. Risks / UNKNOWN

| Item | Status |
|------|--------|
| PSRAM bandwidth vs USB FS Bulk | UNKNOWN — measure |
| Max useful ring before host/device timeouts | UNKNOWN — host-specific |
| Whether pause at 85 % still meets USB timing | UNKNOWN |
| DMA buffers must stay in internal RAM | FACT-adjacent (audit quirks) — keep USB DMA off PSRAM |
| Board PSRAM auto-enabled | REJECTED — need correct `memory_type` |

## 11. References

- [`ARCHITECTURE.md`](ARCHITECTURE.md) — Mode A/B, planes  
- [`CAPTURE.md`](CAPTURE.md) — buffering precursor  
- [`EVIDENCE.md`](EVIDENCE.md) — USB↔WiFi coex  
- [`API.md`](API.md) — future mode + water-level UI fields  
- technology-audit: `esp32-s3-usb-wifi-coex`, method `usb-wifi-coex-discrimination`
