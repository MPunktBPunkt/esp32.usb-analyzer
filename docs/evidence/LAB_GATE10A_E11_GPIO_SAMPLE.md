# Lab MEASURED — Gate 10A / E1.1 GPIO sample (0.0.40)

**Tool:** `tools/lab_e11_gpio_sample.py` → `GET /api/lab/e11-gpio-sample`  
**Topology:** OTG Host↔Device loop · peek-only GPIO19/20 (no remux) · APP-CPU poll + MSC on PRO-CPU  
**Host FW:** `0.0.40`

## Protocol

1. Baseline MSC READ (bus healthy)
2. GPIO19/20 poll burst (`samples=200000`) concurrent with MSC (`traffic_blocks=64`)
3. Post MSC READ (bus still healthy?)

## Results

| Check | Result |
|-------|--------|
| Baseline MSC | **OK** · 842 kB/s |
| Sample rate | **2092 ksps** (200000 samples / 95585 µs) |
| Poll jitter (pp) | **5875 ns** (dt min/avg/max cy: 111/114/1521 @ 240 MHz) |
| Edges D−/D+ | 0 / 0 (sum=0) |
| Meaningful edges | False |
| Concurrent traffic | ok=True · 864 kB/s |
| Bus stable after | **True** · destructive=False · postProbe=True · post MSC=True (842 kB/s) |
| Path A usable | **False** — poll_rate_below_fs_nyquist_need_ge_24Msps_path_b_required |
| L1 capture window | before=4 after=9 events (batch) |

## Verdict

- Peek-only GPIO poll on internal PHY pins is **not bus-destructive** in this run.
- Achieved rate **2092 ksps** vs FS need ≥**24000 ksps** (2× oversample) → **Path A falsified as FS sniffer**.
- Prefer **Path B** (external tap/PHY). Soft remux of GPIO19/20 away from USB was **not** tested (known to kill OTG).

**Claim:** `gate10a_e11_gpio_sample` · experiment **E1.1** · gate **10A**  
**Not claimed:** live D+/D− packet capture / MITM / HU.
