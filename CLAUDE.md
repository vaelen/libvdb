# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Test Commands

```bash
make              # Build library (lib/libvdb.a) and test binaries
make test         # Build and run all tests (35 tests across 3 suites)
make clean        # Remove build artifacts
make debug        # Build with -g -DDEBUG
make valgrind     # Run tests under valgrind
make unix         # Build without -std=c89 (for older Unix systems)
```

Run a single test suite: `make && ./bin/test_hash` (or `test_btree`, `test_db`).

## Architecture

C89 library ported from a Pascal BBS database system. Targets Classic Mac OS 7/8/9, Linux, FreeBSD, and DOS.

Three layers, each with an independently includable header (or use `vdb.h` for all):

- **hash** (`hash.h`/`hash.c`) — CRC-16 (Kermit & XMODEM), CRC-32 (cksum variant), SHA-1
- **btree** (`btree.h`/`btree.c`) — File-backed B-Tree (order 60, 512-byte pages)
- **db** (`db.h`/`db.c`) — Record-oriented database with journaling, free-page management, and secondary indexes built on the B-Tree

On-disk format is little-endian with 512-byte pages. Endian conversion is handled by macros in `include/vdbutil.h` (`HTOLE16`/`LE16TOH`/`PUT_LE32`/`GET_LE32` etc.), which is included via `vdb.h`.

`src/platform.c` provides `ftruncate_()` and Mac/Unix epoch conversion, with `_POSIX_C_SOURCE` defined for POSIX compliance in C89 mode.

## Critical Gotchas

- **64-bit LP64 systems**: `long` is 8 bytes. The `int32`/`uint32` typedefs in `vdbtypes.h` use `int`/`unsigned int` on LP64, not `long`. Getting this wrong corrupts on-disk structures.
- **CRC32 cksum**: Always processes 4 bytes of length (including leading zeros), unlike POSIX cksum. Test vector for "123456789" is `0x8AEAB5FB`.
- **Stack usage**: B-Tree `LeafNode` is ~15KB on stack (60 entries × 250 bytes).
- **Minimal heap**: Only `BTree *primary_index` in `Database` uses malloc. Everything else is stack or caller-provided.

## Naming Conventions

- Types: PascalCase (`Database`, `BTree`, `SHA1Context`)
- Constants/macros: UPPER_SNAKE (`DB_PAGE_SIZE`, `PS_ACTIVE`)
- Public functions: PascalCase (`CreateDatabase`, `BTreeInsert`)
- Internal/static functions: camelCase (`minInt`, `getFileSize`)

## Test Framework

Tests use a simple custom framework: `RUN_TEST(func)` macro calling functions that return 1 (pass) or 0 (fail), with `ASSERT_EQ`/`ASSERT_TRUE` macros. Each test file has its own `main()`.
