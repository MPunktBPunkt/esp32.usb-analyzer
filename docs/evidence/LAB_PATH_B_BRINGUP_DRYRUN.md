# Lab MEASURED — Path B bring-up dry-run (no HW)

**Claim:** `path_b_bringup_dryrun_lab`
**All checks:** PASS

## Checks

- `host_unit_usb_fs_l0`: ok
- `golden_fixture`: ok
- `correlate_classify`: ok
- `live_host`: ok
- `docs_bom_protocol`: ok

## When HW arrives

1. Solder Hi-Z tap per docs/hardware/L0_CAPTURE_POC.md BOM
1. Feed VP/VM digital into ESP GPIO10/11 (or sampler)
1. Start: tools/l0_path_b_ingest.py --decode
1. TX/RX: Host /api/lab/path-b-gpio-rx or external sampler → L0ST :9760
1. Verify: tools/lab_l0_live_pipeline.py --source ingest
1. Gap: tools/lab_l0_stack_gap.py or soft campaign with real .l0samp

**Not claimed:** soldered Path B · live D+/D− FACT.

JSON: `LAB_PATH_B_BRINGUP_DRYRUN.json`
