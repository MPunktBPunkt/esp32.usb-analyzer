# Capability Audit — usb-analyzer 0.0.50

**Datum (UTC):** 2026-11-02T19:57Z  
**Zweck:** Belastbare Messbasis für ein externes/internes Audit — was der Lab-Aufbau **jetzt** beweisen kann.  
**Claim:** `lab_capability_audit_0_0_50`  
**Einstufung:** MEASURED Lab · **nicht** USB-IF · **nicht** Path-B Live-L0 · **nicht** BMW-HU-Wire

---

## 1. Testumgebung (reproduzierbar)

| Komponente | Wert |
|------------|------|
| Host ESP `.88` | analyzer-node **0.0.50** · role `host` · sketch 1545824 B |
| Device ESP `.89` | analyzer-node **0.0.50** · role `device` · sketch 1446016 B |
| DUT auf Host | `303a:1001` · MPB/ANALYZER · FS · serial `lab-gadget` |
| Kabel | USB-C OTG Host↔Device |
| Controller | Debian `:8080` · **0.0.41** |
| Harness | `tools/lab_capability_audit.py` (+ `lab_baseline_campaign.py`, `lab_gate10_perf_campaign.py`) |

### Pflicht-Voraussetzung

Beide Knoten **analyzer-node** derselben Version. Fremd-DUT (hu-probe/pidrive) beweist Visibility — **nicht** `lab_sector_v1`-Pattern-Rate.

```bash
ENV=analyzer-s3-host  ./tools/flash_lab_ota.sh 192.168.178.88
ENV=analyzer-s3-device ./tools/flash_lab_ota.sh 192.168.178.89
python3 tools/lab_capability_audit.py          # Gate10 inklusive
python3 tools/lab_baseline_campaign.py         # volle MSC/TCP-Tabelle
```

---

## 2. Frische Messungen (dieser Lauf)

### 2.1 Capability Audit — **7/7 OK**

Evidence: `docs/evidence/LAB_CAPABILITY_AUDIT_MEASURED.md`  
JSON: `docs/evidence/LAB_CAPABILITY_AUDIT_20261102T195731Z.json`

| Test | Ergebnis |
|------|----------|
| Enum observe | addr=16 · cfg=1 · DUT present |
| MSC OTG sweep | peak **917 kB/s** · pattern_ok alle |
| TCP integrity | **158 kB/s** sample · gaps=0 |
| Controller sync | ok · residual_us≈6952 (Offset-Schätzung, **kein** Hard-µs-Lock) |
| BOT class-hook | CBW/CSW `class_hook_mscd_xfer` OK |
| Gate10 campaign | **9/9** (eingebettet) |
| Path A rejected | GPIO19/20 ≠ FS-Sniffer |

### 2.2 Baseline-Kampagne — Pattern-Verlust **0**

Evidence: `docs/evidence/LAB_BASELINE_MEASURED.md`  
JSON: `docs/evidence/LAB_BASELINE_20261102T195715Z.json`

| Metrik | MEASURED |
|--------|----------|
| USB peak | **917 kB/s** (chunk=8) |
| USB sustained 3× | **907 / 911.7 / 914 kB/s** · failB=0 |
| USB RTT p50 1-sector | ~800 µs |
| USB RTT p50 8-sector | ~4.2 ms |
| TCP peak | **332 kB/s** (2048 B × 64) |

### 2.3 Gate 10 Perf — **9/9 OK**

Evidence: `docs/evidence/LAB_GATE10_PERF_CAMPAIGN.md`  
JSON: `docs/evidence/LAB_GATE10_PERF_CAMPAIGN_20261102T195737Z.json`

Abgedeckt: Enum, MSC, TCP, L0-Feasibility (Path A negativ), E1.1 GPIO-Sample (~2.1 Msps, 0 nutzbare Edges), Path-B GPIO Lab (Self-loop), L0-Sim-Pipeline, Observer-Proxy-Baseline, EP0 Ctrl-Neg 8/8.

---

## 3. Was der Aufbau beweisen kann (Audit-Matrix)

| Fähigkeit | Status | Instrument |
|-----------|--------|------------|
| FS Host↔Device MSC READ mit Byte-Pattern | MEASURED | `/api/lab/usb-host-probe` |
| Throughput / RTT-Perzentile am Kabel | MEASURED | baseline + audit sweep |
| WiFi TCP Frame-Integrität ESP↔ESP | MEASURED | `/api/lab/integrity` |
| Enum-Outcome (addr/cfg/GET_DESC) | MEASURED | `/api/lab/usb-enum-observe` |
| BOT CBW/CSW aus TinyUSB class-hook (nicht App-Fiction) | MEASURED | `lab_bot_class_hook_verify.py` |
| EP0 Negativfälle | MEASURED | `/api/lab/usb-ctrl-neg` |
| Observer-Effekt Proxy vs Direct (Loop) | MEASURED | `lab_observer_effect.py` |
| Path-A „GPIO am OTG = Sniffer“ | **REJECTED** | l0-feasibility + E1.1 |
| L0-Decoder + Correlator-Pipeline | MEASURED synth/GPIO | Gate10 / soft Path-B |
| Fremd-DUT Visibility (pidrive/hu-probe) | MEASURED (andere Evidence) | Stimulus + DUT-Counter |
| Passives Live-D+/D− | **nicht** | Path-B HW offen |
| Echtes HU/NBT-Wire | **nicht** | Feld offen |

---

## 4. Vergleich zu älterer Baseline (Orientierung)

| | 0.0.13/14 (2026-10-31) | **0.0.50 (heute)** |
|--|------------------------|---------------------|
| USB peak | ~934 kB/s | **917 kB/s** |
| USB sustained | ~934–937 | **907–914** |
| Pattern loss | 0 | **0** |
| TCP peak | ~139 kB/s | **332 kB/s** |

USB bleibt im gleichen Band; TCP höher (Pipeline/Stack-Fortschritt). Kein Anspruch auf Identitätsgleichheit der Runs.

---

## 5. Not claimed (verbindlich für Auditor)

1. Kein passiver Live-L0-Sniffer.  
2. Kein gelötetes Path-B-Frontend.  
3. Soft-Reenum / Soft-USB ≠ VBUS-Unplug.  
4. Controller-Sync = Session-Offset-Schätzung, kein Hard-µs-Lock.  
5. Observer-Proxy-Zahlen gelten für OTG-Loop, nicht für PC↔Stick-MITM.  
6. Keine USB-IF-Compliance, kein HS, kein BMW-Feldbeweis in diesem Bericht.

---

## 6. Empfehlung für das Audit

1. Diesen Bericht + JSON-Rohdaten als **Ist-Stand Lab-Fähigkeit 0.0.50** werten.  
2. Harness `lab_capability_audit.py` als **Regressionstor** vor jedem Release wiederholen.  
3. Nächste Erkenntnislücke bewusst **außerhalb** dieses Soft-Perf-Raums: Path-B-Hardware und Feldabgleich — nicht weitere MSC-Sweeps.

**Ende Audit-Bericht.**
