# English text for the Korean game data

The game data ships in CP949 Korean (a few tables in Chinese) and is not part
of this repository, so the English text lives here and is applied over the
loaded data at runtime or as loose files the client prefers over the packed
originals. Every translation table is keyed by the source text rather than
by position, so a line that recurs is translated once, and a slot the table
has no entry for keeps whatever the data file supplied.

| Data | Where the English goes | Translation table | Tool |
|---|---|---|---|
| `Data/Info/String.inf` (UI strings) | `Client/MGameStringTable.cpp` (hand written) | in the source | - |
| `Data/Info/NPCScript.inf` (NPC dialogue) | `Client/MNPCScriptTableEnglish.cpp` | `npcscript.en.tsv` | `npcscript_dump.pl`, `npcscript_gen.pl` |
| `Data/Info/NPC.inf` (NPC names and roles) | `Client/MNPCTableEnglish.cpp` | `npc.en.tsv` | `npc_dump.pl`, `npc_gen.pl` |
| `Data/Info/SkillInfo.inf` (skill display names) | `MSkillInfoTable::UseEnglishNames()` | none: the English skill name the file also carries | - |
| `Data/Ui/txt/*.rpk` (item, skill, help, book, tutorial, progress and title text) | loose files under `ui-text/Data/Ui/txt/` | `uitext.en.tsv` | `uitext_extract.pl`, `uitext_apply.pl` |

The runtime overlays are applied when `Data/Info/Language.inf` selects
English or is missing (`UseEnglishText()` in `MGameStringTable.cpp`). The
loose files are picked up by `CRarFile` whenever they sit beside the archive
(`VS_UI/RarFile.cpp`), whatever the language setting.

## NPC dialogue

`NPCScript.inf` holds every line an NPC speaks - 1193 scripts, each with one
subject (what the NPC says) and any number of contents (the replies the
player picks from). `npcscript_dump.pl` writes `npcscript.ko.tsv` (id, kind
`S`/`C`, index, owner, Korean); `npcscript.en.tsv` (Korean, English) is the
file to edit; `npcscript_gen.pl` writes the C++ table.

```bash
perl tools/i18n/npcscript_dump.pl /path/to/Data/Info/NPCScript.inf tools/i18n/npcscript.ko.tsv
perl tools/i18n/npcscript_gen.pl tools/i18n/npcscript.ko.tsv tools/i18n/npcscript.en.tsv Client/MNPCScriptTableEnglish.cpp
```

The generator reports how many slots it translated and how many distinct
lines are still Korean, which is the number to watch after the data file
changes. The server repository's `tools/i18n/script.en.tsv` is a copy of
`npcscript.en.tsv` (plus the few lines only its database has), so the same
dialogue reads the same whether the client shows its own table or text the
server sends; keep the two in step.

## NPC names and roles

`NPC.inf` holds the name shown over an NPC and the one-line description of
its role. `npc_dump.pl` writes `npc.ko.tsv` (id, sprite, name,
description); `npc.en.tsv` (Korean, English) translates both names and
descriptions in one table; `npc_gen.pl` writes the C++ table.

```bash
perl tools/i18n/npc_dump.pl /path/to/Data/Info/NPC.inf tools/i18n/npc.ko.tsv
perl tools/i18n/npc_gen.pl tools/i18n/npc.ko.tsv tools/i18n/npc.en.tsv Client/MNPCTableEnglish.cpp
```

The server's NPC table, trigger table and quest scripts name the same NPCs;
its `tools/i18n/npc_names.en.tsv` uses the spellings from this file.

## Packed UI text

The item and skill descriptions, the help pages and tips, the library books,
the tutorial mails and quest lists, the monster lore of the progress screen
and the character creation hints ship in seven password-protected archives
under `Data/Ui/txt` (`Item.rpk`, `Skill.rpk`, `Help.rpk`, `Book.rpk`,
`TutorialEtc.rpk`, `progress.rpk`, `title.rpk`; the password is
`RPK_PASSWORD` in `VS_UI/src/header/VS_UI_filepath.h`). Unpack them one
directory per archive, named after the archive:

```bash
for a in Item Skill Help Book TutorialEtc progress title; do
  unrar x -pdarkeden "Data/Ui/txt/$a.rpk" "unpacked/$a/"
done
```

`uitext_extract.pl` lists every distinct line that contains Korean
(`uitext.ko.tsv`: archive/member, line); `uitext.en.tsv` (Korean line,
English line) is the translation; `uitext_apply.pl` writes each member that
had Korean text as an ASCII file with the Korean lines replaced:

```bash
perl tools/i18n/uitext_extract.pl unpacked tools/i18n/uitext.ko.tsv
perl tools/i18n/uitext_apply.pl unpacked tools/i18n/uitext.en.tsv tools/i18n/ui-text
```

The output, `ui-text/Data/Ui/txt/<member>`, is committed: it is what ships.
Copy `ui-text/Data` over a game directory to use it with the native client,
or pass `--overlay tools/i18n/ui-text` to `tools/web/package-assets.py` to
put it in the browser asset pack. `uitext_apply.pl` writes a member only
when every Korean line it holds is translated and lists the lines that are
not, so a half-translated file never ships.

Rules the apply script enforces: every English line is ASCII (the client
decodes loose text as CP949, and ASCII is the same in both); a line stays a
line (the help format is line-oriented and the archive's own line endings
are kept); and the `[==...==]` markers of the help format are never
extracted, since the client parses them verbatim. Keep `%s`, `%d`, `\n`
sequences in the mail templates, the `< file = '...' pos = 'L'>` image tags
of the help pages and the XML markup of the quest lists exactly as the
source has them.

`TutorialEtc.rpk` carries `SimpleGQuest.xml` and `EventGQuest.xml`, the same
files the server reads from its `data/` directory; the server's
`tools/i18n/data.en.tsv` is the subset of `uitext.en.tsv` that translates
them, so a change to a quest text belongs in both repositories.

## Editing the translations

- Both columns of every table carry the dump's escaping: `\\` for a
  backslash, `\t` for a tab, and in the NPC dialogue also `\r`, `\n` and
  `\xNN` for other control bytes. Keep the two-character escapes rather than
  real control characters, or a string will run onto the next line and the
  file will not parse.
- Keep every `%(Name)` parameter - `%(MonsterName)`, `%(QuestZone)`,
  `%(UserName)`, `%(GuildName)`, `%(CastleName)` and the rest. The client
  substitutes them at display time; drop one and the text renders with a
  hole in it.
- English only, ASCII only. The client transcodes CP949 to UTF-8 when it
  renders, and non-ASCII here would be transcoded a second time.
- Keep dialogue subjects under 2048 bytes: `UIDialog` copies one into a
  buffer that size.
- Names: the NPC, zone, monster and item spellings are shared with the
  server repository (`tools/i18n/*.en.tsv` there); change a name in both.

One shipped dialogue string does not decode as CP949 (script 19400's
subject, which has a stray byte in it). The dump keeps it as raw bytes and
counts it in the "undecodable" total; it still round-trips, because the key
is compared byte for byte.
