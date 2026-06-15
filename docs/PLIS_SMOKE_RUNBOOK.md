# ПЛИС: чеклист и команды перед тестом

Краткая шпаргалка для стенда. Контекст переноса и протокол — в [DEPLOYMENT_STAND.md](DEPLOYMENT_STAND.md).

---

## До подключения ПЛИС

```bash
cd ~/Рабочий\ стол/odmr_test
ls -l build/packet_capture && build/packet_capture --help
mkdir -p runs
```

Если бинарника нет:
```bash
cmake -S . -B build -DPython3_EXECUTABLE=$(which python3)
cmake --build build --target packet_capture
```

- Каналы: **ch0** фотоны, **ch2** триггер (дефолт `packet_capture`)
- Для полного `cv_odmr`: SpinCore (`spincore_driver/*.so`), Rigol (`/dev/usbtmc2`), `build/cv_odmr`
- Для **первого smoke** достаточно `packet_capture` + сеть

---

## Сеть (когда кабель к плате подключён)

```bash
ip -br link                                    # имя интерфейса, ожидается enp6s0
export IFACE=enp6s0

sudo bash packet_collector/set_addr.sh         # MAC 00:55:FF:FF:FF:FF
sudo ip addr flush dev $IFACE
sudo ip addr add 192.168.10.2/24 dev $IFACE
sudo ip link set $IFACE up
ip -br addr show $IFACE                        # → 192.168.10.2/24
```

Плата: `192.168.10.10`, UDP **4660**, кадр **1066 B**.

Проверка потока (ПЛИС должна уже слать):
```bash
sudo tcpdump -i $IFACE -c 20 udp port 4660
```

Нет пакетов в `tcpdump` → не запускать захват, чинить кабель / IP / питание.

---

## Smoke-тест приёма (1–5 мин)

**Только приём** (ПЛИС шлёт UDP сама или во время чужого режима):

```bash
cd ~/Рабочий\ стол/odmr_test
export IFACE=enp6s0
export RUN_LABEL=fpga_smoke
bash scripts/run_stream.sh
# 1–5 мин → Ctrl+C
```

Результат: `runs/YYYYMMDD_HHMMSS_fpga_smoke/pulses_grouped.txt`

**Полный ODMR** (два терминала):

| Терминал 1 (сначала) | Терминал 2 |
|----------------------|------------|
| `export IFACE=enp6s0 RUN_LABEL=production` | |
| `bash scripts/run_stream.sh` | `sudo ./build/cv_odmr enp6s0` |

Итог анализа — `runs/.../pulses_grouped.txt` из терминала 1 (не legacy `ch0_0.txt` в CWD).

---

## Критерии «тест прошёл»

| Метрика | Норма |
|---------|-------|
| `tcpdump` | UDP :4660, ~1066 B |
| `packets enqueued` | > 0 |
| `enqueue failures` | 0 |
| `kernel drops` | 0 |
| **`bad pulse windows`** | **0** |
| `pulses_grouped.txt` | не пустой |

---

## Если не сходится (debug)

```bash
RUN=runs/$(date +%Y%m%d_%H%M%S)_debug
mkdir -p "$RUN"
sudo build/packet_capture $IFACE --analyze-stream --record-raw \
  --output-dir "$RUN" --group-size 400
# Ctrl+C
sudo chown -R $(id -un):$(id -gn) "$RUN"
bash scripts/verify_offline.sh "$RUN"    # ожидается IDENTICAL
```

---

## Порядок одной строкой

```
ip link → set_addr.sh → ip 192.168.10.2/24 → tcpdump → run_stream.sh → [cv_odmr] → Ctrl+C → summary + pulses_grouped.txt
```
