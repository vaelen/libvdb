/* Copyright 2026, Andrew C. Young <andrew@vaelen.org> */
/* SPDX-License-Identifier: MIT */

/*
 * search.c - Search operations for useradm
 *
 * Provides functions to find users by username, email, or ID,
 * and to list all users with pagination.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "useradm.h"

/* ---- Search by Username ---- */

/*
 * FindUserByUsername - Look up a user by username via secondary index
 */
bool FindUserByUsername(Database *db, const char *username,
                        UserRecord *user, int32 *record_id)
{
    byte data[USER_RECORD_SIZE];

    if (db == NULL || username == NULL || user == NULL || record_id == NULL) {
        return FALSE;
    }

    if (!FindRecordByString(db, "Username", username, data, record_id)) {
        return FALSE;
    }

    DeserializeUser(data, user);
    return TRUE;
}

/* ---- Search by Email ---- */

/*
 * FindUserByEmail - Look up a user by email via secondary index
 */
bool FindUserByEmail(Database *db, const char *email,
                     UserRecord *user, int32 *record_id)
{
    byte data[USER_RECORD_SIZE];

    if (db == NULL || email == NULL || user == NULL || record_id == NULL) {
        return FALSE;
    }

    if (!FindRecordByString(db, "Email", email, data, record_id)) {
        return FALSE;
    }

    DeserializeUser(data, user);
    return TRUE;
}

/* ---- Search by Record ID ---- */

/*
 * FindUserByRecordID - Look up a user by database Record ID
 */
bool FindUserByRecordID(Database *db, int32 record_id,
                        UserRecord *user)
{
    byte data[USER_RECORD_SIZE];

    if (db == NULL || user == NULL) {
        return FALSE;
    }

    if (!FindRecordByID(db, record_id, data)) {
        return FALSE;
    }

    DeserializeUser(data, user);
    return TRUE;
}

/* ---- List All Users ---- */

/*
 * ListUsers - Display a paginated table of all users
 *
 * Scans all pages in the .DAT file for active records.
 * Displays USERS_PER_PAGE users at a time.
 * After each page, prompts for a user ID or Enter for next page.
 *
 * Returns the selected user's Record ID, or -1 if none selected.
 */
int32 ListUsers(Database *db)
{
    int32 total_pages;
    int32 page_num;
    int count;
    DBPage page;
    byte data[USER_RECORD_SIZE];
    UserRecord user;
    char input[32];
    uint16 pages_per_record;
    int32 sel_id;
    size_t slen;

    if (db == NULL) {
        return -1;
    }

    total_pages = GetTotalPages(db);
    pages_per_record = CalculatePagesNeeded(db->header.record_size);
    count = 0;

    printf("\n%-6s %-20s %-30s %-8s %-6s\n",
           "ID", "Username", "Email", "Access", "Locked");
    printf("------ -------------------- ");
    printf("------------------------------ ");
    printf("-------- ------\n");

    /* Scan pages starting from page 2 (skip header and free list) */
    for (page_num = 2; page_num < total_pages; page_num++) {
        if (!ReadPageFromDisk(db, page_num, &page)) {
            continue;
        }

        if (page.status != PS_ACTIVE) {
            continue;
        }

        /* Read the full record (may span multiple pages) */
        if (!FindRecordByID(db, page.id, data)) {
            continue;
        }

        DeserializeUser(data, &user);

        printf("%-6ld %-20s %-30s %-8d %-6s\n",
               (long)page.id,
               user.username,
               user.email,
               (int)user.access_level,
               user.locked ? "Yes" : "No");

        count++;

        /* Skip continuation pages for this record */
        page_num += (pages_per_record - 1);

        /* Pagination: prompt after every full page */
        if (count % USERS_PER_PAGE == 0) {
            printf("\nEnter user ID to view (or press Enter for next page): ");
            fflush(stdout);
            if (fgets(input, sizeof(input), stdin) != NULL) {
                /* Trim newline */
                slen = strlen(input);
                if (slen > 0 && input[slen - 1] == '\n') {
                    input[slen - 1] = '\0';
                }
                if (strlen(input) > 0) {
                    sel_id = (int32)atol(input);
                    if (sel_id > 0) {
                        return sel_id;
                    }
                    return -1;
                }
            }
            /* Print header again for next page */
            printf("\n%-6s %-20s %-30s %-8s %-6s\n",
                   "ID", "Username", "Email", "Access", "Locked");
            printf("------ -------------------- ");
            printf("------------------------------ ");
            printf("-------- ------\n");
        }
    }

    if (count == 0) {
        printf("No users found.\n");
    } else {
        /* Prompt after final partial page */
        printf("\n%d user(s) total.\n", count);
        printf("\nEnter user ID to view (or press Enter to return): ");
        fflush(stdout);
        if (fgets(input, sizeof(input), stdin) != NULL) {
            slen = strlen(input);
            if (slen > 0 && input[slen - 1] == '\n') {
                input[slen - 1] = '\0';
            }
            if (strlen(input) > 0) {
                sel_id = (int32)atol(input);
                if (sel_id > 0) {
                    return sel_id;
                }
            }
        }
    }

    return -1;
}
