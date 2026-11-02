# Lab MEASURED — Analyzer-Host × hu-probe AOA (foreign DUT) · 0.0.48

**Claim:** `foreign_dut_huprobe_aoa_lab`  
**Date:** 2026-11-02  
**Topology:** `.88` Analyzer Host 0.0.47 OTG ↔ USB-C ↔ `.89` hu-probe **0.3.34** `usb_gadget=aoa_observe`  
**Raw JSON:** `.tmp/huprobe_aoa_host_test.json` · `.tmp/huprobe_aoa_lab_b_phone.json`

## Setup

| Side | Firmware | Role |
|------|----------|------|
| `.88` | usb-analyzer Host | AOA accessory initiator (`/api/lab/aoa-handshake`) |
| `.89` | hu-probe 0.3.34 | AOA observe gadget + Lab A/B `hypothesis_respond` |

Boot DUT identity (observe): **`18d1:2d00`** · manufacturer Google · product `AOA Observe` · serial `HUPROBE-AOA0`.  
Link arm: `POST /api/connected/link/start` · respond: `POST /api/connected/link/hypothesis_respond`.

**Not claimed:** NBT/HU wire · BCL-on-wire · SAS/RHMI · `aoa_equals_bcl` · accessory bulk echo as HU Connected.

## MEASURED — Lab A (hypothesis_respond=true)

| Check | Result |
|-------|--------|
| Host handshake `profile=lab` | `protocolOk` · `stringsOk=6` · `startOk` · `ok=true` |
| hu-probe counters | `aoa51`/`aoa52`/`aoa53` rise · `aoa51_respond_ok` rises |
| P0 flags | `P0_0`…`P0_D` true after traffic (`P0_B_aoa_entry_callback_seen`) |
| Duration | ~1–52 ms on already-accessory identity (no phone→accessory switch) |

## MEASURED — Lab B (hypothesis_respond=false)

| Check | Result |
|-------|--------|
| Host | HTTP 500 body `ok:false` · `err=aoa51_fail` · `protocolOk=false` |
| Counters | `aoa51` +1 · **`aoa51_respond_ok` unchanged** |

Confirms gated respond path (same Lab B pattern as analyzer-device / hu-probe C1b).

## Attempted / not sticky

| Item | Result |
|------|--------|
| `usbGadget=aoa_phone_like` (04E8:6860 pre-accessory) | NVS save + reboot reported `aoa_phone_like`; after boot status/config back to **`aoa_observe`** — **UNKNOWN** cause (Hub/config race?). Full phone→accessory reenum path **not MEASURED** this session. |
| `/api/lab/aoa-bulk` against hu-probe | Host HTTP 500 — no lab echo pipe claim |

## Verdict

Cross-repo AOA control path works: **Analyzer Host × hu-probe observe responder** Lab A/B MEASURED. Complements in-tree analyzer AOA (0.0.29+) with a foreign gadget stack. Does **not** advance Path B / HU-wire.

## Restore

`.89` restored to analyzer Device 0.0.47 after the session.
