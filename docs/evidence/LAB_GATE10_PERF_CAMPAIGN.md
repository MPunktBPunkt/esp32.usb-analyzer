# Lab MEASURED — Gate 10 performance campaign (20261101T114944Z)

**Nodes:** host `0.0.41` · device `0.0.41`
**Result:** **9/9** tests OK

| Test | OK | Summary |
|------|----|---------|
| `usb_enum_observe` | yes | addr=1 cfg=1 desc=True |
| `usb_otg_msc` | yes | 3 cfgs |
| `tcp_wifi_integrity` | yes | rate=148 gaps=None |
| `gate10a_feasibility` | yes | gpio19_20_are_internal_usb_phy_pins_cpu_sample_not_fs_packet_sniffer |
| `e11_gpio_sample` | yes | 2090 ksps jitter=4491ns destructive=False |
| `path_b_gpio_lab` | yes | txrx_l0st+decode |
| `l0_live_pipeline_sim` | yes | ['  "l0_rate_hz": 12000000', '}', 'RESULT OK — live pipeline'] |
| `observer_proxy` | yes | ['  },', '  "note": "otg_loop_proxy_vs_direct_not_pc_stick_mitm"', '}', 'RESULT OK — observer-effect baseline', 'claim: observer_effect_baseline_lab (not transparency FACT)'] |
| `usb_ctrl_neg` | yes | passed=8/8 |

**Claim:** `gate10_perf_campaign_lab`
**Not claimed:** soldered Path-B PHY · BMW/HU 10G · FPGA Path C · true stick MITM unless wired.

JSON: `LAB_GATE10_PERF_CAMPAIGN_20261101T114944Z.json`
