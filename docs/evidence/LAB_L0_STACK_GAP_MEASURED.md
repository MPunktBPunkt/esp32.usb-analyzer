# Lab MEASURED — L0 vs TinyUSB stack gap (stimulus) · 0.0.44

**Question:** What on the wire (injected) did TinyUSB not report?

**Patterns:** `setup_ok, setup_bad_crc, data_bad_crc, eop, se0_reset`
**L0 packets (decoder on injected states):** 5
**L1 Device events:** 4
**Matched L0↔L1:** 0
**Stack gap (L0 without L1):** **5**
**Device Δunmount:** 1 · otg=waiting

## Unmatched L0 (candidates TinyUSB did not mirror)

- idx=0 pid=`SETUP` kind=`ep0` crc_ok=True
- idx=1 pid=`DATA0` kind=`data0` crc_ok=True
- idx=2 pid=`ACK` kind=`handshake` crc_ok=True
- idx=3 pid=`SETUP` kind=`ep0` crc_ok=False
- idx=4 pid=`DATA0` kind=`data0` crc_ok=False

## Verdict

- With Host stack stopped, injected FS-like patterns are **L0 ground truth**.
- Device TinyUSB typically only shows **unmount/usb_stopped** (Host end),
  not SETUP/DATA decode — hence large `stack_gap_n` is **expected** and MEASURED.
- This is the closest lab answer to „was geht an TinyUSB vorbei?“ until Path B PHY.

**Claim:** `l0_stack_gap_stimulus_lab`
**Not claimed:** passive live D+/D− sniffer · Path B soldered.

JSON: `LAB_L0_STACK_GAP_MEASURED.json`
