# Cross-Repo Lab-Bericht — 2026-11-02

Analyzer **0.0.48–0.0.49** als kontrollierter USB-Host gegen **esp32.pidrive** und **hu-probe**.

## Berichte in den Schwester-Repos

| Repo | Bericht |
|------|---------|
| **hu-probe** | [`docs/feld/2026-11-02-lab-usb-analyzer-crosscheck.md`](https://github.com/MPunktBPunkt/hu-probe/blob/main/docs/feld/2026-11-02-lab-usb-analyzer-crosscheck.md) |
| **esp32.pidrive** | [`docs/lab/2026-11-02-lab-usb-analyzer-crosscheck.md`](https://github.com/MPunktBPunkt/esp32.pidrive/blob/main/docs/lab/2026-11-02-lab-usb-analyzer-crosscheck.md) |
| **pidrive** (Pi, Kurzverweis) | [`docs/betrieb/2026-11-02-esp-usb-analyzer-crosscheck.md`](https://github.com/MPunktBPunkt/pidrive/blob/main/docs/betrieb/2026-11-02-esp-usb-analyzer-crosscheck.md) |

## Lokale Evidence

- `docs/evidence/LAB_PIDRIVE_FOREIGN_DUT_MEASURED.md`
- `docs/evidence/LAB_HUPROBE_AOA_FOREIGN_DUT_MEASURED.md`
- `docs/evidence/LAB_BMW_HU_STIMULUS_PIDRIVE_MEASURED.md`
- `docs/evidence/LAB_BMW_HU_STIMULUS_HUPROBE_MEASURED.md`
- `docs/evidence/LAB_HUPROBE_USB_GADGET_NVS_STICKY.md`
- Tool: `tools/lab_bmw_hu_stimulus.py` · API: `POST /api/lab/bmw-hu-stimulus`

## Headline-Findings

1. **pidrive:** MSC-Instrumentierung sieht Analyzer-Host (byte-exact); Bugs: `imageBytes`, Status-`version`, USB-Strings.  
2. **hu-probe:** AOA Lab A/B OK; **`usb_gadget` NVS nicht sticky** (MSC-Tests blockiert); unter AOA korrekte Null-Counter.  
3. Methode löst „blind vs. HU schweigt“ für L1/SCSI-Orakel.
