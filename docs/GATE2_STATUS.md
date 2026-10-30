# Gate 2 status — ESP Device node

**Code status:** MSC gadget scaffold in `analyzer-s3-device`  
**Lab MEASURED:** pending — flash `.89` via COM, plug OTG into PC, confirm enum  
**Claim depth:** device stack present / host sees mass storage — not BOT wire, not MITM

## Build / flash

```bash
cd firmware/analyzer-node
pio run -e analyzer-s3-device
# COM cable on target board:
pio run -e analyzer-s3-device -t upload
```

## Exit criteria

| Criterion | Status |
|-----------|--------|
| Device FW builds | next verify |
| PC `lsusb` / mounts ANALYZER volume | pending |
| HELLO role=device to Debian | pending |
| SET_ADDRESS ≠ Mount labeled in docs/logs | noted in FW |

## NEXT

1. UART-flash `.89` with `analyzer-s3-device`
2. OTG → Proxmox PC; confirm gadget
3. Then Gate 3 TCP link / relay
