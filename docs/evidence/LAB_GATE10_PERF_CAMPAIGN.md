# Lab MEASURED — Gate 10 performance campaign (20261101T114922Z)

**Nodes:** host `0.0.41` · device `0.0.41`
**Result:** **6/9** tests OK

| Test | OK | Summary |
|------|----|---------|
| `usb_enum_observe` | NO | addr=1 cfg=1 desc=False |
| `usb_otg_msc` | yes | 3 cfgs |
| `tcp_wifi_integrity` | yes | rate=155 gaps=None |
| `gate10a_feasibility` | yes | gpio19_20_are_internal_usb_phy_pins_cpu_sample_not_fs_packet_sniffer |
| `e11_gpio_sample` | yes | 2083 ksps jitter=4608ns destructive=False |
| `path_b_gpio_lab` | yes | txrx_l0st+decode |
| `l0_live_pipeline_sim` | yes | ['  "l0_rate_hz": 12000000', '}', 'RESULT OK — live pipeline'] |
| `observer_proxy` | NO | ['    "claim": "msc_bulk_proxy_selftest",', '    "note": "not_pc_path_not_transparent_mitm"', '  },', '  "err": "proxy_path_fail"', '}'] |
| `usb_ctrl_neg` | NO | timed out |

**Claim:** `gate10_perf_campaign_lab`
**Not claimed:** soldered Path-B PHY · BMW/HU 10G · FPGA Path C · true stick MITM unless wired.

JSON: `LAB_GATE10_PERF_CAMPAIGN_20261101T114922Z.json`
