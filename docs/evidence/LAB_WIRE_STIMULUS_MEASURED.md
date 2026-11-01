# Lab MEASURED — Wire stimulus on GPIO19/20 (0.0.43→0.0.43)

**Tool:** `tools/lab_wire_stimulus.py`
**Patterns:** `bus_states,se0_reset,sync,eop,glitch,jk_burst,setup_ok,setup_bad_crc,data_bad_crc`

## Device before

```json
{
  "version": "0.0.43",
  "otg": "connected",
  "mountCount": 5,
  "unmountCount": 4,
  "suspendCount": 0,
  "resumeCount": 0,
  "lastBusEvent": "mount",
  "lastBusEventMs": 101324
}
```

Capture notes (sample): ['class_hook_cbw', 'class_hook_csw', 'READ10_complete', 'class_hook_cbw', 'class_hook_csw', 'READ10_complete', 'class_hook_cbw', 'class_hook_csw']

## Series `slow_100k` @ 100000 Hz

- Host ok=True · achieved **87350 Hz** · states=1533 · wall=0.457s
- Device otg connected → **waiting**
- Δmount=0 · Δunmount=1 · lastBusEvent=unmount
- Capture tail: ['class_hook_cbw', 'class_hook_csw', 'READ10_complete', 'class_hook_cbw', 'class_hook_csw', 'READ10_complete', 'class_hook_cbw', 'class_hook_csw', 'READ10_complete', 'class_hook_cbw', 'class_hook_csw', 'READ10_complete']

- Post-reboot MSC: **OK** 800 kB/s · device connected

## Series `ls_1m5` @ 1500000 Hz

- Host ok=True · achieved **418509 Hz** · states=1533 · wall=0.441s
- Device otg connected → **waiting**
- Δmount=0 · Δunmount=1 · lastBusEvent=unmount
- Capture tail: ['READ10_complete', 'class_hook_cbw', 'class_hook_csw', 'READ10_complete', 'class_hook_cbw', 'gadget_pattern', 'class_hook_csw', 'READ10_complete', 'class_hook_cbw', 'class_hook_csw', 'READ10_complete', 'class_hook_cbw']

- Post-reboot MSC: **OK** 800 kB/s · device connected

## Verdict

- Destructive GPIO19/20 stimulus MEASURED with Device bus-event counters.
- Slow (~100 kHz) is the reliable lab rate; LS 1.5 MHz attempt is best-effort bit-bang.
- Device typically **unmounts** when Host stack stops / SE0 is driven.

**Claim:** `wire_stimulus_lab`
**Not claimed:** FS soft-host · USB-IF SI · parallel Host.
