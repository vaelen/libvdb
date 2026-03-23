# usersadm - User Database Administration Example

This example demonstrates how to use **libvdb** to build a record-oriented
database application with secondary indexes and password hashing. It
implements a simple tool for managing user accounts, with both an
interactive menu and a command-line interface.

## Building

From this directory:

```bash
make
```

This builds `usersadm` and, if necessary, builds `libvdb.a` in the
repository root. The example compiles with `-std=c89` to match the
library's portability target and requires no libraries beyond the
standard C library and libvdb.

To clean up the binary and any database files created during use:

```bash
make clean
```

## Usage

### Interactive menu

Run without arguments to get an interactive menu:

```bash
./usersadm
```

```
=== User Database Administration ===

1) Initialize database
2) List users
3) Create user
4) View user
5) Edit user
6) Delete user
7) Quit

Choice:
```

The menu loops after each operation until you choose quit. This mode
works on systems that lack a command-line shell, such as Classic Mac OS,
where the program is launched by double-clicking.

### Command-line interface

For scripting or Unix-style use, pass a command as an argument:

```
usersadm <command> [args]

Commands:
  init              Create a new user database
  list              List all users
  create            Create a new user (interactive)
  view <id|str>     View user by ID, username, or email
  edit <id>         Edit a user (interactive)
  delete <id>       Delete a user
```

### Quick start

```bash
./usersadm init       # Create the database and indexes
./usersadm create     # Add a user (prompts for fields)
./usersadm list       # Show all users
./usersadm view alice # Look up by username
./usersadm edit 1     # Edit user with ID 1
./usersadm delete 1   # Delete user with ID 1
```

The `init` command must be run once before any other commands. It creates
the database files in the current working directory:

| File | Purpose |
|------|---------|
| `users.dat` | Record data (512-byte pages) |
| `users.idx` | Primary index (record ID to page mapping) |
| `users.jnl` | Transaction journal for crash recovery |
| `users.i00` | Secondary B-Tree index on username |
| `users.i01` | Secondary B-Tree index on email |

## What this example demonstrates

### Record serialization with explicit byte offsets

libvdb stores records as raw byte arrays of a fixed size declared at
database creation. The library has no knowledge of field structure --
that is entirely the application's responsibility.

This example defines a 194-byte record layout using named offsets:

```
Offset  Size  Field
------  ----  ---------------
  0      32   username
 32      64   real_name
 96      20   password_hash (SHA-1 digest)
116      64   email
180       4   created_at
184       4   updated_at
188       4   last_seen
192       1   access_level
193       1   locked
```

Rather than casting a C struct directly to/from the byte buffer, the code
uses `memcpy` with explicit offsets to read and write each field. This is
deliberate:

- **Struct padding is compiler-dependent.** A C compiler is free to
  insert padding bytes between struct members for alignment. The amount
  of padding varies across compilers, platforms, and optimization
  settings. If you write a struct to disk on one system and read it back
  on another, the fields may not line up.

- **The on-disk format must be stable.** Database files may be copied
  between machines or accessed by different builds of the same program.
  Explicit offsets guarantee that byte 116 is always the start of the
  email field, regardless of how the compiler lays out memory.

This is the same approach the library itself uses internally for
serializing its own headers and page structures.

### Endian conversion with PUT_LE32 / GET_LE32

The integer fields (timestamps) are stored in little-endian byte order
using the `PUT_LE32` and `GET_LE32` macros from `vdbutil.h`:

```c
PUT_LE32(buf + OFF_CREATED, created_at);   /* serialize */
created_at = (int32)GET_LE32(buf + OFF_CREATED);  /* deserialize */
```

This matters because libvdb's on-disk format is little-endian. On a
little-endian machine (x86, ARM in LE mode), these macros are no-ops.
On a big-endian machine (68k, PowerPC, SPARC), they byte-swap
automatically. Without this conversion, a database created on a
little-endian machine would contain garbage timestamps when opened on a
big-endian machine, and vice versa.

Single-byte fields like `access_level` and `locked` do not need endian
conversion. String fields are byte sequences and are also
endian-neutral.

### Secondary index management

libvdb's `AddRecord`, `UpdateRecord`, and `DeleteRecord` functions only
maintain the **primary index** (the record ID to page number mapping).
Secondary indexes must be managed by the application. This example
creates two secondary indexes -- on `username` and `email` -- and
maintains them manually:

- **On create:** after `AddRecord`, insert into both indexes:
  ```c
  InsertIntoIndex(&idx_username, StringKey(username), record_id);
  InsertIntoIndex(&idx_email, StringKey(email), record_id);
  ```

- **On edit:** if an indexed field changed, remove the old key and
  insert the new one:
  ```c
  if (strcmp(old_username, new_username) != 0) {
      DeleteFromIndex(&idx_username, StringKey(old_username), record_id);
      InsertIntoIndex(&idx_username, StringKey(new_username), record_id);
  }
  ```

- **On delete:** remove from both indexes before deleting the record:
  ```c
  DeleteFromIndex(&idx_username, StringKey(username), record_id);
  DeleteFromIndex(&idx_email, StringKey(email), record_id);
  ```

Each secondary index is a separate B-Tree file. The application opens
them with `OpenIndexFile`, operates on them with `InsertIntoIndex`,
`DeleteFromIndex`, and `FindInIndex`, and closes them with `CloseBTree`.

### String index lookups and hash collisions

Secondary string indexes use `StringKey()` to convert a string to an
`int32` B-Tree key. This function produces a case-insensitive CRC-16
hash. Because a 16-bit hash has only 65,536 possible values, collisions
are expected -- two different strings can produce the same key.

The B-Tree handles this by storing multiple values (record IDs) per key.
When looking up a user by username, the code must:

1. Compute the key: `StringKey("alice")`
2. Find all record IDs with that key: `FindInIndex(...)`
3. Load each candidate record and verify the actual field value matches

```c
for (i = 0; i < count && !found; i++) {
    if (FindRecordByID(&db, values[i], buf)) {
        memcpy(field, buf + OFF_USERNAME, USERNAME_LEN);
        if (strcmp(field, arg) == 0) {
            record_id = values[i];
            found = true;
        }
    }
}
```

This pattern -- hash lookup followed by field verification -- is
essential for correctness with any hash-based index.

### Listing records by page scan

libvdb does not provide a "scan all records" API. To list every record,
this example iterates over the pages in the data file directly:

```c
fseek(db.data_file, 0, SEEK_END);
total_pages = ftell(db.data_file) / DB_PAGE_SIZE;

for (page_num = 2; page_num < total_pages; page_num++) {
    ReadPage(&db, page_num, &page);
    if (page.status == PS_ACTIVE) {
        ReadRecord(&db, page_num, buf);
        /* ... display the record ... */
    }
}
```

Pages 0 and 1 are the header and free list, so the scan starts at page
2. Deleted or empty pages are skipped by checking `page.status`. The
record ID is available in `page.id`.

### Password hashing with SHA-1

Passwords are hashed using the library's SHA-1 implementation before
storage. The 20-byte digest is stored directly in the record:

```c
SHA1(password, (int16)strlen(password), password_hash);
```

The `view` command displays the hash as a 40-character hex string. The
edit command only rehashes if the user provides a new password; pressing
Enter keeps the existing hash.

Note: SHA-1 is used here because it is the strongest hash algorithm
available in libvdb. For a production system you would want a
purpose-built password hashing function like bcrypt or Argon2, but those
would require external libraries, which this example avoids.
