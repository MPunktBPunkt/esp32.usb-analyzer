# Lab MEASURED — Path B ingest + sampler sim (0.0.39)

**Not claimed:** live D+/D− PHY front-end (hardware still open).

| Step | Result |
|------|--------|
| `l0_path_b_ingest.py :9760` | LISTEN OK |
| `l0_path_b_sampler_sim.py` | pushed 177 line-states |
| `.l0samp` + decode | SETUP/DATA0/ACK packets.json written |
| `GET /api/v1/l0/status` | `latest_exists=true` |

Claim: `path_b_ingest_lab` / `path_b_sampler_sim`.
