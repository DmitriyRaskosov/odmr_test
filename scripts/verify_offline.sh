#!/usr/bin/env bash
# Debug/regression only: compare stream output vs offline analyze.py on raw ch*.txt.
# Requires capture with --record-raw (not used in normal pipeline).
#
# Usage:
#   sudo packet_capture ... --analyze-stream --record-raw --output-dir "$RUN"
#   GROUP_SIZE=2 ./scripts/verify_offline.sh "$RUN"
set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "usage: $0 RUN_DIR" >&2
    exit 1
fi

ODMR_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
RUN="$(cd "$1" && pwd)"
GROUP_SIZE="${GROUP_SIZE:-2}"
REPEATS_PER_FREQ="${REPEATS_PER_FREQ:-$GROUP_SIZE}"

if [[ ! -f "$RUN/ch0_0.txt" || ! -f "$RUN/ch2_0.txt" ]]; then
    echo "missing ch0_0.txt or ch2_0.txt — re-capture with --record-raw" >&2
    exit 1
fi
if [[ ! -f "$RUN/pulses_grouped.txt" ]]; then
    echo "missing pulses_grouped.txt (need --analyze-stream)" >&2
    exit 1
fi

python3 "${ODMR_ROOT}/analyze.py" \
    --ch0 "$RUN/ch0_0.txt" \
    --trig "$RUN/ch2_0.txt" \
    --repeats-per-freq "$REPEATS_PER_FREQ" \
    -o "$RUN/pulses_grouped_offline.txt"

cmp "$RUN/pulses_grouped.txt" "$RUN/pulses_grouped_offline.txt" && echo IDENTICAL
wc -l "$RUN/pulses_grouped.txt" "$RUN/pulses_grouped_offline.txt"
