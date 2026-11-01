# Lab MEASURED — Gate 10A L0 feasibility (0.0.38)

**Date:** 2026-11-01 · Host `0.0.38` · DUT present on OTG  
**Tool:** `tools/lab_l0_feasibility.py` → `GET /api/lab/l0-feasibility`

## Results

| Check | Result |
|-------|--------|
| DWC HPRT Level-2 dump | MEASURED (`hprtRaw=136197`, conn/ena, speed=1 FS) |
| GPIO19/20 level peek | MEASURED (non-destructive); pins = internal PHY |
| Path A GPIO sniffer | **FALSIFIED** — cannot be FS packet sniffer while OTG owns pins |
| Path B preferred | **yes** (external tap/PHY) |
| Live L0 packets | **not claimed** |

## Sample JSON (lab)

```json
{
  "pathA_snifferOk": false,
  "pathA_reason": "gpio19_20_are_internal_usb_phy_pins_cpu_sample_not_fs_packet_sniffer",
  "pathB_preferred": true,
  "prtConn": true,
  "prtEna": true,
  "prtSpeed": 1,
  "claim": "gate10a_feasibility"
}
```

See also: `docs/hardware/L0_CAPTURE_POC.md`.
