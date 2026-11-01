# Lab MEASURED — Observer-effect baseline starter (0.0.38)

**Tool:** `tools/lab_observer_effect.py`  
**Claim:** `observer_effect_baseline_lab` — **not** transparency FACT / **not** L0.

## Mode `loop` (OTG Host↔Device) — this lab

| Metric | Value |
|--------|------:|
| Host MSC READ rate avg | **842 kB/s** (32 blocks, chunk=4, 2 runs) |
| Duration avg | **19 ms** |
| Rate spread | **0** |

Interpretation: repeatability baseline on stack path.  
**Not** PC-stick-only vs MITM Δ (needs `--mode proxy` + MITM wiring).

## Mode `proxy`

Requires FS DUT on Host OTG and Device facing PC/gadget; records proxy-selftest vs host-read and ms Δ. Run when MITM topology is up.
