# ESP32.esp-hub baseline (verbindliche Stufe A)

**Source of truth:** [MPunktBPunkt/ESP32.esp-hub](https://github.com/MPunktBPunkt/ESP32.esp-hub) `esp-hub-base` **v1.10.5**  
**Policy:** [ROLLOUT_DERIVED Stufe A](https://github.com/MPunktBPunkt/ESP32.esp-hub/blob/main/docs/update/ROLLOUT_DERIVED.md) — Core **vendorn/syncen**, nicht neu erfinden.  
**Vendored path:** `firmware/analyzer-node/src/esphub/` (Header „Base repository…“ behalten).

## Drei Ebenen (orthogonal)

```text
Hub (ioBroker :8093)     ← Heartbeat / Rename / OTA-Push   (FW-Verteiler, Projekt-Switch)
Debian (:9750/:8080)     ← Discovery / Session / Lab-UI    (Analyzer Control)
ESP-A ══ USB/TCP ══ ESP-B                                  (Data / später STORE_FORWARD)
```

| Ebene | Rolle | Stört USB-Messung? |
|-------|--------|-------------------|
| **Hub** | FW-Verteilung zwischen Projekten | Nein strukturell (WiFi, selten); Coex wie jedes WiFi |
| **Debian** | Analyzer control / observe | Nein (kein Data-Plane) |
| **USB/TCP** | Capture / Tunnel | Messpfad |

Capture-Session: Hub-Heartbeat ggf. später pausieren (Gate 5+ Timing) — nicht Stufe-A-Blocker.

## Stufe A — vendored

| Component | Use in analyzer-node |
|-----------|----------------------|
| `DiagLog` / `DiagSaved` / `PanicTail` / `Checkpoint` / `Recovery` / `ProgressWatch` / `BootContext` | Boot + post-mortem + `/api/diag/*` |
| `HardwareInfo` / `CpuLoad` | `/api/status` + HELLO telemetry |
| `OtaService` / `WebUi` | gzip UI, `/ota-upload`, Hub-`otaUrl` |
| `HubClient` | Register/heartbeat to ioBroker Hub |
| `ModuleManager` / `BaseModule` | Optional app modules (role status); USB nodes still not modules |

**Not yet (Stufe C/D):** full `AppCore` lifecycle; USB Host/Device as `BaseModule`. Orchestrator remains `main.cpp` + `NetBootstrap` + `ControlClient`.

## Sync rule

1. Edit/fix only from `ESP32.esp-hub/esp-hub-base/src/core/`
2. No silent divergence in `src/esphub/`
3. Builds must succeed with vendored copy only (no live clone required at CI time)
4. GPL-3.0 attribution on copied files; project license note in README

## Do not copy blindly (architecture)

| Hub assumption | Analyzer reality |
|----------------|------------------|
| Single product talking only to Hub | Hub **and** Debian control plane |
| Heartbeat as primary app protocol | Dedicated discovery/session/capture to Debian |
| Generic IO dashboard | USB timeline / MITM lab UI (Debian `:8080`) |
| Optional USB flash via adapter | Analyzer OTG = DUT path; debug = UART/COM |

## Defaults (lab)

- `HUB_HOST` `192.168.178.113` · `HUB_PORT` `8093`
- Debian controller `192.168.178.187:9750` / UI `:8080`
