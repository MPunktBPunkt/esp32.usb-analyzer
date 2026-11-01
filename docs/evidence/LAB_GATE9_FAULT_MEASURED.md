# Lab MEASURED — Gate 9 Fault Injection · 0.0.46

**Claim:** `gate9_fault_inject_lab`
**Pass:** 3/3
**Baseline MSC:** 1000 kB/s

## Ops

- fail: readOk=False senseKey=0 asc=0 recoveryOk=True → PASS
- delay: readMs=82 (arm 80ms) readOk=True → PASS
- disconnect: post_probe=True heal=True → PASS

## Verdict

- Device `fault-inject` arms MSC `onRead` delay/fail/disconnect.
- Host `fault-observe`: fail→`readOk=false` + recovery READ ok; delay inflates `readMs`.
- TinyUSB Device maps fail → CSW FAILED + `medium_not_present`; Host REQUEST_SENSE
  after EspUsbHost BOT-reset often returns empty sense (key/ASC 0) — MEASURED gap.

**Not claimed:** L0 D+/D− · transparent MITM · HU.

JSON: `LAB_GATE9_FAULT_MEASURED.json`
