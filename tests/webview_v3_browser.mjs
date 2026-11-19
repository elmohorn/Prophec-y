// Chromium interaction regression test with a mocked JUCE native event bridge.
import assert from 'node:assert/strict';
import { pathToFileURL } from 'node:url';
import { resolve } from 'node:path';
import { chromium } from 'playwright-core';

const browser = await chromium.launch({ channel: 'chrome', headless: true, args: ['--no-sandbox'] });
const page = await browser.newPage({ viewport: { width: 2100, height: 1000 } });
const errors = [];
page.on('pageerror', error => errors.push(String(error)));
await page.addInitScript(() => {
  window.__nativeEvents = [];
  window.__nativeListeners = {};
  window.__JUCE__ = {
    backend: {
      emitEvent: (name, payload) => window.__nativeEvents.push({ name, payload }),
      addEventListener: (name, callback) => { window.__nativeListeners[name] = callback; }
    }
  };
});
try {
  await page.goto(pathToFileURL(resolve('plugin/ui/web/ProphecY_v3_0.html')).href);
  await page.waitForFunction(() => window.__nativeEvents?.some(e => e.name === 'prophecyReady'), { timeout: 15000 });
  assert.equal(await page.locator('.knob-slot').count(), 39);
  assert.equal(await page.locator('.tactile-btn').count(), 24);
  assert.equal(await page.locator('body').evaluate(el => el.classList.contains('juce-hosted')), true);
  assert.equal(await page.evaluate(() => window.__nativeEvents.filter(e => e.name === 'prophecyParameter').length), 0,
    'Creating Character knobs must not write parameter defaults to the host');

  // Host changes must update UI without being echoed back as parameter edits.
  const hostResult = await page.evaluate(() => {
    window.__nativeEvents.length = 0;
    window.__nativeListeners.prophecyHostState({
      nrpn_0000: 0.8, nrpn_0002: 0.4, nrpn_0160: 0,
      character_tube_drive: 0.65, character_spring_decay: 0.72,
      character_bypass: 0, character_type: 0, character_custom_type: 0
    });
    return {
      knob: document.querySelectorAll('.knob-slot')[0].dataset.val,
      character: document.querySelectorAll('.knob-slot')[30].dataset.val,
      echoed: window.__nativeEvents.filter(e => e.name === 'prophecyParameter').length
    };
  });
  assert.equal(hostResult.knob, '80.00');
  assert.equal(hostResult.character, '65.00');
  assert.equal(hostResult.echoed, 0, 'Host state was echoed back to the DAW');
  await page.evaluate(() => {
    window.__nativeEvents.length = 0;
    window.__nativeListeners.prophecyHostState({character_neon_folds: .8, character_ferro_cutoff: .2});
  });
  assert.equal(await page.locator('.character-fx[data-character-type="5"] .character-main-knob .knob-val').first().textContent(), '80%');
  assert.equal(await page.locator('.character-fx[data-character-type="4"] .character-main-knob .knob-val').first().textContent(), '20%');
  assert.equal(await page.evaluate(() => window.__nativeEvents.filter(e => e.name === 'prophecyParameter').length), 0);
  // A preset load updates many knobs. It must NOT display the final knob
  // (typically Character MIX) as if the user had manually edited it.
  const glassModeBeforePreset = await page.locator('prophecy-glass-panel #display-mode-name').textContent();
  await page.evaluate(() => window.__nativeListeners.prophecyHostState({
    character_spring_mix: 0.35, character_tube_mix: 0.35, nrpn_0000: 0.5
  }));
  assert.equal(await page.locator('prophecy-glass-panel #display-mode-name').textContent(),
    glassModeBeforePreset, 'Preset state hydration must not trigger parameter feedback');

  // Real arpeggiator button dispatches a host parameter, not a demo-only state.
  await page.locator('.tactile-btn').nth(14).click();
  assert.equal(await page.evaluate(() => window.__nativeEvents.some(e =>
    e.name === 'prophecyParameter' && e.payload.id === 'nrpn_0160')), true);

  // Character is single-device: Spring selection changes the actual custom type.
  await page.locator('#character-toggle').click();
  await page.locator('.character-device-picker [data-character-type="3"]').click();
  assert.equal(await page.evaluate(() => window.__nativeEvents.some(e =>
    e.name === 'prophecyParameter' && e.payload.id === 'character_custom_type'
      && Math.abs(e.payload.value - 0.5) < 1e-6)), true);

  // Native Character is a one-device insert: Tube, then Spring, then bypass.
  await page.evaluate(() => { window.__nativeEvents.length = 0; });
  await page.locator('.character-device-picker [data-character-type="1"]').click();
  assert.equal(await page.evaluate(() => window.__nativeEvents.some(e =>
    e.name === 'prophecyParameter' && e.payload.id === 'character_type' && e.payload.value === 1)), true);
  assert.equal(await page.evaluate(() => window.__nativeEvents.some(e =>
    e.name === 'prophecyParameter' && e.payload.id === 'character_custom_type' && e.payload.value === 0)), true);
  await page.evaluate(() => { window.__nativeEvents.length = 0; });
  await page.locator('.character-device-picker [data-character-type="3"]').click();
  assert.equal(await page.evaluate(() => window.__nativeEvents.some(e =>
    e.name === 'prophecyParameter' && e.payload.id === 'character_custom_type' && e.payload.value === .5)), true);
  await page.evaluate(() => { window.__nativeEvents.length = 0; });
  await page.locator('#char-global-on').click();
  assert.equal(await page.evaluate(() => window.__nativeEvents.some(e =>
    e.name === 'prophecyParameter' && e.payload.id === 'character_bypass' && e.payload.value === 1)), true);
  await page.locator('.character-device-picker [data-character-type="2"]').click();
  assert.equal(await page.evaluate(() => window.__nativeEvents.some(e =>
    e.name === 'prophecyParameter' && e.payload.id === 'character_custom_type' && e.payload.value === .25)), true);
  await page.locator('.character-fx.generic[data-character-type="2"] [data-param="character_plasma_drive"]').evaluate(el=>{
    el.value='800';el.dispatchEvent(new Event('input',{bubbles:true}));
  });
  assert.equal(await page.evaluate(() => window.__nativeEvents.some(e =>
    e.name === 'prophecyParameter' && e.payload.id === 'character_plasma_drive' && e.payload.value === .8)), true);
  assert.equal(await page.locator('.character-fx.char-active').count(), 1,
    'Only one Character FX device may be displayed as active');
  await page.locator('#character-position').selectOption('1');
  assert.equal(await page.evaluate(() => window.__nativeEvents.some(e =>
    e.name === 'prophecyParameter' && e.payload.id === 'character_position' && e.payload.value === 1)), true);
  await page.locator('#character-plasma-mode').selectOption('1');
  assert.equal(await page.evaluate(() => window.__nativeEvents.some(e =>
    e.name === 'prophecyParameter' && e.payload.id === 'character_plasma_mode' && e.payload.value === 1)), true);
  await page.evaluate(() => { window.__nativeEvents.length = 0; });
  await page.locator('.character-device-picker [data-character-type="6"]').click();
  assert.equal(await page.evaluate(() => window.__nativeEvents.some(e =>
    e.name === 'prophecyParameter' && e.payload.id === 'character_rotary_enable' && e.payload.value === 1)), true);
  assert.equal(await page.locator('.character-fx.rotary').isVisible(), true);
  await page.evaluate(() => window.__nativeListeners.prophecyTelemetry({characterType:6,characterActive:true,characterInput:.1,rotaryHorn:.2,rotaryDrum:.8,characterDevice:[]}));
  assert.equal(await page.locator('.character-fx.rotary canvas.character-visual').count(), 1);
  await page.locator('[data-param="character_rotary_motion"]').evaluate(el => { el.value='700'; el.dispatchEvent(new Event('input',{bubbles:true})); });
  assert.equal(await page.evaluate(() => window.__nativeEvents.some(e =>
    e.name === 'prophecyParameter' && e.payload.id === 'character_rotary_motion' && e.payload.value === .7)), true);
  assert.equal(await page.locator('#reset-btn').isDisabled(), true);

  // Glass preset navigation must reach native library, not rotate demo names.
  await page.locator('prophecy-glass-panel #next').click();
  assert.equal(await page.evaluate(() => window.__nativeEvents.some(e =>
    e.name === 'prophecyPresetNavigate' && e.payload.delta === 1)), true);
  await page.evaluate(() => window.__nativeListeners.prophecyPresetState({
    name: 'HOST PRESET TEST', index: 2, total: 50
  }));
  assert.equal(await page.locator('prophecy-glass-panel #preset-name').textContent(), 'HOST PRESET TEST');

  // Native library must populate the approved full-height drawer and distinguish selection from load.
  assert.equal(await page.locator('#v3-preset-drawer').count(), 1);
  await page.locator('prophecy-glass-panel .capacitive[data-page="PRESETS"]').click();
  assert.equal(await page.evaluate(() => window.__nativeEvents.some(e =>
    e.name === 'prophecyPresetCatalogRequest')), true);
  await page.evaluate(() => window.__nativeListeners.prophecyPresetCatalog([
    { id: 'factory-1', name: 'REAL BRASS', category: 'Auto / Brass', bank: 'Factory', source: 'FACTORY', favorite: false, bankNumber: 0, program: 0 },
    { id: 'factory-2', name: 'REAL ORGAN', category: 'Auto / Organ', bank: 'Factory', source: 'FACTORY', favorite: true, bankNumber: 0, program: 1 },
    { id: 'user-1', name: 'USER BASS', category: 'Auto / Bass', bank: 'User', source: 'USER', favorite: false, bankNumber: 0, program: 2 }
  ]));
  assert.equal(await page.locator('#v3-preset-drawer').getAttribute('aria-hidden'), 'false');
  const glassPlane = await page.evaluate(() => {
    const chassis=document.querySelector('.synth-chassis');
    const hardware=document.querySelector('prophecy-glass-panel').shadowRoot.querySelector('.hardware');
    const drawer=document.getElementById('v3-preset-drawer');
    const shared=getComputedStyle(chassis,'::after');
    const hw=hardware.getBoundingClientRect();
    const rect=chassis.getBoundingClientRect();
    return {visible:shared.content!=='none',top:parseFloat(shared.top),left:parseFloat(shared.left),
      width:parseFloat(shared.width),right:drawer.offsetLeft+drawer.offsetWidth,
      hostTop:hw.top-rect.top,hostLeft:hw.left-rect.left,
      glassBackground:getComputedStyle(hardware.querySelector('.glass')).backgroundImage};
  });
  assert.equal(glassPlane.visible,true,'Shared glass plane must exist');
  assert.ok(Math.abs(glassPlane.top-glassPlane.hostTop-11)<1,'Shared glass must align to left inset');
  assert.ok(Math.abs(glassPlane.left-glassPlane.hostLeft-11)<1,'Shared glass must align to left inset');
  assert.ok(Math.abs(glassPlane.width-(glassPlane.right-glassPlane.hostLeft-22))<1,
    'One continuous inner frame must extend from left hardware to drawer right edge');
  assert.equal(glassPlane.glassBackground,'none','Independent glass gradient must be removed');
  assert.equal(await page.locator('#g-list .grow').count(), 3);
  await page.locator('#g-search').fill('organ');
  assert.equal(await page.locator('#g-list .grow').count(), 1);
  await page.locator('#g-search').fill('');
  await page.locator('#g-cats [data-filter="USER"]').click();
  assert.equal(await page.locator('#g-list .grow').count(), 1);
  await page.locator('#g-cats [data-filter="ALL"]').click();
  await page.evaluate(() => { window.__nativeEvents.length = 0; });
  await page.locator('#g-list .grow').nth(1).click();
  assert.equal(await page.evaluate(() => window.__nativeEvents.some(e => e.name === 'prophecyPresetLoad')), false);
  await page.locator('#g-load').click();
  assert.equal(await page.evaluate(() => window.__nativeEvents.some(e =>
    e.name === 'prophecyPresetLoad' && e.payload.id === 'factory-2')), true);
  // A single click selects but never loads; a double-click must load the
  // same persistent DOM row (previously replaced during the first click).
  await page.evaluate(() => { window.__nativeEvents.length = 0; });
  await page.locator('#g-list .grow').nth(0).click();
  assert.equal(await page.locator('#g-list .grow').nth(0).getAttribute('aria-selected'), 'true');
  assert.equal(await page.evaluate(() => window.__nativeEvents.some(e =>
    e.name === 'prophecyPresetLoad')), false);
  await page.locator('#g-list .grow').nth(0).dblclick();
  assert.equal(await page.evaluate(() => window.__nativeEvents.filter(e =>
    e.name === 'prophecyPresetLoad' && e.payload.id === 'factory-1').length), 1,
    'Double-click must load the selected preset exactly once');
  await page.locator('#g-list .grow').nth(0).locator('.fav').click();
  assert.equal(await page.evaluate(() => window.__nativeEvents.some(e =>
    e.name === 'prophecyPresetFavorite' && e.payload.id === 'factory-1' && e.payload.favorite === true)), true);
  await page.locator('#g-close').click();
  assert.equal(await page.locator('#v3-preset-drawer').getAttribute('aria-hidden'), 'true');

  // FX and LFO selectors must use verified algorithm/waveform indices.
  await page.locator('.fx-pair .oled-selector').first().click();
  assert.equal(await page.evaluate(() => window.__nativeEvents.some(e =>
    e.name === 'prophecyParameter' && e.payload.id === 'nrpn_0119' && Math.abs(e.payload.value - 0.8) < 1e-6)), true);
  await page.evaluate(() => window.__nativeListeners.prophecyHostState({
    nrpn_0119: 0.6, nrpn_0127: 6/9, nrpn_0090: 1
  }));
  assert.equal(await page.locator('.fx-pair .oled-selector').first().locator('.oled-sel-val').textContent(), 'CHORUS');
  assert.equal(await page.locator('.fx-pair .oled-selector').nth(1).locator('.oled-sel-val').textContent(), 'HALL');
  assert.equal(await page.locator('.mod-row .oled-selector').first().locator('.oled-sel-val').textContent(), 'S&H');

  // Arp selector and key tracking must drive native parameter IDs.
  await page.locator('.arp-oled-row .oled-selector').first().click();
  assert.equal(await page.evaluate(() => window.__nativeEvents.some(e =>
    e.name === 'prophecyParameter' && e.payload.id === 'nrpn_0161' && e.payload.value === 0.25)), true);
  await page.locator('[data-seg="keytrack"] .seg-opt').nth(2).click();
  assert.equal(await page.evaluate(() => window.__nativeEvents.some(e =>
    e.name === 'prophecyParameter' && e.payload.id === 'nrpn_0047' && e.payload.value === 1)), true);
  await page.evaluate(() => window.__nativeListeners.prophecyHostState({
    nrpn_0161: 0.75, nrpn_0162: 1, wrapper_clock_source: 0, nrpn_0047: 0.5
  }));
  assert.equal(await page.locator('.arp-oled-row .oled-selector').first().locator('.oled-sel-val').textContent(), 'RANDOM');
  assert.equal(await page.locator('[data-seg="keytrack"] .seg-opt').nth(1).getAttribute('class').then(s => s.includes('active')), true);

  // Six-track/64-step native editor: snapshots are canonical, not demo data.
  assert.equal(await page.evaluate(() => typeof window.__nativeListeners.prophecySequenceState === 'function'), true);
  await page.evaluate(() => window.__nativeListeners.prophecySequenceState({
    tracks:Array.from({length:6},(_,track)=>Array.from({length:64},(_,step)=>({
      note:60+track,velocity:step===0?100:0,flag:false
    })))
  }));
  assert.equal(await page.locator('#step-seq .step-btn').count(),16);
  assert.equal(await page.locator('#step-seq .step-btn').nth(8).isVisible(),true);
  assert.equal(await page.locator('#step-seq .step-btn').first().locator('.step-note').textContent(),'C4');
  await page.locator('#step-seq .step-btn').first().click({button:'right'});
  assert.equal(await page.evaluate(() => window.__nativeEvents.some(e =>
    e.name==='prophecySequenceEdit' && e.payload.track===0 && e.payload.step===0 &&
    e.payload.note===60 && e.payload.velocity===0)),true);
  await page.locator('#step-seq .step-btn').nth(1).click({button:'right'});
  assert.equal(await page.evaluate(() => window.__nativeEvents.some(e =>
    e.name==='prophecySequenceEdit' && e.payload.step===1 && e.payload.velocity===96)),true);
  // Explicit value avoids Playwright matching option label "1" (track 1, value 0).
  await page.locator('#native-seq-track').selectOption({value:'1'});
  assert.equal(await page.locator('#step-seq .step-btn').first().locator('.step-note').textContent(),'C#4');
  await page.locator('.seq-page').nth(1).click();
  assert.equal(await page.locator('#step-seq .step-btn').first().locator('.step-index').textContent(),'17');
  await page.locator('#native-seq-play').click();
  assert.equal(await page.evaluate(() => window.__nativeEvents.some(e =>
    e.name==='prophecyParameter' && e.payload.id==='runtime_sequence_play' && e.payload.value===1)),true);
  await page.locator('#native-seq-length').fill('32');
  await page.locator('#native-seq-length').dispatchEvent('change');
  assert.equal(await page.evaluate(() => window.__nativeEvents.some(e =>
    e.name==='prophecyParameter' && e.payload.id==='nrpn_0169' &&
    Math.abs(e.payload.value-31/63)<1e-6)),true);
  await page.evaluate(() => window.__nativeListeners.prophecyHostState({
    runtime_sequence_play:0,nrpn_0169:31/63,arp_step_1:1
  }));
  assert.equal(await page.locator('#native-seq-play').textContent(),'PLAY');
  assert.equal(await page.locator('#native-seq-length').inputValue(),'32');
  await page.locator('#arp-advanced-btn').click();
  assert.equal(await page.locator('.seq-arp-character-gates button').count(),8);
  await page.locator('.seq-arp-character-gates button').first().click();
  assert.equal(await page.evaluate(() => window.__nativeEvents.some(e =>
    e.name==='prophecyParameter' && e.payload.id==='arp_step_1')),true);
  await page.evaluate(() => window.__nativeListeners.prophecyTelemetry({
    leftPeak:0.7,rightPeak:0.3,leftRms:0.4,rightRms:0.2,clips:0,
    playStep:2,sequenceStep:17
  }));
  assert.equal(await page.locator('prophecy-glass-panel #db-l').textContent(),'-3.1');
  assert.equal(await page.locator('#step-seq .step-btn').nth(1).getAttribute('class').then(s=>s.includes('playhead')),true);
  // Parameter feedback overlays the live meters briefly; verify the automatic return.
  await page.waitForFunction(() => document.querySelector('prophecy-glass-panel')?.shadowRoot
    ?.getElementById('display-mode-name')?.textContent === 'STEREO · PEAK METERS', { timeout: 4000 });

  assert.deepEqual(errors, [], 'JavaScript runtime errors detected');
  console.log('PASS: hosted HTML, 6x64 sequence editing, Character single-device, presets and native bridge');
} finally {
  await browser.close();
}
