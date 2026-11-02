# Cross-Repo Lab-Bericht — 2026-11-02 (aktualisiert Abend)

Analyzer **0.0.50** (`917007a`) · Controller **0.0.41** · hu-probe **0.3.35** (`f440867`) · pidrive **0.4.61-dev** (`dd2594f`).

## Berichte in den Schwester-Repos

| Repo | Bericht |
|------|---------|
| **hu-probe** | [`docs/feld/2026-11-02-lab-usb-analyzer-crosscheck.md`](https://github.com/MPunktBPunkt/hu-probe/blob/main/docs/feld/2026-11-02-lab-usb-analyzer-crosscheck.md) |
| **esp32.pidrive** | [`docs/lab/2026-11-02-lab-usb-analyzer-crosscheck.md`](https://github.com/MPunktBPunkt/esp32.pidrive/blob/main/docs/lab/2026-11-02-lab-usb-analyzer-crosscheck.md) |
| **Handover (nächste KI)** | [`docs/reports/2026-11-02-handover-0.0.50.md`](./2026-11-02-handover-0.0.50.md) |

## Lokale Evidence

- `docs/evidence/LAB_PIDRIVE_FOREIGN_DUT_MEASURED.md`
- `docs/evidence/LAB_HUPROBE_AOA_FOREIGN_DUT_MEASURED.md`
- `docs/evidence/LAB_BMW_HU_STIMULUS_PIDRIVE_MEASURED.md` — byte-exact Host↔DUT
- `docs/evidence/LAB_BMW_HU_STIMULUS_HUPROBE_MEASURED.md` — sichtbar; `read_count` ≠ Host-READ-Zählung (Semantik)
- `docs/evidence/LAB_HUPROBE_USB_GADGET_NVS_STICKY.md` — Defekt vor Fix
- `docs/evidence/LAB_HUPROBE_USB_GADGET_NVS_FIX.md` — Fix + MEASURED sticky inkl. `aoa_phone_like`
- `docs/evidence/LAB_SOFT_UNPLUG_CLAIM.md` — D+ ≠ VBUS
- Tool: `tools/lab_bmw_hu_stimulus.py` · API: `POST /api/lab/bmw-hu-stimulus`

## Headline-Findings (Stand Abend)

1. **pidrive:** Telemetrie-Fixes verifiziert (`imageBytes=16 MiB`, `version=0.4.61-dev`, USB-Produkt `USB Media`); Stimulus byte-exact.
2. **hu-probe:** NVS sticky via `hpgad` — MEASURED für `msc`, `aoa_observe`, **`aoa_phone_like`**; MSC-Stimulus nicht mehr blockiert.
3. **Analyzer:** Stimulus + Sense/MaxLUN/Sync/`bmw_mount_deep`; Lab-UI Actions; Proxy allowlisted (0.0.41).
4. Methode löst „blind vs. HU schweigt“ für L1/SCSI-Orakel — **nicht** für Wire/L0/echtes NBT.

## Not claimed

Path-B Live-L0 · VBUS-Unplug · vollständige HU-Grammatik (kein MODE_SENSE/PREVENT) · bytegenaue hu-probe `read_count`≡Host-READs.
