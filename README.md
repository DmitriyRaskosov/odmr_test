# odmr_test

UDP packet capture and streaming analyze for lab board (Linux).

Sender: https://github.com/DmitriyRaskosov/udp_spammer

## Build

    git clone git@github.com:DmitriyRaskosov/odmr_test.git ~/odmr
    cd ~/odmr && rm -rf build && cmake -S . -B build && cmake --build build

## Stream vs offline compare

VM terminal 1:

    cd ~/odmr
    ./scripts/compare_stream.sh capture
    sudo chown -R $(id -un):$(id -gn) "$RUN"
    ./scripts/compare_stream.sh verify "$RUN"

Windows terminal 2:

    cd C:\Users\dmitr\Desktop\udp_lab_sim
    .\scripts\spammer_odmr_compare.ps1 -DstHost 192.168.1.9 -Count 400

Expected: IDENTICAL, ~6376 lines.

## Golden regression (no capture)

    cmp tests/golden/20260613_134939_compare/pulses_grouped.txt tests/golden/20260613_134939_compare/pulses_grouped_offline.txt && echo GOLDEN_OK

## VM sync from shared folder

    rsync -av --delete --exclude build/ --exclude runs/ /media/sf_odmr/ ~/odmr/
    bash scripts/ensure_utf8.sh