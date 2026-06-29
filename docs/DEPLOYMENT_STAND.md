# Перенос ODMR: VM + спаммер → Linux-стенд + ПЛИС

**Состояние:** июнь 2026, ветка **`home`**.  
**Репозитории:** [odmr_test](https://github.com/DmitriyRaskosov/odmr_test), [udp_spammer](https://github.com/DmitriyRaskosov/udp_spammer) (только тест без платы).

---

## 1. Цель

Production-контур на стенде:

```
ПЛИС (UDP) → Linux (packet_capture --analyze-stream) → runs/.../pulses_grouped.txt
```

**Не переписываем:** SpinCore, Rigol, `cv_odmr.ini` — это `cv_odmr` / rabi / impulse_odmr.

**Переносим:** приём UDP + online-группировка even/odd фотонов по точкам sweep.

---

## 2. Что проверено на VM (июнь 2026)

| Проверка | Результат |
|----------|-----------|
| Профиль | `-CvOdmrProfile` + `cv_odmr.ini` (36 частот × 100 повторов) |
| Пакетов | 14 400 enqueued, 0 kernel drops |
| Analyze | 36/36 групп, 7200 импульсов, bad windows: 0 |
| Reorder | late drops: 0, gap skips: 0 |
| Выход | `pulses_grouped.txt` (без raw `ch*.txt`) |

Ранние soak-тесты (10 мин / 2 ч, ~56M пакетов, `group_size=400`) — **устаревший** контур с плотным спаммером и другой группировкой; не использовать как эталон для cv_odmr.

---

## 3. Два режима в одном репозитории

| | **Лаборатория (legacy)** | **Production (новый)** |
|--|--------------------------|-------------------------|
| Запуск | `cv_odmr` / rabi / impulse_odmr | `packet_capture` + `run_stream.sh` |
| Оборудование | Rigol, SpinCore, ini | Сеть + ПЛИС (или спаммер на VM) |
| Python при capture | Да (builder) | **Нет** |
| Сырые `ch*.txt` | старый путь `cv_odmr` | **не пишутся** (`--analyze-stream`) |
| Результат | вручную `analyze.py` | сразу `pulses_grouped.txt` |
| Группировка | offline | `repeats_per_freq` + `expected_groups` из ini |

На стенде: **`run_stream.sh` параллельно эксперименту**, не встроенный захват `cv_odmr` без analyze.

---

## 4. Сеть и протокол (ПЛИС)

| Параметр | Значение |
|----------|----------|
| IP платы | 192.168.10.10 |
| IP приёмника | 192.168.10.2 (или IP ПК в подсети) |
| UDP порт | **4660** |
| Кадр Ethernet | **1066 B** (1024 payload) |
| Каналы ODMR | **ch0** фотоны, **ch2** триггер |
| Интервал пар ch0+ch2 | **~255 µs** (типично) |

ПЛИС: переменное число событий в пакете, джиттер — протокол тот же, наполнение другое.

---

## 5. Установка на стенде

```bash
git clone git@github.com:DmitriyRaskosov/odmr_test.git ~/odmr
cd ~/odmr
git checkout home
git pull origin home
rm -rf build
cmake -S . -B build -DPython3_EXECUTABLE="$(which python3)"
cmake --build build --target packet_capture
mkdir -p ~/odmr/runs
cp scripts/stand.env.example scripts/stand.env   # отредактировать IFACE
```

- Исходники: UTF-8 без BOM (`scripts/ensure_utf8.py`).
- Полный `cv_odmr` на стенде: Python ≥3.10, SpinCore, Rigol.

---

## 6. Запуск production

```bash
cd ~/odmr
bash scripts/run_stream.sh
```

Параллельно — штатный `cv_odmr` (или только плата + capture на VM-тесте).  
Остановка: **Ctrl+C** после окончания sweep.

Результат: `~/odmr/runs/YYYYMMDD_HHMMSS_<label>/pulses_grouped.txt`

---

## 7. Критерии go / no-go

**Smoke (5–10 мин):**

- [ ] `tcpdump -i $IFACE udp port 4660` — кадры ~1066 B
- [ ] `packets enqueued` > 0, `enqueue failures`: 0, `kernel drops`: 0
- [ ] `reorder late drops`: **0**, `reorder gap skips`: **0**
- [ ] `bad pulse windows`: **0**
- [ ] `analyze groups` = `expected_groups` из ini
- [ ] `pulses_grouped.txt` — N+1 строк (заголовок + группы)

**Опционально (debug):** capture с `--record-raw` + `verify_offline.sh` — только регрессия, не production.

---

## 8. Риски

| Риск | Действие |
|------|----------|
| Потеря триггера ch2 | `gap skips` > 0 или неполные группы — повторить эксперимент |
| `cv_odmr` без Rigol/SpinCore | Нужно железо или capture-only тест |
| ch0+ch1 в старом pcap | ODMR = ch0 + ch2 |
| Длинный эксперимент | Не включать `--record-raw` (десятки ГБ на диск) |

---

## 9. Спаммер (только тест)

Windows VM-тест:

```powershell
.\scripts\spammer_odmr_compare.ps1 -CvOdmrProfile -DstHost <VM_IP>
```

Не путать с `-Count 400` (короткий legacy smoke без полного sweep).

---

## 10. Контекст для нового чата

> ODMR home: `packet_capture --analyze-stream`, ini `cv_odmr.ini` → `repeats_per_freq` + `expected_groups`, выход `pulses_grouped.txt` (# group, even, odd). VM: `run_stream.sh` + `-CvOdmrProfile`. Reorder: старт counter 0/1, gap skip только на flush. Стенд: 192.168.10.10 → .2:4660, ch0/ch2.
