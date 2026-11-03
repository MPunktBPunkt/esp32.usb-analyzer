# Lab — Host VBUS / HPRT · 0.0.51

**Claim:** `host_vbus_status_lab`
**boardSourcesVbus:** `False`
**HPRT.prtpwr (software):** `True`
**Note:** yd_esp32_s3_devkit_class_otg_does_not_source_5v_use_self_powered_dut_or_hub

## Verdict

- YD-ESP32-S3 / DevKitC-1 class: OTG connector does **not** feed bus 5 V.
- Bus-powered-only keyboards/mice need a **self-powered hub** or external 5 V.
- Lab Device ESP is self-powered via its own supply — OTG loop does not prove Host VBUS.
- HPRT.prtpwr may be 1 in software while the connector pin stays unpowered.

```json
{
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
}
```
