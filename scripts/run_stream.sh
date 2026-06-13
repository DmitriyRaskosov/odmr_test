#!/usr/bin/env bash
# Production capture: online analyze-stream, pulses_grouped.txt only (no raw ch*.txt).
#
# Terminal 1 (VM):
#   ./scripts/run_stream.sh
# Terminal 2 (Windows):
#   scripts/spammer_odmr_compare.ps1
# After spammer finishes, Ctrl+C capture.
set -euo pipefail

ODMR_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
IFACE="${IFACE:-enp0s3}"
RUN_LABEL="${RUN_LABEL:-run}"
GROUP_SIZE="${GROUP_SIZE:-400}"

RUN="${ODMR_ROOT}/runs/$(date +%Y%m%d_%H%M%S)_${RUN_LABEL}"
mkdir -p "$RUN"
echo "RUN=$RUN"
echo "Starting stream capture on $IFACE (Ctrl+C when done)..."
echo "  output: $RUN/pulses_grouped.txt"
echo "  group_size: $GROUP_SIZE"

sudo "${ODMR_ROOT}/build/packet_capture" "$IFACE" \
    --analyze-stream \
    --output-dir "$RUN" \
    --group-size "$GROUP_SIZE"

sudo chown -R "$(id -un):$(id -gn)" "$RUN"
echo "Done. RUN=$RUN"
wc -l "$RUN/pulses_grouped.txt" 2>/dev/null || true
