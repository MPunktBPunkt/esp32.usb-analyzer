# Lab MEASURED — Observer-effect baseline (0.0.39)

**Tool:** `tools/lab_observer_effect.py --mode proxy`  
**Topology:** OTG Host↔Device loop · Host `proxy-source=pattern` · Device `identity=0`

| Path | Result |
|------|--------|
| Direct Host MSC READ | **~844 kB/s**, ~9.5 ms (16 blocks / chunk 4) |
| WiFi MSC-proxy selftest | **~25 kB/s est.**, ~160 ms avg (8 blocks, pattern fill) |
| Δ | proxy slower by **~151 ms** |

**Claim:** `observer_effect_baseline_lab`  
**Not claimed:** PC-stick MITM transparency; DUT-sourced proxy (would recurse on loop).
