# Hardware risks

**Status:** Gate 0 risk register

## R1 — Shared internal USB PHY

| | |
|-|-|
| Status | **FACT** (audit + Espressif DOC) |
| Risk | USB-OTG and USB-Serial/JTAG share one internal PHY on ESP32-S3. Simultaneous use must not be assumed. |
| Mitigation | Prefer **UART console** to Debian for analyzer nodes; or external PHY / eFuse PHY select for advanced boards. |

## R2 — D+/D− pin map vs sampling fantasy

| | |
|-|-|
| Status | DOC: GPIO20=D+, GPIO19=D−; audit pin map still to be pinned; **sampling under live USB = UNKNOWN** |
| Risk | Treating fixed PHY GPIOs as freely sampleable logic-analyzer channels during OTG operation. |
| Mitigation | Separate Gate 10 research; visualize protocol state first. |

## R3 — VBUS / dual supply

| | |
|-|-|
| Status | **FACT** many S3 DevKits do not source Host VBUS; hu-probe notes OTG bridge / VBUS short hazards |
| Risk | Host-side ESP cannot power Side B device; or VBUS fights between HU PSU and lab PSU. |
| Mitigation | Explicit VBUS design per session profile; isolation checklist from `plug-power-reset-isolation`. |

## R4 — Full-Speed only Host

| | |
|-|-|
| Status | **FACT** |
| Risk | HS-only peripherals will not enumerate on S3 Host. |
| Mitigation | Lab devices must offer FS config; document speed in session metadata. |

## R5 — USB ↔ WiFi under Bulk

| | |
|-|-|
| Status | **HYPOTHESIS** set; death under load MEASURED in related projects without single root cause |
| Risk | Capture/control TCP dies while USB path is busy → false “USB timeout” narratives. |
| Mitigation | Mode A tunnel independent of Debian; offline capture option; coex discrimination method. |

## R6 — Observer effect / MITM transparency

| | |
|-|-|
| Status | **UNKNOWN** |
| Risk | Proxy changes host timing → target (e.g. HU) aborts; “fault” attributed to DUT wrongly. |
| Mitigation | Gate 4 baseline: with/without proxy comparison on a **control host** before vehicle claims. |

## R7 — PSRAM misuse

| | |
|-|-|
| Status | **REJECTED** “PSRAM always on”; **REJECTED** “PSRAM heals coex” |
| Risk | Builds without correct memory_type; DMA buffers in PSRAM causing corruption. |
| Mitigation | Board matrix (N8R2 QSPI vs R8 OPI); DMA in internal RAM; PSRAM for journals only. |

## R8 — Timestamp comparability

| | |
|-|-|
| Status | **UNKNOWN** without sync |
| Risk | Timeline UI implies µs alignment that does not exist. |
| Mitigation | Label `timestamp_domain`; show per-node axes until sync protocol MEASURED. |

## R9 — Board dual-USB topology

| | |
|-|-|
| Status | **FACT** (YD-ESP32-S3 Dual-USB-C lab boards): USB = OTG, COM = UART bridge (CH343P class) |
| Risk | Operator plugs analyzer cable into COM port expecting OTG. |
| Mitigation | Label ports in docs/UI; health reports which PHY/controller is active. |

## Debug path recommendation (Gate 0)

```text
ESP32-S3 analyzer node
 ├── USB OTG  → USB Side A/B under test
 ├── UART     → Debian serial / logs
 └── WiFi    → Debian control + capture
```

Advanced:

```text
ESP32-S3
 ├── OTG + external PHY → analyzer USB
 └── USB Serial/JTAG   → debug (optional)
```
