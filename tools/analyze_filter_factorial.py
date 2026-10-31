#!/usr/bin/env python3
"""Paired pre-roll deltas and conditional main effects; no perceptual score."""
import argparse
import csv
from pathlib import Path
import statistics

METRICS=('mr_log_spectral_mae_db','log_band_shape_rmse_db','envelope_mae')
FACTORS=('lp_cutoff_v07','filter_env_depth_v07','filter_env_timing_v07','lp_resonance_v07')

def write(path,rows):
    with path.open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=rows[0]);w.writeheader();w.writerows(rows)

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('comparisons',type=Path);ap.add_argument('--output',type=Path,required=True)
    args=ap.parse_args();rows=list(csv.DictReader(args.comparisons.open()))
    index={(r['case_id'],r['profile'].removeprefix('factorial_'),r['pre_roll_s']):r for r in rows}
    if len(index)!=len(rows):raise ValueError('duplicate case/profile/pre-roll')
    out=[];effects=[]
    for case in sorted({r['case_id'] for r in rows}):
        for bits in sorted({key[1] for key in index if key[0]==case}):
            paired=[(r,index[(case,'0000',pre)]) for (c,b,pre),r in index.items() if c==case and b==bits]
            for metric in METRICS:
                ds=[float(r[metric])-float(base[metric]) for r,base in paired]
                out.append(dict(case_id=case,profile='factorial_'+bits,metric=metric,
                    paired_realizations=len(ds),delta_median=statistics.median(ds),
                    delta_min=min(ds),delta_max=max(ds),improved_realizations=sum(d<0 for d in ds)))
        for factor,name in enumerate(FACTORS):
            for metric in METRICS:
                ds=[]
                for (c,bits,pre),r in index.items():
                    if c!=case or bits[factor]!='0':continue
                    other=bits[:factor]+'1'+bits[factor+1:]
                    ds.append(float(index[(c,other,pre)][metric])-float(r[metric]))
                effects.append(dict(case_id=case,factor=name,metric=metric,
                    numerical_pairs=len(ds),mean_conditional_delta=statistics.mean(ds),
                    min_conditional_delta=min(ds),max_conditional_delta=max(ds),
                    caveat='deterministic model contrasts, not independent hardware trials'))
    args.output.mkdir(parents=True,exist_ok=True)
    write(args.output/'paired_deltas.csv',out);write(args.output/'conditional_factor_effects.csv',effects)
    print(f'wrote {len(out)} paired summaries and {len(effects)} conditional effect summaries')

if __name__=='__main__':main()
