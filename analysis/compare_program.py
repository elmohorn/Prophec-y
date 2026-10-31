#!/usr/bin/env python3
"""Convenience wrapper: compare one indexed hardware preset to a rendered WAV."""
from __future__ import annotations
import argparse, csv, subprocess, sys
from pathlib import Path


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('program', type=int)
    ap.add_argument('reference', type=Path)
    ap.add_argument('candidate', type=Path)
    ap.add_argument('--segments', type=Path, default=Path('reference_segments_coarse.csv'))
    ap.add_argument('--out', type=Path)
    args=ap.parse_args()
    rows=list(csv.DictReader(args.segments.open(encoding='utf-8')))
    hit=next((r for r in rows if int(r['program'])==args.program),None)
    if hit is None: raise SystemExit('program not present in segment index')
    compare=Path(__file__).with_name('compare_audio.py')
    cmd=[sys.executable,str(compare),str(args.reference),str(args.candidate),
         '--ref-start',hit['start_s'],'--ref-end',hit['end_s']]
    if args.out: cmd += ['--out',str(args.out)]
    print(f"program {args.program:03d} {hit['name']}  reference {hit['start_s']}..{hit['end_s']} s", file=sys.stderr)
    raise SystemExit(subprocess.call(cmd))

if __name__=='__main__': main()
