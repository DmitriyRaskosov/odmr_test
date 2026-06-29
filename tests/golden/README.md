# Golden compare runs

Historical regression: stream `pulses_grouped.txt` matched offline `analyze.py` on raw `ch*.txt`.

**Requires debug capture:** `packet_capture --analyze-stream --record-raw --output-dir "$RUN"`.

Production (`run_stream.sh`) does **not** write raw files or use `analyze.py`.

## 20260613_134939_compare

```bash
cmp tests/golden/20260613_134939_compare/pulses_grouped.txt \
    tests/golden/20260613_134939_compare/pulses_grouped_offline.txt && echo GOLDEN_OK
```

Save a new reference after intentional analyze changes:

```bash
./scripts/save_golden_run.sh ~/odmr/runs/YYYYMMDD_HHMMSS_run
```

Current grouping uses `repeats_per_freq` from `cv_odmr.ini`, not legacy `group_size=400`.
