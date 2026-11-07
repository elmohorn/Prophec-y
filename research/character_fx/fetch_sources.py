#!/usr/bin/env python3
"""Acquire pinned, MIT-licensed comparison sources into a private scratch folder.

No Git submodules, SDKs, impulse responses or GUI assets are downloaded.
All source copyright/license notices remain with the inspected files.
"""
import argparse,hashlib,io,json,tarfile,urllib.request
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor

def main():
 ap=argparse.ArgumentParser();ap.add_argument('output',type=Path);a=ap.parse_args();root=a.output.resolve();root.mkdir(parents=True,exist_ok=True)
 registry=Path(__file__).resolve().parents[2]/'calibration/prophecy_fx_sources_v020.json'
 repos={r['repository']:r['sha'] for r in json.loads(registry.read_text())['repositories']}
 pinned_archives={'CristianMoresi/DSPark':'b934c587b6171549c5a2c9bd024e373b48d37b515e7fb31f013db06096a6388e','ShmKnd/Patina':'85af7dd9fdfcf94a57d3b3b915b847b8a4b60af0fab28a2abe4b202fb3584e2e','daisyaudio/DaisySP':'78db7ee9da2d472bf7316a1d16ad7100d174dc1bc90422bc3a55a11760d6014d'}
 def fetch(repo):
  sha=repos[repo];dest=root/repo.split('/')[1];data=urllib.request.urlopen(f'https://codeload.github.com/{repo}/tar.gz/{sha}',timeout=40).read(35*1024*1024+1)
  actual=hashlib.sha256(data).hexdigest()
  if len(data)>35*1024*1024 or actual!=pinned_archives[repo]:raise ValueError('changed/oversized archive '+repo)
  dest.mkdir(exist_ok=True)
  with tarfile.open(fileobj=io.BytesIO(data),mode='r:gz') as t:
   for m in t.getmembers():
    if not m.isfile():continue
    parts=Path(m.name).parts[1:]
    if not parts or '..' in parts:raise ValueError('unsafe archive')
    p=dest.joinpath(*parts);p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(t.extractfile(m).read())
  return dict(repo=repo,commit=sha,archive_sha256=actual,path=str(dest))
 with ThreadPoolExecutor(max_workers=3) as ex:rows=list(ex.map(fetch,pinned_archives))
 for repo,files in [('airwindows/airwindows',['plugins/WinVST/Tube2/Tube2Proc.cpp','plugins/WinVST/Tube2/Tube2.cpp','LICENSE']),('averagenative/0xFX',['src/engine/internal/studio.c','LICENSE'])]:
  dest=root/repo.split('/')[1];hashes={}
  for f in files:
   p=dest/f;p.parent.mkdir(parents=True,exist_ok=True);data=urllib.request.urlopen(f'https://raw.githubusercontent.com/{repo}/{repos[repo]}/{f}',timeout=30).read();p.write_bytes(data);hashes[f]=hashlib.sha256(data).hexdigest()
  rows.append(dict(repo=repo,commit=repos[repo],path=str(dest),files_sha256=hashes))
 (root/'sources.json').write_text(json.dumps(rows,indent=2)+'\n');print(json.dumps(rows,indent=2))
if __name__=='__main__':main()
