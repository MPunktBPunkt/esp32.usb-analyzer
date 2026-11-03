# Lab MEASURED — Heap soak · 0.0.50

**Claim:** `heap_soak_lab`
**Cycles:** 1000/1000 · wall=1777.4s · fail_enum=0 · fail_msc=0
**Host class:** stable · drift heap/heapMax=2152/5120
**Device class:** stable · drift heap/heapMax=4924/0
**Heal MSC:** True

## Epistemik

- Soft-reenum soak approximates alloc churn; not VBUS unplug.
- `heapMax` (largest block) separates leak-like from fragmentation/churn.
- **Not claimed:** absolute leak proof · L0 · MITM.

JSON: `LAB_HEAP_SOAK_MEASURED.json`
