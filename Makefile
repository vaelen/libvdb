CC      = gcc
CFLAGS  = -ansi -pedantic -Wall -Werror -Iinclude
BINDIR  = bin
TESTDIR = $(BINDIR)/test
EXDIR   = $(BINDIR)/examples

TEST_SRC = src/test/test.c
UTIL_SRC = src/util/string.c
CRC_SRC   = src/crc/crc.c
SHA1_SRC  = src/crc/sha1.c
BTREE_SRC = src/btree/btree.c
DB_SRC    = src/db/dbutil.c src/db/db.c src/db/record.c src/db/journal.c src/db/maint.c
USERADM_SRC = examples/useradm/useradm.c examples/useradm/user.c examples/useradm/search.c
USERADM_USER_SRC = examples/useradm/user.c

all: test_test test_types test_string test_crc16 test_sha1 test_btree test_dbutil test_db test_record test_journal test_maint test_useradm useradm

test_test: $(TESTDIR)/test

$(TESTDIR)/test: tests/test/test.c $(TEST_SRC) include/test.h
	@mkdir -p $(TESTDIR)
	$(CC) $(CFLAGS) -o $(TESTDIR)/test tests/test/test.c $(TEST_SRC)

test_types: $(TESTDIR)/types

$(TESTDIR)/types: tests/util/types.c $(TEST_SRC) include/util.h include/util/types.h
	@mkdir -p $(TESTDIR)
	$(CC) $(CFLAGS) -o $(TESTDIR)/types tests/util/types.c $(TEST_SRC)

test_string: $(TESTDIR)/string

$(TESTDIR)/string: tests/util/string.c $(TEST_SRC) $(UTIL_SRC) include/util.h include/util/string.h
	@mkdir -p $(TESTDIR)
	$(CC) $(CFLAGS) -o $(TESTDIR)/string tests/util/string.c $(TEST_SRC) $(UTIL_SRC)

test_crc16: $(TESTDIR)/crc16

$(TESTDIR)/crc16: tests/crc/crc16.c $(TEST_SRC) $(UTIL_SRC) $(CRC_SRC) include/crc.h
	@mkdir -p $(TESTDIR)
	$(CC) $(CFLAGS) -o $(TESTDIR)/crc16 tests/crc/crc16.c $(TEST_SRC) $(UTIL_SRC) $(CRC_SRC)

test_sha1: $(TESTDIR)/sha1

$(TESTDIR)/sha1: tests/crc/sha1.c $(TEST_SRC) $(UTIL_SRC) $(CRC_SRC) $(SHA1_SRC) include/crc.h
	@mkdir -p $(TESTDIR)
	$(CC) $(CFLAGS) -o $(TESTDIR)/sha1 tests/crc/sha1.c $(TEST_SRC) $(UTIL_SRC) $(CRC_SRC) $(SHA1_SRC)

test_btree: $(TESTDIR)/btree

$(TESTDIR)/btree: tests/btree/btree.c $(TEST_SRC) $(UTIL_SRC) $(CRC_SRC) $(BTREE_SRC) include/btree.h
	@mkdir -p $(TESTDIR)
	$(CC) $(CFLAGS) -o $(TESTDIR)/btree tests/btree/btree.c $(TEST_SRC) $(UTIL_SRC) $(CRC_SRC) $(BTREE_SRC)

test_dbutil: $(TESTDIR)/dbutil

$(TESTDIR)/dbutil: tests/db/dbutil.c $(TEST_SRC) $(UTIL_SRC) $(CRC_SRC) $(BTREE_SRC) $(DB_SRC) include/db.h
	@mkdir -p $(TESTDIR)
	$(CC) $(CFLAGS) -o $(TESTDIR)/dbutil tests/db/dbutil.c $(TEST_SRC) $(UTIL_SRC) $(CRC_SRC) $(BTREE_SRC) $(DB_SRC)

test_db: $(TESTDIR)/db

$(TESTDIR)/db: tests/db/db.c $(TEST_SRC) $(UTIL_SRC) $(CRC_SRC) $(BTREE_SRC) $(DB_SRC) include/db.h
	@mkdir -p $(TESTDIR)
	$(CC) $(CFLAGS) -o $(TESTDIR)/db tests/db/db.c $(TEST_SRC) $(UTIL_SRC) $(CRC_SRC) $(BTREE_SRC) $(DB_SRC)

test_record: $(TESTDIR)/record

$(TESTDIR)/record: tests/db/record.c $(TEST_SRC) $(UTIL_SRC) $(CRC_SRC) $(BTREE_SRC) $(DB_SRC) include/db.h
	@mkdir -p $(TESTDIR)
	$(CC) $(CFLAGS) -o $(TESTDIR)/record tests/db/record.c $(TEST_SRC) $(UTIL_SRC) $(CRC_SRC) $(BTREE_SRC) $(DB_SRC)

test_journal: $(TESTDIR)/journal

$(TESTDIR)/journal: tests/db/journal.c $(TEST_SRC) $(UTIL_SRC) $(CRC_SRC) $(BTREE_SRC) $(DB_SRC) include/db.h
	@mkdir -p $(TESTDIR)
	$(CC) $(CFLAGS) -o $(TESTDIR)/journal tests/db/journal.c $(TEST_SRC) $(UTIL_SRC) $(CRC_SRC) $(BTREE_SRC) $(DB_SRC)

test_maint: $(TESTDIR)/maint

$(TESTDIR)/maint: tests/db/maint.c $(TEST_SRC) $(UTIL_SRC) $(CRC_SRC) $(BTREE_SRC) $(DB_SRC) include/db.h
	@mkdir -p $(TESTDIR)
	$(CC) $(CFLAGS) -o $(TESTDIR)/maint tests/db/maint.c $(TEST_SRC) $(UTIL_SRC) $(CRC_SRC) $(BTREE_SRC) $(DB_SRC)

test: test_test test_types test_string test_crc16 test_sha1 test_btree test_dbutil test_db test_record test_journal test_maint test_useradm
	./$(TESTDIR)/test
	./$(TESTDIR)/types
	./$(TESTDIR)/string
	./$(TESTDIR)/crc16
	./$(TESTDIR)/sha1
	./$(TESTDIR)/btree
	./$(TESTDIR)/dbutil
	./$(TESTDIR)/db
	./$(TESTDIR)/record
	./$(TESTDIR)/journal
	./$(TESTDIR)/maint
	./$(TESTDIR)/useradm

test_useradm: $(TESTDIR)/useradm

$(TESTDIR)/useradm: examples/useradm/test.c $(TEST_SRC) $(UTIL_SRC) $(CRC_SRC) $(SHA1_SRC) $(BTREE_SRC) $(DB_SRC) $(USERADM_USER_SRC) examples/useradm/search.c examples/useradm/useradm.h
	@mkdir -p $(TESTDIR)
	$(CC) $(CFLAGS) -Iexamples/useradm -o $(TESTDIR)/useradm examples/useradm/test.c $(TEST_SRC) $(UTIL_SRC) $(CRC_SRC) $(SHA1_SRC) $(BTREE_SRC) $(DB_SRC) $(USERADM_USER_SRC) examples/useradm/search.c

useradm: $(EXDIR)/useradm

$(EXDIR)/useradm: $(USERADM_SRC) $(UTIL_SRC) $(CRC_SRC) $(SHA1_SRC) $(BTREE_SRC) $(DB_SRC) examples/useradm/useradm.h
	@mkdir -p $(EXDIR)
	$(CC) $(CFLAGS) -Iexamples/useradm -o $(EXDIR)/useradm $(USERADM_SRC) $(UTIL_SRC) $(CRC_SRC) $(SHA1_SRC) $(BTREE_SRC) $(DB_SRC)

clean:
	rm -rf $(BINDIR)

.PHONY: all test clean test_test test_types test_string test_crc16 test_sha1 test_btree test_dbutil test_db test_record test_journal test_maint test_useradm useradm
