import { chromium } from 'playwright';
import { writeFile } from 'node:fs/promises';

const url = process.argv[2] ?? 'http://127.0.0.1:18739/web_sprite_tests.html';
const browser = await chromium.launch({ channel: process.env.WEB_TEST_BROWSER ?? 'chrome', headless: true,
  args: process.env.WEB_TEST_SOFTWARE_GL ? ['--use-angle=swiftshader', '--enable-unsafe-swiftshader'] : [],
});
const page = await browser.newPage();
await page.addInitScript(() => {
  const original = HTMLCanvasElement.prototype.getContext;
  HTMLCanvasElement.prototype.getContext = function(type, ...args) {
    const context = original.call(this, type, ...args);
    if (type === 'webgl2' && context) {
      const debug = context.getExtension('WEBGL_debug_renderer_info');
      if (debug) console.log(`WebGL device: ${context.getParameter(debug.UNMASKED_RENDERER_WEBGL)}`);
    }
    return context;
  };
});
const lines = [];
let finish;
const completed = new Promise(resolve => { finish = resolve; });
page.on('console', message => {
  const line = message.text();
  lines.push(line);
  const summary = line.match(/(\d+) test\(s\), (\d+) check\(s\), (\d+) failed/);
  if (summary) finish(Number(summary[3]));
});
page.on('pageerror', error => {
  lines.push(`PAGE ERROR: ${error.message}`);
  finish(1);
});
let timer;
try {
  await page.goto(url, { waitUntil: 'domcontentloaded' });
  const result = await Promise.race([
    completed,
    new Promise(resolve => { timer = setTimeout(() => { lines.push('Renderer tests timed out'); resolve(1); }, 900000); }),
  ]);
  console.log([...new Set(lines.filter(line => /device:|shaders|\[FAIL\]|test\(s\)|PAGE ERROR|timed out/.test(line)))].join('\n'));
  await writeFile(process.env.WEB_TEST_LOG ?? 'web-renderer.log', lines.join('\n') + '\n');
  process.exitCode = result ? 1 : 0;
} finally {
  clearTimeout(timer);
  await browser.close();
}
