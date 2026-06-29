#!/usr/bin/env bash
# Production capture: one cv_odmr experiment from ini, auto-stop when all groups written.
#
# Terminal 1 (VM):
#   ./scripts/run_stream.sh
# Terminal 2 (Windows):
#   scripts/spammer_odmr_compare.ps1 -CvOdmrProfile
# Capture stops when analyze groups == expected from ini (or Ctrl+C).
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

stop_capture() {
    sudo pkill -INT -f "--output-dir ${RUN}" 2>/dev/null || true
    sleep 1
    sudo pkill -TERM -f "--output-dir ${RUN}" 2>/dev/null || true
}

finalize_run() {
    sudo chown -R "$(id -un):$(id -gn)" "$RUN" 2>/dev/null || true
}

on_signal() {
    echo "Stopping capture..." >&2
    stop_capture
    finalize_run
    exit 130
}

trap on_signal INT TERM
trap finalize_run EXIT

echo "RUN=$RUN"
echo "Starting stream capture on $IFACE (stops when ini experiment complete or Ctrl+C)..."
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

set +e
sudo stdbuf -oL -eL "${ODMR_ROOT}/build/packet_capture" "$IFACE" "${CAPTURE_ARGS[@]}"
CAPTURE_EXIT=$?
set -e

finalize_run

if [[ "$CAPTURE_EXIT" -ne 0 && "$CAPTURE_EXIT" -ne 130 ]]; then
    echo "Capture exited with code $CAPTURE_EXIT" >&2
    exit "$CAPTURE_EXIT"
fi

echo "Done. RUN=$RUN"
wc -l "$RUN/pulses_grouped.txt" 2>/dev/null || true
