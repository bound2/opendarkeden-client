# RAR reader fixtures

Unmodified decoded `.rar.uu` fixtures from libarchive commit
`82f69404aacd4794dbb1b5a826e1f7d5101df4df`, under `libarchive/test/`:
https://github.com/libarchive/libarchive/tree/82f69404aacd4794dbb1b5a826e1f7d5101df4df/libarchive/test

The regular and Unicode archives belong to `test_read_format_rar.c`;
the encrypted archives belong to `test_read_format_rar_encryption_data.c`
and `test_read_format_rar_encryption_header.c`. Copyright and redistribution
terms are retained in LICENSE.txt. Both encrypted archives use `12345678`.
These contain upstream test data, not game resources.

The RAR4 and RAR5 solid encrypted fixtures are from
`test_read_format_rar_encryption.c` and use `password`.

The best-compression fixture is also from `test_read_format_rar.c`.
