# odmr_test

UDP packet capture and **online** streaming analyze for the lab board (Linux).

Test sender: [udp_spammer](https://github.com/DmitriyRaskosov/udp_spammer)  
Active branch: **`home`**

## Build

```bash
git clone git@github.com:DmitriyRaskosov/odmr_test.git ~/odmr
cd ~/odmr
git checkout home
rm -rf build
cmake -S . -B build && cmake --build build --target packet_capture
mkdir -p ~/odmr/runs
```

`packet_capture` does **not** need Python at runtime.

## Production pipeline (stream only)

Online analyze writes `pulses_grouped.txt` under `runs/`. **No** raw `ch*.txt`, **no** `analyze.py`.

Grouping uses `cv_odmr.ini`:

- `number_of_repeats` → even/odd pulse pairs per frequency row
- Rigol sweep → `expected_groups` (row count in output)

**VM terminal 1** (start first):

```bash
cd ~/odmr
bash scripts/run_stream.sh
```

Optional stand config: copy `scripts/stand.env.example` → `scripts/stand.env` (`IFACE`, `EXPERIMENT_INI`).

**Windows terminal 2** (after capture is running):

```powershell
cd C:\Users\dmitr\Desktop\udp_lab_sim
.\scripts\spammer_odmr_compare.ps1 -CvOdmrProfile -DstHost 192.168.1.9
```

Quick smoke (3 frequencies × 5 repeats): add `-QuickTest` on Windows and point VM `EXPERIMENT_INI` at matching mini ini.

Stop capture with **Ctrl+C** after the spammer finishes.

Result: `runs/YYYYMMDD_HHMMSS_run/pulses_grouped.txt` — header plus one row per sweep point:

```
# group, even_photons, odd_photons
0, ...
```

Frequency in MHz is **not** in the file; map `group` → MHz from `cv_odmr.ini` when plotting.

### Success criteria (stderr summary)

| Metric | Expected |
|--------|----------|
| `packets enqueued` | matches sender (e.g. 14400 for full `cv_odmr.ini` profile) |
| `reorder late drops` | **0** |
| `reorder gap skips` | **0** |
| `bad pulse windows` | **0** |
| `analyze groups` | `expected_groups` from ini (e.g. 36) |

Manual capture (equivalent to `run_stream.sh`):

```bash
RUN=~/odmr/runs/$(date +%Y%m%d_%H%M%S)_run
sudo ~/odmr/build/packet_capture enp0s3 \
  --analyze-stream \
  --output-dir "$RUN" \
  --experiment-ini ~/odmr/cv_odmr.ini
sudo chown -R "$(id -un):$(id -gn)" "$RUN"
```

`--analyze-stream` disables raw `ch*.txt` automatically (`record_raw=0`).

## Laboratory stand (FPGA)

Same as VM test, but real UDP from the board:

```bash
export IFACE=<eth-to-FPGA>   # or scripts/stand.env
./scripts/run_stream.sh
```

Run **in parallel** with `cv_odmr` (SpinCore + Rigol). See [docs/DEPLOYMENT_STAND.md](docs/DEPLOYMENT_STAND.md).

## Debug / regression (optional)

Not used in normal experiments:

- `packet_capture ... --record-raw` — also writes `ch0_0.txt`, `ch2_0.txt` under `--output-dir`
- `./scripts/verify_offline.sh "$RUN"` — compares stream vs offline `analyze.py`
- Golden: `tests/golden/20260613_134939_compare/` (historical, required `--record-raw`)

## Channels

ch0 photon, ch2 trigger — see `packet_collector/channel_config.h`.

## Source encoding (UTF-8)

C sources must be **UTF-8 without BOM**, LF line endings. Before push from Windows:

```bash
python3 scripts/ensure_utf8.py
```
