# Lab board notes — YD-ESP32-S3 (N8R2 class)

Aligned with technology-audit / hu-probe board notes.

| Item | Value |
|------|-------|
| Flash | 8 MiB |
| PSRAM | 2 MiB Quad (QSPI) — needs `BOARD_HAS_PSRAM` + `qio_qspi` |
| USB port | Native OTG (GPIO19 D− / GPIO20 D+ per Espressif DOC) |
| COM port | USB-UART bridge (CH343P class) — use for Serial monitor |
| RGB LED | WS2812 on **GPIO48** (`RGB_LED_PIN`); some boards need the **RGB** solder bridge closed |

## Gate 1 wiring

```text
PC UART monitor ── COM port ── ESP32-S3
USB DUT (keyboard/stick) ── USB-OTG port ── ESP32-S3 Host
```

Do **not** expect simultaneous USB-Serial/JTAG on the same internal PHY while OTG Host runs.

## VBUS

**Verdict (lab):** This board class does **not** source 5 V on the OTG connector for Host mode.  
API: `GET/POST /api/lab/vbus-status` → `boardSourcesVbus=false` (HPRT.prtpwr is a **software** bit only).  
Evidence: `docs/evidence/LAB_HOST_VBUS_MEASURED.md`.

If you need bus-powered keyboards/mice, use a **self-powered hub** between ESP Host and DUT (EspUsbHost / audit FACT for DevKitC-1 class boards). The OTG-loop Device ESP is self-powered via its own PSU — that does **not** prove Host VBUS.
