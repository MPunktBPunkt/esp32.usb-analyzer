# Übergabebericht — esp32.usb-analyzer v0.0.46

| | |
|--|--|
| **HEAD** | `fea37488d171826bc9838921ebf3b8cbcac4f247` |
| **Tag/Version** | `0.0.46` (`VERSION`, `FW_VERSION`) |
| **Datum** | 2026-11-01 |
| **Lab** | Host `.88` · Device `.89` · OTG-Loop |
| **Repo** | `MPunktBPunkt/esp32.usb-analyzer` |
| **Vorgänger-Analysen** | Mistral-Übergabe + GPT-Gegenlesen (beide Richtung korrekt; Korrekturen in §8) |

---

## 0. Ein-Satz-Urteil

**Sehr gut instrumentierter ESP32-S3 L1/L2-Laboranalysator mit vollständig vorbereitetem L0-Decoder und Path-B-Datenpfad; die physische passive FS-Wire-Erfassung (und damit alle echten L0-/HU-Wire-Claims) ist weiterhin offen.**

Die Restblindheit ist **lokalisiert** (Hardware-Frontend), nicht diffus. Software-seitig gibt es noch billige Lücken (Fuzz, Sense-Isolation, Fremdgeräte, Soak) — aber **keinen** Ersatz für Path-B-Samples.

---

## 1. Architektur der Beobachtungsebenen (verbindlich)

```text
USB physical wire (D+/D−)
        │
   ┌────▼────┐
   │   L0    │  PID · Token · DATA · Handshake · CRC · order
   └────┬────┘  ← Path B Hi-Z (NICHT gelötet) · Path A FALSIFIZIERT
        │
   USB controller / PHY
        │
   ┌────▼────┐
   │ L1 / L2 │  EspUsbHost · TinyUSB · HPRT · class hooks
   └────┬────┘  ← hier ist das Lab stark
        │
   ┌────▼────┐
   │ Class   │  MSC BOT/SCSI · HID · AOA/Vendor · BCL lab
   └─────────┘
```

**Stack-OK ≠ Wire-Fakt.**  
`GET_DESCRIPTOR OK` ≠ SETUP/DATA/ACK auf dem Bus.  
Class-Hook-CBW ≠ DATA0+CRC.  
Synth/GPIO-L0 ≠ passives Live-L0.

---

## 2. Was wir messen können (Messinventar)

Evidence: `docs/evidence/LAB_*.md` (20 Dateien) · Tools: 29× `tools/lab_*.py` · Units: `tests/host/`.

### 2.1 L1/L2 Stack — 🟢 MEASURED

| Bereich | Was | Evidence / Tool |
|---------|-----|-----------------|
| Enum-Outcome | addr≠0, config≠0, GET_CONFIGURATION, soft-reenum | `LAB_ENUM_HID_MEASURED` · `lab_usb_enum_observe.py` |
| EP0 positiv/negativ | GET_DESCRIPTOR + 5 Negativfälle (bad type, bad bRequest, wLength=0, …) | `LAB_SUSPEND_CTRLNEG_MEASURED` · `lab_usb_ctrl_neg.py` |
| EP0 forward (Lab) | WiFi `kCmdCtrl*` Device↔Host | `lab_usb_ctrl_loop_verify.py` — **nicht** SETUP-MITM |
| Bus-Events Device | mount/unmount/lastBusEvent | Wire-Stimulus-Kampagnen |
| Suspend/Resume | Host HPRT `prtsusp` set/clear + Post-Resume GET_DESCRIPTOR | `LAB_SUSPEND_CTRLNEG_MEASURED` |
| MSC READ/WRITE | Pattern, Rate ~800–1000 kB/s, RTT p50/p95 | `LAB_BASELINE_MEASURED` · `lab_usb_loop_probe.py` |
| BOT CBW/CSW | TinyUSB class-hook byte-exact (nicht App-`onRead`-Fiction) | `lab_bot_class_hook_verify.py` · Gate 6 |
| HID Interrupt | Device inject → Host callback / capture | `LAB_ENUM_HID_MEASURED` |
| AOA 51→52→53 | Lab-Responder + optional soft-reenum `18D1:2D00` | `LAB_AOA_HANDSHAKE_MEASURED` |
| Vendor bulk echo | nach START | dieselbe Evidence |
| BCL lab framing | 8-byte BE, HANDSHAKE/DATA-Serie, Host-Units | 0.0.33/0.0.34 · `tests/host/test_bcl_lab_frame.cpp` |
| Gate 9 Fault | delay (~80 ms→readMs≈82), fail→recovery, disconnect→re-enum | `LAB_GATE9_FAULT_MEASURED` · `lab_fault_inject.py` |
| Observer-Effekt | Direct ~844 kB/s / ~9.5 ms vs Proxy ~25 kB/s / ~160 ms | `LAB_OBSERVER_EFFECT_BASELINE` |
| TCP pipeline | ACK-each ~140 → windowed ~260–360 → pipeline ~436 kB/s | `LAB_TCP_PIPELINE_MEASURED` |
| Perf-Kampagne | Sweeps + Gate10-Perf | `LAB_GATE10_PERF_CAMPAIGN` |

### 2.2 L0 Software-Pipeline — 🟢 MEASURED (synth / GPIO / stimulus)

| Stück | Status | Claim-Grenze |
|-------|--------|--------------|
| NRZI + Bit-Stuff + PID/Token/DATA/Handshake | Unit-getestet | Decoder verarbeitet Samples — **nicht** „Bus gesehen“ |
| CRC5 / CRC16 / DATA0/1 | Unit-getestet | dito |
| `.l0samp` + L0ST `:9760` ingest | MEASURED | sim/GPIO-Quelle |
| Sampler-Sim + live pipeline | MEASURED | `lab_l0_live_pipeline.py` |
| Correlator L0↔L1 | synth + soft campaign; Klassen `true_gap_n` / `handshake_expected_n` | kleines n (siehe B4) |
| Path-B GPIO lab (GPIO10/11) | Self-loopback L0ST ~88.5 kHz | **nicht** OTG-PHY |
| GPIO-Methoden-Suite | calibrate, gap/glitch/jitter, error matrix, idle hist, cross-link infra | `jumperLikely=false` ohne Drähte |
| Wire-Stimulus GPIO19/20 | destruktiv, confirm-gated; LS-Versuch ~382 kHz | Host-Stack gestoppt; Reboot heilt |
| L0∖L1 Stack-Gap (Stimulus) | 5 L0 / 0 L1-Matches | **Stimulus-GT**, kein Live-Sniffer |
| Soft Path-B Campaign | synth/GPIO vs live Host L1 | `LAB_PATH_B_SOFT_CAMPAIGN_MEASURED` |
| Path-B Bring-up Dry-Run | Units + Fixture + Checklist | bereit für HW-Ankunft |

### 2.3 Path A — 🔴 FALSIFIZIERT (nicht weiterverfolgen)

| Messung | Wert |
|---------|------|
| E1.1 Peek-Poll GPIO19/20 unter Live-OTG | ~**2.1 Msps**, **0 Edges**, Jitter pp ~5.9 µs |
| FS Bedarf | 12 Mbit/s · sinnvoll ≥**24 MS/s** Oversampling |
| Urteil | CPU-GPIO als FS-Passivsniffer **REJECTED** |

### 2.4 Explizit MEASURED Gap (L1)

**REQUEST_SENSE nach Fail:** Device CSW FAILED + TinyUSB `medium_not_present`; Host `fault-observe` oft `senseKey=0/ASC=0` obwohl `senseOk` nach Retry true sein kann (leerer Sense nach EspUsbHost BOT-Reset). **Nicht isoliert** (Device vs Host vs Timing) — siehe NEXT P0c.

---

## 3. Wo wir blind sind (Priorität)

| ID | Blindheit | Warum es zählt | Geschlossen durch |
|----|-----------|----------------|-------------------|
| **B0** | **Passives L0 am echten Bus** | Kernfrage des Projekts | Path-B Hi-Z + Sampler ≥24 MS/s |
| **B1** | SETUP/DATA/ACK/SOF/DATA0/1 auf dem Wire | Mid-Enum & Traffic unsichtbar | B0 |
| **B2** | Elektrik (Idle-Pegel, SI, VBUS, Resume-K) | Suspend nur HPRT-Stack | B0 + ggf. Referenzinstrument |
| **B3** | SET_ADDRESS / SET_CONFIGURATION paketweise | Enum nur Endzustand | B0 |
| **B4** | Korrelation kleines n (3 Pakete / 2 Matches soft) | Pipeline ja, Quote nein | echte `.l0samp`, n≥100 |
| **B5** | REQUEST_SENSE-Verlustort | Host API oft leer | Isolationstest (ohne HW) |
| **B6** | Gerätediversität n=1 (nur OTG-Loop-Partner) | Stack-Sonderfälle | Fremd-FS-Stick/Maus/Hub |
| **B7** | True MITM (PC↔Stick) | Proxy nur Loop | Verkabelung + Observer erneut |
| **B8** | Harte µs-Sync L0↔L1 | nur Offset-Schätzung | Sync-Protokoll + Path B |
| **B9** | BMW/HU / BCL-on-wire / HS | geparkt | nach B0 + Session-Profil |

**Nicht blind (häufige Fehlannahme):** Decoder, Ingest, Correlator-API, Claim-Disziplin, Gate-9-Basics, Observer-Delta im Loop.

---

## 4. Testmethoden

### 4.1 Vorhanden

1. Host-Unit-Tests (`usb_fs_l0`, BCL-Framing)
2. Lab-Integration (29 `lab_*.py`, dual-ESP HTTP)
3. Negativ-Suiten (EP0-Neg, Gate 9 delay/fail/disconnect)
4. Destruktiver Wire-Stimulus (confirm-gated + Reboot-Heal)
5. Synth/Offline-Korrelation + Soft-Campaign
6. Evidence-Kampagnen (JSON + Claim + Not-claimed)
7. Observer-Effekt-Baseline (quantifiziert)
8. Performance-/TCP-Sweeps
9. Path-B Bring-up Dry-Run (HW-Checkliste)

### 4.2 Noch nicht / schwach genutzt

| Methode | Nutzen | Aufwand | HW? |
|---------|--------|---------|-----|
| **Property-based Fuzz** (hypothesis) für Decoder/Ingest/BCL | verhindert „Wire-Anomaly“-Fehldeutung | gering | nein |
| **Golden-Capture-Regression** (`.l0samp` + expected.json) | Decoder drifts | gering | nein (synth jetzt; live später) |
| **REQUEST_SENSE Isolation** (Device- vs Host-Trace getrennt) | schließt B5 | gering | nein |
| **Fremdgeräte** (2–3 FS-Sticks, Maus, Hub) | hebt B6 | gering | vorhanden/leihen |
| **Soak + Hotplug** (n≥100 attach/enum/read/detach) | Leaks, stale handles | mittel | nein |
| **Statistik** n≥30 Latenz/Recovery/Korrelation | belastbare Quantile | gering | nein |
| **Differential Decoder** (eigener vs usbmon/sigrok/Wireshark) | Orakel für L0 | mittel | Path B oder Capture-Share |
| **Externes Referenzinstrument** | kalibriert Frontend | einmalig | **ja** — nicht „irgendein 24‑MHz-LA“; FS-Receiver/Comparator + ≥24 MS/s (BOM in `L0_CAPTURE_POC.md`) |
| **USBCV / Chapter-9 Orakel** | externe Enum/Control-Validierung | mittel | PC |

**Korrektur zu „billiger 24‑MHz-Logic-Analyzer löst B0“:** hilfreich als Hilfsinstrument, **kein** Ersatz für Path-B-Frontend. Sample-Rate, Threshold, differenzielles D+/D− und Speichertiefe entscheiden — BOM bleibt maßgeblich.

---

## 5. NEXT für die nächste KI (Priorität)

### P0 — ohne neue Hardware (jetzt)

1. **Property-based Fuzz** — `tools/usb_fs_l0/*` + BCL: truncated, bad CRC, illegal PID, missing/double EOP, garbage `.l0samp`, partial L0ST frames.
2. **Golden fixtures erweitern** — synth + bekannte bad-CRC/glitch Captures als Regression.
3. **REQUEST_SENSE isolieren** — Device: CSW/Sense-Quelle loggen; Host: BOT-Reset, Sense-CBW, returned bytes getrennt; Evidence `LAB_GATE9_SENSE_ISOLATION`.
4. **Hotplug/Soak-Kampagne** — n≥50–100; Heap/PSRAM/mount-Mismatch/recovery-ms.
5. **Gate-9 erweitern (Stack)** — mid-xfer drop, explicit STALL, CLEAR_HALT vs BOT Reset (soweit EspUsbHost erlaubt) — **ohne** Wire-CRC-Claims.

### P1 — sobald Hardware da / vor Ort

6. Path-B Hi-Z nach BOM löten; Bring-up über `lab_path_b_bringup_dryrun.py` Checklist.
7. Erste echten `.l0samp` → `lab_l0_live_pipeline.py --source ingest`.
8. Live-Korrelation n≥100; Match-Quote + `true_gap_n` unter MSC/EP0.
9. Direction-Rekonstruktion (Host→Dev vs Dev→Host) in Evidence **explizit**.
10. Optional: Referenzdecoder-Diff auf demselben Capture.

### P2 — danach

11. True MITM: FS-Stick am Host, Device→PC; Observer-Effekt erneut.
12. Fremdgeräte-Diversität systematisch.
13. Erst dann 10G BMW/HU — mit Session-Profil, **keine** Lab-Zahlen als HU-FACT.

### Nicht jetzt (Gate-Freeze)

CDC · UVC · Isochronous · HS · mehr AOA/BCL-Lab-Spiel · Web-UI ohne neue Messquelle · Path-A-Retry · FPGA Path C (nur wenn B Rate/Jitter unzureichend).

---

## 6. Harte Regeln (unverändert)

1. Claim-Klassen: `FACT` / `MEASURED` / `INFERENCE` / `HYPOTHESIS` / `UNKNOWN` / `REJECTED` + **Not claimed**-Zeile pro Evidence.
2. **Niemals claimen** bis MEASURED: transparentes MITM, passiver Live-L0-Sniffer, HS, HU-Wire, L0≡L1-Gleichheit, USB-IF, SOF-Elektrik, Lab-AOA/BCL = BMW-BCL.
3. Destruktive Stimuli nur mit `confirm=…`; danach Reboot/Heal verifizieren.
4. Unit-first für `tools/usb_fs_l0/` und `firmware/common/`; Lab-Claims nur nach Unit-Pass.
5. Gate-Freeze: 10A–10F + Path B vor CDC/UVC/HS; 10G erst mit gelötetem PHY.
6. Neue MEASURED ggf. nach `docs/EVIDENCE.md` / technology-audit zurückspiegeln.
7. Nach Wire-Stimulus: Host braucht oft **Reboot** (Stack released).

---

## 7. Dateikarte (Schnellnavigation)

| Thema | Pfad |
|-------|------|
| Gate-Übersicht | `docs/GATES.md` |
| Gate 10 Status | `docs/GATE10_STATUS.md` |
| Path-B PoC + BOM | `docs/hardware/L0_CAPTURE_POC.md` |
| Stack-Gap Doku | `docs/hardware/L0_STACK_GAP.md` |
| GPIO-Methoden | `docs/hardware/GPIO_LAB_METHODS.md` |
| Evidence | `docs/evidence/LAB_*.md` |
| L0 Code | `tools/usb_fs_l0/` |
| Lab-Tools | `tools/lab_*.py` |
| FW Host/Device | `firmware/analyzer-node/` |
| Fault API | Device `GET/POST /api/lab/fault-inject` · Host `/api/lab/fault-observe` |
| OTA | `./tools/flash_lab_ota.sh <ip>` · `ENV=analyzer-s3-device` für `.89` |

---

## 8. Korrekturen an Mistral-/GPT-Analysen

Beide Analysen treffen die **Richtung**. Präzisierungen:

| Aussage | Korrektur |
|---------|-----------|
| „28 lab_*.py“ | **29** (inkl. 0.0.46) |
| „Testtiefe 8/10, Epistemik 9/10“ | Brauchbar; physisches L0 eher **2/10**, Diversität/Statistik ~**5/10** (GPT-Matrix) |
| „Logic-Analyzer ~30–60 € klärt B1/B2 sofort“ | **Überzogen** — nur mit FS-tauglichem Frontend/Sampler; sonst Pseudokalibrierung |
| „L0∖L1 beantwortet was an TinyUSB vorbei geht“ | Nur unter **Stimulus**/synth — **nicht** Live-Traffic-Fakt (beide Analysen sagen das irgendwo; nächste KI darf es nicht vermischen) |
| Soft-Campaign „2/3 matched“ | Nach Correlator-Fix: Handshake zählt als `handshake_expected`, nicht als Match — Zahlen in Evidence lesen, nicht aus älteren Chat-Summaries |
| Gate 9 „Sense MEASURED leer“ | Ja; zusätzlich: Recovery nach Fail **ok**; Isolation **fehlt** |
| „nächste große Erkenntnis kommt nicht aus Software“ | **Größtenteils wahr** für L0-Wahrheit; P0-Software (Fuzz/Sense/Soak/Fremdgerät) liefert trotzdem echte L1-Erkenntnisse ohne HW |

---

## 9. Der Meilenstein (nicht verwechseln)

**Nicht:** „Es fehlt nur ein Logic Analyzer.“

**Sondern:** Die **komplette Messkette bis zum Correlator ist vorbereitet**; es fehlt die **physische Quelle** echter passiver L0-Daten.

```text
Heute:   synth/GPIO-L0 ──► decoder ──► correlator ◄── L1 (TinyUSB)
Morgen:  USB BUS ──┬──► L1
                   └──► Path-B L0 ──► decoder ──► correlator
                                              └── true live stack_gap
```

Erst der untere Ast beantwortet:

> Was passiert physikalisch auf dem Bus, das der ESP32/TinyUSB-Stack nicht an uns weiterreicht?

---

## 10. Kurz-Scorecard

| Bereich | Score |
|---------|------:|
| L1/L2 Instrumentierung | 9/10 |
| Testautomatisierung / Evidence-Disziplin | 9/10 |
| Fault/Recovery (Gate 9 Basics) | 8/10 |
| Offline-L0 Decoder + Pipeline | 9/10 |
| Statistik / Gerätediversität | 5/10 |
| Physisches passives L0 | 2/10 |
| True MITM | 2/10 |
| BMW/HU-Wire | 0–1/10 |

**Für die nächste Session ohne HW-Kauf:** P0 aus §5 abarbeiten (Fuzz → Sense-Isolation → Soak/Hotplug → Gate-9-Erweiterung).  
**Sobald Tap da:** P1 Bring-up — nicht neue Klassen, nicht Path A, nicht HU-Claims.
