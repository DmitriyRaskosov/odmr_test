# odmr_test

UDP packet capture and online streaming analyze for lab board (Linux).

Sender: https://github.com/DmitriyRaskosov/udp_spammer

## Build

    git clone git@github.com:DmitriyRaskosov/odmr_test.git ~/odmr
    cd ~/odmr && rm -rf build && cmake -S . -B build && cmake --build build

## Production pipeline (stream only)

Online analyze writes `pulses_grouped.txt` under `--output-dir`. No raw ch*.txt, no analyze.py.

VM terminal 1:

    cd ~/odmr
    ./scripts/run_stream.sh

Windows terminal 2:

    cd C:\Users\dmitr\Desktop\udp_lab_sim
    .\scripts\spammer_odmr_compare.ps1 -DstHost 192.168.1.9 -Count 400

Stop capture with Ctrl+C after spammer finishes. Result: `runs/.../pulses_grouped.txt`.

Manual capture:

    RUN=~/odmr/runs/$(date +%Y%m%d_%H%M%S)
    sudo ~/odmr/build/packet_capture enp0s3 \
      --analyze-stream --output-dir "$RUN" --group-size 400
    sudo chown -R $(id -un):$(id -gn) "$RUN"

## Debug / regression (optional)

- `--record-raw` on packet_capture writes ch0/ch2 txt for offline checks
- `./scripts/verify_offline.sh "$RUN"` runs analyze.py and cmp vs stream
- Golden: `cmp tests/golden/20260613_134939_compare/pulses_grouped*.txt`

## VM sync from shared folder

    rsync -av --delete --exclude build/ --exclude runs/ /media/sf_odmr/ ~/odmr/
    bash scripts/ensure_utf8.sh

Channels: ch0 photon, ch2 trigger. See packet_collector/channel_config.h.
## Source encoding (UTF-8)

C sources must be **UTF-8 without BOM**, LF line endings. Some Windows editors
accidentally save as UTF-16; Linux gcc then fails with implicit declarations.

Before commit or push (especially after editing on Windows):

    python3 scripts/ensure_utf8.py
    # or: bash scripts/ensure_utf8.sh

If build on VM fails on analyze_stream.h, run ensure_utf8 locally and push again.

