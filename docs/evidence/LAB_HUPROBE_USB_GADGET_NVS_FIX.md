# Lab — hu-probe `usb_gadget` NVS sticky fix · 0.3.35

**Claim:** `huprobe_usb_gadget_nvs_fix`  
**Date:** 2026-11-02

## Fix

- Dedicated NVS namespace `hpgad` / key `mode` for `usbGadget` (write + verify).
- `save()` persists gadget first; legacy `huprobe/usb_gadget` still written.
- `POST /api/config` returns `gadgetNvsOk`; `POST /api/factory-reset` clears esphub+huprobe+hpgad.
- Gadget reboot re-verifies sticky before restart.

## MEASURED (lab `.89` · 2026-11-02)

1. Booted 0.3.35 still had sticky `aoa_observe` from pre-fix NVS.
2. `POST /api/factory-reset` → reboot → `usbGadget=msc`.
3. `POST /api/config` `{usbGadget:msc,enableHub:false}` → `gadgetNvsOk:true`.
4. Reboot → **sticky** `msc` + `enableHub=false`.
5. Switch `aoa_observe` → reboot → **sticky**; switch back `msc` → **sticky**.
6. BMW stimulus `bmw_mount_scan` vs MSC hu-probe: see `LAB_BMW_HU_STIMULUS_HUPROBE_MEASURED.md`.

## Not claimed

Root-cause of pre-0.3.35 silent drop beyond “full/legacy namespace risk”; field boards may still need one factory-reset.
