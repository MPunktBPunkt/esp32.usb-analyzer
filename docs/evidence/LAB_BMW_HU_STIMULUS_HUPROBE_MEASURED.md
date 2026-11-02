# Lab MEASURED — BMW-HU-like stimulus × hu-probe · 0.0.49

**Claim:** `bmw_hu_stimulus_lab` (+ gadget-mode / NVS findings)  
**Date:** 2026-11-02  
**Host:** Analyzer `.88` **0.0.49**  
**DUT:** hu-probe **0.3.34** on `.89`  
**Tool:** `tools/lab_bmw_hu_stimulus.py`

## A — Intended: MSC Golden Path

Repeated attempts to set `usbGadget=msc` (JSON + form `plain=`), with `enableHub=false` / `autoUpdate=false`, wait, double-save, then `gadget/reboot`:

| Before reboot (RAM) | After reboot (NVS load) |
|---------------------|-------------------------|
| `usbGadget=msc`, `enableHub=false` | **`usb_gadget=aoa_observe`**, `enableHub=true` |

OTA of hu-probe does **not** clear NVS. No public factory-reset HTTP API.  
→ **MEASURED defect / lab blocker:** MSC boot mode is **not sticky** on this unit; BMW-MSC stimulus against hu-probe Golden Path **could not be run**.

## B — Actual boot: `aoa_observe` (18d1:2d00)

Host ran `bmw_mount` (tur=8) against AOA Observe DUT:

| Host | DUT |
|------|-----|
| inquiryOk=0 · capacityOk=0 · turFail=8 · readFail=11 · bytes=0 | read_count **unchanged 0** · tur_hz=0 |

## Verdict

1. **Cross-check method works:** wrong plane (AOA vs MSC) → Host MSC stimulus fails **and** hu-probe shows **zero** MSC reads — not “silent blindness”, but **correct empty counters**.  
2. **hu-probe gap:** cannot enter MSC for controlled BMW-like Host tests until NVS `usb_gadget` persistence / Hub-revert is fixed (or NVS erased via UART).  
3. This matches the field worry (“warten wir auf HU-Zugriffe, die nie ankommen?”): without a known Host stimulus, zero counters are ambiguous; **with** Analyzer Host stimulus, zero means *no MSC traffic*, not *blind hooks*.

**Not claimed:** real NBT AOA/BCL · MSC Golden Path on this board this session.
