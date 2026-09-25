import createDarkEden from './DarkEden.mjs';
import { installTouchControls } from './touch-controls.mjs';

const canvas = document.querySelector('#canvas');
const status = document.querySelector('#status');
const progress = document.querySelector('#progress');
const play = document.querySelector('#play');
const fullscreen = document.querySelector('#fullscreen');
const shell = document.querySelector('#game-shell');
let client;
let touchControls;
function resizeCanvas() {
  if (!client || canvas.hidden) return;
  const rect = canvas.getBoundingClientRect();
  const scale = Math.min(devicePixelRatio || 1, 4096 / Math.max(1, rect.width), 4096 / Math.max(1, rect.height));
  client._darkeden_resize_canvas(Math.round(rect.width * scale), Math.round(rect.height * scale));
}
new ResizeObserver(resizeCanvas).observe(canvas);
window.addEventListener('resize', resizeCanvas);

canvas.addEventListener('contextmenu', event => event.preventDefault());
canvas.addEventListener('webglcontextlost', event => {
  event.preventDefault();
  status.textContent = 'The graphics device was disconnected. Reload the page to reconnect.';
});
fullscreen.addEventListener('click', async () => {
  if (document.fullscreenElement) await document.exitFullscreen();
  else if (shell.classList.contains('expanded')) shell.classList.remove('expanded');
  else {
    try { await shell.requestFullscreen(); }
    catch { shell.classList.add('expanded'); }
  }
  fullscreen.textContent = document.fullscreenElement || shell.classList.contains('expanded') ? 'Exit fullscreen' : 'Fullscreen';
  canvas.focus({ preventScroll: true });
});
document.addEventListener('fullscreenchange', () => {
  fullscreen.textContent = document.fullscreenElement ? 'Exit fullscreen' : 'Fullscreen';
});

function syncSettings(populate) {
  return new Promise((resolve, reject) => client.FS.syncfs(populate, error => error ? reject(error) : resolve()));
}

async function loadAssets() {
  const response = await fetch('./assets/manifest.json');
  if (!response.ok) throw new Error('The game data pack is unavailable.');
  const manifest = await response.json();
  if (manifest.version !== 1 || !Array.isArray(manifest.files) || manifest.files.length === 0)
    throw new Error('The game data manifest is invalid.');
  for (const file of manifest.files) {
    if (!/^Data\//.test(file.path) || file.path.split('/').some(part => !part || part === '.' || part === '..') ||
        file.path.includes('\\') || !Number.isSafeInteger(file.size) || file.size < 0 || !/^[a-f0-9]{64}$/.test(file.sha256))
      throw new Error('The game data manifest contains an invalid entry.');
  }
  progress.max = manifest.files.reduce((total, file) => total + file.size, 0);
  progress.value = 0;
  progress.hidden = false;
  let cache;
  try { cache = await caches.open('darkeden-assets-v1'); } catch { /* Storage can be disabled. */ }
  let next = 0;
  await Promise.all(Array.from({ length: 3 }, async () => {
    while (next < manifest.files.length) {
      const file = manifest.files[next++];
      const url = new URL(`./assets/${file.path.split('/').map(encodeURIComponent).join('/')}?sha256=${file.sha256}`, import.meta.url);
      let data;
      try { data = await cache?.match(url); } catch { /* A failed cache read falls back to HTTP. */ }
      const cached = !!data;
      data ??= await fetch(url);
      if (!data.ok) throw new Error(`Cannot load ${file.path}.`);
      const buffer = await data.arrayBuffer();
      if (buffer.byteLength !== file.size) throw new Error(`Incomplete game data: ${file.path}.`);
      const digest = [...new Uint8Array(await crypto.subtle.digest('SHA-256', buffer))]
        .map(byte => byte.toString(16).padStart(2, '0')).join('');
      if (digest !== file.sha256) {
        await cache?.delete(url);
        throw new Error(`Damaged game data: ${file.path}. Reload to download it again.`);
      }
      if (!cached && cache) {
        try { await cache.put(url, new Response(buffer)); } catch { /* Quota failure must not prevent play. */ }
      }
      const path = `/${file.path}`;
      client.FS.mkdirTree(path.slice(0, path.lastIndexOf('/')));
      client.FS.createDataFile('/', file.path, new Uint8Array(buffer), true, false, true);
      progress.value += file.size;
      status.textContent = `Loading game data… ${Math.floor(progress.value / progress.max * 100)}%`;
    }
  }));
  progress.hidden = true;
}

play.addEventListener('click', async () => {
  play.disabled = true;
  try {
    if (!client) {
      status.textContent = 'Loading game…';
      const response = await fetch('./client-config.json', { cache: 'no-store' });
      if (!response.ok) throw new Error('The server configuration is unavailable.');
      const config = await response.json();
      const gateway = new URL(config.websocketUrl, location.href);
      if (gateway.protocol === 'https:') gateway.protocol = 'wss:';
      if (gateway.protocol === 'http:') gateway.protocol = 'ws:';
      if (!['ws:', 'wss:'].includes(gateway.protocol) || gateway.search || gateway.hash ||
          gateway.username || gateway.password ||
          typeof config.loginHost !== 'string' || !/^[a-zA-Z0-9.-]{1,253}$/.test(config.loginHost) ||
          !Number.isInteger(config.loginPort) || config.loginPort < 1 || config.loginPort > 65535)
        throw new Error('The server configuration is invalid.');
      client = await createDarkEden({
        canvas, noInitialRun: true,
        preRun: [module => {
          module.ENV.DARKEDEN_WEBSOCKET_URL = gateway.href;
          module.ENV.DARKEDEN_LOGIN_HOST = config.loginHost;
          module.ENV.DARKEDEN_LOGIN_PORT = String(config.loginPort);
        }],
        print: line => console.log(line), printErr: line => console.warn(line),
        onAbort: () => { touchControls?.stop(); status.textContent = 'The game stopped unexpectedly. Reload the page to try again.'; },
        onExit: async () => {
          touchControls?.stop();
          try {
            await syncSettings(false);
            status.textContent = 'The game has closed. Reload the page to play again.';
          } catch {
            status.textContent = 'The game has closed, but browser storage could not save your settings.';
          }
        },
      });
      client.FS.mkdirTree('/UserSet');
      client.FS.mount(client.IDBFS, { autoPersist: true }, '/UserSet');
      try { await syncSettings(true); }
      catch { status.textContent = 'Browser storage is unavailable; settings will last for this session.'; }
      client.FS.mkdirTree('/Log');
      await loadAssets();
      // A fresh click starts audio and focuses input after the data download.
      play.textContent = 'Play';
      status.textContent = 'Ready to play.';
      play.disabled = false;
      return;
    }
    canvas.hidden = false;
    shell.hidden = false;
    document.body.classList.add('playing');
    play.hidden = true;
    status.textContent = '';
    canvas.focus();
    touchControls = installTouchControls(client, canvas, shell);
    client.callMain([]);
  } catch (error) {
    console.error(error);
    status.textContent = `${error.message} Reload the page to try again.`;
  }
});
