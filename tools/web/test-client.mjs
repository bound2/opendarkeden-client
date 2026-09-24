import { chromium } from 'playwright';
import assert from 'node:assert/strict';
import { writeFile, mkdir } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';

const url = process.argv[2] ?? 'http://127.0.0.1:18739/';
const output = new URL('../../build/web-smoke/', import.meta.url);
await mkdir(output, { recursive: true });
const browser = await chromium.launchPersistentContext(fileURLToPath(new URL('profile/', output)), {
  channel: process.env.WEB_TEST_BROWSER ?? 'chrome', headless: true,
  viewport: { width: 1280, height: Number(process.env.WEB_TEST_HEIGHT ?? 900) },
  deviceScaleFactor: Number(process.env.WEB_TEST_DPR ?? 1),
});
const page = await browser.newPage();
await page.route('**/launcher.mjs', async route => {
    const response = await route.fetch();
    let body = (await response.text()).replace("client.FS.mkdirTree('/UserSet');",
      "window.testClient = client; client.FS.mkdirTree('/UserSet');");
    if (process.env.WEB_TEST_SPRITE_RENDERER === 'cpu') body = body.replace('preRun: [module => {',
        'preRun: [module => { module.ENV.DARKEDEN_SPRITE_RENDERER = "software";');
    await route.fulfill({ response, body });
});
const devtools = await browser.newCDPSession(page);
await devtools.send('Network.clearBrowserCache');
await devtools.send('Network.setCacheDisabled', { cacheDisabled: true });
const lines = [];
const errors = [];
page.on('console', message => lines.push(message.text()));
page.on('pageerror', error => { errors.push(error.message); lines.push(`PAGE ERROR: ${error.message}`); console.error(error.message); });
await page.addInitScript(() => {
  window.browserFrames = 0;
  const request = window.requestAnimationFrame.bind(window);
  window.requestAnimationFrame = callback => request(time => { ++window.browserFrames; callback(time); });
});
try {
  await page.goto(url, { waitUntil: 'domcontentloaded' });
  await page.locator('#play').click();
  await page.waitForFunction(() => document.querySelector('#play').textContent === 'Play' ||
    document.querySelector('#status').textContent.includes('Reload'), null, { timeout: 300000 });
  const status = await page.locator('#status').textContent();
  if (status !== 'Ready to play.') throw new Error(status);
  await page.evaluate(() => { window.browserFrames = 0; });
  await page.locator('#play').click();
  await page.waitForFunction(() => window.browserFrames >= 120, null, { timeout: 120000 });
  // The original title overlay fades for two seconds before exposing the menu.
  await page.waitForTimeout(3000);
  const checkSize = async () => {
    const size = await page.evaluate(() => {
    const canvas = document.querySelector('canvas');
    const rect = canvas.getBoundingClientRect();
    const scale = Math.min(devicePixelRatio || 1, 4096 / rect.width, 4096 / rect.height);
    return { width: canvas.width, height: canvas.height,
      expectedWidth: Math.round(rect.width * scale), expectedHeight: Math.round(rect.height * scale) };
    });
    assert.equal(size.width, size.expectedWidth, `Canvas width: ${JSON.stringify(size)}`);
    assert.equal(size.height, size.expectedHeight, `Canvas height: ${JSON.stringify(size)}`);
  };
  await checkSize();
  if (await page.locator('#status').textContent()) throw new Error(await page.locator('#status').textContent());
  if (lines.some(line => /Rejected pack index|file open failed|RuntimeError|TypeError|Aborted\(/i.test(line)))
    throw new Error('The game reported a resource or runtime failure.');
  await page.screenshot({ path: fileURLToPath(new URL('client.png', output)) });
  const clickGame = async (x, y) => {
    const rect = await page.locator('canvas').boundingBox();
    const scale = Math.min(rect.width / 800, rect.height / 600);
    await page.mouse.click(rect.x + (rect.width - 800 * scale) / 2 + x * scale,
      rect.y + (rect.height - 600 * scale) / 2 + y * scale, { delay: 100 });
    await page.waitForTimeout(400);
  };
  await clickGame(714, 380);
  const login = await page.screenshot({ path: fileURLToPath(new URL('login.png', output)) });
  await page.keyboard.type('browserprobe', { delay: 60 });
  await page.waitForTimeout(400);
  const typed = await page.screenshot({ path: fileURLToPath(new URL('login-text.png', output)) });
  assert(!login.equals(typed), 'Typing must update the login field');
  await clickGame(452, 188);
  await page.locator('#fullscreen').click();
  await page.waitForFunction(() => document.fullscreenElement === document.querySelector('canvas'));
  await page.waitForTimeout(400);
  await checkSize();
  await page.screenshot({ path: fileURLToPath(new URL('fullscreen.png', output)) });
  await page.evaluate(() => document.exitFullscreen());
  await page.setViewportSize({ width: 1000, height: 780 });
  await page.waitForTimeout(400);
  await checkSize();
  await clickGame(714, 524);
  await page.waitForFunction(() => document.querySelector('#status').textContent.startsWith('The game has closed.'));
  await page.screenshot({ path: fileURLToPath(new URL('exit.png', output)) });
  const settings = await page.evaluate(() => Array.from(window.testClient.FS.readFile('/UserSet/UserOption.set')));
  assert(settings.length > 0, 'Normal shutdown must save game settings');
  await page.reload({ waitUntil: 'domcontentloaded' });
  await page.locator('#play').click();
  await page.waitForFunction(() => document.querySelector('#play').textContent === 'Play', null, { timeout: 300000 });
  const restored = await page.evaluate(() => Array.from(window.testClient.FS.readFile('/UserSet/UserOption.set')));
  assert.deepEqual(restored, settings, 'Settings must survive a page reload');
  if (errors.length) throw new Error(errors.join('\n'));
  console.log('Client passed startup, typing, resize, fullscreen, shutdown and settings reload checks.');
  console.log(lines.filter(line => /renderer|effects:|xBRZ:|ERROR|Error|error/.test(line)).slice(-20).join('\n'));
} catch (error) {
  console.error(error);
  console.error(lines.slice(-25).join('\n'));
  process.exitCode = 1;
} finally {
  await writeFile(new URL('client.log', output), lines.join('\n') + '\n');
  await browser.close();
}
