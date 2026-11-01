# Lab MEASURED — GPIO methods campaign (20261101T154628Z)

**Result:** **10/10**

| Test | OK | Summary |
|------|----|---------|
| `calibrate_host` | yes | cyclesPerUs=240 errPpm=1000 |
| `calibrate_device` | yes | cyclesPerUs=240 errPpm=0 |
| `host_self_loopback_l0st` | yes | achieved=88500 mm=0 l0st=True |
| `error_matrix_path_b` | yes | 7 patterns |
| `gap_sweep` | yes | 6 gaps |
| `glitch_sweep` | yes | 10 glitches |
| `jitter_sweep` | yes | 5 jitters |
| `crosslink_host_tx_device_rx` | yes | syncSeen=False edges=0 jumperLikely=False |
| `device_idle_histogram` | yes | edges=0 J=0 SE0=200 |
| `ls_bit_time_attempt` | yes | target=1.5M achieved=382022 loop=True |

## Cross-link note

jumperLikely=False — without Host↔Device wires on GPIO10/11/12 this stays false; software+protocol still MEASURED.

**Claim:** `gpio_methods_campaign_lab`
**Not claimed:** Path-B PHY tap · FS soft-host · cross-link pass without jumpers.

JSON: `LAB_GPIO_METHODS_20261101T154628Z.json`

Wiring: `docs/hardware/GPIO_LAB_METHODS.md`
