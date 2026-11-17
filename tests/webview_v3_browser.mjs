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
  assert.equal(await page.locator('.tactile-btn').count(), 21);
  assert.equal(await page.locator('body').evaluate(el => el.classList.contains('juce-hosted')), true);

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

  // Real arpeggiator button dispatches a host parameter, not a demo-only state.
  await page.locator('.tactile-btn').nth(14).click();
  assert.equal(await page.evaluate(() => window.__nativeEvents.some(e =>
    e.name === 'prophecyParameter' && e.payload.id === 'nrpn_0160')), true);

  // Character is single-device: Spring selection changes the actual custom type.
  await page.locator('#character-toggle').click();
  await page.locator('.tactile-btn').nth(18).click();
  assert.equal(await page.evaluate(() => window.__nativeEvents.some(e =>
    e.name === 'prophecyParameter' && e.payload.id === 'character_custom_type'
      && Math.abs(e.payload.value - 0.5) < 1e-6)), true);

  assert.equal(await page.locator('.tactile-btn').nth(19).isDisabled(), true);
  assert.equal(await page.locator('#reset-btn').isDisabled(), true);

  // Glass preset navigation must reach native library, not rotate demo names.
  await page.locator('prophecy-glass-panel #next').click();
  assert.equal(await page.evaluate(() => window.__nativeEvents.some(e =>
    e.name === 'prophecyPresetNavigate' && e.payload.delta === 1)), true);
  await page.evaluate(() => window.__nativeListeners.prophecyPresetState({
    name: 'HOST PRESET TEST', index: 2, total: 50
  }));
  assert.equal(await page.locator('prophecy-glass-panel #preset-name').textContent(), 'HOST PRESET TEST');

  assert.deepEqual(errors, [], 'JavaScript runtime errors detected');
  console.log('PASS: hosted HTML load, host-to-UI state, no feedback, arp, Character, presets and unsupported controls');
} finally {
  await browser.close();
}
