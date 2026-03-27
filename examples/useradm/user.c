/* Copyright 2026, Andrew C. Young <andrew@vaelen.org> */
/* SPDX-License-Identifier: MIT */

/*
 * user.c - User record helpers for useradm
 *
 * Provides serialization, deserialization, display, validation,
 * and index management functions for UserRecord structures.
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "useradm.h"

/* ---- Initialization ---- */

/*
 * InitUserRecord - Zero all fields in a UserRecord
 */
void InitUserRecord(UserRecord *user)
{
    if (user == NULL) {
        return;
    }
    memset(user, 0, sizeof(UserRecord));
}

/* ---- Serialization ---- */

/*
 * SerializeUser - Write a UserRecord to a byte buffer
 *
 * Layout (USER_RECORD_SIZE = 216 bytes):
 *   Offset   0: username      (32 bytes)
 *   Offset  32: real_name     (64 bytes)
 *   Offset  96: email         (64 bytes)
 *   Offset 160: password_hash (41 bytes)
 *   Offset 201: pad           (1 byte)
 *   Offset 202: created_date  (4 bytes, little-endian)
 *   Offset 206: updated_date  (4 bytes, little-endian)
 *   Offset 210: last_seen     (4 bytes, little-endian)
 *   Offset 214: access_level  (1 byte)
 *   Offset 215: locked        (1 byte)
 */
void SerializeUser(const UserRecord *user, byte *buf)
{
    if (user == NULL || buf == NULL) {
        return;
    }

    memset(buf, 0, USER_RECORD_SIZE);

    /* String fields */
    memcpy(buf + USER_OFF_USERNAME, user->username, USER_USERNAME_SIZE);
    memcpy(buf + USER_OFF_REALNAME, user->real_name, USER_REALNAME_SIZE);
    memcpy(buf + USER_OFF_EMAIL, user->email, USER_EMAIL_SIZE);
    memcpy(buf + USER_OFF_PASSHASH, user->password_hash, USER_PASSHASH_SIZE);

    /* Padding byte */
    buf[USER_OFF_PASSHASH + USER_PASSHASH_SIZE] = 0;

    /* Integer fields (little-endian) */
    WriteInt32LE(buf + USER_OFF_CREATED, user->created_date);
    WriteInt32LE(buf + USER_OFF_UPDATED, user->updated_date);
    WriteInt32LE(buf + USER_OFF_LASTSEEN, user->last_seen);

    /* Byte fields */
    buf[USER_OFF_ACCESS] = user->access_level;
    buf[USER_OFF_LOCKED] = user->locked;
}

/*
 * DeserializeUser - Read a UserRecord from a byte buffer
 */
void DeserializeUser(const byte *buf, UserRecord *user)
{
    if (buf == NULL || user == NULL) {
        return;
    }

    memset(user, 0, sizeof(UserRecord));

    /* String fields */
    memcpy(user->username, buf + USER_OFF_USERNAME, USER_USERNAME_SIZE);
    user->username[USER_USERNAME_SIZE - 1] = '\0';

    memcpy(user->real_name, buf + USER_OFF_REALNAME, USER_REALNAME_SIZE);
    user->real_name[USER_REALNAME_SIZE - 1] = '\0';

    memcpy(user->email, buf + USER_OFF_EMAIL, USER_EMAIL_SIZE);
    user->email[USER_EMAIL_SIZE - 1] = '\0';

    memcpy(user->password_hash, buf + USER_OFF_PASSHASH, USER_PASSHASH_SIZE);
    user->password_hash[USER_PASSHASH_SIZE - 1] = '\0';

    /* Integer fields (little-endian) */
    user->created_date = ReadInt32LE(buf + USER_OFF_CREATED);
    user->updated_date = ReadInt32LE(buf + USER_OFF_UPDATED);
    user->last_seen = ReadInt32LE(buf + USER_OFF_LASTSEEN);

    /* Byte fields */
    user->access_level = buf[USER_OFF_ACCESS];
    user->locked = buf[USER_OFF_LOCKED];
}

/* ---- Password Hashing ---- */

/*
 * HashPassword - Compute SHA-1 hex digest of a password
 */
void HashPassword(const char *password, char *hex_out)
{
    byte digest[SHA1_DIGEST_SIZE];

    if (password == NULL || hex_out == NULL) {
        return;
    }

    Sha1HashString(password, digest);
    Sha1ToHex(digest, hex_out);
}

/* ---- Display ---- */

/*
 * FormatTimestamp - Format a Unix timestamp as YYYY-MM-DD HH:MM:SS
 */
void FormatTimestamp(int32 timestamp, char *buf, size_t buf_size)
{
    time_t t;
    struct tm *tm_info;

    if (buf == NULL || buf_size == 0) {
        return;
    }

    if (timestamp == 0) {
        StrNCopy(buf, "(none)", buf_size);
        return;
    }

    t = (time_t)timestamp;
    tm_info = localtime(&t);
    if (tm_info == NULL) {
        StrNCopy(buf, "(invalid)", buf_size);
        return;
    }

    /* Manual formatting for C89 compatibility (avoid strftime %F %T) */
    if (buf_size >= 20) {
        sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d",
                tm_info->tm_year + 1900,
                tm_info->tm_mon + 1,
                tm_info->tm_mday,
                tm_info->tm_hour,
                tm_info->tm_min,
                tm_info->tm_sec);
    } else {
        StrNCopy(buf, "?", buf_size);
    }
}

/*
 * DisplayUser - Print a user record in human-readable format
 */
void DisplayUser(const UserRecord *user, int32 record_id)
{
    char date_buf[24];

    if (user == NULL) {
        return;
    }

    printf("\n--- User Record (ID: %ld) ---\n", (long)record_id);
    printf("Username:     %s\n", user->username);
    printf("Real Name:    %s\n", user->real_name);
    printf("Email:        %s\n", user->email);
    printf("Access Level: %d\n", (int)user->access_level);
    printf("Locked:       %s\n", user->locked ? "Yes" : "No");

    FormatTimestamp(user->created_date, date_buf, sizeof(date_buf));
    printf("Created:      %s\n", date_buf);

    FormatTimestamp(user->updated_date, date_buf, sizeof(date_buf));
    printf("Updated:      %s\n", date_buf);

    FormatTimestamp(user->last_seen, date_buf, sizeof(date_buf));
    printf("Last Seen:    %s\n", date_buf);
}

/* ---- Validation ---- */

/*
 * ValidateUsername - Check username validity
 *
 * Rules: 1-31 chars, alphanumeric, underscore, or hyphen only.
 */
bool ValidateUsername(const char *username)
{
    size_t len;
    size_t i;

    if (username == NULL) {
        return FALSE;
    }

    len = strlen(username);
    if (len == 0 || len >= USER_USERNAME_SIZE) {
        return FALSE;
    }

    for (i = 0; i < len; i++) {
        char c = username[i];
        if ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '_' || c == '-') {
            continue;
        }
        return FALSE;
    }

    return TRUE;
}

/*
 * ValidateEmail - Basic email validation
 *
 * Checks for: at least one char, '@', at least one char, '.', at least one char.
 */
bool ValidateEmail(const char *email)
{
    size_t len;
    size_t i;
    int at_pos;
    int dot_pos;

    if (email == NULL) {
        return FALSE;
    }

    len = strlen(email);
    if (len < 5 || len >= USER_EMAIL_SIZE) {
        return FALSE;
    }

    at_pos = -1;
    dot_pos = -1;

    for (i = 0; i < len; i++) {
        if (email[i] == '@') {
            if (at_pos >= 0) {
                return FALSE;  /* Multiple @ signs */
            }
            at_pos = (int)i;
        }
        if (email[i] == '.' && at_pos >= 0) {
            dot_pos = (int)i;
        }
    }

    /* '@' must exist, not be first, have chars after it, and a '.' after '@' */
    if (at_pos < 1) {
        return FALSE;
    }
    if (dot_pos <= at_pos + 1) {
        return FALSE;
    }
    if (dot_pos >= (int)(len - 1)) {
        return FALSE;
    }

    return TRUE;
}

/* ---- Index Management ---- */

/*
 * AddUserToIndexes - Insert username and email keys into secondary indexes
 */
bool AddUserToIndexes(BTree *username_idx, BTree *email_idx,
                      const UserRecord *user, int32 record_id)
{
    int32 uname_key;
    int32 email_key;

    if (username_idx == NULL || email_idx == NULL || user == NULL) {
        return FALSE;
    }

    uname_key = StringKey(user->username);
    email_key = StringKey(user->email);

    if (!BTreeInsert(username_idx, uname_key, record_id)) {
        return FALSE;
    }
    if (!BTreeInsert(email_idx, email_key, record_id)) {
        return FALSE;
    }

    return TRUE;
}

/*
 * RemoveUserFromIndexes - Remove username and email keys from secondary indexes
 */
bool RemoveUserFromIndexes(BTree *username_idx, BTree *email_idx,
                           const UserRecord *user, int32 record_id)
{
    int32 uname_key;
    int32 email_key;

    if (username_idx == NULL || email_idx == NULL || user == NULL) {
        return FALSE;
    }

    uname_key = StringKey(user->username);
    email_key = StringKey(user->email);

    BTreeDeleteValue(username_idx, uname_key, record_id);
    BTreeDeleteValue(email_idx, email_key, record_id);

    return TRUE;
}
