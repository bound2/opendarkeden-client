# Browser client and WebSocket transport

The C++ client builds to WebAssembly with SDL2 and WebGL 2. Sprite composition,
palette effects, lighting and xBRZ run on the GPU. The browser owns the frame
loop; assets live in the virtual filesystem and user settings persist in IndexedDB.
The browser sends the existing binary protocol through the server's WebSocket
gateway. Native clients can opt into the same gateway or continue using TCP.

## Build

Docker is sufficient on Windows; the pinned Emscripten 6.0.10 image builds on a
Linux volume, then copies the deployable files to `build/web`:

```powershell
./tools/web/build.ps1
```

The script accepts `-Jobs` and `-Volume`. On Linux, use the equivalent command:

```sh
docker run --rm --mount type=bind,source="$PWD",target=/src,readonly \
  --mount type=volume,source=darkeden-webgl-work,target=/work \
  emscripten/emsdk:6.0.10@sha256:e077d54e2b8970575ebc4f185ac1de0b95c05f2b266134d4ba27449af7aebf65 \
  bash /src/tools/web/build-container.sh
mkdir -p build/web
docker run --rm --mount type=volume,source=darkeden-webgl-work,target=/work,readonly \
  --mount type=bind,source="$PWD/build/web",target=/out \
  alpine:3.22 cp -r /work/build-wasm/bin/. /out/
```

With the same SDK activated locally, configure with `emcmake cmake -S . -B build/wasm
-DCMAKE_BUILD_TYPE=Release -DBUILD_ENGINE=OFF -DBUILD_TESTS=OFF`, then build
`DarkEden`, `web_sprite_tests` and `transport_tests` with one `cmake --build
build/wasm --target <name>` each: several targets in one parallel Makefile build
race on the iconv external project's configure step in a fresh tree.
CMake downloads the pinned SDL ports and GNU iconv 1.18.

## Assets and server configuration

Game assets are distributed separately. Prepare an asset ZIP with `Data/` at
its root, and Noto Sans CJK Regular's `NotoSansCJK-Regular.ttc` with its
`LICENSE.txt` beside it. Then run:

```sh
python tools/web/package-assets.py path/to/assets.zip build/web/assets \
  --font path/to/NotoSansCJK-Regular.ttc --overlay tools/i18n/ui-text
```

The packer rejects traversal, symlinks and duplicate case-insensitive paths.
It excludes `UserSet` and writes a SHA-256 manifest. `--overlay` copies a
directory of loose files (with `Data/` at its root) over the archive's
contents; `tools/i18n/ui-text` is the English text of the packed item, skill,
help, book and tutorial resources, which the client prefers over the Korean
members of the `.rpk` archives (see `tools/i18n/README.md`). The launcher verifies every
file and caches downloads by hash. The tested pack has 2,022 files and is about
1.85 GB, including the font. This first version loads the complete pack before
play, so allow several GB of browser memory and disk space. Asset streaming and
mobile/touch controls are future work.

Edit `build/web/client-config.json` for the deployment:

```json
{
  "websocketUrl": "/game",
  "loginHost": "127.0.0.1",
  "loginPort": 9999
}
```

`loginHost` and `loginPort` identify the original protocol endpoint. They must
match a gateway route; advertised game endpoints from login/world handoffs must
also be configured there. `websocketUrl` may be relative or an absolute
`ws://`/`wss://` URL, without a query string, fragment or credentials. HTTPS pages
require WSS. The gateway carries the verified client IPv4 address to dedicated
loopback listeners, preserving the server's IP-keyed login/world handoffs.

Follow the server's [gateway deployment guide](https://github.com/bound2/opendarkeden-server/blob/master/docs/websocket.md)
for listener ports, allowed browser origins, TLS, reverse-proxy trust and routes.
Do not point the gateway at the ordinary TCP listeners: they do not consume its
PROXY header. Raw peer listeners and optional UDP checks are disabled in
WebSocket mode; party positions use the existing server relay.

For a local static server:

```sh
python -m http.server 18739 --bind 127.0.0.1 --directory build/web
```

Open `http://127.0.0.1:18739`, choose **Load game**, then **Play** after the download.
The second click enables audio. Use the launcher button for fullscreen. A normal
in-game exit flushes settings to IndexedDB; disabled browser storage permits
session-only settings. Deploy over HTTPS and serve `.wasm` as `application/wasm`.
Publish each JS/WASM bundle atomically, and revalidate launcher/module files to
avoid mixing versions. The asset cache uses separate content hashes. No
cross-origin isolation headers or browser plugins are required.

## Container

`Dockerfile` builds the client in the pinned Emscripten image and serves it
with nginx 1.27, which also proxies `/game` to the WebSocket gateway so the
page and its socket share one origin (the browser then sends the page's origin,
and the gateway's `origins` must list it). The image carries no assets: mount
the pack produced by `package-assets.py` at `/usr/share/nginx/html/assets`.
`client-config.json` is rendered at container start from `DARKEDEN_WEBSOCKET_URL`
(default `/game`), `DARKEDEN_LOGIN_HOST` and `DARKEDEN_LOGIN_PORT`;
`GATEWAY_UPSTREAM` (default `odk-server:8080`) is where nginx forwards `/game`.

`docker/docker-compose.yml` runs it as `odk-web` next to the server repository's
stack: it joins that stack's network (`ODK_NETWORK`, default `docker_odk-network`),
mounts `DARKEDEN_ASSETS` (default `../build/web/assets`) and publishes
`127.0.0.1:18739`. On a VPS, keep it behind a TLS-terminating reverse proxy, set
`DARKEDEN_LOGIN_HOST`/`PORT` to the advertised login endpoint, and list this
container's address in the gateway's `trustedProxies` so players keep their own
IPv4 identity instead of the proxy's.

```sh
cd docker
docker compose up -d --build
```

## Native WebSockets

Native transport remains TCP by default. To use a gateway:

```powershell
$env:DARKEDEN_WEBSOCKET_URL = 'wss://game.example.com/game'
$env:DARKEDEN_LOGIN_HOST = '127.0.0.1'
$env:DARKEDEN_LOGIN_PORT = '9999'
```

Start the native client as usual. The gateway must enable `allowNative`. Remove
`DARKEDEN_WEBSOCKET_URL` to return to TCP. Native WSS verifies TLS certificates;
Windows/Linux use OpenSSL and macOS uses Secure Transport. CMake fetches a pinned
IXWebSocket commit (v12.0.1). Windows manifest dependencies include OpenSSL;
existing classic vcpkg installations need `openssl:x64-windows`. Linux needs
`libssl-dev` in addition to the usual SDL dependencies.

## Verification

The login/menu cursor renders between logic ticks, with the same 15 ms fallback
draw interval as gameplay. The intermediate game framebuffer is marked transient:
it is redrawn after a device reset instead of synchronously read back after every
presentation. Persistent terrain and minimap surfaces still receive reset
checkpoints. Sprite and glyph batches reuse unchanged render state, and WebGL
effect shaders cache uniform locations and avoid per-draw error queries.

The browser mixer uses 4,096 samples instead of the native 1,024.
[SDL's browser audio callback](https://github.com/libsdl-org/SDL/blob/SDL2/src/audio/emscripten/SDL_emscriptenaudio.c)
runs on the main thread, so this gives busy frames roughly
85–93 ms of buffering at common device rates, at the cost of additional audio
latency. It does not isolate audio from arbitrarily long main-thread stalls.
Black translucent UI panels use the GPU gamma path on every platform.

Portal and safe-zone overlays load their `.mip` metadata through the same path
normalizer as other assets; raw Windows backslashes cannot be opened in the
browser filesystem.

On 2026-09-25, native Release unit/UI tests and the renderer oracle passed. The
29 renderer tests passed 3,175,964 checks both in native OpenGL and Chrome WebGL 2
(ANGLE/D3D11, RTX 5080). The repository source checks also passed.
One comparison of the 120-frame mixed-scene benchmark against the prior renderer
measured 32.083 → 24.758 ms/frame for GPU composition and 36.975 → 26.842 ms/frame
with 2× xBRZ, including a GPU completion wait. This benchmark excludes gameplay's
intermediate-framebuffer checkpoint. These are synthetic timings; populated
combat, minimap overlays with game data, and ALT-loot audio need a live retest.

The browser CI builds the complete client, runs the shared renderer pixel oracle
in Chrome/WebGL 2, and runs the real C++ socket adapter against a pinned production
server gateway fixture. Proprietary assets are unnecessary for these tests.

The checks are one Rust program, `tools/web/browser-tests`, that drives an
installed Chrome or Chromium in new headless mode over the DevTools protocol; it
needs a Rust toolchain and nothing from npm. Serve `build/web` on
`http://127.0.0.1:18739` (see above), then:

```sh
cargo run --release --manifest-path tools/web/browser-tests/Cargo.toml -- renderer
cargo run --release --manifest-path tools/web/browser-tests/Cargo.toml -- transport
cargo run --release --manifest-path tools/web/browser-tests/Cargo.toml -- client
```

Each command exits 0 on success, 1 on a failed check or an invalid environment
value, and 2 on invalid arguments; `-- help` lists the arguments and environment
variables. An uncaught page error during the transport probe fails it even when
the probe itself reports success. The browser is found through
`WEB_TEST_BROWSER` (an executable path or a name on `PATH`), then `CHROME`, then
`google-chrome`, `google-chrome-stable`, `chromium` or `chromium-browser` on
`PATH`, then the standard Chrome install location. WebGL may fall back to
SwiftShader where no GPU is usable. Running as root or with `CI` set adds
`--no-sandbox`, which containers and CI runners need.

`renderer [url]` loads `web_sprite_tests.html` and waits up to 15 minutes for the
summary line. `WEB_TEST_SOFTWARE_GL=1` renders with SwiftShader instead of the GPU,
as CI does. The WebGL device, shader status, failures and summary are printed; the
full console goes to `WEB_TEST_LOG` (default `web-renderer.log`).

`transport [origin] [endpoint...]` runs `transport_tests.mjs` once per endpoint in a
fresh page, each under a 35-second watchdog kept outside the page. It expects the
server repository's fixture on ports 18740/18741: in that repository's
`src/server/websocketproxyserver`, start `cargo run --release --bin client-fixture` first. The
fixture admits only the origin `http://127.0.0.1:18739`. The same fixture accepts
native `transport_tests ws://127.0.0.1:18740/game` and the adversarial endpoint on
18741. The probes exercise binary data, partial reads, login/world/relogin
connections, route rejection and text-frame rejection. They do not need an account
or database.

`client [url]` additionally requires the packaged game data. It exercises the real
menu, typing, canvas sizing, fullscreen, normal exit and settings reload, with the
HTTP cache disabled and a persistent profile in `build/web-smoke/profile`.
`WEB_TEST_HEIGHT` (default 900) sets the viewport height, `WEB_TEST_DPR=2` checks
high-DPI output and `WEB_TEST_SPRITE_RENDERER=cpu` checks software composition.
The test reads the game's file system and selects the sprite renderer by patching
`launcher.mjs` as it is served, through DevTools request interception, so the
shipped launcher carries no test hooks; the check fails if the launcher no longer
contains the patched statements. Screenshots and `client.log` are written under
`build/web-smoke`. Live authenticated gameplay against a populated server is a
separate integration check; these probes do not claim that coverage.
