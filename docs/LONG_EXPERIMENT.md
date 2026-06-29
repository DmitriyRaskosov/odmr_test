# Long ODMR capture experiments

Endurance tests for `packet_capture --analyze-stream` with the Windows [udp_spammer](https://github.com/DmitriyRaskosov/udp_spammer) sender.

**Production path:** streaming analyze only — no `ch*.txt`, no `--record-raw`, no `analyze.py`.

---

## Test tiers

| ID | Duration | VM capture | Windows spammer | ~UDP packets |
|----|----------|------------|-----------------|--------------|
| T0 | ~30 s | `run_stream.sh` | `-CvOdmrProfile` | 14 400 |
| T1 | 10 min | `run_soak.sh` SOAK=600 | `-LongCvOdmr -DurationSec 600` | ~4.7M |
| T2 | 1 h | `run_soak.sh` SOAK=3600 | `-SoakOdmrPair -DurationSec 3600` | ~28M |
| T3 | 10 h+ | same, longer | same | ~280M |

At **255 µs** between ch0+ch2 pairs: **~7843 packets/s** (one pair every 255 µs → 2 packets).

---

## Success criteria

From stderr / `soak.log` at end of run:

| Metric | Expected |
|--------|----------|
| `enqueue failures` | **0** |
| `kernel drops` | **0** |
| `reorder late drops` | **0** |
| `reorder gap skips` | **0** |
| `bad pulse windows` | **0** |

T0 additionally: `analyze groups: 36 (expected 36)`.

Soak runs use `--soak` → `expected_groups=0` (no fixed row count; `pulses_grouped.txt` grows with completed frequency blocks).

---

## Architecture notes

- **Memory:** analyze is streaming; photon buffer trimmed per pulse; output size O(completed groups), not O(packets).
- **Variable UDP count per pulse** is OK — ch2 photons summed in the trigger window.
- **uint16 packet counter** wraps every ~65536 packets (~8.4 s at full rate). Long soaks validate reorder across many wraps.
- **`--record-raw` forbidden** on T1+ (disk).

Frequency boundaries are still by **pulse count** (`repeats_per_freq` from ini), not Rigol hardware marker (future work).

---

## Commands

### Build (VM)

```bash
cd ~/odmr && git pull origin home
cmake --build build --target packet_capture
```

### T0 — functional (done)

```bash
bash scripts/run_stream.sh
```

```powershell
.\scripts\spammer_odmr_compare.ps1 -CvOdmrProfile -DstHost 192.168.1.9
```

### T1 — 10 min cv_odmr loop

**VM first**, then Windows:

```bash
SOAK_DURATION_SEC=600 RUN_LABEL=cv_odmr_10m bash scripts/run_soak.sh
```

```powershell
.\scripts\spammer_odmr_compare.ps1 -LongCvOdmr -DurationSec 600 -DstHost 192.168.1.9
```

### T2 — 1 h dense OdmrPair soak

```bash
SOAK_DURATION_SEC=3600 RUN_LABEL=soak_1h bash scripts/run_soak.sh
```

```powershell
.\scripts\spammer_odmr_compare.ps1 -SoakOdmrPair -DurationSec 3600 -DstHost 192.168.1.9
```

---

## `run_soak.sh` behaviour

- `packet_capture --soak` → `--analyze-stream`, no raw, `expected_groups=0`
- Still reads `cv_odmr.ini` for `repeats_per_freq` (grouping window size)
- `timeout` stops capture after `SOAK_DURATION_SEC` (default 3600)
- stderr tee'd to `runs/.../soak.log`
- Periodic stats every few seconds: enqueued, queue, pulses, groups, photon_peak, bad_win

---

## Future phases

- P5: frequency boundary from marker / CH3 in `analyze_stream`
- P6: auto-stop capture aligned with cv_odmr lifecycle
- Spammer: interval from `t1..t5` in ini (optional)

See also [udp_spammer TODO](https://github.com/DmitriyRaskosov/udp_spammer/blob/home/TODO.md).
