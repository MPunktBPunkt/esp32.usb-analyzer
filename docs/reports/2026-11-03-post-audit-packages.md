# Post-Audit Packages — 2026-11-03

Nach technology-audit `esp32-usb-analyzer-2026-0.0.50` (closed). Soft-Lab-Pakete umgesetzt und getestet; HW/Feld bewusst offen.

## Geliefert (Software)

| Paket | Artefakt | Lab-Status |
|-------|----------|------------|
| Version matrix | `lab_version_matrix.py` · `LAB_VERSION_MATRIX_MEASURED` | **OK** — Host/Device 0.0.50 |
| Power-cycle sticky | `lab_power_cycle_sticky.py` | **BLOCKED** — `.89` trägt analyzer (Name „Hu-probe“ ist NVS-Display); braucht hu-probe-OTA + Operator-Netzteil |
| Heap soak | `lab_heap_soak.py` | **OK 1000/1000** · wall≈29.6 min · class=stable · fail=0 · `LAB_HEAP_SOAK_MEASURED` |
| msc_reads Semantik | `LAB_MSC_READS_SEMANTICS.md` + Stimulus-Tool | **docs+code** (Live-hu-probe-Diff braucht DUT-Swap) |
| Release gate | `release_gate_capability.py` | **OK 7/7** (`LAB_RELEASE_GATE_MEASURED`) |
| SF discrimination | `lab_sf_discrimination.py` | **OK** — USB≈888–914 vs TCP≈168–242 kB/s · `sf_warranted=true` |
| Control-host Observer | `lab_observer_control_host.py` | **OK** checklist · topology=`otg_loop_analyzer` |
| Path-B soft prep | `lab_path_b_bringup_dryrun.py` | **OK** |
| VBUS checklist | `docs/hardware/VBUS_SWITCH_CHECKLIST.md` | docs only |

## Bewusst nicht „fertig implementiert“

| Thema | Grund |
|-------|--------|
| STORE_FORWARD SF-0…5 (PSRAM rings) | Architektur-Vertrag; Discrimination MEASURED — Rings = eigener FW-Zyklus |
| Path-B Hi-Z Tap gelötet | Hardware |
| VBUS-Switch gelötet | Hardware |
| Transparent MITM PC↔Stick | Verkabelung + Diff (Checkliste bereit) |
| HU/Feld NBT | Fahrzeug |

## Operator next

1. Optional: hu-probe auf `.89` OTA → `lab_power_cycle_sticky.py` (soft + Netzteil).  
2. Danach Analyzer Device wieder auf `.89` für OTG-Capability.  
3. Path-B BOM löten → Bring-up über bestehende Dry-Run-Checkliste.  
4. FW-Tag nur nach grünem `release_gate_capability.py`.
