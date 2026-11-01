# Lab MEASURED — Path B GPIO lab substitute (0.0.41)

**Tool:** `tools/lab_path_b_gpio.py` → `GET /api/lab/path-b-gpio`  
**Host FW:** `0.0.43`  
**Pins:** DP=GPIO10 · DM=GPIO11 (**not** OTG 19/20)

## Question

Can the Host ESP deliberately drive slow synthetic FS line-states on free GPIOs
for decoder/ingest tests?

## Result

| Check | Value |
|-------|-------|
| Mode | txrx_l0st |
| Target rate | 100000 Hz |
| Achieved rate | **84729 Hz** |
| States TX/RX | 177 / 177 |
| Loopback mismatches | 0 · ok=True |
| L0ST push | ok=True · bytes=286 → 192.168.178.187:9760 |
| Duration | 2089 µs |

## Verdict

- **Yes** — Host can bit-bang known SETUP/DATA0/ACK line-states on free GPIOs and
  self-sample them (digital loopback) for automated Path-B pipe tests.
- Rate is lab-slow (CPU bit-bang), not FS 12 MHz wire capture.
- Does **not** replace a Hi-Z passive tap on the real OTG bus.

**Claim:** `path_b_gpio_lab`  
**Not claimed:** live OTG D+/D− PHY · Path A · HU.
