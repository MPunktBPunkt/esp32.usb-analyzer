# Lab MEASURED — Capability Audit · 20261103T075841Z

**Claim:** `lab_capability_audit_0_0_50`
**FW:** Host `0.0.50` · Device `0.0.50` · Controller `0.0.41`
**Result:** **7/7** OK
**DUT:** `{'id': '303a:1001', 'product': 'ANALYZER', 'mfr': 'MPB', 'serial': 'lab-gadget', 'speed': 'full'}`

## Limits (this run)

- USB OTG MSC peak: **917 kB/s**
- WiFi TCP integrity sample: **240 kB/s** (gaps=0 required)

## Tests

| Test | OK | Summary |
|------|----|---------|
| `usb_enum_observe` | yes | addr=26 cfg=1 |
| `usb_otg_msc_sweep` | yes | peak=917 kB/s n=8 |
| `tcp_wifi_integrity` | yes | rate=240 gaps=0 |
| `controller_sync_offset` | yes | offset_us=1383269907 residual=143409 |
| `bot_class_hook` | yes | ['CSW status 0 residue 0 sig USBS hex 5553425338020000', 'RESULT OK — class-hook BOT CBW/CSW', 'claim: class_hook_mscd_xfer (not wire PHY, not transparent MITM)'] |
| `gate10_perf_campaign` | yes | skipped |
| `path_a_rejected` | yes | gpio19_20_are_internal_usb_phy_pins_cpu_sample_not_fs_packet_sniffer |

## Epistemik

- Proves OTG-loop Host↔Device MSC pattern integrity + rate, WiFi TCP integrity,
  enum observe, BOT class-hook path, controller sync estimate, Path-A rejection.
- **Not claimed:** passive D+/D− L0 · Path-B soldered PHY · BMW NBT wire · HS · USB-IF.
- Soft USB cycle ≠ VBUS unplug.

JSON: `LAB_CAPABILITY_AUDIT_20261103T075841Z.json`

## Repro

```bash
# Both nodes must run analyzer-node 0.0.50 (Host env + Device env)
python3 tools/lab_capability_audit.py
# optional full Gate10 suite is included unless --skip-heavy
```
