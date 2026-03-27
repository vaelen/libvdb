CC      = gcc
CFLAGS  = -ansi -pedantic -Wall -Werror -Iinclude
BINDIR  = bin

TEST_SRC = src/test/test.c
UTIL_SRC = src/util/string.c
CRC_SRC   = src/crc/crc.c
SHA1_SRC  = src/crc/sha1.c
BTREE_SRC = src/btree/btree.c
DB_SRC    = src/db/dbutil.c src/db/db.c src/db/record.c src/db/journal.c src/db/maint.c
USERADM_SRC = examples/useradm/useradm.c examples/useradm/user.c examples/useradm/search.c
USERADM_USER_SRC = examples/useradm/user.c

all: test_test test_types test_string test_crc16 test_sha1 test_btree test_dbutil test_db test_record test_journal test_maint test_useradm useradm

test_test: $(BINDIR)/test_test

$(BINDIR)/test_test: tests/test/test.c $(TEST_SRC) include/test.h
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/test_test tests/test/test.c $(TEST_SRC)

test_types: $(BINDIR)/test_types

$(BINDIR)/test_types: tests/util/types.c $(TEST_SRC) include/util.h include/util/types.h
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/test_types tests/util/types.c $(TEST_SRC)

test_string: $(BINDIR)/test_string

$(BINDIR)/test_string: tests/util/string.c $(TEST_SRC) $(UTIL_SRC) include/util.h include/util/string.h
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/test_string tests/util/string.c $(TEST_SRC) $(UTIL_SRC)

test_crc16: $(BINDIR)/test_crc16

$(BINDIR)/test_crc16: tests/crc/crc16.c $(TEST_SRC) $(UTIL_SRC) $(CRC_SRC) include/crc.h
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/test_crc16 tests/crc/crc16.c $(TEST_SRC) $(UTIL_SRC) $(CRC_SRC)

test_sha1: $(BINDIR)/test_sha1

$(BINDIR)/test_sha1: tests/crc/sha1.c $(TEST_SRC) $(UTIL_SRC) $(CRC_SRC) $(SHA1_SRC) include/crc.h
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/test_sha1 tests/crc/sha1.c $(TEST_SRC) $(UTIL_SRC) $(CRC_SRC) $(SHA1_SRC)

test_btree: $(BINDIR)/test_btree

$(BINDIR)/test_btree: tests/btree/btree.c $(TEST_SRC) $(UTIL_SRC) $(CRC_SRC) $(BTREE_SRC) include/btree.h
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/test_btree tests/btree/btree.c $(TEST_SRC) $(UTIL_SRC) $(CRC_SRC) $(BTREE_SRC)

test_dbutil: $(BINDIR)/test_dbutil

$(BINDIR)/test_dbutil: tests/db/dbutil.c $(TEST_SRC) $(UTIL_SRC) $(CRC_SRC) $(BTREE_SRC) $(DB_SRC) include/db.h
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/test_dbutil tests/db/dbutil.c $(TEST_SRC) $(UTIL_SRC) $(CRC_SRC) $(BTREE_SRC) $(DB_SRC)

test_db: $(BINDIR)/test_db

$(BINDIR)/test_db: tests/db/db.c $(TEST_SRC) $(UTIL_SRC) $(CRC_SRC) $(BTREE_SRC) $(DB_SRC) include/db.h
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/test_db tests/db/db.c $(TEST_SRC) $(UTIL_SRC) $(CRC_SRC) $(BTREE_SRC) $(DB_SRC)

test_record: $(BINDIR)/test_record

$(BINDIR)/test_record: tests/db/record.c $(TEST_SRC) $(UTIL_SRC) $(CRC_SRC) $(BTREE_SRC) $(DB_SRC) include/db.h
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/test_record tests/db/record.c $(TEST_SRC) $(UTIL_SRC) $(CRC_SRC) $(BTREE_SRC) $(DB_SRC)

test_journal: $(BINDIR)/test_journal

$(BINDIR)/test_journal: tests/db/journal.c $(TEST_SRC) $(UTIL_SRC) $(CRC_SRC) $(BTREE_SRC) $(DB_SRC) include/db.h
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/test_journal tests/db/journal.c $(TEST_SRC) $(UTIL_SRC) $(CRC_SRC) $(BTREE_SRC) $(DB_SRC)

test_maint: $(BINDIR)/test_maint

$(BINDIR)/test_maint: tests/db/maint.c $(TEST_SRC) $(UTIL_SRC) $(CRC_SRC) $(BTREE_SRC) $(DB_SRC) include/db.h
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -o $(BINDIR)/test_maint tests/db/maint.c $(TEST_SRC) $(UTIL_SRC) $(CRC_SRC) $(BTREE_SRC) $(DB_SRC)

test: test_test test_types test_string test_crc16 test_sha1 test_btree test_dbutil test_db test_record test_journal test_maint test_useradm
	./$(BINDIR)/test_test
	./$(BINDIR)/test_types
	./$(BINDIR)/test_string
	./$(BINDIR)/test_crc16
	./$(BINDIR)/test_sha1
	./$(BINDIR)/test_btree
	./$(BINDIR)/test_dbutil
	./$(BINDIR)/test_db
	./$(BINDIR)/test_record
	./$(BINDIR)/test_journal
	./$(BINDIR)/test_maint
	./$(BINDIR)/test_useradm

test_useradm: $(BINDIR)/test_useradm

$(BINDIR)/test_useradm: examples/useradm/test.c $(TEST_SRC) $(UTIL_SRC) $(CRC_SRC) $(SHA1_SRC) $(BTREE_SRC) $(DB_SRC) $(USERADM_USER_SRC) examples/useradm/search.c examples/useradm/useradm.h
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -Iexamples/useradm -o $(BINDIR)/test_useradm examples/useradm/test.c $(TEST_SRC) $(UTIL_SRC) $(CRC_SRC) $(SHA1_SRC) $(BTREE_SRC) $(DB_SRC) $(USERADM_USER_SRC) examples/useradm/search.c

useradm: $(BINDIR)/useradm

$(BINDIR)/useradm: $(USERADM_SRC) $(UTIL_SRC) $(CRC_SRC) $(SHA1_SRC) $(BTREE_SRC) $(DB_SRC) examples/useradm/useradm.h
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -Iexamples/useradm -o $(BINDIR)/useradm $(USERADM_SRC) $(UTIL_SRC) $(CRC_SRC) $(SHA1_SRC) $(BTREE_SRC) $(DB_SRC)

clean:
	rm -rf $(BINDIR)

.PHONY: all test clean test_test test_types test_string test_crc16 test_sha1 test_btree test_dbutil test_db test_record test_journal test_maint test_useradm useradm
