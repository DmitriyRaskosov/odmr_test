# odmr_test

UDP packet capture and streaming analyze for lab board (Linux).

Sender for integration tests: https://github.com/DmitriyRaskosov/udp_spammer

## Build

```bash
git clone https://github.com/DmitriyRaskosov/odmr_test.git ~/odmr
cd ~/odmr
rm -rf build
cmake -S . -B build
cmake --build build
```

## Capture (streaming analyze)

```bash
RUN=~/odmr/runs/$(date +%Y%m%d_%H%M%S)
sudo ~/odmr/build/packet_capture enp0s3 --analyze-stream --output-dir "$RUN" --group-size 2
```

Windows sender:

```powershell
python -u udp_spammer.py --dst-host 192.168.1.9 --odmr-pair --body-mode timestamps --timing fixed --interval 255e-6 --count 200
```

## VM sync from shared folder

```bash
rsync -av --delete --exclude build/ --exclude runs/ /media/sf_odmr/ ~/odmr/
bash scripts/ensure_utf8.sh
```

Channels: ch0 photon, ch2 trigger. See packet_collector/channel_config.h.