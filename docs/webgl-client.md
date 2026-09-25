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
-DCMAKE_BUILD_TYPE=Release -DBUILD_ENGINE=OFF -DBUILD_TESTS=OFF`, then run
`cmake --build build/wasm --target DarkEden web_sprite_tests transport_tests`.
CMake downloads the pinned SDL ports and GNU iconv 1.18.

## Assets and server configuration

Game assets are distributed separately. Prepare an asset ZIP with `Data/` at
its root, and Noto Sans CJK Regular's `NotoSansCJK-Regular.ttc` with its
`LICENSE.txt` beside it. Then run:

```sh
python tools/web/package-assets.py path/to/assets.zip build/web/assets \
  --font path/to/NotoSansCJK-Regular.ttc
```

The packer rejects traversal, symlinks and duplicate case-insensitive paths.
It excludes `UserSet` and writes a SHA-256 manifest. The launcher verifies every
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

The browser CI builds the complete client, runs the shared renderer pixel oracle
in Chrome/WebGL 2, and runs the real C++ socket adapter against a pinned production
server gateway fixture. Proprietary assets are unnecessary for these tests.

```sh
cd tools/web
npm ci --ignore-scripts
node test-renderer.mjs
node test-transport.mjs
node test-client.mjs
```

The default static origin is `http://127.0.0.1:18739`. Transport probes expect the
server repository's `tools/websocket/client-fixture.mjs` on ports 18740/18741;
install its npm dependencies and start it first. The same fixture accepts native
`transport_tests ws://127.0.0.1:18740/game` and the adversarial endpoint on 18741.
The probes exercise binary data, partial reads, login/world/relogin connections,
route rejection and text-frame rejection. They do not need an account or database.

`test-client.mjs` additionally requires the packaged game data and installed
Chrome. It exercises the real menu, typing, canvas sizing, fullscreen, normal exit
and settings reload. `WEB_TEST_DPR=2` checks high-DPI output;
`WEB_TEST_SPRITE_RENDERER=cpu` checks software composition. Screenshots and logs
are written under `build/web-smoke`. `WEB_TEST_BROWSER` selects another installed
Playwright Chromium channel. Live authenticated gameplay against a populated
server is a separate integration check; these probes do not claim that coverage.
