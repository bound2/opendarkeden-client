# Versioned runtime assets

`build_assets_v2.py` builds the text-cleaned runtime archive from the exact published `assets-v1` ZIP and the original `DARKEDEN.zip` distribution linked in the root README. It requires Python 3.11 or newer and the Python standard library.

```powershell
gh release download assets-v1 --pattern darkeden-assets-v1.zip --dir build/assets-v2
python tools/assets/build_assets_v2.py build/assets-v2/darkeden-assets-v1.zip build/assets-v2/darkeden-assets-v2.zip --original C:/path/to/DARKEDEN.zip
```

Use `--audit-only` to parse the tables and write the change manifest without creating a ZIP. Outputs are the archive, a `.zip.sha256` checksum, and a `.changes.json` field-by-field audit.

The builder verifies the published v1 SHA-256 before reading records. It updates length-prefixed display strings in `Item.inf`, `Zone.inf`, `Creature.inf`, and `String.inf`, preserving legacy encodings. Item/map naming tags and website-only descriptions are removed. Creature names consisting solely of the old server name become `Creature`. Legacy English messages use `DarkEden` or customer-support wording; the obsolete website link is cleared, not redirected to an invented URL.

The original distribution is also SHA-256 pinned (`c7e277c1104a13db305619e2c5b1d4b69fa27d6e7e6919ab2239713ac6042e21`). Eight hidden files were omitted from v1: `NPC.inf`, `NPCScript.inf`, and the `mixingforge`, `monsterlevel`, and `trace` SPK/SPKI pairs. The missing NPC table prevents startup with a fresh v1 extraction. V2 restores these eight files byte-for-byte from the original distribution, records their hashes in the manifest, and includes no other files from that archive.

Validation reparses the rewritten tables, checks idempotence, reads all 2,021 ZIP entries to verify their CRCs, and compares every unmodified file with v1 by SHA-256 and every restored file with the original. The v1 archive layout is retained, including its empty `UserSet/`; no local saves or account settings enter the archive. Raster artwork is unchanged.

Four old zone records contain the literal five bytes `dk2th` in numeric property/music fields, outside their names. Those bytes retain their original numeric values. The verification permits those four occurrences while rejecting branding in the cleaned display strings and every other archive entry. Existing zero-filled table tails are also preserved.
