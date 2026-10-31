# Gate 3 status — TCP integrity

**Claim scope:** ESP↔ESP WiFi TCP data plane, sequenced frames, CRC + pattern match.  
**Not claimed:** USB Host⇄Device MITM proxy (Gate 4), capture integrity (Gate 5).

## Lab MEASURED (2026-10-30)

| Check | Result |
|-------|--------|
| `.89` MSC gadget on PC | **YES** — `303a:1001` BOT/SCSI, serial `68B6B32BC568` |
| `.88` Host `usb_devices` | **0** — no DUT on Host OTG |
| USB path through both ESPs | **blocked** — no Gate-4 proxy; Host empty |
| TCP integrity `:9752` | FW 0.0.5 + `tools/lab_integrity_test.py` |
| `.88`→`.89` 64×1024 B | **OK** (byte-identical + ACK) |
| `.89`→`.88` 64×1024 B | **OK** |

## How to run

```bash
./tools/lab_integrity_test.py --both-ways --frames 64 --frame-bytes 1024
```

## NEXT

1. Gate 4: USB proxy Host↔TCP↔Device (needs DUT on Host OTG; Device may face PC or loop)
2. Performance sweep (frame size × rate) after integrity OK
