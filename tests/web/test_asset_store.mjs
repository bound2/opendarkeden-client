// Uses the shipped Wasm zlib and Emscripten filesystem; no game data required.
// Run after building: node --test tests/web/test_asset_store.mjs
import assert from 'node:assert/strict';
import { createHash, randomFillSync } from 'node:crypto';
import { readFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { deflateSync } from 'node:zlib';
import { test } from 'node:test';
import createDarkEden from '../../build/web/DarkEden.mjs';
import { BLOCK_SIZE, CACHE_BYTES, createAssetStore, parseAsset } from '../../web/asset-store.mjs';

const client = await createDarkEden({ noInitialRun: true });
const { FS } = client;
let fileId = 0;

function pack(source) {
  const count = Math.ceil(source.length / BLOCK_SIZE);
  const header = Buffer.alloc(16 + count * 4);
  header.write('DEZ1');
  header.writeUInt32LE(BLOCK_SIZE, 4);
  header.writeUInt32LE(source.length, 8);
  header.writeUInt32LE(count, 12);
  const blocks = [header];
  for (let i = 0; i < count; ++i) {
    const raw = source.subarray(i * BLOCK_SIZE, (i + 1) * BLOCK_SIZE);
    const compressed = deflateSync(raw, { level: 3 });
    const useRaw = compressed.length >= raw.length;
    const stored = useRaw ? raw : compressed;
    header.writeUInt32LE(stored.length + (useRaw ? 0x80000000 : 0), 16 + i * 4);
    blocks.push(stored);
  }
  return new Uint8Array(Buffer.concat(blocks));
}

function mount(source, store = createAssetStore(client)) {
  const path = `/Data/Probe${++fileId}.spk`;
  store.add(path, pack(source), source.length);
  return { path, store };
}

test('empty, partial, compressed and incompressible blocks preserve bytes and stat size', () => {
  for (const source of [Buffer.alloc(0), Buffer.from('x'), Buffer.alloc(BLOCK_SIZE * 3 + 19, 42),
    randomFillSync(Buffer.alloc(BLOCK_SIZE * 3 + 17))]) {
    const { path } = mount(source);
    assert.equal(FS.stat(path).size, source.length);
    assert.deepEqual(Buffer.from(FS.readFile(path.toLowerCase())), source);
  }
});

test('seeking, cross-block reads, EOF and concurrent file positions', () => {
  const source = Buffer.alloc(BLOCK_SIZE * 4 + 27);
  for (let i = 0; i < source.length; ++i) source[i] = (i * 37 + (i >>> 16)) & 255;
  const { path } = mount(source);
  const first = FS.open(path, 'r'), second = FS.open(path, 'r');
  const data = new Uint8Array(BLOCK_SIZE + 31);
  for (const position of [0, BLOCK_SIZE - 7, BLOCK_SIZE * 2 + 3, source.length - 4, source.length, source.length + 10]) {
    FS.llseek(first, position, 0);
    const count = FS.read(first, data, 3, data.length - 3);
    assert.equal(count, Math.min(data.length - 3, Math.max(0, source.length - position)));
    assert.deepEqual(Buffer.from(data.subarray(3, 3 + count)), source.subarray(position, position + count));
    assert.equal(second.position, 0);
  }
  assert.equal(FS.llseek(first, -9, 2), source.length - 9);
  assert.equal(FS.llseek(first, 2, 1), source.length - 7);
  assert.throws(() => FS.llseek(first, -1, 0));
  FS.close(first);
  FS.close(second);
});

test('the decoded cache is bounded across files and evicted data is decoded again', () => {
  const store = createAssetStore(client);
  const source = Buffer.alloc(CACHE_BYTES + BLOCK_SIZE * 2 + 19, 73);
  const first = mount(source, store), second = mount(source, store);
  for (const { path } of [first, second, first]) {
    assert.deepEqual(Buffer.from(FS.readFile(path)), source);
    const stats = store.stats();
    assert.ok(stats.cachedBytes <= CACHE_BYTES);
    assert.ok(stats.storedBytes + stats.indexBytes + stats.cachedBytes + stats.workspaceBytes < source.length * 2);
  }
  assert.ok(store.stats().inflations >= Math.ceil(source.length / BLOCK_SIZE) * 3);
  const stream = FS.open(first.path, 'r');
  const data = new Uint8Array(10);
  FS.read(stream, data, 0, 10, source.length - 10);
  const before = store.stats().inflations;
  FS.read(stream, data, 0, 10, source.length - 10);
  assert.equal(store.stats().inflations, before);
  FS.close(stream);
});

test('writes and truncation cannot change the compressed file', () => {
  const { path } = mount(Buffer.alloc(BLOCK_SIZE, 3));
  assert.throws(() => FS.open(path, 'r+'));
  // Even if a caller bypasses mode bits, stream_ops must still reject writes.
  const ignored = FS.ignorePermissions;
  FS.ignorePermissions = true;
  try {
    const stream = FS.open(path, 'r+');
    assert.throws(() => FS.write(stream, new Uint8Array([4]), 0, 1));
    FS.close(stream);
  } finally {
    FS.ignorePermissions = ignored;
  }
  assert.throws(() => FS.truncate(path, 2));
  assert.throws(() => FS.chmod(path, 0o666));
  assert.equal(FS.readFile(path)[0], 3);
});

test('invalid headers, lengths and truncated/trailing data are rejected', () => {
  const source = Buffer.alloc(BLOCK_SIZE + 7, 9);
  const valid = pack(source);
  assert.throws(() => parseAsset(valid, source.length + 1));
  for (const offset of [0, 4, 8, 12, 16, 20]) {
    const changed = valid.slice();
    new DataView(changed.buffer).setUint32(offset, 0xffffffff, true);
    assert.throws(() => parseAsset(changed, source.length));
  }
  for (const data of [valid.slice(0, 12), valid.slice(0, 20), valid.slice(0, -1),
    new Uint8Array([...valid, 0])]) assert.throws(() => parseAsset(data, source.length));
});

test('damaged compressed blocks fail reads instead of returning partial garbage', () => {
  const source = Buffer.alloc(BLOCK_SIZE, 12), packed = pack(source);
  packed[20] ^= 0xff;
  const path = `/Data/Probe${++fileId}.spk`;
  createAssetStore(client).add(path, packed, source.length);
  assert.throws(() => FS.readFile(path));
});

test('a read into Wasm memory survives memory growth during decompression', () => {
  const source = Buffer.alloc(BLOCK_SIZE * 2 + 13, 29);
  const { path } = mount(source);
  const destination = client._malloc(source.length);
  const oldHeap = client.HEAPU8;
  const uncompress = client._uncompress;
  let grow = true;
  client._uncompress = (...args) => {
    const result = uncompress(...args);
    if (grow) {
      grow = false;
      const allocation = client._malloc(client.HEAPU8.length);
      assert.ok(allocation);
      client._free(allocation);
    }
    return result;
  };
  const stream = FS.open(path, 'r');
  try {
    assert.equal(FS.read(stream, oldHeap, destination, source.length), source.length);
    assert.notEqual(client.HEAPU8.buffer, oldHeap.buffer);
    assert.deepEqual(Buffer.from(client.HEAPU8.subarray(destination, destination + source.length)), source);
  } finally {
    client._uncompress = uncompress;
    FS.close(stream);
    client._free(destination);
  }
});

test('every compressed file in a local asset pack matches its original SHA-256',
  { skip: !process.env.WEB_TEST_ASSETS }, async () => {
    const root = resolve(process.env.WEB_TEST_ASSETS);
    const manifest = JSON.parse(await readFile(resolve(root, 'manifest.json'), 'utf8'));
    const store = createAssetStore(client);
    const buffer = new Uint8Array(BLOCK_SIZE);
    let checked = 0;
    for (const entry of manifest.files.filter(file => file.compressed)) {
      const packed = await readFile(resolve(root, entry.compressed.path));
      assert.equal(createHash('sha256').update(packed).digest('hex'), entry.compressed.sha256);
      const path = `/VerifiedPack/${++checked}`;
      store.add(path, packed, entry.size);
      const stream = FS.open(path, 'r');
      const digest = createHash('sha256');
      let count;
      while ((count = FS.read(stream, buffer, 0, buffer.length)) > 0)
        digest.update(buffer.subarray(0, count));
      FS.close(stream);
      assert.equal(digest.digest('hex'), entry.sha256, entry.path);
      assert.ok(store.stats().cachedBytes <= CACHE_BYTES);
      FS.unlink(path);
    }
    assert.ok(checked > 0, 'the pack must contain compressed assets');
    console.log(`Verified all ${checked} compressed assets against their original hashes.`);
  });
