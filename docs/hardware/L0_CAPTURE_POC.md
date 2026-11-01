# L0 Capture Hardware PoC — Path B (preferred after 10A)

**Status:** Design + software ingest ready (0.0.38). Physical front-end **not yet soldered**.  
**Claim:** HYPOTHESIS / design — not MEASURED wire packets from a real tap.

## Why not Path A (GPIO on S3 OTG pins)

GPIO19 = D−, GPIO20 = D+ are the **internal USB PHY** pins.  
10A MEASURED: IO mux shows USB function while OTG runs; CPU GPIO sampling cannot provide a trustworthy 12 Mbit/s packet decoder and reconfiguring the pins is destructive to the Host/Device stack. → **Path A falsified as sniffer**.

## Path B — external digital front-end

```text
HU/DUT D+ ──●──────────────── peer
            │ Hi-Z tap / buffer
            ▼
         USB FS receiver / comparator / PHY
            │ line state (J/K/SE0) or recovered bits
            ▼
         Sampler (CPLD/MCU timer/DMA)  ≥24 MS/s preferred
            │
            ▼
         ESP32-S3 (or Debian) ingest ──► tools/usb_fs_l0 decoder
```

### PoC goals (minimum)

1. Passive (no series FET in data path for Phase 1)
2. Deliver **line-state stream** or **NRZI bits** into `.l0samp` files
3. Decode with `tools/usb_fs_l0` → `l0_packet.v1`
4. Correlate against ESP L1 capture (`lab_l0_correlate.py`)

### Sample file (software interface already)

Magic `L0S1` — see `tools/usb_fs_l0/sample_io.py`:

| Field | Content |
|-------|---------|
| rate_hz | e.g. 12000000 or 24000000 |
| format | `line_state` (2-bit J/K/SE0/SE1) or `nrzi_bits` |
| payload | packed samples |

Synthetic generator: `python3 -m tools.usb_fs_l0.gen_synth_setup`.

### Electrical caution

- Tap must not violate FS eye / impedance; use proper USB differential probe / buffer.
- Do **not** parallel raw ESP PHY pins onto a live bus without a designed front-end.
- ESD / VBUS isolation as needed.

## Path C (later)

FPGA bit recovery when Path B cannot sustain continuous decode + timestamping.

## Next hardware lab step

Procure/build a FS differential line-state tap → feed ESP UART/SPI/I2S DMA → write `.l0samp` → run decoder. Until then, all L0 packet MEASURED claims are **synthetic/offline only**.
