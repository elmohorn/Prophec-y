#!/usr/bin/env python3
"""Generate declared gate-duration hypotheses without pretending to infer MIDI.

Only note durations change. Original onsets, pitches and velocities stay fixed.
The gate fractions are sensitivity experiments, not recovered key-release truth.
"""
import argparse,csv,json,math
from pathlib import Path


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('manifest',type=Path);ap.add_argument('--case',required=True)
    ap.add_argument('--output',type=Path,required=True);a=ap.parse_args()
    rows=list(csv.DictReader(a.manifest.open()));matches=[r for r in rows if r['case_id']==a.case]
    if len(matches)!=1:ap.error('expected exactly one source case')
    case=matches[0];base=a.manifest.resolve().parent
    events=list(csv.reader((base/case['events']).open()));header,body=events[0],events[1:]
    a.output.mkdir(parents=True,exist_ok=True);cases=[]
    for fraction in (.25,.5,.75,1.):
        tag=f'gate{round(fraction*100):03}';path=a.output/(tag+'_events.csv')
        with path.open('w',newline='') as f:
            w=csv.writer(f);w.writerow(header)
            for row in body:
                if not row:continue
                changed=row.copy();duration=float(row[3])
                if not math.isfinite(duration) or duration<=0:raise ValueError('finite positive original durations required')
                changed[3]=f'{duration*fraction:.9f}';w.writerow(changed)
        r=case.copy();r['case_id']=a.case+'_'+tag;r['events']=str(path.resolve())
        for k in ('reference','sysex'):r[k]=str((base/r[k]).resolve())
        r['trust']='C';r['provenance']='Gate fraction '+str(fraction)+' of audible-region duration; sensitivity only, no verified note-off reconstruction'
        cases.append(r)
    with (a.output/'manifest.csv').open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=cases[0]);w.writeheader();w.writerows(cases)
    profiles={}
    for maximum in (10.,30.,60.):
        profiles[f'ampmax{maximum:g}']=dict(renderer_args=['--amp-env-max-seconds',str(maximum)],description='Only the provisional amp segment max is varied; no filter change',promotion_status='sensitivity-only')
    (a.output/'profiles.json').write_text(json.dumps(dict(schema_version=1,profiles=profiles),indent=2)+'\n')
    print(a.output/'manifest.csv')

if __name__=='__main__':main()
