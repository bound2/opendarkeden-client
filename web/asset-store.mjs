// Read-only, seekable assets: independent 64 KiB zlib blocks and one shared LRU.
// Large source files stay compressed outside the Wasm heap. Decoded game
// resources still belong to the engine and follow its existing lifetimes.
export const BLOCK_SIZE = 65536;
export const CACHE_BYTES = 4 * 1024 * 1024;
const RAW_BLOCK = 0x80000000;

export function parseAsset(bytes, expectedSize) {
  const invalid = () => { throw new Error('Invalid compressed game data.'); };
  if (!(bytes instanceof Uint8Array) || bytes.length < 16) invalid();
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const size = view.getUint32(8, true);
  const count = view.getUint32(12, true);
  if (view.getUint32(0, true) !== 0x315a4544 || view.getUint32(4, true) !== BLOCK_SIZE ||
      size !== expectedSize || count !== Math.ceil(size / BLOCK_SIZE) ||
      16 + count * 4 > bytes.length) invalid();
  const offsets = new Uint32Array(count + 1);
  let offset = 16 + count * 4;
  for (let i = 0; i < count; ++i) {
    offsets[i] = offset;
    const stored = view.getUint32(16 + i * 4, true);
    const length = stored & ~RAW_BLOCK;
    const decoded = Math.min(BLOCK_SIZE, size - i * BLOCK_SIZE);
    if (!length || length > decoded || ((stored & RAW_BLOCK) && length !== decoded)) invalid();
    offset += length;
    if (offset > bytes.length) invalid();
  }
  offsets[count] = offset;
  if (offset !== bytes.length) invalid();
  return { bytes, view, offsets, size };
}

export function createAssetStore(client, cacheBytes = CACHE_BYTES) {
  if (!Number.isSafeInteger(cacheBytes) || cacheBytes < BLOCK_SIZE)
    throw new Error('Asset cache must hold at least one block.');
  const { FS } = client;
  const cache = new Map();
  let cachedBytes = 0;
  let storedBytes = 0;
  let indexBytes = 0;
  let nextId = 0;
  let workspace = 0;
  let inflations = 0;
  // Emscripten/musl errno values. These are independent of host OS errno.
  const denied = () => { throw new FS.ErrnoError(30); }; // EROFS
  const invalid = () => { throw new FS.ErrnoError(28); }; // EINVAL

  function inflate(input, size) {
    // One allocation, reused by every file. uLongf is 32 bits on wasm32.
    if (!workspace) {
      workspace = client._malloc(BLOCK_SIZE * 2 + 4);
      if (!workspace) throw new FS.ErrnoError(48); // ENOMEM
    }
    const output = workspace + BLOCK_SIZE;
    const length = output + BLOCK_SIZE;
    client.HEAPU8.set(input, workspace);
    client.HEAPU32[length >>> 2] = size;
    if (client._uncompress(output, length, workspace, input.length) !== 0 ||
        client.HEAPU32[length >>> 2] !== size)
      throw new FS.ErrnoError(29); // EIO
    ++inflations;
    return client.HEAPU8.slice(output, output + size);
  }

  function block(asset, id, index) {
    const start = asset.offsets[index];
    const end = asset.offsets[index + 1];
    if (asset.view.getUint32(16 + index * 4, true) & RAW_BLOCK)
      return asset.bytes.subarray(start, end);
    const key = `${id}:${index}`;
    let decoded = cache.get(key);
    if (decoded) cache.delete(key);
    else {
      const size = Math.min(BLOCK_SIZE, asset.size - index * BLOCK_SIZE);
      // Evict before allocating the next block, keeping peak cache use bounded.
      while (cachedBytes + size > cacheBytes) {
        const oldest = cache.keys().next().value;
        cachedBytes -= cache.get(oldest).byteLength;
        cache.delete(oldest);
      }
      decoded = inflate(asset.bytes.subarray(start, end), size);
      cachedBytes += decoded.byteLength;
    }
    cache.set(key, decoded);
    return decoded;
  }

  return {
    add(path, bytes, size) {
      const asset = parseAsset(bytes, size);
      const id = nextId++;
      FS.mkdirTree(path.slice(0, path.lastIndexOf('/')));
      FS.createDataFile('/', path, new Uint8Array(0), true, false, true);
      const node = FS.lookupPath(path).node;
      // Keep regular MEMFS directory lookup/stat semantics, including case folding.
      // Override every operation that could interpret compressed bytes as raw data.
      node.contents = bytes;
      node.usedBytes = size;
      const metadata = node.node_ops;
      node.node_ops = {
        ...metadata,
        setattr(target, attributes) {
          if (attributes.size !== undefined || (attributes.mode !== undefined && (attributes.mode & 0o222))) denied();
          metadata.setattr(target, attributes);
        },
      };
      node.stream_ops = {
        llseek(stream, offset, whence) {
          let position = offset;
          if (whence === 1) position += stream.position;
          else if (whence === 2) position += size;
          else if (whence !== 0) invalid();
          if (!Number.isSafeInteger(position) || position < 0) invalid();
          return position;
        },
        read(stream, buffer, offset, length, position) {
          if (!Number.isSafeInteger(position) || position < 0) invalid();
          const count = Math.min(length, Math.max(0, size - position));
          const wasHeap = buffer.buffer === client.HEAPU8.buffer;
          const byteOffset = buffer.byteOffset;
          const byteLength = buffer.byteLength;
          let copied = 0;
          while (copied < count) {
            const at = position + copied;
            const data = block(asset, id, Math.floor(at / BLOCK_SIZE));
            // zlib's allocations can grow Wasm memory and detach its old views.
            if (wasHeap && buffer.buffer !== client.HEAPU8.buffer)
              buffer = new Uint8Array(client.HEAPU8.buffer, byteOffset, byteLength);
            const start = at % BLOCK_SIZE;
            const take = Math.min(count - copied, data.length - start);
            buffer.set(data.subarray(start, start + take), offset + copied);
            copied += take;
          }
          return count;
        },
        write: denied,
        allocate: denied,
        mmap() { throw new FS.ErrnoError(43); }, // ENODEV: use ordinary reads
        msync: denied,
      };
      storedBytes += bytes.byteLength;
      indexBytes += asset.offsets.byteLength;
    },
    stats() {
      return { storedBytes, indexBytes, cachedBytes, cacheLimit: cacheBytes,
        workspaceBytes: workspace ? BLOCK_SIZE * 2 + 4 : 0, inflations };
    },
  };
}
