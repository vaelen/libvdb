/*
 * VDB Database Tests
 *
 * Full CRUD cycle, multi-page records, persistence, validation, and compaction.
 */

#include "../include/vdb.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int tests_run = 0;
static int tests_passed = 0;

#define RUN_TEST(test_func) do { \
    printf("  " #test_func "... "); \
    tests_run++; \
    if (test_func()) { \
        printf("PASSED\n"); \
        tests_passed++; \
    } else { \
        printf("FAILED\n"); \
    } \
} while(0)

#define ASSERT_EQ(expected, actual) do { \
    if ((long)(expected) != (long)(actual)) { \
        printf("Expected: %ld, Got: %ld\n", (long)(expected), (long)(actual)); \
        return 0; \
    } \
} while(0)

#define ASSERT_TRUE(cond) do { \
    if (!(cond)) { \
        printf("Assertion failed: %s\n", #cond); \
        return 0; \
    } \
} while(0)

static const char *DB_TEST_NAME = "tmp/testdb";

static void cleanup(void) {
    remove("tmp/testdb.dat");
    remove("tmp/testdb.jnl");
    remove("tmp/testdb.idx");
}

/* Verify type sizes */
int test_type_sizes(void) {
    ASSERT_EQ(1, (int)sizeof(byte));
    ASSERT_EQ(2, (int)sizeof(int16));
    ASSERT_EQ(2, (int)sizeof(uint16));
    ASSERT_EQ(4, (int)sizeof(int32));
    ASSERT_EQ(4, (int)sizeof(uint32));
    return 1;
}

/* Verify DB header fields survive a create/open round-trip */
int test_db_header_roundtrip(void) {
    Database db;

    cleanup();
    ASSERT_TRUE(CreateDatabase(DB_TEST_NAME, 256));
    ASSERT_TRUE(OpenDatabase(DB_TEST_NAME, &db));

    ASSERT_TRUE(memcmp(db.header.signature, "RETRODB\0", 8) == 0);
    ASSERT_EQ(1, db.header.version);
    ASSERT_EQ(512, db.header.page_size);
    ASSERT_EQ(256, db.header.record_size);
    ASSERT_EQ(0, db.header.record_count);
    ASSERT_EQ(1, db.header.next_record_id);
    ASSERT_EQ(0, db.header.journal_pending);
    ASSERT_EQ(0, db.header.index_count);

    CloseDatabase(&db);
    cleanup();
    return 1;
}

int test_db_create(void) {
    cleanup();
    ASSERT_TRUE(CreateDatabase(DB_TEST_NAME, 128));
    cleanup();
    return 1;
}

int test_db_open_close(void) {
    Database db;
    cleanup();
    ASSERT_TRUE(CreateDatabase(DB_TEST_NAME, 128));
    ASSERT_TRUE(OpenDatabase(DB_TEST_NAME, &db));
    ASSERT_TRUE(db.is_open);
    ASSERT_EQ(128, db.header.record_size);
    CloseDatabase(&db);
    ASSERT_TRUE(!db.is_open);
    cleanup();
    return 1;
}

int test_db_add_find(void) {
    Database db;
    byte data[128];
    byte found[128];
    int32 id;

    cleanup();
    ASSERT_TRUE(CreateDatabase(DB_TEST_NAME, 128));
    ASSERT_TRUE(OpenDatabase(DB_TEST_NAME, &db));

    memset(data, 0, 128);
    strcpy((char *)data, "Hello, Database!");

    ASSERT_TRUE(AddRecord(&db, data, &id));
    ASSERT_EQ(1, id);

    memset(found, 0, 128);
    ASSERT_TRUE(FindRecordByID(&db, id, found));
    ASSERT_TRUE(memcmp(data, found, 128) == 0);

    CloseDatabase(&db);
    cleanup();
    return 1;
}

int test_db_multiple_records(void) {
    Database db;
    byte data[128];
    byte found[128];
    int32 id1, id2, id3;

    cleanup();
    ASSERT_TRUE(CreateDatabase(DB_TEST_NAME, 128));
    ASSERT_TRUE(OpenDatabase(DB_TEST_NAME, &db));

    memset(data, 0, 128);
    strcpy((char *)data, "Record One");
    ASSERT_TRUE(AddRecord(&db, data, &id1));

    memset(data, 0, 128);
    strcpy((char *)data, "Record Two");
    ASSERT_TRUE(AddRecord(&db, data, &id2));

    memset(data, 0, 128);
    strcpy((char *)data, "Record Three");
    ASSERT_TRUE(AddRecord(&db, data, &id3));

    ASSERT_EQ(3, db.header.record_count);

    memset(found, 0, 128);
    ASSERT_TRUE(FindRecordByID(&db, id2, found));
    ASSERT_TRUE(strcmp((char *)found, "Record Two") == 0);

    CloseDatabase(&db);
    cleanup();
    return 1;
}

int test_db_update(void) {
    Database db;
    byte data[128];
    byte found[128];
    int32 id;

    cleanup();
    ASSERT_TRUE(CreateDatabase(DB_TEST_NAME, 128));
    ASSERT_TRUE(OpenDatabase(DB_TEST_NAME, &db));

    memset(data, 0, 128);
    strcpy((char *)data, "Original");
    ASSERT_TRUE(AddRecord(&db, data, &id));

    memset(data, 0, 128);
    strcpy((char *)data, "Updated!");
    ASSERT_TRUE(UpdateRecord(&db, id, data));

    memset(found, 0, 128);
    ASSERT_TRUE(FindRecordByID(&db, id, found));
    ASSERT_TRUE(strcmp((char *)found, "Updated!") == 0);

    CloseDatabase(&db);
    cleanup();
    return 1;
}

int test_db_delete(void) {
    Database db;
    byte data[128];
    byte found[128];
    int32 id1, id2;

    cleanup();
    ASSERT_TRUE(CreateDatabase(DB_TEST_NAME, 128));
    ASSERT_TRUE(OpenDatabase(DB_TEST_NAME, &db));

    memset(data, 0, 128);
    strcpy((char *)data, "Keep");
    ASSERT_TRUE(AddRecord(&db, data, &id1));

    memset(data, 0, 128);
    strcpy((char *)data, "Delete");
    ASSERT_TRUE(AddRecord(&db, data, &id2));

    ASSERT_TRUE(DeleteRecord(&db, id2));
    ASSERT_EQ(1, db.header.record_count);

    ASSERT_TRUE(!FindRecordByID(&db, id2, found));
    ASSERT_TRUE(FindRecordByID(&db, id1, found));

    CloseDatabase(&db);
    cleanup();
    return 1;
}

int test_db_persistence(void) {
    Database db;
    byte data[128];
    byte found[128];
    int32 id;

    cleanup();
    ASSERT_TRUE(CreateDatabase(DB_TEST_NAME, 128));
    ASSERT_TRUE(OpenDatabase(DB_TEST_NAME, &db));

    memset(data, 0, 128);
    strcpy((char *)data, "Persistent");
    ASSERT_TRUE(AddRecord(&db, data, &id));
    CloseDatabase(&db);

    /* Reopen */
    ASSERT_TRUE(OpenDatabase(DB_TEST_NAME, &db));
    memset(found, 0, 128);
    ASSERT_TRUE(FindRecordByID(&db, id, found));
    ASSERT_TRUE(strcmp((char *)found, "Persistent") == 0);

    CloseDatabase(&db);
    cleanup();
    return 1;
}

int test_db_multi_page_record(void) {
    Database db;
    byte *data;
    byte *found;
    int32 id;
    int i;
    uint16 big_size = 1024;

    cleanup();
    ASSERT_TRUE(CreateDatabase(DB_TEST_NAME, big_size));
    ASSERT_TRUE(OpenDatabase(DB_TEST_NAME, &db));

    data = (byte *)malloc(big_size);
    found = (byte *)malloc(big_size);
    if (!data || !found) {
        if (data) free(data);
        if (found) free(found);
        return 0;
    }

    /* Fill with pattern */
    for (i = 0; i < big_size; i++)
        data[i] = (byte)(i & 0xFF);

    ASSERT_TRUE(AddRecord(&db, data, &id));

    memset(found, 0, big_size);
    ASSERT_TRUE(FindRecordByID(&db, id, found));
    ASSERT_TRUE(memcmp(data, found, big_size) == 0);

    free(data);
    free(found);
    CloseDatabase(&db);
    cleanup();
    return 1;
}

int test_db_validate(void) {
    Database db;
    byte data[128];
    int32 id;

    cleanup();
    ASSERT_TRUE(CreateDatabase(DB_TEST_NAME, 128));
    ASSERT_TRUE(OpenDatabase(DB_TEST_NAME, &db));

    memset(data, 0, 128);
    strcpy((char *)data, "Test");
    ASSERT_TRUE(AddRecord(&db, data, &id));

    ASSERT_TRUE(ValidateDatabase(&db));

    CloseDatabase(&db);
    cleanup();
    return 1;
}

int test_db_compact(void) {
    Database db;
    byte data[128];
    byte found[128];
    int32 id1, id2, id3;

    cleanup();
    ASSERT_TRUE(CreateDatabase(DB_TEST_NAME, 128));
    ASSERT_TRUE(OpenDatabase(DB_TEST_NAME, &db));

    memset(data, 0, 128);
    strcpy((char *)data, "First");
    ASSERT_TRUE(AddRecord(&db, data, &id1));

    memset(data, 0, 128);
    strcpy((char *)data, "Second");
    ASSERT_TRUE(AddRecord(&db, data, &id2));

    memset(data, 0, 128);
    strcpy((char *)data, "Third");
    ASSERT_TRUE(AddRecord(&db, data, &id3));

    /* Delete middle record */
    ASSERT_TRUE(DeleteRecord(&db, id2));

    /* Compact */
    ASSERT_TRUE(CompactDatabase(&db));

    /* Verify remaining records */
    memset(found, 0, 128);
    ASSERT_TRUE(FindRecordByID(&db, id1, found));
    ASSERT_TRUE(strcmp((char *)found, "First") == 0);

    memset(found, 0, 128);
    ASSERT_TRUE(FindRecordByID(&db, id3, found));
    ASSERT_TRUE(strcmp((char *)found, "Third") == 0);

    ASSERT_EQ(2, db.header.record_count);
    ASSERT_EQ(0, db.free_list.free_page_count);

    CloseDatabase(&db);
    cleanup();
    return 1;
}

int test_db_add_index(void) {
    Database db;
    byte data[128];
    int32 id;

    cleanup();
    ASSERT_TRUE(CreateDatabase(DB_TEST_NAME, 128));
    ASSERT_TRUE(OpenDatabase(DB_TEST_NAME, &db));

    memset(data, 0, 128);
    strcpy((char *)data, "Test");
    ASSERT_TRUE(AddRecord(&db, data, &id));

    ASSERT_TRUE(AddIndex(&db, "username", IT_STRING));
    ASSERT_EQ(1, db.header.index_count);

    CloseDatabase(&db);

    /* Clean up secondary index file too */
    remove("tmp/testdb.i00");
    cleanup();
    return 1;
}

int main(void) {
    system("mkdir -p tmp");

    printf("=== Database Tests ===\n");

    RUN_TEST(test_type_sizes);
    RUN_TEST(test_db_header_roundtrip);
    RUN_TEST(test_db_create);
    RUN_TEST(test_db_open_close);
    RUN_TEST(test_db_add_find);
    RUN_TEST(test_db_multiple_records);
    RUN_TEST(test_db_update);
    RUN_TEST(test_db_delete);
    RUN_TEST(test_db_persistence);
    RUN_TEST(test_db_multi_page_record);
    RUN_TEST(test_db_validate);
    RUN_TEST(test_db_compact);
    RUN_TEST(test_db_add_index);

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);

    cleanup();
    return (tests_passed == tests_run) ? 0 : 1;
}
