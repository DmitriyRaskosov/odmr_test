# Golden compare runs

Reference captures where `pulses_grouped.txt` (stream) matches `analyze.py` offline output.

## 20260613_134939_compare

Verified on VM: cmp -> IDENTICAL, 6376 lines each, groups N, 2, 2 with --group-size 2.

Quick check after clone:

    cmp tests/golden/20260613_134939_compare/pulses_grouped.txt tests/golden/20260613_134939_compare/pulses_grouped_offline.txt && echo GOLDEN_OK

## Save a new golden run from VM

    cd ~/odmr
    ./scripts/save_golden_run.sh ~/odmr/runs/YYYYMMDD_HHMMSS_compare
    git add tests/golden/