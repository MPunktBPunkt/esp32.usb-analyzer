# Lab MEASURED — Offline FS L0 decoder + L0↔L1 correlator (0.0.38)

**Not claimed:** live D+/D− tap, HU wire, hard µs sync to TinyUSB.

## Decoder (10B–D)

Host unit `tests/host/run_usb_fs_l0_tests.sh`:

| Check | Result |
|-------|--------|
| Token CRC5 SETUP | PASS |
| DATA0 CRC16 | PASS |
| NRZI + bit-stuff roundtrip SETUP/DATA0/ACK | PASS |
| `.l0samp` write/read | PASS |

## Correlator (10F)

`tools/lab_l0_correlate.py --synth` → **3/3** matches (SETUP/DATA0/ACK ↔ enum_info).

Live correlation awaits Path B `.l0samp` from an external tap.
