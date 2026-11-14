#!/usr/bin/env python3
"""Extract source-faithful controls from the user's APPROVED FLATTENED concept boards.

Source assets stay unchanged. Nothing is fabricated by drawing a new style.
The output is a transient high-DPI atlas intended for review and for eventual
JUCE sprite-backed components. Dial rings are stationary while caps can rotate.
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter
import json, numpy as np

BASE=Path(__file__).parent
OUT=BASE/'photo_assets'
SRC=[BASE/'a_detailed_high_quality_ui_design_spec_sheet_dar.png',BASE/'a_highly_detailed_photoreal_ui_style_component_l.png',BASE/'Biblioteca UI Prophec‑Y v2.1 Complementarios.png']
S=[Image.open(p).convert('RGB') for p in SRC]

# Raster positions are measured from the 1536x1024 concept sheet, NOT the
# nominal logical pixel sizes. Users can tune crop geometry without redrawing.
DIALS={
 'xl-hero':(1,96,152,70,59,64),
 'l-standard':(1,260,151,60,47,48),
 'm-compact':(1,387,153,47,35,36),
 's-trim':(1,485,152,31,23,24),
 'l-brushed-cap':(1,585,150,44,37,48),
 'l-dark-cap':(1,685,152,41,36,48),
 'm-knurled-cap':(1,766,152,42,35,36),
 'm-scalloped-cap':(1,835,151,39,37,36),
 'encoder-led':(1,946,151,48,40,48),
 'macro-morph':(1,1047,152,55,45,64),
 'rotary-selector':(1,1192,151,61,54,48),
 'micro':(1,1398,142,24,19,18),
 'nano':(1,1475,143,23,19,14),
 'l-pointer-cap':(2,531,145,61,49,48),
 'detented-encoder':(2,928,145,57,47,36),
}
# For rectangular texture modules, filenames and source box exactly match the
# approved sheet; tight alpha feather hides boundaries, with no fabricated UI.
RECTS={
 'fader-vertical-long':(1,(36,310,99,449)),
 'fader-vertical-compact':(1,(117,322,160,435)),
 'fader-vertical-mini':(1,(192,340,234,427)),
 'slider-horizontal':(1,(279,334,449,372)),
 'slider-horizontal-compact':(1,(283,386,401,419)),
 'slider-horizontal-trim':(1,(280,432,396,454)),
 'switch-low-profile-on':(1,(510,334,600,381)),
 'toggle-mini':(1,(624,326,682,400)),
 'switch-3-position':(1,(697,329,779,380)),
 'toggle-pill':(1,(786,336,859,385)),
 'button-square-on':(1,(868,329,944,397)),
 'button-round-on':(1,(1033,329,1087,392)),
 'button-utility':(1,(1091,330,1148,390)),
 'button-icon':(1,(1160,329,1209,389)),
 'step-sequencer':(1,(1239,329,1521,391)),
 'capacitive-preset':(1,(28,532,275,594)),
 'dropdown-preview':(1,(315,542,627,603)),
 'oled-preset-strip':(1,(676,535,1064,594)),
 'vu-analog':(1,(1102,525,1282,611)),
 'meter-dual-led':(1,(1288,525,1381,614)),
 'scope-spectrum':(1,(28,674,194,742)),
 'scope-oscilloscope':(1,(203,674,373,742)),
 'scope-morph':(1,(379,674,555,742)),
 'xy-performance':(1,(604,669,823,755)),
 'adsr-envelope':(1,(872,674,1175,745)),
 'lfo-wave-selector':(1,(1214,674,1522,742)),
 'modulation-matrix':(1,(20,819,595,863)),
 'navigation-tabs':(1,(609,826,953,868)),
 'progress-red':(1,(1303,828,1521,860)),
 'rocker-2-pos':(2,(60,474,225,539)),
 'toggle-3-pos':(2,(475,455,543,544)),
 'vu-amber-compact':(2,(857,457,1076,545)),
 'status-oled-mini':(2,(1268,473,1485,535)),
 'led-bar-vertical':(2,(95,626,126,734)),
 'led-bar-horizontal':(2,(490,628,761,677)),
 'value-chip':(2,(873,642,1078,700)),
 'module-header':(2,(1257,646,1508,697)),
 'icon-button-set':(2,(44,807,325,863)),
 'status-lamps':(2,(719,813,893,864)),
}

(OUT/'dial').mkdir(parents=True,exist_ok=True)
(OUT/'module').mkdir(parents=True,exist_ok=True)
metadata={'source_files':[p.name for p in SRC], 'design_sources_are_flattened':True,'dials':{},'modules':{}}

def radial_mask(n,capRadius,outerRadius,width=1.8):
    yy,xx=np.ogrid[:n,:n]
    cx=cy=(n-1)/2
    rr=np.sqrt((xx-cx)**2+(yy-cy)**2)
    return (np.clip((outerRadius+.5-rr)/width,0,1)*255).astype(np.uint8)

for key,(src,cx,cy,ro,rc,nom) in DIALS.items():
    img=S[src]
    crop=img.crop((cx-ro,cy-ro,cx+ro,cy+ro)).convert('RGBA')
    w,h=crop.size
    yy,xx=np.ogrid[:h,:w]
    rr=np.sqrt((xx-(w-1)/2)**2+(yy-(h-1)/2)**2)
    # Full panel-exact crop, without source labels or bordering panel bevel.
    a=np.clip((ro-1-rr)/2,0,1)
    crop.putalpha(Image.fromarray(np.uint8(a*255),'L'))
    crop.save(OUT/'dial'/f'{key}.png',optimize=True)
    # Photographed moving cap is a separate layer; original ring stays fixed.
    cap=crop.copy(); alpha=np.clip((rc+.5-rr)/1.4,0,1)*a
    cap.putalpha(Image.fromarray(np.uint8(alpha*255),'L'))
    cap.save(OUT/'dial'/f'{key}-cap.png',optimize=True)
    ring=crop.copy(); ringalpha=np.clip((rr-(rc-.9))/1.5,0,1)*a
    ring.putalpha(Image.fromarray(np.uint8(ringalpha*255),'L'))
    ring.save(OUT/'dial'/f'{key}-ring.png',optimize=True)
    metadata['dials'][key]={'source':src,'center':[cx,cy], 'cropRadius':ro,'capRadius':rc,'nominalDiameter':nom,'dial':f'dial/{key}.png','cap':f'dial/{key}-cap.png','ring':f'dial/{key}-ring.png'}

for key,(src,rect) in RECTS.items():
    im=S[src].crop(rect).convert('RGBA')
    w,h=im.size;xy=np.mgrid[:h,:w]
    dist=np.minimum.reduce([xy[0],h-1-xy[0],xy[1],w-1-xy[1]])
    a=np.clip(dist/5.0,0,1)
    im.putalpha(Image.fromarray(np.uint8(255*a),'L'))
    im.save(OUT/'module'/f'{key}.png',optimize=True)
    metadata['modules'][key]={'source':src,'rect':list(rect),'size':[w,h], 'image':f'module/{key}.png'}

(OUT/'metadata.json').write_text(json.dumps(metadata,indent=2,ensure_ascii=False),encoding='utf-8')
print('Exported',len(DIALS),'dial types +',len(RECTS),'mechanical/display modules =',len(DIALS)+len(RECTS),'source-derived components')
