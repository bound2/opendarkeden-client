import assert from 'node:assert/strict';
import { chromium } from 'playwright';

const origin = process.argv[2] ?? 'http://127.0.0.1:18739';
const endpoints = process.argv.slice(3);
if (!endpoints.length) endpoints.push('ws://127.0.0.1:18740/game', 'ws://127.0.0.1:18741/game');
const browser = await chromium.launch({ channel: process.env.WEB_TEST_BROWSER ?? 'chrome', headless: true });
try {
  for (const endpoint of endpoints) {
    const page = await browser.newPage();
    page.on('console', message => console.log(message.text()));
    await page.goto(origin, { waitUntil: 'domcontentloaded' });
    let watchdog;
    try {
      const result = await Promise.race([
        page.evaluate(async endpoint => {
          const { default: createProbe } = await import('./transport_tests.mjs');
          return new Promise((resolve, reject) => {
            createProbe({ arguments: [endpoint], print: console.log, printErr: console.error,
              onExit: resolve,
              onAbort: reason => reject(new Error(String(reason))),
            }).catch(reject);
          });
        }, endpoint),
        // Keep the watchdog outside the page: a blocked WASM main thread
        // cannot run its own JavaScript timeout.
        new Promise((_, reject) => { watchdog = setTimeout(() => reject(new Error('Transport probe did not exit')), 35000); }),
      ]);
      assert.equal(result, 0, `Transport probe failed: ${endpoint}`);
    } finally { clearTimeout(watchdog); await page.close(); }
  }
} finally { await browser.close(); }
