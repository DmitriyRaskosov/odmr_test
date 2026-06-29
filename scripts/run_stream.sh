#!/usr/bin/env bash
# Production capture: online analyze-stream, pulses_grouped.txt only (no raw ch*.txt).
#
# Terminal 1 (VM):
#   ./scripts/run_stream.sh
# Terminal 2 (Windows):
#   scripts/spammer_odmr_compare.ps1 -CvOdmrProfile
# After spammer finishes, Ctrl+C capture.
set -euo pipefail

ODMR_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
if [[ -f "${ODMR_ROOT}/scripts/stand.env" ]]; then
    # shellcheck source=/dev/null
    source "${ODMR_ROOT}/scripts/stand.env"
fi

IFACE="${IFACE:-enp0s3}"
RUN_LABEL="${RUN_LABEL:-run}"
EXPERIMENT_INI="${EXPERIMENT_INI:-${ODMR_ROOT}/cv_odmr.ini}"

RUN="${ODMR_ROOT}/runs/$(date +%Y%m%d_%H%M%S)_${RUN_LABEL}"
mkdir -p "$RUN"
echo "RUN=$RUN"
echo "Starting stream capture on $IFACE (Ctrl+C when done)..."
echo "  output: $RUN/pulses_grouped.txt"
echo "  experiment ini: $EXPERIMENT_INI"

CAPTURE_ARGS=(
    --analyze-stream
    --output-dir "$RUN"
    --experiment-ini "$EXPERIMENT_INI"
)
if [[ -n "${REPEATS_PER_FREQ:-}" ]]; then
    CAPTURE_ARGS+=(--repeats-per-freq "$REPEATS_PER_FREQ")
    echo "  repeats_per_freq override: $REPEATS_PER_FREQ"
fi
if [[ -n "${EXPECTED_GROUPS:-}" && "${EXPECTED_GROUPS}" != "0" ]]; then
    CAPTURE_ARGS+=(--expected-groups "$EXPECTED_GROUPS")
    echo "  expected_groups override: $EXPECTED_GROUPS"
fi

sudo "${ODMR_ROOT}/build/packet_capture" "$IFACE" "${CAPTURE_ARGS[@]}"

sudo chown -R "$(id -un):$(id -gn)" "$RUN"
echo "Done. RUN=$RUN"
wc -l "$RUN/pulses_grouped.txt" 2>/dev/null || true
