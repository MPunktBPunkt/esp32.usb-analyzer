# Lab MEASURED — CLEAR_HALT vs BOT_RESET · 0.0.48

**Claim:** `clear_halt_vs_bot_lab`
**Locus:** `both_clear_halt_and_bot_reset`
**Host recoveryOk:** True · readOk=False
**Hooks:** ep_clear_halt=4 · msc_bot_reset=2
**clear_halt need_reset flags:** [0, 0, 0, 0]

## Verdict

- Device TinyUSB control path emits separate capture notes for CLEAR_FEATURE(ENDPOINT_HALT) vs MSC BOT Reset (0xFF).
- Desk FACT remains: Clear-Halt does **not** cure NEED_RESET (stall continues).

**Not claimed:** L0 wire · Host EspUsbHost internal path identity · USB-IF.

```json
{
  "ok": true,
  "claim": "clear_halt_vs_bot_lab",
  "arm": {
    "ok": true,
    "op": "fail",
    "delayMs": 0,
    "delayRemaining": 0,
    "failRemaining": 1,
    "okBeforeFail": 0,
    "failMidXfer": false,
    "disconnectAfter": 0,
    "hits": 0,
    "failHits": 0,
    "delayHits": 0,
    "okHits": 0,
    "deviceSenseKey": 0,
    "deviceAsc": 0,
    "deviceAsq": 0,
    "deviceSenseSource": "",
    "lastFailLba": 0,
    "lastFailOffset": 0,
    "claim": "gate9_fault_inject_lab",
    "note": "msc_gadget_onRead_not_l0"
  },
  "host_observe": {
    "readOk": false,
    "recoveryOk": true,
    "sensePhase": "after_recovery"
  },
  "device_hooks": {
    "ep_clear_halt": 4,
    "msc_bot_reset": 2,
    "clear_halt_need_reset_flags": [
      0,
      0,
      0,
      0
    ],
    "events": [
      {
        "version": 1,
        "session_id": "lab-3060",
        "node_id": "Hu-probe",
        "seq": 25,
        "timestamp_us": 12175894,
        "timestamp_domain": "local",
        "role": "device",
        "event_type": "error",
        "status": "ok",
        "length": 0,
        "duration_us": 0,
        "transfer_type": "bulk",
        "layer": "L1_lab",
        "claim": "not_wire_phy",
        "decoded": {
          "protocol": "msc-bot",
          "command": "msc_bot_reset",
          "ep_addr": 255,
          "need_reset_stage": 0,
          "source": "class_hook_mscd_control"
        },
        "note": "msc_bot_reset"
      },
      {
        "version": 1,
        "session_id": "lab-3060",
        "node_id": "Hu-probe",
        "seq": 26,
        "timestamp_us": 12176122,
        "timestamp_domain": "local",
        "role": "device",
        "event_type": "error",
        "status": "ok",
        "length": 0,
        "duration_us": 0,
        "transfer_type": "bulk",
        "layer": "L1_lab",
        "claim": "not_wire_phy",
        "decoded": {
          "protocol": "msc-bot",
          "command": "ep_clear_halt",
          "ep_addr": 130,
          "need_reset_stage": 0,
          "source": "class_hook_mscd_control"
        },
        "note": "ep_clear_halt"
      },
      {
        "version": 1,
        "session_id": "lab-3060",
        "node_id": "Hu-probe",
        "seq": 27,
        "timestamp_us": 12176349,
        "timestamp_domain": "local",
        "role": "device",
        "event_type": "error",
        "status": "ok",
        "length": 0,
        "duration_us": 0,
        "transfer_type": "bulk",
        "layer": "L1_lab",
        "claim": "not_wire_phy",
        "decoded": {
          "protocol": "msc-bot",
          "command": "ep_clear_halt",
          "ep_addr": 2,
          "need_reset_stage": 0,
          "source": "class_hook_mscd_control"
        },
        "note": "ep_clear_halt"
      },
      {
        "version": 1,
        "session_id": "lab-3060",
        "node_id": "Hu-probe",
        "seq": 28,
        "timestamp_us": 17176892,
        "timestamp_domain": "local",
        "role": "device",
        "event_type": "error",
        "status": "ok",
        "length": 0,
        "duration_us": 0,
        "transfer_type": "bulk",
        "layer": "L1_lab",
        "claim": "not_wire_phy",
        "decoded": {
          "protocol": "msc-bot",
          "command": "msc_bot_reset",
          "ep_addr": 255,
          "need_reset_stage": 0,
          "source": "class_hook_mscd_control"
        },
        "note": "msc_bot_reset"
      },
      {
        "version": 1,
        "session_id": "lab-3060",
        "node_id": "Hu-probe",
        "seq": 29,
        "timestamp_us": 17177147,
        "timestamp_domain": "local",
        "role": "device",
        "event_type": "error",
        "status": "ok",
        "length": 0,
        "duration_us": 0,
        "transfer_type": "bulk",
        "layer": "L1_lab",
        "claim": "not_wire_phy",
        "decoded": {
          "protocol": "msc-bot",
          "command": "ep_clear_halt",
          "ep_addr": 130,
          "need_reset_stage": 0,
          "source": "class_hook_mscd_control"
        },
        "note": "ep_clear_halt"
      },
      {
        "version": 1,
        "session_id": "lab-3060",
        "node_id": "Hu-probe",
        "seq": 30,
        "timestamp_us": 17177330,
        "timestamp_domain": "local",
        "role": "device",
        "event_type": "error",
        "status": "ok",
        "length": 0,
        "duration_us": 0,
        "transfer_type": "bulk",
        "layer": "L1_lab",
        "claim": "not_wire_phy",
        "decoded": {
          "protocol": "msc-bot",
          "command": "ep_clear_halt",
          "ep_addr": 2,
          "need_reset_stage": 0,
          "source": "class_hook_mscd_control"
        },
        "note": "ep_clear_halt"
      }
    ]
  },
  "note": "class_control_hooks_not_l0",
  "locus": "both_clear_halt_and_bot_reset"
}
```
