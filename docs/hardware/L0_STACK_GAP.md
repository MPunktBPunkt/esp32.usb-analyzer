# L0 vs TinyUSB (stack gap)

**Goal:** See what is on the wire that the stack does not report.

## Today (0.0.45)

| Method | Meaning |
|--------|---------|
| `lab_l0_stack_gap.py` | Inject known patterns on OTG D+/D− (wire-stimulus), decode as L0 GT, diff vs Device `/api/capture` |
| `correlate.stack_gap_n` | Count of L0 packets with no compatible L1 event |

**Claim:** `l0_stack_gap_stimulus_lab` — controlled injection, not passive sniffer.

## Path B (next hardware)

```text
USB D+/D− ── Hi-Z / comparator ──► VP/VM digital ──► ESP GPIO (e.g. 10/11)
                                                      path-b-gpio-rx / L0ST
                                                      ↓
                                              decode → correlate vs L1
                                              unmatched_l0 = live stack gap
```

Bring-up: `docs/hardware/L0_CAPTURE_POC.md` · RX API already: `/api/lab/path-b-gpio-rx`.

## Audit note

Stimulus gap ≠ live traffic TinyUSB missed. It proves the **measurement chain** for gap reports and shows stack silence under known wire content after Host PHY release.
