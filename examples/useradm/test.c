/* Copyright 2026, Andrew C. Young <andrew@vaelen.org> */
/* SPDX-License-Identifier: MIT */

/*
 * tests/useradm/user.c - Tests for UserRecord helpers
 *
 * Tests serialization, deserialization, validation, password hashing,
 * and index management functions.
 */

#include <stdio.h>
#include <string.h>
#include "test.h"
#include "useradm.h"

/* ---- Test: Record Size ---- */

static void test_record_size(void)
{
    TestAssertEq(USER_RECORD_SIZE, 216, "USER_RECORD_SIZE should be 216");

    /* Verify struct can hold all data */
    TestAssertTrue(sizeof(UserRecord) >= USER_RECORD_SIZE,
                   "UserRecord struct should be at least USER_RECORD_SIZE");
}

/* ---- Test: Field Offsets ---- */

static void test_field_offsets(void)
{
    TestAssertEq(USER_OFF_USERNAME, 0,
                 "Username offset should be 0");
    TestAssertEq(USER_OFF_REALNAME, 32,
                 "Real name offset should be 32");
    TestAssertEq(USER_OFF_EMAIL, 96,
                 "Email offset should be 96");
    TestAssertEq(USER_OFF_PASSHASH, 160,
                 "Password hash offset should be 160");
    TestAssertEq(USER_OFF_CREATED, 202,
                 "Created date offset should be 202");
    TestAssertEq(USER_OFF_UPDATED, 206,
                 "Updated date offset should be 206");
    TestAssertEq(USER_OFF_LASTSEEN, 210,
                 "Last seen offset should be 210");
    TestAssertEq(USER_OFF_ACCESS, 214,
                 "Access level offset should be 214");
    TestAssertEq(USER_OFF_LOCKED, 215,
                 "Locked offset should be 215");
}

/* ---- Test: Init ---- */

static void test_init_user(void)
{
    UserRecord user;

    InitUserRecord(&user);

    TestAssertEq(0, (long)user.username[0], "Username should be empty");
    TestAssertEq(0, (long)user.real_name[0], "Real name should be empty");
    TestAssertEq(0, (long)user.email[0], "Email should be empty");
    TestAssertEq(0, (long)user.password_hash[0], "Password hash empty");
    TestAssertEq(0, (long)user.created_date, "Created date should be 0");
    TestAssertEq(0, (long)user.updated_date, "Updated date should be 0");
    TestAssertEq(0, (long)user.last_seen, "Last seen should be 0");
    TestAssertEq(0, (long)user.access_level, "Access level should be 0");
    TestAssertEq(0, (long)user.locked, "Locked should be FALSE");
}

/* ---- Test: Serialization Round-Trip ---- */

static void test_serialize_roundtrip(void)
{
    UserRecord original;
    UserRecord restored;
    byte buf[USER_RECORD_SIZE];

    InitUserRecord(&original);
    StrNCopy(original.username, "testuser", USER_USERNAME_SIZE);
    StrNCopy(original.real_name, "Test User", USER_REALNAME_SIZE);
    StrNCopy(original.email, "test@example.com", USER_EMAIL_SIZE);
    StrNCopy(original.password_hash,
             "a94a8fe5ccb19ba61c4c0873d391e987982fbbd3",
             USER_PASSHASH_SIZE);
    original.created_date = 1000000;
    original.updated_date = 2000000;
    original.last_seen = 3000000;
    original.access_level = 42;
    original.locked = TRUE;

    SerializeUser(&original, buf);
    DeserializeUser(buf, &restored);

    TestAssertStrEq("testuser", restored.username,
                    "Username round-trip");
    TestAssertStrEq("Test User", restored.real_name,
                    "Real name round-trip");
    TestAssertStrEq("test@example.com", restored.email,
                    "Email round-trip");
    TestAssertStrEq("a94a8fe5ccb19ba61c4c0873d391e987982fbbd3",
                    restored.password_hash,
                    "Password hash round-trip");
    TestAssertEq(1000000, (long)restored.created_date,
                 "Created date round-trip");
    TestAssertEq(2000000, (long)restored.updated_date,
                 "Updated date round-trip");
    TestAssertEq(3000000, (long)restored.last_seen,
                 "Last seen round-trip");
    TestAssertEq(42, (long)restored.access_level,
                 "Access level round-trip");
    TestAssertEq(TRUE, (long)restored.locked,
                 "Locked round-trip");
}

/* ---- Test: Serialization Buffer Layout ---- */

static void test_serialize_layout(void)
{
    UserRecord user;
    byte buf[USER_RECORD_SIZE];

    InitUserRecord(&user);
    StrNCopy(user.username, "admin", USER_USERNAME_SIZE);
    StrNCopy(user.email, "admin@test.com", USER_EMAIL_SIZE);
    user.access_level = 255;
    user.locked = FALSE;
    user.created_date = 0x01020304;

    SerializeUser(&user, buf);

    /* Check username starts at offset 0 */
    TestAssertEq('a', (long)buf[0], "Username byte 0 should be 'a'");
    TestAssertEq('d', (long)buf[1], "Username byte 1 should be 'd'");

    /* Check email starts at offset 96 */
    TestAssertEq('a', (long)buf[96], "Email byte 0 should be 'a'");

    /* Check access level at offset 214 */
    TestAssertEq(255, (long)buf[USER_OFF_ACCESS],
                 "Access level byte should be 255");

    /* Check locked at offset 215 */
    TestAssertEq(FALSE, (long)buf[USER_OFF_LOCKED],
                 "Locked byte should be 0");

    /* Check created_date little-endian at offset 202 */
    TestAssertEq(0x04, (long)buf[USER_OFF_CREATED],
                 "Created date byte 0 (LE)");
    TestAssertEq(0x03, (long)buf[USER_OFF_CREATED + 1],
                 "Created date byte 1 (LE)");
    TestAssertEq(0x02, (long)buf[USER_OFF_CREATED + 2],
                 "Created date byte 2 (LE)");
    TestAssertEq(0x01, (long)buf[USER_OFF_CREATED + 3],
                 "Created date byte 3 (LE)");
}

/* ---- Test: Null Safety ---- */

static void test_null_safety(void)
{
    UserRecord user;
    byte buf[USER_RECORD_SIZE];

    /* These should not crash */
    InitUserRecord(NULL);
    SerializeUser(NULL, buf);
    SerializeUser(&user, NULL);
    DeserializeUser(NULL, &user);
    DeserializeUser(buf, NULL);
    HashPassword(NULL, user.password_hash);
    HashPassword("test", NULL);
    DisplayUser(NULL, 1);

    TestAssertTrue(1, "Null safety: no crashes");
}

/* ---- Test: Password Hashing ---- */

static void test_hash_password(void)
{
    char hex[USER_PASSHASH_SIZE];

    /* SHA-1 of "test" is a94a8fe5ccb19ba61c4c0873d391e987982fbbd3 */
    HashPassword("test", hex);
    TestAssertStrEq("a94a8fe5ccb19ba61c4c0873d391e987982fbbd3", hex,
                    "SHA-1 of 'test'");

    /* SHA-1 of empty string is da39a3ee5e6b4b0d3255bfef95601890afd80709 */
    HashPassword("", hex);
    TestAssertStrEq("da39a3ee5e6b4b0d3255bfef95601890afd80709", hex,
                    "SHA-1 of empty string");
}

/* ---- Test: Username Validation ---- */

static void test_validate_username(void)
{
    TestAssertTrue(ValidateUsername("admin"),
                   "Valid: simple alpha");
    TestAssertTrue(ValidateUsername("user_123"),
                   "Valid: alphanumeric with underscore");
    TestAssertTrue(ValidateUsername("my-user"),
                   "Valid: with hyphen");
    TestAssertTrue(ValidateUsername("a"),
                   "Valid: single char");

    TestAssertTrue(!ValidateUsername(NULL),
                   "Invalid: NULL");
    TestAssertTrue(!ValidateUsername(""),
                   "Invalid: empty string");
    TestAssertTrue(!ValidateUsername("user name"),
                   "Invalid: contains space");
    TestAssertTrue(!ValidateUsername("user@name"),
                   "Invalid: contains @");
    TestAssertTrue(!ValidateUsername("user.name"),
                   "Invalid: contains dot");

    /* 31-char username should be valid */
    TestAssertTrue(ValidateUsername("abcdefghijklmnopqrstuvwxyz12345"),
                   "Valid: 31 characters");
}

/* ---- Test: Email Validation ---- */

static void test_validate_email(void)
{
    TestAssertTrue(ValidateEmail("user@example.com"),
                   "Valid: standard email");
    TestAssertTrue(ValidateEmail("a@b.c"),
                   "Valid: minimal email");
    TestAssertTrue(ValidateEmail("test.user@domain.co.uk"),
                   "Valid: multi-dot email");

    TestAssertTrue(!ValidateEmail(NULL),
                   "Invalid: NULL");
    TestAssertTrue(!ValidateEmail(""),
                   "Invalid: empty");
    TestAssertTrue(!ValidateEmail("noatsign"),
                   "Invalid: no @ sign");
    TestAssertTrue(!ValidateEmail("@domain.com"),
                   "Invalid: nothing before @");
    TestAssertTrue(!ValidateEmail("user@"),
                   "Invalid: nothing after @");
    TestAssertTrue(!ValidateEmail("user@domain"),
                   "Invalid: no dot after @");
    TestAssertTrue(!ValidateEmail("user@@domain.com"),
                   "Invalid: double @");
    TestAssertTrue(!ValidateEmail("user@domain."),
                   "Invalid: trailing dot");
}

/* ---- Test: Timestamp Formatting ---- */

static void test_format_timestamp(void)
{
    char buf[24];

    /* Test zero timestamp */
    FormatTimestamp(0, buf, sizeof(buf));
    TestAssertStrEq("(none)", buf,
                    "Zero timestamp should show (none)");

    /* Test non-zero timestamp produces a non-empty string */
    FormatTimestamp(1000000000, buf, sizeof(buf));
    TestAssertTrue(strlen(buf) > 0,
                   "Non-zero timestamp should produce output");
    TestAssertTrue(strlen(buf) == 19,
                   "Formatted timestamp should be 19 chars");

    /* Test small buffer */
    FormatTimestamp(1000000000, buf, 5);
    TestAssertTrue(1, "Small buffer should not crash");
}

/* ---- Test: Multiple Serialization Round-Trips ---- */

static void test_multiple_records(void)
{
    UserRecord users[3];
    UserRecord restored;
    byte buf[USER_RECORD_SIZE];
    int i;

    for (i = 0; i < 3; i++) {
        InitUserRecord(&users[i]);
    }

    StrNCopy(users[0].username, "alice", USER_USERNAME_SIZE);
    StrNCopy(users[0].email, "alice@test.com", USER_EMAIL_SIZE);
    users[0].access_level = 100;
    users[0].locked = FALSE;

    StrNCopy(users[1].username, "bob", USER_USERNAME_SIZE);
    StrNCopy(users[1].email, "bob@test.com", USER_EMAIL_SIZE);
    users[1].access_level = 50;
    users[1].locked = TRUE;

    StrNCopy(users[2].username, "charlie", USER_USERNAME_SIZE);
    StrNCopy(users[2].email, "charlie@test.com", USER_EMAIL_SIZE);
    users[2].access_level = 0;
    users[2].locked = FALSE;

    for (i = 0; i < 3; i++) {
        SerializeUser(&users[i], buf);
        DeserializeUser(buf, &restored);

        TestAssertStrEq(users[i].username, restored.username,
                        "Multi-record username round-trip");
        TestAssertStrEq(users[i].email, restored.email,
                        "Multi-record email round-trip");
        TestAssertEq((long)users[i].access_level,
                     (long)restored.access_level,
                     "Multi-record access_level round-trip");
        TestAssertEq((long)users[i].locked,
                     (long)restored.locked,
                     "Multi-record locked round-trip");
    }
}

/* ---- Test: Max-Length Fields ---- */

static void test_max_length_fields(void)
{
    UserRecord user;
    UserRecord restored;
    byte buf[USER_RECORD_SIZE];
    char long_name[USER_REALNAME_SIZE];

    InitUserRecord(&user);

    /* Fill real_name with max-length data */
    memset(long_name, 'x', USER_REALNAME_SIZE - 1);
    long_name[USER_REALNAME_SIZE - 1] = '\0';
    StrNCopy(user.real_name, long_name, USER_REALNAME_SIZE);

    StrNCopy(user.username, "maxuser", USER_USERNAME_SIZE);
    StrNCopy(user.email, "max@test.com", USER_EMAIL_SIZE);

    SerializeUser(&user, buf);
    DeserializeUser(buf, &restored);

    TestAssertEq((long)(USER_REALNAME_SIZE - 1),
                 (long)strlen(restored.real_name),
                 "Max-length real name preserved");
    TestAssertStrEq(long_name, restored.real_name,
                    "Max-length real name content preserved");
}

/* ---- Helpers for Search/Index Tests ---- */

static void CleanupTestFiles(void)
{
    remove("_TUSRR.DAT");
    remove("_TUSRR.IDX");
    remove("_TUSRR.JNL");
    remove("_TUSRR.I00");
    remove("_TUSRR.I01");
    remove("_TUSRR.TMP");
}

/*
 * SetupTestDB - Create a test database with username and email indexes
 *
 * Returns TRUE on success. Caller must close db and BTrees when done.
 */
static bool SetupTestDB(Database *db, BTree *uname_idx, BTree *email_idx)
{
    CleanupTestFiles();

    if (!CreateDatabase("_TUSRR", USER_RECORD_SIZE)) {
        return FALSE;
    }
    if (!OpenDatabase("_TUSRR", db)) {
        return FALSE;
    }
    if (!AddIndex(db, "Username", IT_STRING)) {
        CloseDatabase(db);
        return FALSE;
    }
    if (!AddIndex(db, "Email", IT_STRING)) {
        CloseDatabase(db);
        return FALSE;
    }
    if (!OpenBTree(uname_idx, "_TUSRR.I00")) {
        CloseDatabase(db);
        return FALSE;
    }
    if (!OpenBTree(email_idx, "_TUSRR.I01")) {
        CloseBTree(uname_idx);
        CloseDatabase(db);
        return FALSE;
    }
    return TRUE;
}

static void TeardownTestDB(Database *db, BTree *uname_idx, BTree *email_idx)
{
    CloseBTree(email_idx);
    CloseBTree(uname_idx);
    CloseDatabase(db);
    CleanupTestFiles();
}

/* ---- Test: FindUserByUsername ---- */

static void test_find_by_username(void)
{
    Database db;
    BTree uname_idx;
    BTree email_idx;
    UserRecord user;
    UserRecord found;
    byte data[USER_RECORD_SIZE];
    int32 record_id;
    int32 found_id;

    if (!SetupTestDB(&db, &uname_idx, &email_idx)) {
        TestAssertTrue(0, "SetupTestDB failed");
        return;
    }

    /* Create and insert a user */
    InitUserRecord(&user);
    StrNCopy(user.username, "alice", USER_USERNAME_SIZE);
    StrNCopy(user.real_name, "Alice Smith", USER_REALNAME_SIZE);
    StrNCopy(user.email, "alice@test.com", USER_EMAIL_SIZE);
    HashPassword("secret", user.password_hash);
    user.access_level = 10;
    user.locked = FALSE;

    SerializeUser(&user, data);
    TestAssertTrue(AddRecord(&db, data, &record_id),
                   "AddRecord succeeds");
    TestAssertTrue(AddUserToIndexes(&uname_idx, &email_idx, &user, record_id),
                   "AddUserToIndexes succeeds");

    /* Find by username */
    TestAssertTrue(FindUserByUsername(&db, "alice", &found, &found_id),
                   "FindUserByUsername finds alice");
    TestAssertEq((long)record_id, (long)found_id,
                 "Found record ID matches");
    TestAssertStrEq("alice", found.username,
                    "Found username matches");
    TestAssertStrEq("Alice Smith", found.real_name,
                    "Found real name matches");

    /* Search for non-existent username */
    TestAssertTrue(!FindUserByUsername(&db, "bob", &found, &found_id),
                   "FindUserByUsername returns FALSE for unknown user");

    TeardownTestDB(&db, &uname_idx, &email_idx);
}

/* ---- Test: FindUserByEmail ---- */

static void test_find_by_email(void)
{
    Database db;
    BTree uname_idx;
    BTree email_idx;
    UserRecord user;
    UserRecord found;
    byte data[USER_RECORD_SIZE];
    int32 record_id;
    int32 found_id;

    if (!SetupTestDB(&db, &uname_idx, &email_idx)) {
        TestAssertTrue(0, "SetupTestDB failed");
        return;
    }

    InitUserRecord(&user);
    StrNCopy(user.username, "bob", USER_USERNAME_SIZE);
    StrNCopy(user.real_name, "Bob Jones", USER_REALNAME_SIZE);
    StrNCopy(user.email, "bob@test.com", USER_EMAIL_SIZE);
    user.access_level = 20;

    SerializeUser(&user, data);
    TestAssertTrue(AddRecord(&db, data, &record_id),
                   "AddRecord succeeds");
    TestAssertTrue(AddUserToIndexes(&uname_idx, &email_idx, &user, record_id),
                   "AddUserToIndexes succeeds");

    /* Find by email */
    TestAssertTrue(FindUserByEmail(&db, "bob@test.com", &found, &found_id),
                   "FindUserByEmail finds bob");
    TestAssertEq((long)record_id, (long)found_id,
                 "Found record ID matches");
    TestAssertStrEq("bob@test.com", found.email,
                    "Found email matches");

    /* Search for non-existent email */
    TestAssertTrue(!FindUserByEmail(&db, "nobody@test.com", &found, &found_id),
                   "FindUserByEmail returns FALSE for unknown email");

    TeardownTestDB(&db, &uname_idx, &email_idx);
}

/* ---- Test: FindUserByRecordID ---- */

static void test_find_by_record_id(void)
{
    Database db;
    BTree uname_idx;
    BTree email_idx;
    UserRecord user;
    UserRecord found;
    byte data[USER_RECORD_SIZE];
    int32 record_id;

    if (!SetupTestDB(&db, &uname_idx, &email_idx)) {
        TestAssertTrue(0, "SetupTestDB failed");
        return;
    }

    InitUserRecord(&user);
    StrNCopy(user.username, "charlie", USER_USERNAME_SIZE);
    StrNCopy(user.real_name, "Charlie Brown", USER_REALNAME_SIZE);
    StrNCopy(user.email, "charlie@test.com", USER_EMAIL_SIZE);
    user.access_level = 5;
    user.locked = TRUE;

    SerializeUser(&user, data);
    TestAssertTrue(AddRecord(&db, data, &record_id),
                   "AddRecord succeeds");

    /* Find by record ID */
    TestAssertTrue(FindUserByRecordID(&db, record_id, &found),
                   "FindUserByRecordID finds the record");
    TestAssertStrEq("charlie", found.username,
                    "Found username matches");
    TestAssertEq(TRUE, (long)found.locked,
                 "Found locked flag matches");

    /* Search for non-existent ID */
    TestAssertTrue(!FindUserByRecordID(&db, 9999, &found),
                   "FindUserByRecordID returns FALSE for bad ID");

    TeardownTestDB(&db, &uname_idx, &email_idx);
}

/* ---- Test: AddUserToIndexes ---- */

static void test_add_to_indexes(void)
{
    Database db;
    BTree uname_idx;
    BTree email_idx;
    UserRecord user;
    UserRecord found;
    byte data[USER_RECORD_SIZE];
    int32 record_id;
    int32 found_id;

    if (!SetupTestDB(&db, &uname_idx, &email_idx)) {
        TestAssertTrue(0, "SetupTestDB failed");
        return;
    }

    /* Add two users and verify both are findable */
    InitUserRecord(&user);
    StrNCopy(user.username, "dave", USER_USERNAME_SIZE);
    StrNCopy(user.email, "dave@test.com", USER_EMAIL_SIZE);
    SerializeUser(&user, data);
    TestAssertTrue(AddRecord(&db, data, &record_id), "AddRecord user1");
    TestAssertTrue(AddUserToIndexes(&uname_idx, &email_idx, &user, record_id),
                   "AddUserToIndexes user1");

    InitUserRecord(&user);
    StrNCopy(user.username, "eve", USER_USERNAME_SIZE);
    StrNCopy(user.email, "eve@test.com", USER_EMAIL_SIZE);
    SerializeUser(&user, data);
    TestAssertTrue(AddRecord(&db, data, &record_id), "AddRecord user2");
    TestAssertTrue(AddUserToIndexes(&uname_idx, &email_idx, &user, record_id),
                   "AddUserToIndexes user2");

    /* Both should be findable */
    TestAssertTrue(FindUserByUsername(&db, "dave", &found, &found_id),
                   "dave findable by username");
    TestAssertTrue(FindUserByEmail(&db, "eve@test.com", &found, &found_id),
                   "eve findable by email");

    /* Null safety */
    TestAssertTrue(!AddUserToIndexes(NULL, &email_idx, &user, 1),
                   "AddUserToIndexes NULL username_idx");
    TestAssertTrue(!AddUserToIndexes(&uname_idx, NULL, &user, 1),
                   "AddUserToIndexes NULL email_idx");
    TestAssertTrue(!AddUserToIndexes(&uname_idx, &email_idx, NULL, 1),
                   "AddUserToIndexes NULL user");

    TeardownTestDB(&db, &uname_idx, &email_idx);
}

/* ---- Test: RemoveUserFromIndexes ---- */

static void test_remove_from_indexes(void)
{
    Database db;
    BTree uname_idx;
    BTree email_idx;
    UserRecord user;
    UserRecord found;
    byte data[USER_RECORD_SIZE];
    int32 record_id;
    int32 found_id;

    if (!SetupTestDB(&db, &uname_idx, &email_idx)) {
        TestAssertTrue(0, "SetupTestDB failed");
        return;
    }

    /* Create and index a user */
    InitUserRecord(&user);
    StrNCopy(user.username, "frank", USER_USERNAME_SIZE);
    StrNCopy(user.email, "frank@test.com", USER_EMAIL_SIZE);
    SerializeUser(&user, data);
    TestAssertTrue(AddRecord(&db, data, &record_id), "AddRecord");
    TestAssertTrue(AddUserToIndexes(&uname_idx, &email_idx, &user, record_id),
                   "AddUserToIndexes");

    /* Verify findable before removal */
    TestAssertTrue(FindUserByUsername(&db, "frank", &found, &found_id),
                   "frank findable before removal");
    TestAssertTrue(FindUserByEmail(&db, "frank@test.com", &found, &found_id),
                   "frank email findable before removal");

    /* Remove from indexes */
    TestAssertTrue(RemoveUserFromIndexes(&uname_idx, &email_idx,
                                         &user, record_id),
                   "RemoveUserFromIndexes succeeds");

    /* No longer findable by username or email */
    TestAssertTrue(!FindUserByUsername(&db, "frank", &found, &found_id),
                   "frank not findable by username after removal");
    TestAssertTrue(!FindUserByEmail(&db, "frank@test.com", &found, &found_id),
                   "frank not findable by email after removal");

    /* Record still exists by ID (indexes removed, not the record) */
    TestAssertTrue(FindUserByRecordID(&db, record_id, &found),
                   "frank still findable by ID after index removal");

    /* Null safety */
    TestAssertTrue(!RemoveUserFromIndexes(NULL, &email_idx, &user, 1),
                   "RemoveUserFromIndexes NULL username_idx");
    TestAssertTrue(!RemoveUserFromIndexes(&uname_idx, NULL, &user, 1),
                   "RemoveUserFromIndexes NULL email_idx");
    TestAssertTrue(!RemoveUserFromIndexes(&uname_idx, &email_idx, NULL, 1),
                   "RemoveUserFromIndexes NULL user");

    TeardownTestDB(&db, &uname_idx, &email_idx);
}

/* ---- Test: Search Null Safety ---- */

static void test_search_null_safety(void)
{
    UserRecord user;
    int32 found_id;

    /* All search functions should handle NULL gracefully */
    TestAssertTrue(!FindUserByUsername(NULL, "test", &user, &found_id),
                   "FindUserByUsername NULL db");
    TestAssertTrue(!FindUserByUsername((Database *)1, NULL, &user, &found_id),
                   "FindUserByUsername NULL username");
    TestAssertTrue(!FindUserByEmail(NULL, "test@t.com", &user, &found_id),
                   "FindUserByEmail NULL db");
    TestAssertTrue(!FindUserByEmail((Database *)1, NULL, &user, &found_id),
                   "FindUserByEmail NULL email");
    TestAssertTrue(!FindUserByRecordID(NULL, 1, &user),
                   "FindUserByRecordID NULL db");
    TestAssertTrue(!FindUserByRecordID((Database *)1, 1, NULL),
                   "FindUserByRecordID NULL user");

    TestAssertTrue(1, "Search null safety: no crashes");
}

/* ---- Main ---- */

int main(void)
{
    TestInit("UserRecord Tests");

    TestAdd("Record size", test_record_size);
    TestAdd("Field offsets", test_field_offsets);
    TestAdd("Init user record", test_init_user);
    TestAdd("Serialization round-trip", test_serialize_roundtrip);
    TestAdd("Serialization buffer layout", test_serialize_layout);
    TestAdd("Null safety", test_null_safety);
    TestAdd("Password hashing", test_hash_password);
    TestAdd("Username validation", test_validate_username);
    TestAdd("Email validation", test_validate_email);
    TestAdd("Timestamp formatting", test_format_timestamp);
    TestAdd("Multiple records", test_multiple_records);
    TestAdd("Max-length fields", test_max_length_fields);
    TestAdd("Find by username", test_find_by_username);
    TestAdd("Find by email", test_find_by_email);
    TestAdd("Find by record ID", test_find_by_record_id);
    TestAdd("Add to indexes", test_add_to_indexes);
    TestAdd("Remove from indexes", test_remove_from_indexes);
    TestAdd("Search null safety", test_search_null_safety);

    return TestRun();
}
