#!/usr/bin/env python3
import sys
import os
import bisect
import argparse

def read_photons(filename):
    photons = []
    if not os.path.exists(filename):
        print(f"Warning: {filename} not found")
        return photons
    
    with open(filename, 'r', encoding='utf-8', errors='ignore') as f:
        for line in f:
            line = ''.join(c for c in line if c.isprintable())
            if not line:
                continue
            try:
                ts = float(line.strip())
                photons.append(ts)
            except ValueError:
                continue
    return sorted(photons)

def read_triggers(filename):
    triggers = []
    if not os.path.exists(filename):
        print(f"Warning: {filename} not found")
        return triggers
    
    with open(filename, 'r', encoding='utf-8', errors='ignore') as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            # Убираем последнюю цифру (фронт), оставляем только время
            if '.' in line:
                parts = line.split('.')
                if len(parts) == 2:
                    # Оставляем целую часть и дробную без последней цифры
                    fractional = parts[1][:-1] if len(parts[1]) > 1 else '0'
                    line = f"{parts[0]}.{fractional}"
            try:
                triggers.append(float(line))
            except ValueError:
                continue
    return triggers

def count_photons_between(photons, start, end):
    if not photons:
        return 0
    # ИСПРАВЛЕНО: теперь включаем start (>=), но не включаем end (<)
    left = bisect.bisect_left(photons, start)   # первый индекс >= start
    right = bisect.bisect_left(photons, end)    # первый индекс >= end
    return right - left

def main():
    parser = argparse.ArgumentParser(description='Count photons in even/odd pulses')
    parser.add_argument('--ch0', default='ch0_0.txt', help='CH0 photons file')
    parser.add_argument('--trig', default='ch2_0.txt', help='Triggers file')
    parser.add_argument('--n', type=int, default=400, help='Number of pulse pairs per group (default: 400)')
    parser.add_argument('--output', '-o', default='pulses_grouped.txt', help='Output file')
    
    args = parser.parse_args()
    
    print("Loading data...")
    photons_ch0 = read_photons(args.ch0)
    triggers = read_triggers(args.trig)
    
    print(f"  CH0 photons: {len(photons_ch0)}")
    print(f"  Triggers: {len(triggers)}")
    
    if len(triggers) == 0:
        print("No triggers found!")
        return
    
    # Формируем пары (триггеры идут парами: начало, конец)
    if len(triggers) % 2 != 0:
        print(f"  Warning: odd number of triggers, dropping last one")
        triggers = triggers[:-1]
    
    pulses = [(triggers[i], triggers[i+1]) for i in range(0, len(triggers), 2)]
    total_pulses = len(pulses)
    print(f"  Total pulses: {total_pulses}")
    
    # Считаем фотоны для каждого пульса
    pulse_counts = []
    for start, end in pulses:
        cnt = count_photons_between(photons_ch0, start, end)
        pulse_counts.append(cnt)
    
    print(f"  Pulse counts: {pulse_counts}")
    
    # Группируем по n пар (чётный + нечётный)
    n = args.n
    results = []
    
    # Проходим по парам пульсов (чётный-нечётный)
    for group_start in range(0, len(pulse_counts) // 2, n):
        even_sum = 0
        odd_sum = 0
        
        # Суммируем n чётных и n нечётных пульсов
        for i in range(n):
            even_idx = (group_start + i) * 2
            odd_idx = even_idx + 1
            
            if even_idx < len(pulse_counts):
                even_sum += pulse_counts[even_idx]
            if odd_idx < len(pulse_counts):
                odd_sum += pulse_counts[odd_idx]
        
        results.append((even_sum, odd_sum))
    
    # Сохраняем в файл
    with open(args.output, 'w') as f:
        f.write("# group, even_photons, odd_photons\n")
        for i, (even_val, odd_val) in enumerate(results):
            f.write(f"{i}, {even_val}, {odd_val}\n")
    
    print(f"\nSaved to {args.output}")
    print(f"  Groups: {len(results)} (each group = {n} even + {n} odd pulses)")
    
    # Вывод результатов
    print("\n=== RESULTS ===")
    for i, (even_val, odd_val) in enumerate(results):
        print(f"Group {i}: even={even_val}, odd={odd_val}")
    
    # Статистика
    if results:
        even_vals = [r[0] for r in results]
        odd_vals = [r[1] for r in results]
        print(f"\n=== STATISTICS ===")
        print(f"Even groups: min={min(even_vals)}, max={max(even_vals)}, avg={sum(even_vals)/len(even_vals):.2f}")
        print(f"Odd groups:  min={min(odd_vals)}, max={max(odd_vals)}, avg={sum(odd_vals)/len(odd_vals):.2f}")

if __name__ == "__main__":
    main()