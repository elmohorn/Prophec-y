#!/usr/bin/env python3
"""Chord companion controls and uncertain-age preset507 diagnostic comparisons."""
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import math
from pathlib import Path
import subprocess
import numpy as np
from scipy.io import wavfile
from scipy.signal import welch
from analyze_sync_stages import power_harmonics,write_csv
from compare_online_suite import decode,compare,SR
from compare_sync_recordings import error


def peak_pitch(x,expected):
    mono=x.mean(axis=1);n=len(mono);p=abs(np.fft.rfft((mono-mono.mean())*np.hanning(n),n*8))**2;f=np.fft.rfftfreq(n*8,1/SR)
    use=np.flatnonzero(abs(1200*np.log2(np.maximum(f,1e-20)/expected))<35)
    k=use[np.argmax(p[use])]
    if k in [use[0],use[-1]]:raise ValueError('pitch search edge')
    return float(f[k])


def chord_harmonics(x,pitches):
    f,p=welch(x,SR,nperseg=len(x),axis=0);p=p.mean(axis=1)
    values=[];valid=[]
    for i,f0 in enumerate(pitches):
        powers=[]
        for h in range(1,17):
            target=f0*h;width=max(3,target*.003)
            powers.append(float(p[abs(f-target)<=width].sum()))
            collision=any(abs(target-other*k)<2*width for j,other in enumerate(pitches) if j!=i for k in range(1,21))
            valid.append(not collision)
        v=10*np.log10(np.maximum(np.array(powers)/max(powers[0],1e-30),1e-24));values.extend(v)
    return np.array(values),np.array(valid)


def main():
    root=Path('reference_private/sync_search');out=Path('reference_private/sync_comparison');(out/'controls').mkdir(exist_ok=True);(out/'preset507').mkdir(exist_ok=True)
    specs=[]
    for wave,shape in [('Saw',.5),('Pulse',1.)]:
        x=decode(root/f'OB6-P6-Vergleich-Waveform-{wave}_P6_right.wav')
        for start,notes in [(1.5,[48,51,54]),(6.5,[50,53,56]),(10.5,[48,51,54]),(15.5,[50,53,56])]:
            ref=x[round(start*SR):round((start+1)*SR)]
            pitches=[peak_pitch(ref,440*2**((n-69)/12)) for n in notes]
            for os in [2,8]:specs.append((wave,shape,start,notes,pitches,os,ref))
    protocol={'chord_controls':{'windows_s':[1.5,6.5,10.5,15.5],'notes':[[48,51,54],[50,53,56]],'saw_shape':.5,'pulse_shape':1,'pulse_width':.5,'oversampling':[2,8],'duration_s':1,'metric':'per-note harmonic dBc, exclude overlapping partials; equal-gain three-note sum; unknown real mixer/filter/envelopes'},'preset507':{'sources':[{'file':'507-SyncLead-Uni.mp3','start_s':13.5,'duration_s':.25,'note_input':79,'expected_f0_hz':392},{'file':'Audiofanzine_507.mp3','start_s':13.75,'duration_s':.25,'note_input':57,'expected_f0_hz':110}],'profiles':['v09','depth12','reset_half'],'prerolls_s':[0,.5,1.5],'candidate_note_ages_s':[.25,.75,1.25],'velocity':1,'gate_s':1.75,'oversampling':2,'assumptions':'factory007 is only an assumed match for published507; the sound-bank version, note gates, controllers and previous-note history are not known; sustain snippets are not performance reconstructions'},'frozen_before_control_or_preset_model_scores':True}
    Path('calibration/v019_sync_comparison/control_preset_protocol.json').write_text(json.dumps(protocol,indent=2)+'\n')
    def control(spec):
        wave,shape,start,notes,pitches,os,ref=spec;name=f'{wave}_{start:g}_os{os}';parts=[]
        for i,(note,f0) in enumerate(zip(notes,pitches)):
            dest=out/'controls'/f'{name}_voice{i}.wav';offset=12*math.log2(f0/(440*2**((note-69)/12)))
            cmd=['build-check/p6_sync_probe',str(dest),'--free-running','--note',str(note),'--slave-semis',str(offset),'--shape1',str(shape),'--oversampling',str(os),'--seconds','1','--preroll','.5','--seed',str(982+i)]
            subprocess.run(cmd,check=True,capture_output=True);parts.append(decode(dest))
        y=sum(parts)/3;dest=out/'controls'/(name+'.wav');wavfile.write(dest,SR,y.astype('float32'))
        rh,mask=chord_harmonics(ref,pitches);ch,_=chord_harmonics(y,pitches);mask &= rh>=-40
        metrics=compare(ref,y,kind='steady')
        return dict(wave=wave,start_s=start,oversampling=os,pitches_hz=json.dumps(pitches),included_partials=int(mask.sum()),harmonic_rmse_floor60_db=float(np.sqrt(np.mean((np.maximum(rh[mask],-60)-np.maximum(ch[mask],-60))**2))),**{k:metrics[k] for k in ['mr_log_spectral_mae_db','log_band_shape_rmse_db']},wav=str(dest))
    with ThreadPoolExecutor(max_workers=4) as pool:controls=list(pool.map(control,specs))
    write_csv(out/'chord_controls.csv',controls)
    jobs=[(source,profile,pre) for source in protocol['preset507']['sources'] for profile in protocol['preset507']['profiles'] for pre in protocol['preset507']['prerolls_s']]
    def preset(job):
        source,profile,pre=job;name=Path(source['file']).stem;wav=out/'preset507'/f'{name}_{profile}_pre{pre:g}.wav'
        cmd=['build-check/p6_render_cal','reference_private/007.syx',str(wav),str(source['note_input']),'--reference-a-v09','--note-seconds','1.75','--release-seconds','.05','--pre-roll-seconds',str(pre)]
        if profile=='depth12':cmd+=['--filter-env-octaves','12']
        elif profile=='reset_half':cmd+=['--sync-reset-phase','.5']
        result=subprocess.run(cmd,check=True,capture_output=True,text=True);wav.with_suffix('.log').write_text(result.stdout+result.stderr)
        ref=decode(root/source['file'])[round(source['start_s']*SR):round((source['start_s']+source['duration_s'])*SR)];y=decode(wav);rm,rh=power_harmonics(ref,source['expected_f0_hz'],16);rh-=rh[0];rows=[]
        if rm['f0_search_edge']:raise ValueError('preset reference pitch boundary')
        for age in protocol['preset507']['candidate_note_ages_s']:
            candidate=y[round(age*SR):round((age+.25)*SR)];cm,ch=power_harmonics(candidate,source['expected_f0_hz'],16);ch-=ch[0];m=compare(ref,candidate,kind='steady')
            rows.append(dict(source=source['file'],reference_start_s=source['start_s'],profile=profile,preroll_s=pre,assumed_note_age_s=age,note=source['note_input'],reference_f0_hz=rm['power_f0_hz'],candidate_f0_hz=cm['power_f0_hz'],candidate_f0_edge=cm['f0_search_edge'],harmonic_rmse_floor60_db=error(ch,rh,range(1,17)),**{k:m[k] for k in ['mr_log_spectral_mae_db','log_band_shape_rmse_db']},wav=str(wav)))
        return rows
    with ThreadPoolExecutor(max_workers=4) as pool:rows=[r for group in pool.map(preset,jobs) for r in group]
    write_csv(out/'preset507_comparisons.csv',rows)
    summary=[]
    for source in protocol['preset507']['sources']:
        for profile in protocol['preset507']['profiles']:
            ss=[x for x in rows if x['source']==source['file'] and x['profile']==profile]
            summary.append(dict(source=source['file'],profile=profile,windows=len(ss),median_harmonic_rmse_db=float(np.median([x['harmonic_rmse_floor60_db'] for x in ss])),median_mr_spectral_mae_db=float(np.median([x['mr_log_spectral_mae_db'] for x in ss])),median_band_rmse_db=float(np.median([x['log_band_shape_rmse_db'] for x in ss])),f0_edge_count=sum(x['candidate_f0_edge'] for x in ss)))
    write_csv(out/'preset507_summary.csv',summary)
    (out/'control_preset_provenance.json').write_text(json.dumps({'component_renders':48,'preset_renders':18,'additional_prior_pitch_check_renders':1,'sysex_sha256':hashlib.sha256(Path('reference_private/007.syx').read_bytes()).hexdigest(),'factory_bank_sha256':hashlib.sha256(Path('reference_private/factory.syx').read_bytes()).hexdigest(),'factory_zip_url':'https://sequential.com/wp-content/uploads/2015/07/P6-Factory-Programs-ReadMe.zip','production_changes':False},indent=2)+'\n')
    print(json.dumps({'controls':controls,'preset507':summary}))


if __name__=='__main__':main()
