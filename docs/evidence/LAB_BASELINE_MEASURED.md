# Lab baseline — MEASURED (OTG loop + WiFi TCP)

**Date (UTC):** 2026-11-02T19:57Z  
**Firmware:** Host `0.0.50` · Device `0.0.50`  
**Topology:** `.88` Host OTG ↔ USB-C ↔ `.89` Device MSC; WiFi TCP parallel  
**Raw JSON:** [`LAB_BASELINE_20261102T195715Z.json`](LAB_BASELINE_20261102T195715Z.json)  
**Repro:** `python3 tools/lab_baseline_campaign.py`

Classification: **MEASURED** (this lab run). Not USB-IF compliance. Not Gate‑4 MITM.

## Claim boundary

| In scope | Out of scope |
|----------|----------------|
| FS USB Host→Device MSC READ on OTG cable | D+/D− wire / packet capture |
| Byte pattern integrity (`lab_sector_v1`) | High-Speed USB |
| WiFi TCP ESP↔ESP PUSH/ACK integrity | USB-over-WiFi proxy transparency |
| Throughput + transfer RTT percentiles | Observer-effect vs real HU |

## A) USB OTG MSC (cable) — Host → Device

DUT: `303a:1001` · speed **full** · Inquiry `MPB`/`ANALYZER` · capacity **512×512 B**

### Throughput vs chunk size (pattern always OK, failB=0)

| blocks | chunk | kB/s | RTT p50 (µs) | RTT p95 (µs) | RTT max (µs) |
|-------:|------:|-----:|-------------:|-------------:|-------------:|
| 1 | 1 | 500 | 800 | 800 | 800 |
| 8 | 1 | 666 | 801 | 817 | 817 |
| 32 | 1 | 571 | 845 | 1078 | 1091 |
| 64 | 1 | 615 | 802 | 824 | 1219 |
| 128 | 1 | 615 | 764 | 941 | 1222 |
| 256 | 1 | 618 | 802 | 855 | 1290 |
| 512 | 1 | 622 | 802 | 817 | 1252 |
| 64 | 2 | 780 | 1253 | 1338 | 1373 |
| 128 | 2 | 771 | 1254 | 1352 | 1410 |
| 256 | 4 | 853 | 2248 | 2414 | 2523 |
| 512 | 4 | 856 | 2239 | 2513 | 2721 |
| 512 | 8 | **914–917** | ~4220 | ~4310 | ~4600 |

### Sustained (3× full media, chunk=8)

| Metric | Value |
|--------|------:|
| Rate min/avg/max | **907 / 911.7 / 914 kB/s** |
| Pattern fail bytes | **0** |
| Transfers OK | 3/3 |

### USB limits (this stack, 0.0.50)

| Limit | MEASURED |
|-------|----------|
| Peak payload rate | **~0.92 MB/s** (~7.3 Mbit/s useful) |
| vs FS theoretical bulk | Below ~12 Mbit/s line — stack + 512 B BOT overhead |
| Single-sector RTT p50 | **~0.80 ms** |
| 8-sector transfer RTT p50 | **~4.2 ms** |
| Data loss / pattern errors | **none** |

Compare prior 0.0.13/0.0.14 baseline (~934 kB/s peak): same order of magnitude; small variance expected (WiFi load, DUT address churn).

## B) WiFi TCP integrity (ESP↔ESP)

| frame_bytes | frames | ok | kB/s | RTT p50 (µs) |
|------------:|-------:|:--:|-----:|-------------:|
| 64 | 64 | ✓ | 2 | ~232 k |
| 256 | 64 | ✓ | 52 | ~23 k |
| 1024 | 64 | ✓ | 205 | ~17 k |
| 2048 | 64 | ✓ | **332** | ~19 k |
| 4096 | 64 | ✓ | 209 | ~36 k |
| 2048 | 128 | ✓ | 279 | ~20 k |

TCP peak this run: **332 kB/s** (higher than early 0.0.13 ~139 kB/s baseline after pipeline work).

## Repro

```bash
# Device role on .89: analyzer-s3-device @ same FW as host
python3 tools/lab_baseline_campaign.py
```
