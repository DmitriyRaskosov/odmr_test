#!/usr/bin/env bash
# Long-run soak capture: streaming analyze, no raw files, no fixed group count.
#
# Terminal 1 (VM) — start BEFORE Windows spammer:
#   SOAK_DURATION_SEC=3600 RUN_LABEL=soak_1h bash scripts/run_soak.sh
#
# Terminal 2 (Windows):
#   .\scripts\spammer_odmr_compare.ps1 -SoakOdmrPair -DurationSec 3600 -DstHost 192.168.1.9
#
# 10 min cv_odmr loop test:
#   SOAK_DURATION_SEC=600 RUN_LABEL=cv_odmr_10m bash scripts/run_soak.sh
#   .\scripts\spammer_odmr_compare.ps1 -LongCvOdmr -DurationSec 600 -DstHost 192.168.1.9
set -euo pipefail

ODMR_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
if [[ -f "${ODMR_ROOT}/scripts/stand.env" ]]; then
    # shellcheck source=/dev/null
    source "${ODMR_ROOT}/scripts/stand.env"
fi

IFACE="${IFACE:-enp0s3}"
RUN_LABEL="${RUN_LABEL:-soak}"
SOAK_DURATION_SEC="${SOAK_DURATION_SEC:-3600}"
EXPERIMENT_INI="${EXPERIMENT_INI:-${ODMR_ROOT}/cv_odmr.ini}"

RUN="${ODMR_ROOT}/runs/$(date +%Y%m%d_%H%M%S)_${RUN_LABEL}"
mkdir -p "$RUN"
SOAK_LOG="${RUN}/soak.log"

echo "RUN=$RUN"
echo "Soak capture on $IFACE for ${SOAK_DURATION_SEC}s (log: soak.log)"
echo "  output: $RUN/pulses_grouped.txt"
echo "  experiment ini (repeats only): $EXPERIMENT_INI"

CAPTURE_ARGS=(
    --soak
    --output-dir "$RUN"
    --experiment-ini "$EXPERIMENT_INI"
)
if [[ -n "${REPEATS_PER_FREQ:-}" ]]; then
    CAPTURE_ARGS+=(--repeats-per-freq "$REPEATS_PER_FREQ")
    echo "  repeats_per_freq override: $REPEATS_PER_FREQ"
fi

set -o pipefail
timeout --signal=INT "${SOAK_DURATION_SEC}" \
    sudo "${ODMR_ROOT}/build/packet_capture" "$IFACE" "${CAPTURE_ARGS[@]}" \
    2>&1 | tee "$SOAK_LOG"
CAPTURE_EXIT=${PIPESTATUS[0]}

sudo chown -R "$(id -un):$(id -gn)" "$RUN"

echo ""
echo "=== soak summary (${RUN_LABEL}, ${SOAK_DURATION_SEC}s) ==="
grep -E 'enqueue_fail|kernel_drops|reorder late|gap skip|bad pulse|bad_win|analyze groups' "$SOAK_LOG" | tail -20 || true
tail -5 "$SOAK_LOG" || true

if [[ "$CAPTURE_EXIT" -eq 124 ]]; then
    echo "Capture stopped by timeout (${SOAK_DURATION_SEC}s) — expected for soak."
elif [[ "$CAPTURE_EXIT" -ne 0 ]]; then
    echo "Capture exited with code $CAPTURE_EXIT" >&2
    exit "$CAPTURE_EXIT"
fi

wc -l "$RUN/pulses_grouped.txt" 2>/dev/null || true
echo "Done. RUN=$RUN"
