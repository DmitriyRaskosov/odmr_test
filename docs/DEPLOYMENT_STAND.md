# Перенос ODMR: VM + спаммер → Linux-стенд + ПЛИС

Документ для ссылки при новой установке Cursor / развёртывании на реальном стенде.

**Состояние на:** июнь 2026.
**Репозитории:** [odmr_test](https://github.com/DmitriyRaskosov/odmr_test) (приём + analyze), [udp_spammer](https://github.com/DmitriyRaskosov/udp_spammer) (только тест без платы).

---

## 1. Цель переноса

Заменить тестовый контур **Windows спаммер → VM packet_capture** на production на стенде:

```
ПЛИС (UDP) → Linux ПК (packet_capture --analyze-stream) → runs/.../pulses_grouped.txt
```

**Не цель:** переписать управление экспериментом (SpinCore, Rigol) — это по-прежнему cv_odmr / rabi / impulse_odmr.

**Цель:** принимать **реальный** UDP с платы и получать тот же pulses_grouped.txt, что проверен на VM (10 мин и 2 ч без потерь, bad pulse windows: 0).

---

## 2. Что уже проверено (VM + спаммер)

| Проверка | Результат |
|----------|-----------|
| Интервал между парами ch0+ch2 | **255 µs** (как в pcap платы) |
| Скорость | ~7840 UDP-пак/с, ~3922 пары/с |
| 10 мин | 4 705 882 пакета, 0 потерь, bad windows: 0 |
| 2 ч | 56 470 588 пакетов, 0 потерь, bad windows: 0 |
| Photon buffer peak | ~1021 (стабильно) |
| Reorder | peak 3, fix double-free в packet_reorder.c (cbdbcf1) |
| Выход | pulses_grouped.txt, group_size=400 |

Спаммер: udp_lab_sim, scripts/spammer_odmr_compare.ps1, -Count N (пакеты, не пары).

Приём: ~/odmr/scripts/run_stream.sh, бинарник build/packet_capture.

---

## 3. Два мира в одном odmr (не путать)

| | **Лаборатория (legacy)** | **Production (новый)** |
|--|--------------------------|-------------------------|
| Запуск | cv_odmr / rabi / impulse_odmr | packet_capture + run_stream.sh |
| Оборудование | Rigol (/dev/usbtmc1), SpinCore, ini | Только сеть + ПЛИС |
| Python при старте | Да (builder.py) | Нет |
| Сырые файлы | ch0_0.txt, ch2_0.txt в CWD | по умолчанию **не пишутся** |
| Результат | вручную analyze.py | сразу pulses_grouped.txt |
| Спаммер | не нужен | только для тестов без платы |

**На стенде:** для pulses_grouped использовать **packet_capture --analyze-stream** параллельно эксперименту, а не встроенный захват cv_odmr без analyze.

---

## 4. Сеть и протокол (ПЛИС)

| Параметр | Значение |
|----------|----------|
| IP платы | 192.168.10.10 |
| IP приёмника | 192.168.10.2 (или фактический IP ПК в подсети) |
| UDP порт | **4660** |
| Кадр Ethernet | **1066** B (1024 payload) |
| Payload | байт 0 = канал, 2–3 = counter, 4–1023 = **255×uint32** timestamps |
| ODMR каналы | **ch0** фотоны, **ch2** триггер |
| Частота кадров | **~255 µs** между парами (не 1–5 µs) |

Отличие ПЛИС от спаммера: **переменное** число событий в пакете, джиттер, реальные нс — протокол тот же, наполнение другое.

---

## 5. Установка на Linux-стенде

```bash
git clone git@github.com:DmitriyRaskosov/odmr_test.git ~/odmr
cd ~/odmr
git pull github main
rm -rf build
cmake -S . -B build -DPython3_EXECUTABLE=$(which python3)
cmake --build build --target packet_capture
mkdir -p ~/odmr/runs
```

- Исходники: **UTF-8 без BOM** (scripts/ensure_utf8.py после правок с Windows).
- packet_capture **не требует** Python в runtime.
- cv_odmr/rabi — нужны Python **≥3.10** dev headers и SpinCore/Rigol при полном эксперименте.

---

## 6. Запуск production на стенде

```bash
cd ~/odmr
export IFACE=<eth к плате>
export RUN_LABEL=production
./scripts/run_stream.sh
```

Параллельно — штатный запуск эксперимента. Остановка захвата: **Ctrl+C** после окончания эксперимента.

Результат: ~/odmr/runs/YYYYMMDD_HHMMSS_<label>/pulses_grouped.txt

---

## 7. Критерии go / no-go на ПЛИС

**Smoke (5–10 мин):**

- [ ] tcpdump -i $IFACE udp port 4660 — кадры ~1066 B
- [ ] packets enqueued > 0
- [ ] enqueue failures: 0, kernel drops: 0
- [ ] **bad pulse windows: 0**
- [ ] photon buffer peak порядка сотен–тысяч
- [ ] pulses_grouped.txt не пустой

**Желательно (debug):** --record-raw + ./scripts/verify_offline.sh (IDENTICAL).

---

## 8. Риски совместимости

| Риск | Действие |
|------|----------|
| cv_odmr без Rigol/SpinCore | На стенде нужно железо или правки |
| Старые скрипты ищут ch0_0.txt в CWD | Обновить пути на runs/... |
| ch0+ch1 в старом pcap vs ch0+ch2 ODMR | Прошивка должна слать ch2 trigger |
| Откат | odmr_legacy или старый коммит |

---

## 9. Что не переносится со спаммера

- Windows udp_lab_sim — только тест.
- IP 192.168.1.9 VM — на стенде 192.168.10.x.
- Идеальные 255 событий × 100 ns в каждом пакете — артефакт симулятора.

---

## 10. Ключевые коммиты

| Репо | Что |
|------|-----|
| odmr_test | packet_capture, analyze_stream.c, packet_reorder.c, run_stream.sh |
| udp_spammer | OdmrPairFactory fast path, 255 µs, коммит 276c5d5 |
| Golden | tests/golden/20260613_134939_compare/ |

---

## 11. Контекст для нового чата в Cursor

> Проект ODMR: приём UDP с платы (ch0 photon, ch2 trigger), online analyze в packet_capture --analyze-stream, выход pulses_grouped.txt (group_size=400). На VM проверено со спаммером 255 µs, 2 ч без потерь. Переносим на Linux-стенд с реальной ПЛИС: run_stream.sh, сеть 192.168.10.10→.2:4660, не путать с cv_odmr/Rigol/SpinCore. Репо: odmr_test + udp_spammer (только тест).

---

## 12. Следующие шаги

- [ ] Первый smoke на ПЛИС с IFACE стенда
- [ ] При необходимости — короткий --record-raw + verify_offline.sh
- [ ] Полный эксперимент + сверка числа групп с длительностью sweep
- [ ] (Опционально) нерегулярные фотоны в спаммере — без ускорения UDP до 1–5 µs
- [ ] (Опционально) графики из pulses_grouped