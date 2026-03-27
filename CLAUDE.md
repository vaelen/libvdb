# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Vaelen's Database (vDB) is a portable ANSI C (C89) database system targeting retro and modern platforms (DOS, MacOS 6-9, Linux, UNIX, Windows). The project is currently in the specification/early implementation phase. Detailed specifications live in `docs/`.

## Build & Test

```bash
make                    # build everything
make test               # build and run all tests
./bin/test_<name>       # run a specific test suite (e.g., ./bin/test_btree)
```

Code must compile in **strict C89 mode** (e.g., `gcc -ansi -pedantic`).

## Architecture

The system is built in layers, each depending only on layers below it:

```
db               — page-based database with journaling and indexing (docs/db.md)
   ↓
btree            — file-based B-Tree for indexes (docs/btree.md)
   ↓
crc              — CRC-16 and SHA-1 hashing
   ↓
util             — types (bool, uint16, int32, etc.), string helpers, ANSI C polyfills
   ↓
test             — minimal unit testing framework
```

An example program (`useradm`) lives in `examples/useradm/`.

Implementation order follows `docs/goals.md` (test framework → util → crc → btree → db).

## Coding Rules (docs/rules.md)

These are **strictly enforced** — read `docs/rules.md` for the full set:

- **C89 only**: variables at top of functions, `/* */` comments only, no C99+ features.
- **Naming**: `snake_case` for variables, `CamelCase` for functions and types.
- **8.3 filenames**: all filenames must be DOS-compatible (max 8.3).
- **Header guards**: `#ifndef`/`#define`/`#endif` in every header.
- **No external or OS-specific libraries**: wrap OS-specific code in separate `.c` files behind a shared `.h` interface (e.g., `os.h` → `unix.c`, `dos.c`).
- **Memory**: libraries should avoid allocating memory. Callers pass in pointers. When libraries must manage memory (e.g., btree), wrap `malloc`/`free` in OS-dependent wrappers.
- **Testing**: all library functions need comprehensive tests using the shared test framework.

## Project Structure

```
src/<lib>/<lib>.c      — library source (e.g., src/util/util.c)
include/<lib>.h        — primary header (e.g., include/util.h)
include/<lib>/*.h      — additional headers (e.g., include/util/strings.h)
tests/<lib>/*.c        — test files (e.g., tests/util/strings.c)
examples/<name>/       — example programs (e.g., examples/useradm/)
docs/                  — specifications and design documents
.claude/agents/        — Claude Code agent configurations (sw-eng, qa-eng)
```

## Agent Workflow

Two agents are configured in `.claude/agents/`:

- **sw-eng**: plans, writes tests, implements code, then hands off to QA.
- **qa-eng**: reviews for test coverage, security, buffer overruns, memory issues, and rule compliance. Sends issues back to the software engineer or reports pass.

## Key Specifications

- **docs/db.md**: database file format (512-byte pages, `.DAT`/`.IDX`/`.I??`/`.JNL` files), journaling protocol, helper function API, data types. All multi-byte values are little-endian.
- **docs/btree.md**: B-Tree file format, page layout, data structures, function signatures. Currently a simplified single-leaf implementation.
- **docs/useradm.md**: menu-driven CLI for user CRUD with username/email indexes. No screen-drawing libraries — standard C + ANSI escapes only.
