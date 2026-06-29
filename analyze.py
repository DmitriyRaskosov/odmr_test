#!/usr/bin/env python3
"""Offline debug: same grouping logic as analyze_stream (legacy, optional)."""
import argparse
import bisect
import os


def read_photons(filename):
    photons = []
    if not os.path.exists(filename):
        print(f"Warning: {filename} not found")
        return photons

    with open(filename, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            line = "".join(c for c in line if c.isprintable())
            if not line:
                continue
            try:
                photons.append(float(line.strip()))
            except ValueError:
                continue
    return sorted(photons)


def read_triggers(filename):
    triggers = []
    if not os.path.exists(filename):
        print(f"Warning: {filename} not found")
        return triggers

    with open(filename, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            line = "".join(c for c in line.strip() if c.isprintable())
            if not line or len(line) < 2:
                continue
            try:
                triggers.append(float(line[:-1]))
            except ValueError:
                continue
    return triggers


def count_photons_between(photons, start, end):
    if not photons:
        return 0
    left = bisect.bisect_left(photons, start)
    right = bisect.bisect_left(photons, end)
    return max(0, right - left)


def main():
    parser = argparse.ArgumentParser(
        description="Count photons in even/odd pulses per sweep group"
    )
    parser.add_argument("--ch0", default="ch0_0.txt", help="CH0 photons file")
    parser.add_argument("--trig", default="ch2_0.txt", help="Triggers file")
    parser.add_argument(
        "--repeats-per-freq",
        "--n",
        type=int,
        dest="repeats_per_freq",
        default=1000,
        help="Even+odd pulse pairs per group",
    )
    parser.add_argument("--output", "-o", default="pulses_grouped.txt", help="Output file")

    args = parser.parse_args()
    repeats = args.repeats_per_freq

    print("Loading data...")
    photons_ch0 = read_photons(args.ch0)
    triggers = read_triggers(args.trig)

    print(f"  CH0 photons: {len(photons_ch0)}")
    print(f"  Triggers: {len(triggers)}")
    print(f"  repeats_per_freq: {repeats}")

    if len(triggers) == 0:
        print("No triggers found!")
        return 1

    if len(triggers) % 2 != 0:
        print("  Warning: odd number of triggers, dropping last one")
        triggers = triggers[:-1]

    pulses = [(triggers[i], triggers[i + 1]) for i in range(0, len(triggers), 2)]
    print(f"  Total pulses: {len(pulses)}")

    pulse_counts = [
        count_photons_between(photons_ch0, start, end) for start, end in pulses
    ]

    results = []
    for group_start in range(0, len(pulse_counts) // 2, repeats):
        even_sum = 0
        odd_sum = 0
        for i in range(repeats):
            even_idx = (group_start + i) * 2
            odd_idx = even_idx + 1
            if even_idx < len(pulse_counts):
                even_sum += pulse_counts[even_idx]
            if odd_idx < len(pulse_counts):
                odd_sum += pulse_counts[odd_idx]
        results.append((even_sum, odd_sum))

    with open(args.output, "w", encoding="utf-8") as f:
        f.write("# group, even_photons, odd_photons\n")
        for i, (even_val, odd_val) in enumerate(results):
            f.write(f"{i}, {even_val}, {odd_val}\n")

    print(f"\nSaved to {args.output}")
    print(f"  Groups written: {len(results)} (each = {repeats} even + {repeats} odd pulses)")

    print("\n=== RESULTS ===")
    for i, (even_val, odd_val) in enumerate(results):
        print(f"Group {i}: even={even_val}, odd={odd_val}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
