# Soft-Unplug / Soft-USB — claim boundary

**Claim class:** `soft_usb_dplus_pullup`  
**Date:** 2026-11-02 · usb-analyzer 0.0.50

## What soft-unplug **is**

- Gadget-side `tud_disconnect()` / `tud_connect()` (TinyUSB) toggles the **D+ pull-up**.
- Host sees detach → re-enumerate (FS enum).
- Lab APIs: hu-probe/pidrive `POST /api/usb/cycle`; controller `POST /api/v1/lab/usb-cycle`.
- Offline end strategy `soft_detach` on hu-probe uses the same D+ path.

## What it is **not**

| Not claimed | Why |
|-------------|-----|
| VBUS / OTG power cycle | D+ only; 5 V rail stays up |
| Physical cable unplug | Connector / contact bounce absent |
| Host-side port power policy | Analyzer Host does not cut VBUS |
| HU cache / NBT media-hide parity | HU may treat VBUS loss differently from D+ drop |
| Path B L0 PHY evidence | No D+/D− sampler |

## Operator note

Use soft-cycle for **fast lab re-enum** and soak. For power-rail / HU-faithful unplug, use a **controlled VBUS switch** (Path B HW) or a physical cable event — document which.

## Related

- pidrive MessCapture: `soft_usb=D+_pullup_not_VBUS`
- hu-probe: `/api/usb/cycle`, offline `soft_detach`
- Lab UI :8080 → “Soft-cycle device/host”
