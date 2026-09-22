# UnRAR 7.3.1

C++ sources and notices from
https://www.rarlab.com/rar/unrarsrc-7.3.1.tar.gz
(SHA-256 `634900842a3737d9cc15bbcc71d4c74cc713437e0bca296a573424fe5f2660ab`).
Upstream: https://www.rarlab.com/rar_add.htm . See `license.txt` and `acknow.txt`.
The local CMake file builds the upstream DLL API into a static library, retaining
encryption support. No runtime UnRAR DLL or external unpacking process is needed.
Project warning flags and sanitizers apply to this target too.

Local source patch: `os.hpp` leaves `ALLOW_MISALIGNED` undefined on all CPUs.
This selects upstream's portable integer/AES/PPM paths: its optimized reads
dereference misaligned integer pointers and fail UBSan even for valid archives.
All other upstream files retain their original bytes. The archive regression
fixtures cover the portable paths under plain and sanitized builds.

`basic/RarArchive.cpp` uses the test-to-callback API to read bytes into bounded
memory. It never requests filesystem extraction and refuses volumes and links.

UnRAR source code may be used in any software to handle
RAR archives without limitations free of charge, but cannot be
used to develop RAR (WinRAR) compatible archiver and to
re-create RAR compression algorithm, which is proprietary.
Distribution of modified UnRAR source code in separate form
or as a part of other software is permitted, provided that
full text of this paragraph, starting from "UnRAR source code"
words, is included in license, or in documentation if license
is not available, and in source code comments of resulting package.
