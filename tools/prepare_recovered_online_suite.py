#!/usr/bin/env python3
"""Restore private reference inputs from existing listening ZIPs and factory bank.

Usage: prepare_recovered_online_suite.py FACTORY.syx LISTEN_V08.zip LISTEN_V09.zip
Only specific known members are read; no archive paths are extracted wholesale.
Inferred event CSVs are project data under calibration/recovered_events/.
"""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('bank',type=Path);ap.add_argument('listen_v08',type=Path)
    ap.add_argument('listen_v09',type=Path)
    ap.add_argument('--output',type=Path,default=Path('reference_private/online'))
    a=ap.parse_args()
    raw=a.bank.read_bytes()
    messages=[m+b'\xf7' for m in raw.split(b'\xf7') if m]
    if len(messages)!=500 or any(len(m)!=1178 or m[:4]!=b'\xf0\x01\x2d\x02' for m in messages):
        ap.error('expected original 500-program Prophet-6 factory bank')
    for n,m in enumerate(messages):
        if m[4]*100+m[5]!=n:ap.error('unexpected bank/program ordering')
    members={
        0:('v08','p6_referenceA_listen_v0.8/000_Brassed_Off_A_REFERENCE.wav'),
        24:('v08','p6_referenceA_listen_v0.8/024_Solid_Bass_A_REFERENCE.wav'),
        67:('v08','p6_referenceA_listen_v0.8/067_Carpenter_Square_A_REFERENCE.wav'),
        266:('v09','p6_referenceA_listen_v0.9/01_SyncStack_REFERENCE_note41.wav'),
        99:('v09','p6_referenceA_listen_v0.9/04_PolyModster_REFERENCE_note36.wav')}
    with zipfile.ZipFile(a.listen_v08) as v08,zipfile.ZipFile(a.listen_v09) as v09:
        # Validate members before creating partial inputs.
        content={n:(v08 if version=='v08' else v09).read(member) for n,(version,member) in members.items()}
    a.output.mkdir(parents=True,exist_ok=True)
    for n,wav in content.items():
        (a.output/f'{n:03}.syx').write_bytes(messages[n])
        (a.output/f'{n:03}_reference.wav').write_bytes(wav)
    print(json.dumps({'programs':len(content),'output':str(a.output),
        'bank_sha256':hashlib.sha256(raw).hexdigest()}))

if __name__=='__main__':main()
