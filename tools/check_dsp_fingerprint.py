#!/usr/bin/env python3
"""Compare stable core render metrics; hashes remain diagnostic across toolchains."""
import csv
import math
import sys


def read(path):
    with open(path, newline='', encoding='utf-8') as stream:
        rows = csv.DictReader(stream)
        result = {}
        for row in rows:
            if row['scenario'] == 'headroom_scenario':
                break
            result[row['scenario']] = row
        return result


def main():
    reference, actual = (read(p) for p in sys.argv[1:3])
    if set(reference) != set(actual):
        raise SystemExit('Fingerprint scenarios differ')
    for scenario, expected in reference.items():
        for metric in ('rms', 'peak', 'stereo_diff_rms', 'weighted_abs'):
            a, b = float(expected[metric]), float(actual[scenario][metric])
            if not math.isfinite(b) or not math.isclose(a, b, rel_tol=0.0001, abs_tol=0.00001):
                raise SystemExit(f'{scenario} {metric}: expected {a}, got {b}')
    print('Core DSP metrics match the versioned reference')


if __name__ == '__main__':
    main()
