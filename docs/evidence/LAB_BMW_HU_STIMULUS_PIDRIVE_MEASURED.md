# Lab MEASURED — BMW-HU-like stimulus × pidrive · 0.0.50

**Claim:** `bmw_hu_stimulus_lab`
**Profile:** `bmw_mount_scan` · tur=32 · scan_blocks=160 · file_lba0=81
**Host stimulus ok:** True · inquiry=1 capacity=3 tur=32/0 sense=1/0 maxLun=1 sync=1 reads=31/0 bytes=109056 · 174 ms
**DUT visible:** True · delta={"readCount": 31, "bytesRead": 109056, "bytesBoot": 12800, "bytesFat": 14336, "bytesDir": 0, "bytesFile": 81920, "playGuessDelta": 0, "host_after": {"inquiry": 1, "capacity": 3, "tur": 66, "turNotReady": 0, "prevent": 0, "otherScsi": 1, "startStop": 0, "lastOpcode": 53, "msToInquiry": 25058, "msToCapacity": 25059, "msToTur": 25059, "hint": "poll-heavy"}}

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
  "dut_kind": "pidrive",
  "profile": "bmw_mount_scan",
  "wall_s": 0.19,
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
    "inquiryProduct": "USB_MEDIA       ",
    "duration_ms": 174,
    "claim": "bmw_hu_stimulus_lab",
    "note": "not_real_hu_no_mode_sense_no_prevent;+sense+maxlun+sync"
  },
  "before": {
    "fw": "0.4.61-dev",
    "fwType": "pidrive",
    "readCount": 0,
    "bytesRead": 0,
    "bytesBoot": 0,
    "bytesFat": 0,
    "bytesDir": 0,
    "bytesFile": 0,
    "lastReadLba": 0,
    "playGuessCount": 0,
    "host": {
      "inquiry": 0,
      "capacity": 0,
      "tur": 0,
      "turNotReady": 0,
      "prevent": 0,
      "otherScsi": 0,
      "startStop": 0,
      "lastOpcode": 0,
      "msToInquiry": 0,
      "msToCapacity": 0,
      "msToTur": 0,
      "hint": "unknown"
    },
    "huContract": {
      "scanHeadBlocks": 160,
      "scanHeadBytes": 81920,
      "menuReread": "usb_cycle",
      "softRemount": "noop_on_nbt",
      "freshNotRequired": true,
      "bodyDynamic": true,
      "fullMenu": false,
      "slots": 4,
      "slotBytes": 8388608,
      "geo": "FAT16 L3 16MiB fav0=8M"
    },
    "slot0": {
      "i": 0,
      "uid": "fav0",
      "name": "Rock Antenne",
      "path": "FAVORIT/01.MP3",
      "kind": "station",
      "lba0": 81,
      "lba1": 16464,
      "startCluster": 4,
      "fileBytes": 8388608,
      "active": true,
      "bytes": 0,
      "fromHead": 0,
      "midFile": 0,
      "maxSeq": 0,
      "lastAgeMs": 0
    }
  },
  "after": {
    "fw": "0.4.61-dev",
    "fwType": "pidrive",
    "readCount": 31,
    "bytesRead": 109056,
    "bytesBoot": 12800,
    "bytesFat": 14336,
    "bytesDir": 0,
    "bytesFile": 81920,
    "lastReadLba": 233,
    "playGuessCount": 0,
    "host": {
      "inquiry": 1,
      "capacity": 3,
      "tur": 66,
      "turNotReady": 0,
      "prevent": 0,
      "otherScsi": 1,
      "startStop": 0,
      "lastOpcode": 53,
      "msToInquiry": 25058,
      "msToCapacity": 25059,
      "msToTur": 25059,
      "hint": "poll-heavy"
    },
    "huContract": {
      "scanHeadBlocks": 160,
      "scanHeadBytes": 81920,
      "menuReread": "usb_cycle",
      "softRemount": "noop_on_nbt",
      "freshNotRequired": true,
      "bodyDynamic": true,
      "fullMenu": false,
      "slots": 4,
      "slotBytes": 8388608,
      "geo": "FAT16 L3 16MiB fav0=8M"
    },
    "slot0": {
      "i": 0,
      "uid": "fav0",
      "name": "Rock Antenne",
      "path": "FAVORIT/01.MP3",
      "kind": "station",
      "lba0": 81,
      "lba1": 16464,
      "startCluster": 4,
      "fileBytes": 8388608,
      "active": true,
      "bytes": 81920,
      "fromHead": 2,
      "midFile": 18,
      "maxSeq": 81920,
      "lastAgeMs": 476
    }
  },
  "delta": {
    "readCount": 31,
    "bytesRead": 109056,
    "bytesBoot": 12800,
    "bytesFat": 14336,
    "bytesDir": 0,
    "bytesFile": 81920,
    "playGuessDelta": 0,
    "host_after": {
      "inquiry": 1,
      "capacity": 3,
      "tur": 66,
      "turNotReady": 0,
      "prevent": 0,
      "otherScsi": 1,
      "startStop": 0,
      "lastOpcode": 53,
      "msToInquiry": 25058,
      "msToCapacity": 25059,
      "msToTur": 25059,
      "hint": "poll-heavy"
    }
  },
  "visible_on_dut": true,
  "not_emitted": [
    "MODE_SENSE6",
    "PREVENT_ALLOW"
  ],
  "note": "lab_host_nbt_like_not_real_hu"
}
```
