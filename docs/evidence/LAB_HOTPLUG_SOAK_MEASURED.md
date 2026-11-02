# Lab MEASURED — Hotplug/Soak · 0.0.48

**Claim:** `hotplug_soak_lab`
**Cycles:** 100 · fail_enum=0 · fail_msc=0
**Recovery ms p50/p95:** 1685.0508451461792 / 1943.4057235717773
**Heap drift Host/Device:** 208 / 236
**Final heal MSC:** True

## Verdict

- Soft-reenum approximates hotplug on OTG loop (not physical unplug).
- Pass if enum failures=0 and MSC fails ≤5% and final heal ok.

**Not claimed:** physical VBUS unplug · L0 · MITM.

JSON: `LAB_HOTPLUG_SOAK_MEASURED.json`
