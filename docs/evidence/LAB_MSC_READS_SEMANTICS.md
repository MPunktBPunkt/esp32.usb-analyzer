# Evidence — hu-probe `msc_reads` vs `read_count`

**Claim:** `huprobe_msc_reads_semantics`  
**Date:** 2026-11-03 · Cross-repo with Analyzer BMW stimulus

## FACT (code)

hu-probe `/api/status` exposes **two** counters:

| Field | Source | Counts |
|-------|--------|--------|
| `msc_reads` | `MscGadget::readCount()` | MSC READ path completions (closer to Host READ10/BOT data stage) |
| `read_count` | `ReadInstrument::readCount()` | Instrument/synth/BOT-data/aggregate events — **not** 1:1 Host READ CBW |

See `hu-probe/firmware/src/web/WebUi.cpp` (`msc_reads` + `read_count`).

## MEASURED (lab)

`LAB_BMW_HU_STIMULUS_HUPROBE_MEASURED`: Host stimulus emitted many READ10; DUT `read_count` delta was **12**, not equal to Host `readOk`. That mismatch is **semantic**, not proof the DUT was blind.

## Lab rule

- Prefer **`msc_reads`** (and/or event stream opcodes) when comparing Analyzer Host READ counts.
- Treat `read_count` as instrumentation intensity, not SCSI READ10 counter.
- Tools: `lab_bmw_hu_stimulus.py` records both and scores visibility on `msc_reads` first.

## Not claimed

- Byte-identity `msc_reads` ≡ Host READ10 under all profiles  
- Real NBT grammar completeness (MODE_SENSE/PREVENT still absent on Analyzer Host)
