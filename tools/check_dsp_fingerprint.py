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


def read_headroom(path, embedded=False):
    with open(path, newline='', encoding='utf-8') as stream:
        if embedded:
            for line in stream:
                if line.startswith('headroom_scenario,'):
                    break
            else:
                raise ValueError('Missing headroom table')
            # The header was consumed above; the column names are fixed by the
            # fingerprint producer, not inferred from the unrelated first table.
            names = line.strip().split(',')
            rows = csv.DictReader(stream, fieldnames=names)
        else:
            rows = csv.DictReader(stream)
        result = {}
        for row in rows:
            key = (row['headroom_scenario'], row['voices'])
            if key in result:
                raise ValueError(f'Duplicate headroom case: {key}')
            result[key] = row
        return result


def main():
    if len(sys.argv) != 4:
        raise SystemExit('Usage: check_dsp_fingerprint.py REFERENCE ACTUAL HEADROOM_REFERENCE')
    reference, actual = (read(p) for p in sys.argv[1:3])
    if set(reference) != set(actual):
        raise SystemExit('Fingerprint scenarios differ')
    for scenario, expected in reference.items():
        for metric in ('rms', 'peak', 'stereo_diff_rms', 'weighted_abs'):
            a, b = float(expected[metric]), float(actual[scenario][metric])
            if not math.isfinite(b) or not math.isclose(a, b, rel_tol=0.0001, abs_tol=0.00001):
                raise SystemExit(f'{scenario} {metric}: expected {a}, got {b}')
    expected_headroom = read_headroom(sys.argv[3])
    actual_headroom = read_headroom(sys.argv[2], embedded=True)
    if not expected_headroom or set(expected_headroom) != set(actual_headroom):
        raise SystemExit('Headroom scenarios differ or are empty')
    for scenario, expected in expected_headroom.items():
        for metric in ('poly_rms', 'poly_peak', 'poly_peak_dbfs',
                       'unison_rms', 'unison_peak', 'unison_peak_dbfs'):
            a, b = float(expected[metric]), float(actual_headroom[scenario][metric])
            if not math.isfinite(b) or not math.isclose(a, b, rel_tol=0.0001, abs_tol=0.00001):
                raise SystemExit(f'{scenario} {metric}: expected {a}, got {b}')
    print('Core DSP and headroom metrics match the versioned references')


if __name__ == '__main__':
    main()
