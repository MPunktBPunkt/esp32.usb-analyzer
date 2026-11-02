# Lab MEASURED — BMW-HU-like stimulus × huprobe · 0.0.50

**Claim:** `bmw_hu_stimulus_lab`
**Profile:** `bmw_mount_scan` · tur=32 · scan_blocks=160 · file_lba0=9
**Host stimulus ok:** True · inquiry=1 capacity=3 tur=32/0 sense=1/0 maxLun=1 sync=1 reads=31/0 bytes=109056 · 155 ms
**DUT visible:** True · delta={"read_count": 12, "tur_hz_after": 47.19764, "event_codes_tail": []}

## Epistemik

- Desk FACT mount grammar from hu-probe feld (P-MOUNT / P-MREAD).
- Analyzer Host emits INQUIRY + MaxLUN + CAPACITY + TUR + REQUEST_SENSE + READ10 + SYNCHRONIZE_CACHE.
- **Not emitted:** MODE_SENSE6 / PREVENT_ALLOW (EspUsbHost API gap — mscCommand private).
- **Not claimed:** real BMW NBT · HU cache prediction · Path B L0.

## Verdict

- DUT **sees** controlled Host accesses → instrumentation not blind for this SCSI/READ subset.

```json
{
  "ok": true,
  "claim": "bmw_hu_stimulus_lab",
  "dut_kind": "huprobe",
  "profile": "bmw_mount_scan",
  "wall_s": 0.172,
  "host_stimulus": {
    "ok": true,
    "profile": "bmw_mount_scan",
    "inquiryOk": 1,
    "capacityOk": 3,
    "turOk": 32,
    "turFail": 0,
    "senseOk": 1,
    "senseFail": 0,
    "maxLunOk": 1,
    "maxLun": 0,
    "syncCacheOk": 1,
    "readOk": 31,
    "readFail": 0,
    "bytesRead": 109056,
    "capacityBlocks": 32768,
    "inquiryProduct": "HU_PROBE        ",
    "duration_ms": 155,
    "claim": "bmw_hu_stimulus_lab",
    "note": "not_real_hu_no_mode_sense_no_prevent;+sense+maxlun+sync"
  },
  "before": {
    "fw": "0.3.35",
    "fwType": "hu-probe",
    "usb_gadget": "msc",
    "read_count": 0,
    "tur_hz": 0,
    "session": {
      "active": true,
      "session_id": "20261102-192354_lab_debian_s0_plug_enum",
      "vehicle": "lab_debian",
      "scenario": "s0_plug_enum",
      "profile_id": "lab_debian",
      "phase": "enum",
      "transport": "tcp_live",
      "seq": 58,
      "seq_first": 1
    },
    "media": {
      "variant": "folders_30",
      "label": "3 Ordner x 10 Tracks 20 s (Traversierung)",
      "synthetic": true,
      "hash": "0x265fa9ab",
      "fs": "FAT16",
      "sector_count": 32768,
      "spc": 8,
      "reserved": 1,
      "fat_sectors": 16,
      "root_lba": 33,
      "data_lba": 65,
      "cluster_count": 4087,
      "volume_id": "0x48500131",
      "fs_label": "HUPROBE",
      "usb_serial_override": "",
      "nodes": 34,
      "truncated": false,
      "mutation_gen": 0,
      "ua_pending": false,
      "ua_sent": 0
    },
    "offline": "idle"
  },
  "after": {
    "fw": "0.3.35",
    "fwType": "hu-probe",
    "usb_gadget": "msc",
    "read_count": 12,
    "tur_hz": 47.19764,
    "session": {
      "active": true,
      "session_id": "20261102-192354_lab_debian_s0_plug_enum",
      "vehicle": "lab_debian",
      "scenario": "s0_plug_enum",
      "profile_id": "lab_debian",
      "phase": "enum",
      "transport": "tcp_live",
      "seq": 215,
      "seq_first": 1
    },
    "media": {
      "variant": "folders_30",
      "label": "3 Ordner x 10 Tracks 20 s (Traversierung)",
      "synthetic": true,
      "hash": "0x265fa9ab",
      "fs": "FAT16",
      "sector_count": 32768,
      "spc": 8,
      "reserved": 1,
      "fat_sectors": 16,
      "root_lba": 33,
      "data_lba": 65,
      "cluster_count": 4087,
      "volume_id": "0x48500131",
      "fs_label": "HUPROBE",
      "usb_serial_override": "",
      "nodes": 34,
      "truncated": false,
      "mutation_gen": 0,
      "ua_pending": false,
      "ua_sent": 0
    },
    "offline": "idle"
  },
  "delta": {
    "read_count": 12,
    "tur_hz_after": 47.19764,
    "event_codes_tail": []
  },
  "visible_on_dut": true,
  "not_emitted": [
    "MODE_SENSE6",
    "PREVENT_ALLOW"
  ],
  "note": "lab_host_nbt_like_not_real_hu"
}
```
