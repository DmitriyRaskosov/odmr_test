# Golden compare runs

Regression reference: stream `pulses_grouped.txt` matched offline analyze.py (debug capture with --record-raw).

## 20260613_134939_compare

    cmp tests/golden/20260613_134939_compare/pulses_grouped.txt \
        tests/golden/20260613_134939_compare/pulses_grouped_offline.txt && echo GOLDEN_OK

Normal production runs do not use analyze.py or raw files.

Save a new golden grouped output:

    ./scripts/save_golden_run.sh ~/odmr/runs/YYYYMMDD_HHMMSS_run