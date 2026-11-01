# GPIO lab methods (Gate 10) — dual ESP

**FW ≥ 0.0.44** · Claim discipline: lab MEASURED only; not USB-IF / not OTG PHY sniffer.

## Pin map (free GPIOs — never 19/20 while claiming USB)

| Signal | Host | Device | Notes |
|--------|------|--------|-------|
| Lab D+ | GPIO10 | GPIO10 | Path B TX/RX / cross-link |
| Lab D− | GPIO11 | GPIO11 | |
| Sync | GPIO12 | GPIO12 | Rising edge before TX |

OTG USB: Host/Device **GPIO20=D+ / GPIO19=D−** — only for destructive `wire-stimulus`.

## Cross-link wiring (optional)

```text
Host GPIO10 ──jumper── Device GPIO10
Host GPIO11 ──jumper── Device GPIO11
Host GPIO12 ──jumper── Device GPIO12
```

Short wires; optional 22–39 Ω series. Without jumpers, campaign records `jumperLikely=false`.

## APIs

| API | Role | Purpose |
|-----|------|---------|
| `/api/lab/gpio-calibrate` | both | Cycle-count vs `esp_timer` |
| `/api/lab/path-b-gpio` | both | TX/TXRX patterns, jitter, sync pulse, L0ST |
| `/api/lab/path-b-gpio-rx` | both | RX + edge hist + sync wait + L0ST |
| `/api/lab/wire-stimulus` | host | Destructive OTG 19/20 patterns |

## Patterns

`setup_ok`, `setup_bad_crc`, `data_bad_crc`, `pid_bad`, `bus_states`, `sync`, `eop`, `se0_reset`, `glitch`, `jk_burst`, `gap_<n>`, `glitch_se0_<n>`, `glitch_k_<n>`, `hold_j_<n>`, `hold_se0_<n>`

## Campaign

```bash
./tools/lab_gpio_methods_campaign.py
```

Evidence: `docs/evidence/LAB_GPIO_METHODS_MEASURED.md`
