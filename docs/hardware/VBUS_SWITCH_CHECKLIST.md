# VBUS switch checklist (physical unplug)

**Status:** HW open — Soft-USB ≠ VBUS (see `docs/evidence/LAB_SOFT_UNPLUG_CLAIM.md`).  
**Goal:** Controlled 5 V rail drop on Host OTG port for HU-faithful unplug / Idle-SOF experiments.

## Why

| Mechanism | What Host sees | HU fidelity |
|-----------|----------------|-------------|
| Soft `tud_disconnect` | D+ pull-up drop | Lab-fast; **not** rail loss |
| Cable yank | Contact bounce + VBUS | Uncontrolled |
| **VBUS switch** | Clean rail off/on | Closest desk proxy to HU unplug |

## BOM (minimal)

| Qty | Item | Notes |
|----:|------|-------|
| 1 | High-side load switch / P-FET + gate driver for 5 V OTG VBUS | ≥500 mA FS stick |
| 1 | GPIO from Debian or free ESP pin (not GPIO19/20) | Confirm-gated |
| 1 | Current sense (optional) | Detect stick attach current |
| — | Common ground Host↔switch | Avoid ground loops with Path-B tap |

Do **not** hard-short VBUS. Keep Path-B Hi-Z tap electrically separate from switch control.

## Bring-up steps

1. Verify Host port sources VBUS (many S3 kits do **not** — FACT in EVIDENCE).
2. Insert switch in VBUS only; D+/D− untouched.
3. Confirm soft-reenum still works with switch ON (pass-through).
4. Cycle OFF→ON under `lab_hotplug_soak` / dedicated VBUS tool; compare vs soft-reenum.
5. Archive Evidence with topology tag `vbus_switch` (never `soft_usb`).

## Software hooks (when HW arrives)

- Confirm string required (same pattern as `wire_stimulus_destructive`).
- Prefer Debian GPIO or ESP free pin — document which.
- Status field: `vbus_forced=off|on|unknown`.

## Not claimed until MEASURED

- HU cache / media-hide parity  
- Idle/SOF electrical identity with car  
- Path-B L0 during VBUS events (needs soldered PHY)
