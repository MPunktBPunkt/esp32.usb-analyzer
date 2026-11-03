# Lab MEASURED — HID scenario (kbd + mouse) · 0.0.51

**Claim:** `hid_scenario_lab_loop`
**Text:** `Hello World` · keyboard_ok=True · mouse_ok=True
**VBUS boardSourcesVbus:** False · HPRT.prtpwr=True · yd_esp32_s3_devkit_class_otg_does_not_source_5v_use_self_powered_dut_or_hub

## Epistemik

- Device inject → Host EspUsbHost keyboard/mouse callbacks (class path).
- Soft-reenum between phases is lab theater on the **same** composite — not two physical HID devices.
- Host OTG connector does **not** source 5 V (DevKit/YD class FACT); HPRT.prtpwr is a software bit, not a meter reading.

JSON: `LAB_HID_SCENARIO_MEASURED.json`

```json
{
  "ok": true,
  "claim": "hid_scenario_lab_loop",
  "timestamp_utc": "20261103T090732Z",
  "text": "Hello World",
  "keyboard_ok": true,
  "mouse_ok": true,
  "vbus": {
    "ok": true,
    "prtpwr": true,
    "prtlnsts": 1,
    "prtConn": true,
    "prtEna": true,
    "hprtRaw": 136197,
    "boardSourcesVbus": false,
    "boardNote": "yd_esp32_s3_devkit_class_otg_does_not_source_5v_use_self_powered_dut_or_hub",
    "claim": "host_vbus_status_lab",
    "note": "hprt_software_bit_not_connector_5v_meter"
  },
  "board_sources_vbus": false,
  "phases": [
    {
      "phase": "keyboard_text",
      "text": "Hello World",
      "inject": {
        "ok": true,
        "mode": "text",
        "injects": 11,
        "textLen": 11,
        "claim": "hid_interrupt_lab_loop",
        "note": "gadget_inject_not_hu"
      },
      "observes": [
        {
          "ok": true,
          "got": true,
          "kind": "key",
          "keycode": 11,
          "ascii": 72,
          "modifiers": 2,
          "iface": 1,
          "reportLen": 9,
          "reportHex": "0102000b0000000000",
          "mouseX": 0,
          "mouseY": 0,
          "mouseWheel": 0,
          "mouseButtons": 0,
          "mouseMoved": false,
          "age_ms": 4,
          "reports": 2,
          "mouseReports": 0,
          "duration_ms": 199,
          "claim": "hid_interrupt_lab_loop",
          "note": "class_callback_not_l0_wire_not_hu"
        },
        {
          "ok": true,
          "got": true,
          "kind": "key",
          "keycode": 0,
          "ascii": 0,
          "modifiers": 1,
          "iface": 1,
          "reportLen": 9,
          "reportHex": "010000000000000000",
          "mouseX": 0,
          "mouseY": 0,
          "mouseWheel": 0,
          "mouseButtons": 0,
          "mouseMoved": false,
          "age_ms": 1,
          "reports": 5,
          "mouseReports": 0,
          "duration_ms": 5,
          "claim": "hid_interrupt_lab_loop",
          "note": "class_callback_not_l0_wire_not_hu"
        },
        {
          "ok": true,
          "got": true,
          "kind": "key",
          "keycode": 8,
          "ascii": 101,
          "modifiers": 0,
          "iface": 1,
          "reportLen": 9,
          "reportHex": "010000000000000000",
          "mouseX": 0,
          "mouseY": 0,
          "mouseWheel": 0,
          "mouseButtons": 0,
          "mouseMoved": false,
          "age_ms": 0,
          "reports": 9,
          "mouseReports": 0,
          "duration_ms": 10,
          "claim": "hid_interrupt_lab_loop",
          "note": "class_callback_not_l0_wire_not_hu"
        },
        {
          "ok": true,
          "got": true,
          "kind": "key",
          "keycode": 15,
          "ascii": 108,
          "modifiers": 0,
          "iface": 1,
          "reportLen": 9,
          "reportHex": "0100000f0000000000",
          "mouseX": 0,
          "mouseY": 0,
          "mouseWheel": 0,
          "mouseButtons": 0,
          "mouseMoved": false,
          "age_ms": 4,
          "reports": 12,
          "mouseReports": 0,
          "duration_ms": 5,
          "claim": "hid_interrupt_lab_loop",
          "note": "class_callback_not_l0_wire_not_hu"
        },
        {
          "ok": true,
          "got": true,
          "kind": "key",
          "keycode": 15,
          "ascii": 108,
          "modifiers": 0,
          "iface": 1,
          "reportLen": 9,
          "reportHex": "0100000f0000000000",
          "mouseX": 0,
          "mouseY": 0,
          "mouseWheel": 0,
          "mouseButtons": 0,
          "mouseMoved": false,
          "age_ms": 0,
          "reports": 17,
          "mouseReports": 0,
          "duration_ms": 4,
          "claim": "hid_interrupt_lab_loop",
          "note": "class_callback_not_l0_wire_not_hu"
        }
      ],
      "ok": true
    },
    {
      "phase": "soft_reenum",
      "result": {
        "ok": true,
        "scheduled": true,
        "claim": "usb_enum_outcome_lab",
        "note": "soft_reenum_not_control_mitm"
      },
      "ok": true
    },
    {
      "phase": "mouse",
      "rows": [
        {
          "step": "move",
          "params": {
            "dx": 50,
            "dy": -30,
            "click": "none"
          },
          "inject": {
            "ok": true,
            "dx": 50,
            "dy": -30,
            "buttons": 0,
            "mouseInjects": 1,
            "claim": "hid_interrupt_lab_loop",
            "note": "gadget_mouse_inject_not_hu"
          },
          "observe": {
            "ok": true,
            "got": true,
            "kind": "mouse",
            "keycode": 0,
            "ascii": 0,
            "modifiers": 0,
            "iface": 1,
            "reportLen": 0,
            "reportHex": "",
            "mouseX": 50,
            "mouseY": -30,
            "mouseWheel": 0,
            "mouseButtons": 0,
            "mouseMoved": true,
            "age_ms": 2,
            "reports": 56,
            "mouseReports": 1,
            "duration_ms": 180,
            "claim": "hid_interrupt_lab_loop",
            "note": "class_callback_not_l0_wire_not_hu"
          },
          "ok": true
        },
        {
          "step": "move2",
          "params": {
            "dx": -20,
            "dy": 40,
            "click": "none"
          },
          "inject": {
            "ok": true,
            "dx": -20,
            "dy": 40,
            "buttons": 0,
            "mouseInjects": 2,
            "claim": "hid_interrupt_lab_loop",
            "note": "gadget_mouse_inject_not_hu"
          },
          "observe": {
            "ok": true,
            "got": true,
            "kind": "mouse",
            "keycode": 0,
            "ascii": 0,
            "modifiers": 0,
            "iface": 1,
            "reportLen": 0,
            "reportHex": "",
            "mouseX": -20,
            "mouseY": 40,
            "mouseWheel": 0,
            "mouseButtons": 0,
            "mouseMoved": true,
            "age_ms": 1,
            "reports": 57,
            "mouseReports": 2,
            "duration_ms": 245,
            "claim": "hid_interrupt_lab_loop",
            "note": "class_callback_not_l0_wire_not_hu"
          },
          "ok": true
        },
        {
          "step": "left",
          "params": {
            "click": "left"
          },
          "inject": {
            "ok": true,
            "dx": 0,
            "dy": 0,
            "buttons": 1,
            "mouseInjects": 3,
            "claim": "hid_interrupt_lab_loop",
            "note": "gadget_mouse_inject_not_hu"
          },
          "observe": {
            "ok": true,
            "got": true,
            "kind": "mouse",
            "keycode": 0,
            "ascii": 0,
            "modifiers": 0,
            "iface": 1,
            "reportLen": 0,
            "reportHex": "",
            "mouseX": 0,
            "mouseY": 0,
            "mouseWheel": 0,
            "mouseButtons": 1,
            "mouseMoved": false,
            "age_ms": 4,
            "reports": 58,
            "mouseReports": 3,
            "duration_ms": 159,
            "claim": "hid_interrupt_lab_loop",
            "note": "class_callback_not_l0_wire_not_hu"
          },
          "ok": true
        },
        {
          "step": "right",
          "params": {
            "click": "right"
          },
          "inject": {
            "ok": true,
            "dx": 0,
            "dy": 0,
            "buttons": 2,
            "mouseInjects": 4,
            "claim": "hid_interrupt_lab_loop",
            "note": "gadget_mouse_inject_not_hu"
          },
          "observe": {
            "ok": true,
            "got": true,
            "kind": "mouse",
            "keycode": 0,
            "ascii": 0,
            "modifiers": 0,
            "iface": 1,
            "reportLen": 0,
            "reportHex": "",
            "mouseX": 0,
            "mouseY": 0,
            "mouseWheel": 0,
            "mouseButtons": 
```
