# Maßnahmen nach Feldmatrix-Review · 2026-11-06

Auswertung von Mistral / GPT / Grok zu Commit `688a40b` und den Feldmessungen.  
Quellen: Feldbericht, Evidence, Rohdaten; Abgleich mit Code (`CaptureLog::kCapacity=256`, `kWriteCache=64` Ring-Eviction).

---

## Konsens der drei Reviews

| Punkt | Urteil |
|-------|--------|
| Session | Meilenstein: Analyzer als **Feld-Messinstrument** an Fremd-Hosts |
| Größter Gewinn | **Host-Fingerprint** (nicht nur „AOA=0“ oder „6 Hosts“) |
| BMW | P-MOUNT real bestätigt; Soft-Reenum/Remount nützlich |
| AOA | Identity-getrieben; `303a:1001` ≠ Connected-Apps-Probe |
| Write-Cache | 106 Sektoren vs. 64 Slots = **Messfrage**, nicht bewiesenes Datenloch |
| Capture | Ring-Overflow (`dropped≈2476`) = technischer Engpass |
| NEXT | **nicht** mehr generische Hosts → Identity/Content am BMW + Metriken |

---

## Maßnahmen (Priorität)

### P0 — Feld / Operator (nächste Auto-Session)

| ID | Maßnahme | Warum | Done wenn |
|----|----------|-------|-----------|
| **P0-A** | **BMW C0** — gleiche Hardware, Identity `303a:1001`, aber **echtes FAT16/32** (pidrive oder Image mit `/MUSIC/*.MP3`) | Identity vs. Content trennen; UI-Stille erklären | MEASURED: SCSI/LBA/READ-Größen + Operator UI ja/nein |
| **P0-B** | **BMW C1** — hu-probe **`aoa_phone_like` / `aoa_observe`** (Erfolgssignatur) auf `.89` | Connected-Apps-Gate; AOA51+ oder klare Negativmessung | MEASURED aoa51/52/53 + AAIdrive USB-Outcome |
| **P0-C** | **Self-powered Hub** Phone → `.88` Host | schließt VBUS-Lücke vor MITM | DUT present / Enum MEASURED |
| **P0-D** | C0 und C1 **nicht mischen** (eine Variable pro Steckversuch) | sonst wieder „beides falsch“ | Evidence mit klarer Topology-Zeile |

Reihenfolge: **C0 (FAT) und C1 (AOA) parallel planbar**; MITM/Phone erst nach C1-Positiv + P0-C.

### P0 — Firmware / Lab (ohne neue Hardware)

| ID | Maßnahme | Warum | Done wenn |
|----|----------|-------|-----------|
| **P0-E** | **Persistente SCSI-Counter + Histograms** neben dem Ring: Opcode-Counts, READ10 transfer-length hist, grobe LBA-Buckets, Inter-Arrival optional, `clear`/`status` API | Overflow verliert Historie; Fingerprint braucht Aggregate | `/api/status` oder `/api/lab/scsi-stats` zeigt Counts auch bei `dropped>0` |
| **P0-F** | **Write-LBA-Audit**: pro WRITE LBA, n, hash/kurz, cache hit/miss, **evict-counter**; optional letzte N LBA-Liste | 54 KiB vs. 32 KiB Cache — GPT/Grok: Eviction nicht claimen ohne Audit | MEASURED: unique LBAs, evictHits, Replay-Read außerhalb Cache → Pattern? |
| **P0-G** | Capture-Ring **nicht** blind auf 2k Events aufblasen als einzige Lösung | RAM knapp; Counter lösen Fingerprint besser | P0-E geliefert; Ring evtl. leicht erhöhen nur als Bonus |

### P1 — Nach C1 Positiv

| ID | Maßnahme | Warum |
|----|----------|-------|
| **P1-A** | USB-C1 MITM: Phone ↔ Analyzer ↔ HU (Forward + Capture) | BCL-Transport sichtbar machen |
| **P1-B** | BCL framing capture / Decoder-Anbindung | Connected-Apps Control Plane |
| **P1-C** | CLEAR_HALT / CSW-residue gezielt vs. Opcode korrelieren | Host-Recovery-Signatur (nicht als Bug claimen) |
| **P1-D** | Host-Fingerprint-Metriken in Evidence-Schema: mount latency, remount count, READ size hist, WRITE sectors | Matrix skalierbar halten |

### P2 / Park

| ID | Maßnahme |
|----|----------|
| Weitere generische Hosts (Soundbar², …) | geringer Grenznutzen |
| L0/Path-B | parallel nur wenn HW da; nicht Blocker für C0/C1 |
| Cache blind vergrößern | erst nach P0-F |
| „BMW kann kein AOA“ claimen | REJECTED ohne C1 |

---

## Explizite Korrekturen an den Reviews (verbindlich für NEXT)

1. **Mistral „Datenloch“** → umklassifiziert zu **HYPOTHESIS**; erst P0-F entscheidet MEASURED/REJECTED. Code: Ring-Eviction überschreibt Slot ohne Zähler (`storeWrite`).
2. **„AAIdrive MEASURED gap“** → nur als **Negativ unter MSC-Identity**; kein AAIdrive-Protokoll-Claim (alle drei Reviews einig).
3. **AOA=0** → „kein ACCESSORY_START gegen diese Identity“, nicht „Host unterstützt kein AOA“.
4. **4 KiB-Raster** → künftig als Metrik führen (GPT), nicht nur Bytes/Reads-Quotient als Paketgröße lesen.

---

## Roadmap (eine Zeile)

```text
Host-Matrix ✅
    → P0-E/F (Counters + Write-Audit)   [Software, sofort]
    → BMW C0 FAT (303a:1001)            [Feld]
    → BMW C1 AOA identity               [Feld]
    → Hub + Phone enum                  [Infrastruktur]
    → USB-C1 MITM → BCL → Etch          [erst nach C1]
```

---

## Empfohlene Reihenfolge der Umsetzung (Engineering)

1. **Diese Woche Software:** P0-E (scsi-stats) + P0-F (write audit counters) — billig, rettet nächste HU-Session.
2. **Nächste Feld-Session:** P0-A C0 FAT **oder** P0-B C1 AOA (Operator wählt; ideal beide nacheinander).
3. **Hub beschaffen/stecken:** P0-C bevor Phone↔HU MITM.
4. **Erst dann** Proxy/BCL-Experimente (P1).

---

## Claim-Grenze dieses Dokuments

Maßnahmenplan / INFERENCE aus Reviews. Keine neuen Lab-FACTs außer dem Code-Abgleich:
- `CaptureLog::kCapacity = 256`
- `kWriteCache = 64` mit Round-Robin-Slot-Overwrite ohne `evict`-Counter
