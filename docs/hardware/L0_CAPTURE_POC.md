# L0 Capture Hardware PoC — Path B

**Software status (0.0.41):** ingest `:9760`, sampler sim, **Path B GPIO lab** (Host bit-bang GPIO10/11 + L0ST), live pipeline — MEASURED.  
**Hardware status:** external FS front-end **not soldered** — live OTG D+/D− PHY still UNKNOWN.

## Can the Host ESP send test data on GPIO?

**Yes (lab).** `/api/lab/path-b-gpio` drives synthetic SETUP/DATA0/ACK line-states on **free** pins (default GPIO10=D+, GPIO11=D−), self-samples them, optionally pushes `L0ST` to Debian.  
Default rate **100 kHz** (slow, intentional). Cap 2 MHz — **not** a claim of FS 12 MHz wire capture.  
**Never** uses GPIO19/20 (OTG PHY).

## Why not Path A

GPIO19/20 = internal USB PHY. 10A falsified CPU sniffer (`LAB_GATE10A_FEASIBILITY.md`).  
E1.1 MEASURED peek-poll rate/jitter under live MSC (`LAB_GATE10A_E11_GPIO_SAMPLE.md`) — too slow for FS; remux not tested (kills OTG).

## Architecture

```text
USB D+/D− ── Hi-Z tap / buffer ──► FS receiver / comparator
                                      │ line state J/K/SE0
                                      ▼
                                 Sampler ≥24 MS/s
                                      │ TCP L0ST :9760
                                      ▼
                          Debian l0_path_b_ingest.py
                                      │ .l0samp
                                      ▼
                          usb_fs_l0 decoder → correlate ↔ ESP L1
```

## Software (shipped)

| Piece | Tool |
|-------|------|
| Protocol | `tools/usb_fs_l0/path_b_protocol.md` |
| Ingest | `tools/l0_path_b_ingest.py --decode` |
| Sampler stand-in | `tools/l0_path_b_sampler_sim.py` |
| Live pipeline | `tools/lab_l0_live_pipeline.py --source sim\|ingest` |
| Controller | `GET /api/v1/l0/status` |

## BOM (procure for real PHY)

| Qty | Item | Role |
|----:|------|------|
| 1 | USB FS differential probe / SN65LVDS2 or USB3340-class ULPI PHY eval | Line receive |
| 1 | Series 22–39 Ω / common-mode choke as designed | Bus protection |
| 1 | Fast MCU/CPLD with DMA ≥24 MS/s (or second ESP32-S3 **not** on GPIO19/20) | Sampler |
| 1 | Isolated 3V3 supply | Avoid ground loops |
| — | Short matched D+/D− tap wires | Signal integrity |

Do **not** hard-parallel ESP OTG pins onto a live bus.

## Lab bring-up (sim → real)

```bash
./tools/l0_path_b_ingest.py --listen 0.0.0.0:9760 --out logs/l0 --decode &
./tools/l0_path_b_sampler_sim.py --target 127.0.0.1:9760
./tools/lab_l0_live_pipeline.py --source ingest --trigger-ep0
```

Replace sampler_sim with hardware TCP client speaking `L0ST` when the front-end exists.
