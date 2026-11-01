# Schema — `l0_packet.v1`

Compact FS USB packet record (post-decode). **Not** raw ADC samples.

```json
{
  "version": 1,
  "schema": "l0_packet.v1",
  "timestamp_us": 0,
  "timestamp_domain": "capture_local",
  "dir": "host_to_device",
  "pid": "SETUP",
  "pid_byte": 45,
  "addr": 5,
  "endp": 0,
  "data_toggle": null,
  "payload_hex": "",
  "payload_len": 0,
  "crc_ok": true,
  "crc_value": 0,
  "raw_valid": true,
  "note": ""
}
```

| Field | Meaning |
|-------|---------|
| `dir` | `host_to_device` / `device_to_host` / `unknown` |
| `pid` | `OUT` `IN` `SETUP` `DATA0` `DATA1` `ACK` `NAK` `STALL` `SOF` `PRE` `…` |
| `addr`/`endp` | Token fields (null for handshake-only) |
| `data_toggle` | `0`/`1` for DATA PIDs else null |
| `crc_ok` | CRC5 (token) or CRC16 (data) check |
| `timestamp_us` | Sampler clock; **not** hard-synced to TinyUSB until 10F live |

Batch file: `{"version":1,"packets":[...]}`.
