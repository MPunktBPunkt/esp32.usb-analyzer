# Lab MEASURED — Wire stimulus on GPIO19/20 (0.0.42)

**Tool:** `tools/lab_wire_stimulus.py` → `/api/lab/wire-stimulus`
**Host FW:** `0.0.42` → reboot restore
**Rate target:** 100000 Hz

## Idea check (Grok)

Host can drive D+/D− **only after** stopping the USB Host / releasing the PHY.
This is a **destructive test mode**, not parallel Host operation.
Electrical drive is GPIO-grade (lab cable OK); not USB-IF conformant.

## Device before

```json
{
  "version": "0.0.41",
  "otg": "connected",
  "mounted": null,
  "suspended": null,
  "suspendCount": 0,
  "resumeCount": 0,
  "raw_otg": {
    "mode": "device",
    "state": "connected",
    "connected": true,
    "suspended": false,
    "meaning": "gadget_mounted_on_usb_host"
  },
  "raw_gadget": {
    "readOps": 690,
    "readBytes": 1413120,
    "writeOps": 0,
    "writeBytes": 0,
    "pattern": "lab_sector_v1",
    "proxy": false,
    "proxyPeer": "",
    "identityApplied": false,
    "appliedId": "303a:1001",
    "blockCount": 512,
    "blockSize": 512,
    "proxyHits": 12,
    "proxyFails": 1,
    "proxyBytes": 49152,
    "hid": true,
    "hidInjects": 0,
    "suspendCount": 0,
    "resumeCount": 0,
    "claim": "gadget_identity_from_dut_snapshot",
    "aoa": {
      "respond": false,
      "reenumOnStart": true,
      "bulkEcho": true,
      "vendorMounted": true,
      "setup": 0,
      "aoa51": 0,
      "aoa52": 0,
      "aoa53": 0,
      "aoa51Ok": 0,
      "stringBytes": 0,
      "bulkRx": 0,
      "bulkTx": 0,
      "bulkFifo": 0,
      "bulkPolls": 82155,
      "tudVendorAvail": 0,
      "lastStringIndex": 255,
      "claim": "aoa_control_handshake_lab",
      "note": "not_hu_wire_not_bcl"
    },
    "proxiedDut": {
      "present": false
    }
  }
}
```

## Patterns

| Pattern | Host OK | Achieved Hz | Device otg after | suspendCount |
|---------|---------|-------------|------------------|--------------|
| `bus_states` | True | 87378 | waiting | 0 |
| `se0_reset` | True | 87071 | waiting | 0 |
| `sync` | True | 87431 | waiting | 0 |
| `setup_ok` | True | 87450 | waiting | 0 |
| `setup_bad_crc` | True | 86693 | waiting | 0 |

## Post-reboot MSC

**OK** {'ok': True, 'msc_ready': True, 'pattern_ok': True, 'pattern_fail_bytes': 0, 'dut': '303a:1001', 'inquiry': {'vendor': 'MPB     ', 'product': 'ANALYZER        ', 'revision': '0001'}, 'capacity_blocks': 512, 'block_size': 512, 'chunk': 4, 'transfers': 2, 'blocks_read': 8, 'bytes_read': 4096, 'duration_ms': 5, 'rate_kBs': 800, 'rtt_us': {'min': 2286, 'avg': 2315, 'p50': 2344, 'p95': 2344, 'max': 2344}, 'dir': 'read', 'path': 'usb_otg_host_to_device_msc', 'note': 'direct_otg_cable_not_wifi_mitm'}

## Verdict

- Ran **5** stimulus pattern(s) with Host stack stopped.
- Useful as **controlled wire stimulus** for Device-stack / future L0 compare.
- Prefer **GPIO10/11 Path B lab** for non-destructive decoder pipe tests.
- Prefer **soldered Path B tap** for real FS capture.

**Claim:** `wire_stimulus_lab`
**Not claimed:** parallel USB Host · FS 12 Mbit/s soft-host · USB-IF signal quality.
