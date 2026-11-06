# Lab MEASURED — Field host matrix (Analyzer MSC gadget) · 2026-11-06

**Claim:** `field_host_matrix_analyzer_msc_gadget_2026_11_06`  
**Date:** 2026-11-06  
**Gadget:** `192.168.178.89` analyzer-node **0.0.52** Device · identity `303a:1001` · pattern `lab_sector_v1` · 512×512 B (=256 KiB)  
**Raw:** [`field_host_matrix_2026-11-06/`](field_host_matrix_2026-11-06/) · summary [`MATRIX_SUMMARY.json`](field_host_matrix_2026-11-06/MATRIX_SUMMARY.json)

**Not claimed:** AOA/Zubehör-Erfolg an diesem Tag · AAIdrive-USB · BCL/Etch on wire · transparent MITM · FAT-Mediaplayer-UI · persistenter Flash-Stick · USB-IF

---

## Topology

```text
USB Host under test ──OTG/C──► ESP Device .89 (MSC lab gadget)
ESP Host .88 (only for phone attempt) — usually idle / no DUT
```

Operator field session (car + home USB ports). Same gadget firmware throughout.

---

## Host matrix (MEASURED)

| Host | USB Host? | Mount / Connect | Δ Reads (session slice) | AOA 51/52/53 | Notes |
|------|-----------|-----------------|-------------------------|--------------|-------|
| BMW NBT / HU | yes | yes | cumulative →76 then →104+ under campaigns | **0** | P-MOUNT SCSI; Soft-Reenum remounts; no UI media |
| TV | yes | yes (on) / unmount (off) | +1 / 4 KiB in timed slice | **0** | mostly TUR in ring |
| Soundbar | yes | yes | +10 then replug +20 / 80 KiB | **0** | Stick scan SCSI |
| Ergometer USB | **no** | waiting / unmount only | 0 | 0 | no host activity while “plugged” |
| FRITZ!Box USB | yes | yes | +37 / ~148 KiB | **0** | NAS-style MSC |
| Windows laptop | yes | yes | large READ surge; **writes 31 / 54272 B** | **0** | Format + text file |
| Android phone → `.88` Host | n/a | **no DUT** | — | — | `boardSourcesVbus=false` |

---

## BMW HU detail

- Identity stayed `303a:1001`; AOA respond on/off did not produce `aoa51`.
- Capture ring SCSI (class-hook CBW): READ10, TUR, PREVENT_ALLOW, MODE_SENSE6, READ_CAPACITY10 (+ `ep_clear_halt`).
- Soft-Reenum → remount + more READs; HID inject API ok, not HU Connected-Apps.
- Host-only APIs on Device correctly 400 (`bmw-hu-stimulus`, `aoa-handshake`, …).

Evidence: `bmw_hu/capture_scsi_ring.json`, `bmw_hu/watch.json`, status snapshots.

---

## Windows laptop detail

- Enumerates and mounts lab MSC; Format produces **WRITE** traffic (`writeOps=31`, `writeBytes=54272` after format/file ops).
- File survives USB unplug/replug while ESP stays powered → **RAM write-cache** (`kWriteCache=64` sectors ≈32 KiB), not Flash.
- After Windows unplug: Device `waiting` / `lastBusEvent=unmount`; write counters retained in RAM.

Evidence: `windows_laptop/after_format_file.json`, `windows_laptop/windows_measure.json`.

---

## Phone / MITM gap

Phone on Analyzer Host `.88`: `dut.present=false`, `no_dut` on enum/AOA/probe.  
Board note: OTG does not source 5 V — self-powered hub required for typical phone device-mode enum.

Evidence: `phone_on_host88_vbus_gap.json`.

---

## Cross-host conclusion (MEASURED)

1. Analyzer `lab_sector_v1` MSC is accepted as a **storage gadget** by HU, TV, Soundbar, FRITZ!Box, Windows.
2. **No host** in this matrix started AOA (`aoa51=0`) against this identity.
3. Visible media UI needs a real FAT/media image (e.g. pidrive) — pattern stick is silent on HU/TV/Soundbar UI.
4. Prior operator FACT (AOA accessory + AAIdrive USB signature) was **not reproduced** on analyzer MSC today — restore hu-probe AOA identity for that path.
