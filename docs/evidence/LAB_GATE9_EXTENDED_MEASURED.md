# Lab MEASURED — Gate 9 extended fault matrix

**Claim:** `gate9_extended_lab`
**Pass:** 3/3 · heal=True

## Cases

- `stall_alias`: PASS — {"obs": {"readOk": false, "recoveryOk": true, "senseKeyBefore": 0}}
- `fail_after`: PASS — {"first_ok": true, "second_readOk": false, "recoveryOk": true}
- `fail_mid`: PASS — {"note": "mid_path_not_exercised_single_callback", "lastFailOffset": 0, "failHits": 1, "readOk": true, "recoveryOk": true}

## Verdict

- `stall` aliases `fail` (TinyUSB −1 → CSW FAILED).
- `fail_after` allows N successful onRead then fails.
- `fail_mid` targets offset≠0; may be UNKNOWN if MSC delivers one callback.

**Not claimed:** L0 wire CRC/malformed · CLEAR_HALT vs BOT as separate FACT.

JSON: `LAB_GATE9_EXTENDED_MEASURED.json`
