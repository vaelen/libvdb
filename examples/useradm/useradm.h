/* Copyright 2026, Andrew C. Young <andrew@vaelen.org> */
/* SPDX-License-Identifier: MIT */

/*
 * useradm.h - User Database Administration for vDB
 *
 * Defines the UserRecord structure and helper functions for
 * the menu-driven user database administration program.
 *
 * The user database stores records in USERS.DAT with two
 * secondary indexes: username (USERS.I00) and email (USERS.I01).
 *
 * Depends on: db, btree, crc (SHA-1), util
 */

#ifndef USERADM_H
#define USERADM_H

#include "db.h"
#include "btree.h"
#include "crc.h"
#include "util.h"

/* ---- Constants ---- */

#define USER_DB_NAME     "USERS"

/* Field sizes (including null terminator where applicable) */
#define USER_USERNAME_SIZE    32
#define USER_REALNAME_SIZE    64
#define USER_EMAIL_SIZE       64
#define USER_PASSHASH_SIZE    41  /* 40 hex chars + null */
#define USER_PASSHASH_PAD      1  /* padding to reach even offset */

/* UserRecord total size: 32 + 64 + 64 + 41 + 1 + 4 + 4 + 4 + 1 + 1 = 216 */
#define USER_RECORD_SIZE     216

/* Field offsets within the serialized byte buffer */
#define USER_OFF_USERNAME      0
#define USER_OFF_REALNAME     32
#define USER_OFF_EMAIL        96
#define USER_OFF_PASSHASH    160
#define USER_OFF_CREATED     202  /* 160 + 41 + 1 */
#define USER_OFF_UPDATED     206
#define USER_OFF_LASTSEEN    210
#define USER_OFF_ACCESS      214
#define USER_OFF_LOCKED      215

/* List pagination */
#define USERS_PER_PAGE        10

/* ---- Data Structures ---- */

/*
 * UserRecord - In-memory representation of a user
 *
 * Fields:
 *   username      - Unique login name (case-insensitive, max 31 chars)
 *   real_name     - Display name (max 63 chars)
 *   email         - Email address (unique, case-insensitive, max 63 chars)
 *   password_hash - SHA-1 hex string of the password (40 chars + null)
 *   created_date  - Unix timestamp when the user was created
 *   updated_date  - Unix timestamp of last modification
 *   last_seen     - Unix timestamp of last login
 *   access_level  - Permission level (0-255)
 *   locked        - TRUE if the account is locked
 */
typedef struct {
    char  username[USER_USERNAME_SIZE];
    char  real_name[USER_REALNAME_SIZE];
    char  email[USER_EMAIL_SIZE];
    char  password_hash[USER_PASSHASH_SIZE];
    byte  pad;
    int32 created_date;
    int32 updated_date;
    int32 last_seen;
    byte  access_level;
    bool  locked;
} UserRecord;

/* ---- User Record Helpers (user.c) ---- */

/*
 * InitUserRecord - Initialize a UserRecord to default values
 *
 * Zeros all fields. Should be called before populating a new record.
 *
 * Parameters:
 *   user - Pointer to the UserRecord to initialize
 */
void InitUserRecord(UserRecord *user);

/*
 * SerializeUser - Serialize a UserRecord to a byte buffer
 *
 * Writes all fields in a portable format (little-endian for integers).
 * The buffer must be at least USER_RECORD_SIZE bytes.
 *
 * Parameters:
 *   user - Pointer to the UserRecord to serialize
 *   buf  - Output buffer (USER_RECORD_SIZE bytes)
 */
void SerializeUser(const UserRecord *user, byte *buf);

/*
 * DeserializeUser - Deserialize a UserRecord from a byte buffer
 *
 * Reads all fields from the portable format. The buffer must be
 * at least USER_RECORD_SIZE bytes.
 *
 * Parameters:
 *   buf  - Input buffer (USER_RECORD_SIZE bytes)
 *   user - Pointer to the UserRecord to populate
 */
void DeserializeUser(const byte *buf, UserRecord *user);

/*
 * HashPassword - Hash a password string with SHA-1
 *
 * Computes SHA-1 of the password and writes the 40-character
 * hex string (plus null terminator) to hex_out.
 *
 * Parameters:
 *   password - Plain-text password string
 *   hex_out  - Output buffer (at least 41 bytes)
 */
void HashPassword(const char *password, char *hex_out);

/*
 * DisplayUser - Print a user record to stdout
 *
 * Displays all fields in a human-readable format.
 * The record_id parameter is the database Record ID.
 *
 * Parameters:
 *   user      - Pointer to the UserRecord to display
 *   record_id - Database Record ID
 */
void DisplayUser(const UserRecord *user, int32 record_id);

/*
 * ValidateUsername - Check if a username is valid
 *
 * A valid username is 1-31 characters, containing only
 * alphanumeric characters, underscores, or hyphens.
 *
 * Parameters:
 *   username - The username to validate
 *
 * Returns:
 *   TRUE if valid, FALSE otherwise
 */
bool ValidateUsername(const char *username);

/*
 * ValidateEmail - Check if an email address has basic validity
 *
 * Checks that the string contains exactly one '@' with at least
 * one character before and after it, and a '.' after the '@'.
 *
 * Parameters:
 *   email - The email address to validate
 *
 * Returns:
 *   TRUE if valid, FALSE otherwise
 */
bool ValidateEmail(const char *email);

/*
 * FormatTimestamp - Format a Unix timestamp as a date string
 *
 * Writes a human-readable date to the output buffer.
 * Format: "YYYY-MM-DD HH:MM:SS" (19 chars + null).
 *
 * Parameters:
 *   timestamp - Unix timestamp (seconds since epoch)
 *   buf       - Output buffer (at least 20 bytes)
 *   buf_size  - Size of the output buffer
 */
void FormatTimestamp(int32 timestamp, char *buf, size_t buf_size);

/* ---- Search Operations (search.c) ---- */

/*
 * FindUserByUsername - Find a user by username
 *
 * Searches the username index and retrieves the user record.
 *
 * Parameters:
 *   db        - Open database handle
 *   username  - Username to search for (case-insensitive)
 *   user      - Output: the found user record
 *   record_id - Output: the database Record ID
 *
 * Returns:
 *   TRUE if found, FALSE otherwise
 */
bool FindUserByUsername(Database *db, const char *username,
                        UserRecord *user, int32 *record_id);

/*
 * FindUserByEmail - Find a user by email address
 *
 * Searches the email index and retrieves the user record.
 *
 * Parameters:
 *   db        - Open database handle
 *   email     - Email to search for (case-insensitive)
 *   user      - Output: the found user record
 *   record_id - Output: the database Record ID
 *
 * Returns:
 *   TRUE if found, FALSE otherwise
 */
bool FindUserByEmail(Database *db, const char *email,
                     UserRecord *user, int32 *record_id);

/*
 * FindUserByID - Find a user by Record ID
 *
 * Retrieves the user record with the given ID.
 *
 * Parameters:
 *   db        - Open database handle
 *   record_id - The Record ID to look up
 *   user      - Output: the found user record
 *
 * Returns:
 *   TRUE if found, FALSE otherwise
 */
bool FindUserByRecordID(Database *db, int32 record_id,
                        UserRecord *user);

/*
 * ListUsers - Display a paginated list of all users
 *
 * Scans the .DAT file for active records and displays them
 * in a table, USERS_PER_PAGE rows at a time. After each page,
 * prompts for a user ID or Enter for next page.
 *
 * Parameters:
 *   db - Open database handle
 *
 * Returns:
 *   The selected user's Record ID, or -1 if none selected
 */
int32 ListUsers(Database *db);

/* ---- Index Management ---- */

/*
 * AddUserToIndexes - Add a user's username and email to secondary indexes
 *
 * Parameters:
 *   username_idx - Open BTree for the username index
 *   email_idx    - Open BTree for the email index
 *   user         - The user record
 *   record_id    - The database Record ID
 *
 * Returns:
 *   TRUE on success, FALSE on failure
 */
bool AddUserToIndexes(BTree *username_idx, BTree *email_idx,
                      const UserRecord *user, int32 record_id);

/*
 * RemoveUserFromIndexes - Remove a user from secondary indexes
 *
 * Parameters:
 *   username_idx - Open BTree for the username index
 *   email_idx    - Open BTree for the email index
 *   user         - The user record to remove
 *   record_id    - The database Record ID
 *
 * Returns:
 *   TRUE on success, FALSE on failure
 */
bool RemoveUserFromIndexes(BTree *username_idx, BTree *email_idx,
                           const UserRecord *user, int32 record_id);

#endif /* USERADM_H */
