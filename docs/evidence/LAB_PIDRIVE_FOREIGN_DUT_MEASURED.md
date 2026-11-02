# Lab MEASURED — Analyzer-Host × esp32.pidrive (foreign DUT) · 0.0.48

**Claim:** `foreign_dut_pidrive_msc_lab`  
**Date:** 2026-11-02  
**Topology:** `.88` Analyzer Host 0.0.47 OTG ↔ USB-C ↔ `.89` `esp32.pidrive` **pidrive-s3-l3** (`0.4.60-dev`, FAT16 L3)  
**Raw JSON:** `.tmp/pidrive_host_test.json`

## Setup

| Side | Firmware | Role |
|------|----------|------|
| `.88` | usb-analyzer Host 0.0.47 | EspUsbHost MSC initiator |
| `.89` | esp32.pidrive L3 | MSC gadget, large FS + MP3/ID3 |

Flash: OTA via `tools/flash_lab_ota.sh` with `BIN_SRC=…/pidrive-s3-l3/firmware.bin`.  
**Not claimed:** BMW/HU wire · PiDrive audio path · transparent MITM · L0.

## MEASURED

| Check | Result |
|-------|--------|
| Enum / DUT id | `303a:1001` · product Espressif DevKit string · serial `PD0332` |
| SCSI INQUIRY | vendor `PIDRIVE` · product `USB_MEDIA` · rev `0332` |
| Capacity | **32768** blocks × 512 = **16 MiB** |
| Boot sector | `mkfs.fat` · **FAT16** · 32768 secs · sig `55aa` |
| Geometry (device) | `FAT16 L3 16MiB fav0=8M` · 4 slots · fav0 `FAVORIT/01.MP3` 8 MiB @ LBA 81 |
| Lab pattern probe | `ok=true`, **`pattern_ok=false`** (expected — not analyzer lab pattern) |
| MSC READ rate | ~592 kB/s (32 blocks chunk=4) |
| fav0 / fav1 head | magic **`ID3`** · v2.4 · frame `TSSE` / `Lavf61.7.103` (+ Xing/Info) |
| HU-like scan head | 160 blocks from LBA 81 · **81920 B OK** in ~0.98 s |
| Device side-effect | `playGuessCount=1` · host hint `poll-heavy` after scan |

## Verdict

Analyzer Host can drive a **production-like foreign MSC gadget** (large FAT16 + MP3 with ID3 head) over the OTG loop. Useful as Fremdstack-Diversität / Host-Regression — **not** a HU FACT.

## Restore

`.89` restored to analyzer Device 0.0.47 after the session.
