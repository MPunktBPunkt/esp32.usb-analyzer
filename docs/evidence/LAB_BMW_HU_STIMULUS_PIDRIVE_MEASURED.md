# Lab MEASURED — BMW-HU-like stimulus × pidrive L3 · 0.0.49

**Claim:** `bmw_hu_stimulus_lab`  
**Date:** 2026-11-02  
**Host:** Analyzer `.88` **0.0.49** · `POST /api/lab/bmw-hu-stimulus`  
**DUT:** esp32.pidrive L3 on `.89` (FAT16 16 MiB, fav0 @ LBA 81)  
**Profile:** `bmw_mount_scan` · tur=32 · scan_blocks=160 · file_lba0=81  
**Tool:** `tools/lab_bmw_hu_stimulus.py`  
**JSON:** `LAB_BMW_HU_STIMULUS_PIDRIVE_MEASURED.json`

Desk FACT grammar (hu-probe feld 2026-10-19): **P-MOUNT** / **P-MREAD**.

## Host emitted (MEASURED)

| Step | Result |
|------|--------|
| INQUIRY ×1 | ok |
| READ_CAPACITY ×3 | ok · 32768 blocks |
| TUR burst ×32 | turOk=32 · turFail=0 |
| P-MREAD (11 READ10) + scan 160 | readOk=31 · readFail=0 · **109056 B** |
| Duration | **174 ms** |
| Not emitted | MODE_SENSE6 · PREVENT_ALLOW (EspUsbHost API gap) |

## DUT visibility (pidrive counters) — not blind

| Counter delta | Value |
|---------------|------:|
| readCount | **+31** |
| bytesRead | **+109056** (matches Host) |
| bytesBoot / Fat / File | +12800 / +14336 / **+81920** (scan head) |
| bytesDir | 0 (classification / LBA map) |
| host.inquiry / capacity / tur | 1 / 3 / **66** |
| host.hint | `poll-heavy` |
| playGuessDelta | 0 (scan alone ≠ play.guess threshold here) |

## Verdict

Analyzer Host can drive a **BMW-NBT-like MSC mount+scan** sequence; pidrive **sees the same traffic** in SCSI/read counters (byte-exact).  
This is the cross-check method for “are we blind on the gadget?” — **VISIBLE** for this subset.

**Not claimed:** real NBT · HU cache · MODE_SENSE/PREVENT · L0 wire.
