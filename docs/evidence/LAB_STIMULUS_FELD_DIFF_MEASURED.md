# Lab MEASURED — Stimulus ↔ Feld P-MOUNT/P-MREAD diff · 0.0.52

**Claim:** `bmw_stimulus_feld_diff_lab`
**Stimulus ok:** True · mount_ok=True · pmread_ok=True · mode+prevent=True
**DUT delta:** msc_reads=11 read_count=11

## P-MOUNT (Feld FACT vs emitted)

| Op | want | got | ok |
|----|-----:|----:|:--:|
| INQUIRY | 1 | 1 | yes |
| READ_CAPACITY10 | 3 | 3 | yes |
| MODE_SENSE6 | 6 | 6 | yes |
| PREVENT_ALLOW | 7 | 7 | yes |
| TUR | 32 | 32 | yes |

**P-MREAD:** expect 11 READ10 · got readOk=11 fail=0

## Epistemik

- Feld FACT from hu-probe 2026-10-19 independent analysis (all BMW mounts).
- Analyzer emits lab approximation — not timing/wire identity with NBT.
- MODE_SENSE6/PREVENT required by P-MOUNT FACT; shipped via mscCommand lab unlock.

JSON: `LAB_STIMULUS_FELD_DIFF_MEASURED.json`

```json
{
  "ok": true,
  "claim": "bmw_stimulus_feld_diff_lab",
  "timestamp_utc": "20261103T182731Z",
  "feld_source": "hu-probe/docs/feld/2026-10-19-unabhaengige-auswertung.md",
  "profile": "bmw_mount",
  "wall_s": 0.098,
  "p_mount_rows": [
    {
      "op": "INQUIRY",
      "want": 1,
      "got": 1,
      "ok": true
    },
    {
      "op": "READ_CAPACITY10",
      "want": 3,
      "got": 3,
      "ok": true
    },
    {
      "op": "MODE_SENSE6",
      "want": 6,
      "got": 6,
      "ok": true
    },
    {
      "op": "PREVENT_ALLOW",
      "want": 7,
      "got": 7,
      "ok": true
    },
    {
      "op": "TUR",
      "want": 32,
      "got": 32,
      "ok": true
    }
  ],
  "p_mread_expected_n": 11,
  "p_mread_pairs": [
    [
      0,
      8
    ],
    [
      0,
      8
    ],
    [
      24,
      8
    ],
    [
      0,
      8
    ],
    [
      0,
      1
    ],
    [
      1,
      3
    ],
    [
      7,
      2
    ],
    [
      1,
      1
    ],
    [
      2,
      1
    ],
    [
      3,
      5
    ],
    [
      24,
      8
    ]
  ],
  "emitted": {
    "INQUIRY": 1,
    "READ_CAPACITY10": 3,
    "MODE_SENSE6": 6,
    "PREVENT_ALLOW": 7,
    "TUR": 32,
    "READ10_ok": 11
  },
  "fails": {
    "MODE_SENSE6_fail": 0,
    "PREVENT_fail": 0,
    "TUR_fail": 0,
    "READ_fail": 0
  },
  "mode_prevent_in_stimulus": true,
  "dut_delta": {
    "msc_reads": 11,
    "read_count": 11
  },
  "stimulus": {
    "ok": true,
    "inquiryOk": 1,
    "capacityOk": 3,
    "turOk": 32,
    "turFail": 0,
    "senseOk": 1,
    "modeSenseOk": 6,
    "modeSenseFail": 0,
    "preventOk": 7,
    "preventFail": 0,
    "readOk": 11,
    "readFail": 0,
    "bytesRead": 27136,
    "duration_ms": 87,
    "err": null,
    "note": "not_real_hu;+sense+maxlun+sync+mode_sense6x6+preventx7"
  },
  "not_claimed": [
    "byte_identical_to_nbt_trace",
    "timing_identical_to_hu",
    "real_hu_wire"
  ],
  "verdict": "P-MOUNT/P-MREAD counts covered incl. MODE_SENSE6+PREVENT"
}
```
