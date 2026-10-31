# Lab MEASURED — Suspend/Resume + EP0 control-neg (0.0.36)

**Not claimed:** L0 D+/D−, transparent SETUP MITM, HU, High-Speed, USB-IF compliance.

## Suspend/Resume (`usb_bus_suspend_resume_lab`)

| Check | Class |
|-------|-------|
| Host HPRT `prtsusp` set/clear | MEASURED (stack register poke) |
| Post-resume GET_DESCRIPTOR (Host) | MEASURED |
| Device TinyUSB SUSPEND/RESUME counters | UNKNOWN on this HPRT OTG path (often STOPPED/`waiting` instead) |
| Electrical idle / SOF absence on wire | UNKNOWN / not claimed |

Tool: `tools/lab_usb_suspend_resume.py`  
Hard gate: Host cycle `ok` + `postTrafficOk`. Soft-reenum recovery if Device drops to `waiting`.  
Optional: `--require-device-events` when TinyUSB suspend is actually observed.

## EP0 control-neg (`usb_ctrl_neg_lab`)

Expectations = TinyUSB/EspUsbHost lab MEASURED (not USB-IF).

| Case | Expect |
|------|--------|
| GET_DESCRIPTOR device | ok |
| GET_STATUS | ok |
| GET_DESCRIPTOR short (8) | ok |
| Bad descriptor type `0x7F` | fail |
| Invalid bRequest `0xEE` | fail |
| Unimplemented vendor IN `0xC0/0xAB` | fail |
| GET_INTERFACE | ok |
| GET_DESCRIPTOR wLength=0 | fail |

Note: bogus **string index** often returns a stub (ok) on TinyUSB — not used as negative.

Tool: `tools/lab_usb_ctrl_neg.py`
