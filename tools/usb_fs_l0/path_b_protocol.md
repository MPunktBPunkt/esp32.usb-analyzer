# Path B sampler ↔ Debian ingest protocol

**Port:** TCP `9760` (Debian controller host)  
**Purpose:** External FS line-state front-end pushes samples; ingest writes `.l0samp` and can feed live decode.

## Framing

```text
uint32 magic = 'L0ST' (0x4C305354)
uint16 ver   = 1
uint16 type  = 1 line_state_chunk | 2 end_of_burst | 3 hello
uint32 rate_hz
uint32 seq
uint32 nbytes
uint8  payload[nbytes]   # type1: one byte/sample SE0=0 J=1 K=2 SE1=3
```

## Hello (type=3)

payload JSON UTF-8 optional: `{"sampler":"esp-dma|cpld|sim","note":"..."}`

## Tools

- Ingest: `tools/l0_path_b_ingest.py --listen 0.0.0.0:9760 --out logs/l0/`
- Simulator (no HW): `tools/l0_path_b_sampler_sim.py --target 127.0.0.1:9760`
- Live pipeline: `tools/lab_l0_live_pipeline.py --source ingest|sim|file`
