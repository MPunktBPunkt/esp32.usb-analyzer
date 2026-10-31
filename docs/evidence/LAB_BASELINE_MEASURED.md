# Lab baseline — MEASURED (OTG loop + WiFi TCP)

**Date (UTC):** 2026-10-31T12:07Z  
**Firmware:** Host `0.0.13` · Device `0.0.14`  
**Topology:** `.88` Host OTG ↔ USB-C ↔ `.89` Device MSC; WiFi TCP parallel  
**Raw JSON:** [`LAB_BASELINE_20261031T120705Z.json`](LAB_BASELINE_20261031T120705Z.json)  
**Repro:** `python3 tools/lab_baseline_campaign.py`

Classification: **MEASURED** (this lab run). Not USB-IF compliance. Not Gate‑4 MITM.

## Claim boundary

| In scope | Out of scope |
|----------|----------------|
| FS USB Host→Device MSC READ on OTG cable | D+/D− wire / packet capture |
| Byte pattern integrity (`lab_sector_v1`) | High-Speed USB |
| WiFi TCP ESP↔ESP PUSH/ACK integrity | USB-over-WiFi proxy (Gate 4) |
| Throughput + transfer RTT percentiles | Observer-effect vs real HU |

## A) USB OTG MSC (cable) — Host → Device

DUT: `303a:1001` · speed **full** · Inquiry `MPB`/`ANALYZER` · capacity **512×512 B**

### Throughput vs chunk size (pattern always OK, failB=0)

| blocks | chunk | kB/s | RTT p50 (µs) | RTT p95 (µs) | RTT max (µs) |
|-------:|------:|-----:|-------------:|-------------:|-------------:|
| 1 | 1 | 500 | 748 | 748 | 748 |
| 64 | 1 | 653 | 729 | 768 | 792 |
| 512 | 1 | 651 | 730 | 802 | 1319 |
| 128 | 2 | 780 | 1249 | 1271 | 1670 |
| 512 | 4 | 885 | 2171 | 2239 | 2525 |
| 512 | 8 | **934** | 4127 | 4267 | 4590 |

### Sustained (3× full media, chunk=8)

| Metric | Value |
|--------|------:|
| Rate min/avg/max | **934 / 936 / 937 kB/s** |
| Pattern fail bytes | **0** |
| Transfers OK | 3/3 |

### USB limits (this stack)

| Limit | MEASURED |
|-------|----------|
| Peak payload rate | **~0.93 MB/s** (~7.5 Mbit/s useful) |
| vs FS theoretical bulk | Well below ~12 Mbit/s line / ~1.2 MB/s class ceiling — stack + 512 B BOT overhead |
| Single-sector RTT p50 | **~0.73 ms** |
| 8-sector transfer RTT p50 | **~4.1 ms** |
| Data loss / pattern errors | **none** in this campaign (after Device multi-LBA fill fix in 0.0.14) |
| Media size | 256 KiB gadget — long streams wrap / stop at capacity |

## B) WiFi TCP integrity (ESP↔ESP)

| frame_bytes | frames | ok | kB/s | RTT p50 (µs) |
|------------:|-------:|:--:|-----:|-------------:|
| 64 | 64 | ✓ | ~0 | ~249 900 |
| 1024 | 64 | ✓ | 4 | ~249 800 |
| 2048 | 64 | ✓ | 125 | ~12 700 |
| 2048 | 128 | ✓ | **139** | ~11 700 |
| 4096 | 64 | ✓ | 53 | ~16 700 (p95 spikes) |

### TCP limits

| Limit | MEASURED |
|-------|----------|
| Peak | **~139 kB/s** |
| Small-frame regime | RTT-dominated (~250 ms PUSH→ACK) → near-zero useful rate |
| Frame errors | **0** in this sweep |
| Relation to USB cable | TCP ≪ USB OTG MSC; WiFi hop is the bottleneck for Mode‑A data plane today |

## Audit takeaways

1. **USB OTG loop is integrity-clean** at measured loads (pattern match, 0 fail bytes).  
2. **Useful USB ceiling on this lab path ≈ 0.93 MB/s** with 4 KiB MSC READs — not wire-rate.  
3. **WiFi TCP ACK-each ≈ 0.14 MB/s**; with **window=8** bulk (0.0.15) ≈ **0.26 MB/s** MEASURED — still ≪ USB cable; window=16 can regress.  
4. **MSC proxy Host serve** (0.0.15): TCP `MscReq`→Host READ DUT→payload OK + pattern match (lab).  
5. **Not claimed:** transparent MITM, HU timing fidelity, or L0 bit-level visibility.

## Earlier false positive (document for audit hygiene)

Campaign `…T120435Z` (Device still 0.0.12) showed `pattern_ok=false` for `chunk>1`. **Cause:** gadget filled multi-sector callbacks with a single LBA base — verification bug, **not** USB bit errors. Fixed in Device **0.0.14**; re-run `…T120705Z` is authoritative.
