# Feldbericht — USB-Host-Matrix & Connected-Apps-Lage · 2026-11-06

| | |
|--|--|
| **Datum** | 2026-11-06 |
| **FW** | analyzer-node **0.0.52** · Device `.89` · Host `.88` |
| **Evidence** | [`LAB_FIELD_HOST_MATRIX_2026-11-06_MEASURED.md`](../evidence/LAB_FIELD_HOST_MATRIX_2026-11-06_MEASURED.md) · [`field_host_matrix_2026-11-06/`](../evidence/field_host_matrix_2026-11-06/) |
| **Claim** | `field_host_matrix_analyzer_msc_gadget_2026_11_06` |

---

## 1. Ziel der Session

1. Prüfen, ob sich ein **Messreihe „ESP als Device an Fremd-Hosts“** lohnt.
2. Im Auto: HU an `.89`, Phone an `.88`, AAIdrive / AOA-Zubehörpfad.
3. Danach weitere USB-Hosts (TV, Soundbar, Ergometer, FRITZ!Box, Windows) am gleichen Gadget.

Strategischer Kontext (Analyse Connected-Apps): Fokus künftig USB→BCL→Etch — dafür braucht es zuerst einen **funktionierenden Phone↔HU-Pfad**. Heute war der Gadget-Zustand **Lab-MSC**, nicht die frühere AOA-Erfolgssignatur.

---

## 2. Was wir gemessen haben (kurz)

### 2.1 BMW HU (Auto)

| Beobachtung | Klasse |
|-------------|--------|
| HU mountet Analyzer-Gadget `303a:1001` | MEASURED |
| SCSI P-MOUNT: TUR, MODE_SENSE6, PREVENT, READ_CAPACITY10, READ10 | MEASURED |
| Soft-Reenum → Remount + weitere READs | MEASURED |
| `aoa51/52/53 = 0` trotz AOA-respond | MEASURED |
| Kein erkennbares Media/Zubehör auf dem HU-Display | Operator + MEASURED (nur Stick-Pfad) |
| AAIdrive wartete auf Fahrzeug; USB-seitig kein Connected-Apps-Traffic am Analyzer | MEASURED gap |

**Interpretation:** NBT behandelt das Lab-Gadget als **USB-Stick**, nicht als Phone/AOA-Zubehör. Der frühere Erfolg (Zubehör erkannt + AAIdrive USB-Signatur) braucht die **damalige Identity** (hu-probe `aoa_observe` / `aoa_phone_like`), nicht Analyzer-MSC.

### 2.2 Phone an `.88` (MITM-Versuch)

| Beobachtung | Klasse |
|-------------|--------|
| Host: `waiting_for_dut`, `no_dut` | MEASURED |
| `boardSourcesVbus=false`, `prtlnsts=0` | MEASURED |
| AOA-Handshake / Enum-Observe fail `no_dut` | MEASURED |

**Inference:** Ohne Bus-5 V / self-powered Hub enumeriert das Phone am YD-OTG typischerweise nicht. MITM Phone↔HU war heute elektrisch nicht möglich.

### 2.3 Fremd-Host-Matrix (`.89` Device)

| Host | Ergebnis |
|------|----------|
| **TV** | Mount bei Einschalten; Unmount bei Aus; wenig Traffic (+1 READ in Slice); kein AOA |
| **Soundbar** | Aktiver MSC-Host; Replug + Mount/Unmount; READ10/INQUIRY/…; kein AOA |
| **Ergometer** | **Kein** USB-Host-Verhalten (waiting, keine neuen Reads) |
| **FRITZ!Box** | Klarer Stick-Mount (+37 READs / ~148 KiB); kein AOA |
| **Windows Laptop** | Volle MSC-Nutzung; **Format + Textdatei** → Writes MEASURED; Datei überlebt USB-Replug |

### 2.4 Wo die Windows-Datei liegt

MEASURED/Code: Sektoren liegen im **RAM-Write-Cache** des ESP (`kWriteCache = 64` × 512 B ≈ 32 KiB) in `UsbDeviceNode`.  
USB aus/ein löscht den Cache nicht, solange der ESP eigenversorgt bleibt. ESP-Reset / Strom weg → weg. **Kein Flash-Persist.**

---

## 3. Rohdaten auf GitHub

Unter `docs/evidence/field_host_matrix_2026-11-06/`:

| Ordner / Datei | Inhalt |
|----------------|--------|
| `MATRIX_SUMMARY.json` | Gesamtmatrix |
| `bmw_hu/` | Status, Watch, Fault-Phasen, Capture-SCSI-Ring |
| `tv/` | `tv_measure.json` |
| `soundbar/` | Measure + Replug |
| `ergometer/` | Negativmessung |
| `router_fritzbox/` | Measure |
| `windows_laptop/` | Measure + nach Format/Datei |
| `stick_sim/` | Stick-only Versuch |
| `phone_on_host88_vbus_gap.json` | Phone/VBUS-Lücke |

---

## 4. Erkenntnisse (verdichtet)

1. **Fremd-Host-Messreihe lohnt sich** — in einem Nachmittag: klare Positiv-/Negativ-Hosts, SCSI-Profile, Windows WRITE-Pfad.
2. **Analyzer-MSC ist ein guter Storage-Probe**, aber ein **schlechter Connected-Apps-Probe**.
3. **AOA bleibt der kritische Pfad** für AAIdrive/HU — heute nicht reproduziert, weil Identity falsch war; Operator-FACT aus früherem Lauf bleibt Orientierung.
4. **MITM braucht VBUS-Hub** am Host `.88` plus phone-like/AOA auf `.89`.
5. **UI-Stille** auf HU/TV/Soundbar ≠ kein USB — oft stiller Mount ohne FAT/Media.
6. **L0/Path-B** war heute nicht nötig und nicht gemessen; Session war L1/Gadget-Outcome.

---

## 5. NEXT (aus dieser Session)

Ausführlich nach Mistral/GPT/Grok-Review: [`2026-11-06-field-matrix-actions.md`](2026-11-06-field-matrix-actions.md).

| Prio | Aktion |
|------|--------|
| P0 | **BMW C0:** `303a:1001` + echtes **FAT**/Media (Identity vs Content) |
| P0 | **BMW C1:** hu-probe **AOA-Erfolgssignatur** (`aoa_phone_like` / `aoa_observe`) |
| P0 | Self-powered Hub Phone → `.88` |
| P0 | Software: SCSI-Counter/Histograms + Write-LBA-Audit (Ring-Overflow / Cache) |
| P1 | Erst nach C1: BCL-Capture / MITM (USB-C1) |

---

## 6. Claim-Grenze

Alles hier ist **Feld-MEASURED** am Analyzer-MSC-Gadget 0.0.52 bzw. dokumentierte Negativmessungen.  
Nicht beansprucht: reproduzierter AOA-Erfolg heute, AAIdrive-Funktionsbeweis, BCL/Etch, transparentes MITM, NBT-Wire-FACT.
