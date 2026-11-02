# Lab MEASURED — hu-probe `usb_gadget` NVS not sticky · 0.0.49

**Claim:** `huprobe_usb_gadget_nvs_lab`  
**Date:** 2026-11-02 · DUT `.89` hu-probe 0.3.34

## MEASURED

1. `POST /api/config` with `usbGadget=msc` (and `aoa_phone_like` earlier) updates **RAM** (`GET /api/config` readback OK).  
2. `POST /api/connected/gadget/reboot` reports saved value.  
3. After reboot, both status `usb_gadget` and config are again **`aoa_observe`**; `enableHub` returns to **true** even when saved false.  
4. Reproduced with Hub disabled in RAM + 5 s settle before save — still reverts.  
5. OTA does not wipe NVS; no HTTP factory-reset endpoint found.

## Impact

- Blocks lab MSC regression and Analyzer-Host BMW-MSC stimulus against hu-probe.  
- Explains earlier `aoa_phone_like` “not sticky” finding (0.0.48).  
- Field/lab operators can believe they switched gadget mode when they did not.

## NEXT

- Fix ConfigStore save/load for `usb_gadget` (verify Preferences write result; avoid Hub overwrite; optional `/api/factory-reset`).  
- UART `nvs erase` once to recover this board to default `msc`.  
- Re-run `lab_bmw_hu_stimulus.py --dut huprobe` on MSC boot.

**Not claimed:** root-cause (NVS full vs Hub vs save bug) beyond “does not stick across reboot”.
