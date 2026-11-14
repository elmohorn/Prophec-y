from PIL import Image
from pathlib import Path
import json,base64
D=Path(__file__).parent
src=D/'photo_assets'
meta=json.loads((src/'metadata.json').read_text())
OUT=D/'atlases'
OUT.mkdir(exist_ok=True)

def pack(files,name):
    width=1400
    # largest first, shelf bin packing (simple, reproducible)
    items=sorted([(rel,Image.open(src/rel).convert('RGBA')) for rel in files],key=lambda p:p[1].height,reverse=True)
    x=y=2;rh=0;records={}
    for rel,im in items:
        if x+im.width+3>width:x=2;y+=rh+3;rh=0
        records[rel]=[x,y,im.width,im.height]
        x+=im.width+3;rh=max(rh,im.height)
    height=y+rh+2
    a=Image.new('RGBA',(width,height),(0,0,0,0))
    for rel,im in items:
        xx,yy,_,_=records[rel];a.alpha_composite(im,(xx,yy))
    a.save(OUT/name,optimize=True,compress_level=9)
    print(name,a.size,(OUT/name).stat().st_size)
    return records

dialfiles=[f'dial/{name}{suffix}.png' for name in meta['dials'] for suffix in ('','-ring','-cap')]
modfiles=[f'module/{name}.png' for name in meta['modules']]
a=pack(dialfiles,'dials_atlas.png');b=pack(modfiles,'modules_atlas.png')
R={'schema':1,'sources':meta['source_files'],'dial_atlas':'dials_atlas.png','module_atlas':'modules_atlas.png','dials':{},'modules':{}}
for name,data in meta['dials'].items():
    R['dials'][name]={**{x:data[x] for x in ('nominalDiameter','cropRadius','capRadius','source')},'full':a[data['dial']],'ring':a[data['ring']],'cap':a[data['cap']]}
for name,data in meta['modules'].items():
    R['modules'][name]={**{x:data[x] for x in ('size','source')},'rect':b[data['image']]}
(OUT/'atlas_manifest.json').write_text(json.dumps(R,ensure_ascii=False,indent=2),encoding='utf8')
# Pack bytes into a textual transfer bridge; actual binary assets remain PNG.
bundle={'dials_atlas':base64.b64encode((OUT/'dials_atlas.png').read_bytes()).decode(),
        'modules_atlas':base64.b64encode((OUT/'modules_atlas.png').read_bytes()).decode()}
t=D/'ProphecY_GH_Atlas_Transfer.html'
t.write_text('<!doctype html><script id="atlas" type="application/json">'+json.dumps(bundle,separators=(',',':'))+'</script>',encoding='utf8')
print('transfer',t,'bytes',t.stat().st_size,'manifest',len(R['dials']),len(R['modules']))
