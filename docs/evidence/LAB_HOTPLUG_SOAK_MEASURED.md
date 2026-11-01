# Lab MEASURED — Hotplug/Soak · 0.0.47

**Claim:** `hotplug_soak_lab`
**Cycles:** 12 · fail_enum=0 · fail_msc=0
**Recovery ms p50/p95:** 2181.549072265625 / 2322.1781253814697
**Heap drift Host/Device:** 56 / 20
**Final heal MSC:** True

## Verdict

- Soft-reenum approximates hotplug on OTG loop (not physical unplug).
- Pass if enum failures=0 and MSC fails ≤5% and final heal ok.

**Not claimed:** physical VBUS unplug · L0 · MITM.

JSON: `LAB_HOTPLUG_SOAK_MEASURED.json`
