# Lab MEASURED — TCP sliding-window pipeline (0.0.37)

**Date:** 2026-11-01 · FW Host/Device `0.0.37` · `.88`→`.89` · RSSI −24/−25  
**Not claimed:** USB MITM, L0, hard WiFi link-rate, USB-IF.

## Modes

| Mode | API | Behavior |
|------|-----|----------|
| ack_each | `window=1` | PUSH → ACK → next |
| windowed | `window=N` (default 8) | send N, then collect N ACKs |
| pipeline | `window=N&pipeline=1` | sliding window: each ACK frees one send |

Tool: `tools/lab_perf_sweep.py --quick`

## Results (`frames=64`)

| size | win | mode | kB/s |
|-----:|----:|------|-----:|
| 2048 | 1 | ack_each | **137** |
| 2048 | 8 | windowed | **304** |
| 2048 | 8 | pipeline | **344** |
| 4096 | 8 | windowed | **301** |
| 4096 | 8 | pipeline | **436** |
| 4096 | 12 | pipeline | **419** |
| 4096 | 16 | pipeline | **44** (cliff) |

## Takeaway

Pipeline at sweet spot **`4096×window=8`** raises useful rate **~300 → ~436 kB/s** (~+45 %) vs barrier windowed on this lab hop. Window ≥16 still cliffs.
