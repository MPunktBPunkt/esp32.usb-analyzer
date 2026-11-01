# Lab MEASURED — Path B soft campaign (no PHY) · 0.0.46

**Claim:** `path_b_soft_campaign_lab`
**MSC probe:** True @ 1000 kB/s
**L0 packets:** 3 · **L1 events:** 39
**Matched:** 2
**stack_gap_n:** 1 (true_gap=0, handshake_expected=1)

## Unmatched L0 (classified)

- idx=2 pid=`ACK` kind=`handshake` class=`handshake_expected`

## Verdict

- Soft L0 (synth / optional GPIO lab) correlates against live Host L1.
- Correlator splits handshake/token gaps from true stack_gap classes.
- Ready for Path-B Hi-Z samples when HW arrives (same pipeline).

**Not claimed:** soldered Path B PHY · live OTG D+/D− sniffer.

JSON: `LAB_PATH_B_SOFT_CAMPAIGN_MEASURED.json`
