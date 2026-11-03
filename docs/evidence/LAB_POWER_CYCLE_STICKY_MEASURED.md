# Lab MEASURED — Power-cycle sticky · hu-probe

**Claim:** `huprobe_power_cycle_sticky_lab`
**FW:** 0.3.35 · soft_only=True
**Soft sticky:** True · **Power sticky:** None

## Epistemik

- Soft `ESP.restart` ≠ Netzteil/Power-Cycle (rail drop).
- Soft sticky already known MEASURED; this run revalidates and optionally adds rail.
- **Not claimed:** VBUS OTG switch HW · phone→accessory Vollenum.

## Rows

```json
[
  {
    "mode": "msc",
    "soft": {
      "set": {
        "config": {
          "ok": true,
          "gadgetNvsOk": true,
          "usbGadget": "msc",
          "err": null
        },
        "usb_gadget": "msc",
        "gadgetNvsOk": true
      },
      "reboot": {
        "path": "http://192.168.178.89/api/connected/gadget/reboot",
        "ok": true,
        "action": "gadget_reboot",
        "usbGadget": "msc",
        "gadgetNvsOk": true,
        "note": "restart_after_nvs_save"
      },
      "usb_gadget": "msc",
      "resetReason": null,
      "ok": true
    },
    "power": {}
  },
  {
    "mode": "aoa_observe",
    "soft": {
      "set": {
        "config": {
          "ok": true,
          "gadgetNvsOk": true,
          "usbGadget": "aoa_observe",
          "err": null
        },
        "usb_gadget": "aoa_observe",
        "gadgetNvsOk": true
      },
      "reboot": {
        "path": "http://192.168.178.89/api/connected/gadget/reboot",
        "ok": true,
        "action": "gadget_reboot",
        "usbGadget": "aoa_observe",
        "gadgetNvsOk": true,
        "note": "restart_after_nvs_save"
      },
      "usb_gadget": "aoa_observe",
      "resetReason": null,
      "ok": true
    },
    "power": {}
  },
  {
    "mode": "aoa_phone_like",
    "soft": {
      "set": {
        "config": {
          "ok": true,
          "gadgetNvsOk": true,
          "usbGadget": "aoa_phone_like",
          "err": null
        },
        "usb_gadget": "aoa_phone_like",
        "gadgetNvsOk": true
      },
      "reboot": {
        "path": "http://192.168.178.89/api/connected/gadget/reboot",
        "ok": true,
        "action": "gadget_reboot",
        "usbGadget": "aoa_phone_like",
        "gadgetNvsOk": true,
        "note": "restart_after_nvs_save"
      },
      "usb_gadget": "aoa_phone_like",
      "resetReason": null,
      "ok": true
    },
    "power": {}
  }
]
```

JSON: `LAB_POWER_CYCLE_STICKY_MEASURED.json`
