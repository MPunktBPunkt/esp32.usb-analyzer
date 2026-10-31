# Lab MEASURED — USB enum outcome + HID interrupt (0.0.35)

**Claim class:** MEASURED on OTG Host↔Device loop.  
**Not claimed:** control MITM, mid-enum SETUP sniff, L0 D+/D−, HU, High-Speed.

## Enum outcome (`usb_enum_outcome_lab`)

| Check | Result |
|-------|--------|
| Host post-enum `address` ≠ 0 | MEASURED |
| Host post-enum `configurationValue` ≠ 0 | MEASURED |
| Caller EP0 GET_DESCRIPTOR(DEVICE) | MEASURED (lab) |
| Caller EP0 GET_CONFIGURATION matches cached cfg | MEASURED |
| Soft-reenum Detach→Attach + re-observe | MEASURED (optional `--reenum`) |
| Mid-enum SET_ADDRESS / SET_CONFIGURATION on wire | **UNKNOWN** / not claimed |

Tool: `tools/lab_usb_enum_observe.py`

## HID interrupt (`hid_interrupt_lab_loop`)

| Check | Result |
|-------|--------|
| Device composite presents HID IF | MEASURED (`hidIface`) |
| Device `hid-inject` queues keyboard report | MEASURED |
| Host EspUsbHost keyboard/HID callback | MEASURED |
| Capture `hid_input` | MEASURED (class callback) |
| PHY interrupt packet / L0 | **UNKNOWN** / not claimed |

Tool: `tools/lab_hid_interrupt_verify.py`
