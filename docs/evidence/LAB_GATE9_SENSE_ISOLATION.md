# Lab MEASURED — Gate 9 REQUEST_SENSE isolation

**Claim:** `gate9_sense_isolation_lab`
**Locus:** `host_sense_lost_before_or_during_bot_reset`
**Device expect:** key=2 ASC=58 source=`inject_expect_tud_medium_not_present`
**Host before recovery:** ok=False key=0 ASC=0
**Host after recovery:** ok=True key=0 ASC=0
**readOk=False recoveryOk=True**

## Verdict

- Device records expected TinyUSB sense (NOT_READY / 0x3A) at inject time.
- Host logs REQUEST_SENSE before and after recovery separately.
- Current locus: **host_sense_lost_before_or_during_bot_reset**.

**Not claimed:** L0 · USB-IF · transparent MITM.

JSON: `LAB_GATE9_SENSE_ISOLATION.json`
