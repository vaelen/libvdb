/* Copyright 2026, Andrew C. Young <andrew@vaelen.org> */
/* SPDX-License-Identifier: MIT */

/*
 * useradm.c - User Database Administration Program
 *
 * Menu-driven CLI for managing a user database built on vDB.
 * Provides create, read, update, and delete operations for
 * user records with username and email secondary indexes.
 *
 * Usage: useradm
 *
 * On first run, creates USERS.DAT, USERS.IDX, USERS.I00, USERS.I01.
 * On subsequent runs, opens the existing database.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "useradm.h"

/* ---- Global State ---- */

static Database g_db;
static BTree g_username_idx;
static BTree g_email_idx;

/* ---- Forward Declarations ---- */

static bool InitDatabase(void);
static void CleanupDatabase(void);
static void ClearScreen(void);
static void ShowMainMenu(void);
static void ReadLine(char *buf, size_t buf_size);
static void TrimNewline(char *str);
static int32 GetCurrentTime(void);

static void DoFindByUsername(void);
static void DoFindByEmail(void);
static void DoFindByID(void);
static void DoListUsers(void);
static void DoCreateUser(void);
static void DoUserSubmenu(UserRecord *user, int32 record_id);
static void DoEditUser(UserRecord *user, int32 record_id);
static void DoDeleteUser(UserRecord *user, int32 record_id);

/* ---- Utility Helpers ---- */

/*
 * ClearScreen - Clear the terminal using ANSI escape codes
 */
static void ClearScreen(void)
{
    printf("\033[2J\033[H");
    fflush(stdout);
}

/*
 * ReadLine - Read a line of input, stripping the trailing newline
 */
static void ReadLine(char *buf, size_t buf_size)
{
    if (buf == NULL || buf_size == 0) {
        return;
    }
    buf[0] = '\0';
    if (fgets(buf, (int)buf_size, stdin) != NULL) {
        TrimNewline(buf);
    }
}

/*
 * TrimNewline - Remove trailing newline from a string
 */
static void TrimNewline(char *str)
{
    size_t len;
    if (str == NULL) {
        return;
    }
    len = strlen(str);
    if (len > 0 && str[len - 1] == '\n') {
        str[len - 1] = '\0';
    }
}

/*
 * GetCurrentTime - Get current Unix timestamp
 */
static int32 GetCurrentTime(void)
{
    return (int32)time(NULL);
}

/* ---- Database Initialization ---- */

/*
 * InitDatabase - Open or create the user database and indexes
 *
 * Returns TRUE on success, FALSE on failure.
 */
static bool InitDatabase(void)
{
    FILE *test_file;
    char dat_filename[16];

    BuildFilename(USER_DB_NAME, "DAT", dat_filename, sizeof(dat_filename));

    /* Check if database already exists */
    test_file = fopen(dat_filename, "rb");
    if (test_file != NULL) {
        /* Database exists, just open it */
        fclose(test_file);
    } else {
        /* First run: create the database */
        printf("Creating new user database...\n");

        if (!CreateDatabase(USER_DB_NAME, USER_RECORD_SIZE)) {
            printf("Error: Could not create database.\n");
            return FALSE;
        }

        if (!OpenDatabase(USER_DB_NAME, &g_db)) {
            printf("Error: Could not open new database.\n");
            return FALSE;
        }

        /* Add secondary indexes */
        if (!AddIndex(&g_db, "Username", IT_STRING)) {
            printf("Error: Could not create username index.\n");
            CloseDatabase(&g_db);
            return FALSE;
        }
        if (!AddIndex(&g_db, "Email", IT_STRING)) {
            printf("Error: Could not create email index.\n");
            CloseDatabase(&g_db);
            return FALSE;
        }

        CloseDatabase(&g_db);
        printf("Database created successfully.\n");
    }

    /* Open the database */
    if (!OpenDatabase(USER_DB_NAME, &g_db)) {
        printf("Error: Could not open database.\n");
        return FALSE;
    }

    /* Open secondary index BTrees */
    if (!OpenBTree(&g_username_idx, "USERS.I00")) {
        printf("Error: Could not open username index.\n");
        CloseDatabase(&g_db);
        return FALSE;
    }
    if (!OpenBTree(&g_email_idx, "USERS.I01")) {
        printf("Error: Could not open email index.\n");
        CloseBTree(&g_username_idx);
        CloseDatabase(&g_db);
        return FALSE;
    }

    return TRUE;
}

/*
 * CleanupDatabase - Close the database and all indexes
 */
static void CleanupDatabase(void)
{
    CloseBTree(&g_email_idx);
    CloseBTree(&g_username_idx);
    CloseDatabase(&g_db);
}

/* ---- Main Menu ---- */

/*
 * ShowMainMenu - Display the main menu and return
 */
static void ShowMainMenu(void)
{
    printf("\n=== User Database Administration ===\n");
    printf("1. Find user by username\n");
    printf("2. Find user by email address\n");
    printf("3. Find user by ID\n");
    printf("4. List all users\n");
    printf("5. Create new user\n");
    printf("6. Exit\n");
    printf("\nEnter choice: ");
    fflush(stdout);
}

/* ---- Menu Actions ---- */

/*
 * DoFindByUsername - Prompt for username and look up user
 */
static void DoFindByUsername(void)
{
    char username[USER_USERNAME_SIZE];
    UserRecord user;
    int32 record_id;

    printf("\nEnter username: ");
    fflush(stdout);
    ReadLine(username, sizeof(username));

    if (strlen(username) == 0) {
        printf("No username entered.\n");
        return;
    }

    if (FindUserByUsername(&g_db, username, &user, &record_id)) {
        DisplayUser(&user, record_id);
        DoUserSubmenu(&user, record_id);
    } else {
        printf("User '%s' not found.\n", username);
    }
}

/*
 * DoFindByEmail - Prompt for email and look up user
 */
static void DoFindByEmail(void)
{
    char email[USER_EMAIL_SIZE];
    UserRecord user;
    int32 record_id;

    printf("\nEnter email address: ");
    fflush(stdout);
    ReadLine(email, sizeof(email));

    if (strlen(email) == 0) {
        printf("No email entered.\n");
        return;
    }

    if (FindUserByEmail(&g_db, email, &user, &record_id)) {
        DisplayUser(&user, record_id);
        DoUserSubmenu(&user, record_id);
    } else {
        printf("User with email '%s' not found.\n", email);
    }
}

/*
 * DoFindByID - Prompt for ID and look up user
 */
static void DoFindByID(void)
{
    char input[32];
    int32 record_id;
    UserRecord user;

    printf("\nEnter user ID: ");
    fflush(stdout);
    ReadLine(input, sizeof(input));

    if (strlen(input) == 0) {
        printf("No ID entered.\n");
        return;
    }

    record_id = (int32)atol(input);
    if (record_id <= 0) {
        printf("Invalid ID.\n");
        return;
    }

    if (FindUserByRecordID(&g_db, record_id, &user)) {
        DisplayUser(&user, record_id);
        DoUserSubmenu(&user, record_id);
    } else {
        printf("User with ID %ld not found.\n", (long)record_id);
    }
}

/*
 * DoListUsers - List all users with pagination
 *
 * If the user selects a record ID from the list, display the
 * user detail and show the edit/delete/return submenu.
 */
static void DoListUsers(void)
{
    int32 sel_id;
    UserRecord user;

    sel_id = ListUsers(&g_db);
    if (sel_id > 0) {
        if (FindUserByRecordID(&g_db, sel_id, &user)) {
            DisplayUser(&user, sel_id);
            DoUserSubmenu(&user, sel_id);
        } else {
            printf("User with ID %ld not found.\n", (long)sel_id);
        }
    }
}

/*
 * DoCreateUser - Prompt for all fields and create a new user
 */
static void DoCreateUser(void)
{
    UserRecord user;
    UserRecord existing;
    char input[128];
    char password[64];
    int32 record_id;
    int32 existing_id;
    int32 now;
    byte data[USER_RECORD_SIZE];

    InitUserRecord(&user);

    /* Username */
    printf("\nEnter username: ");
    fflush(stdout);
    ReadLine(input, sizeof(input));
    if (!ValidateUsername(input)) {
        printf("Invalid username. Use 1-31 alphanumeric, underscore, ");
        printf("or hyphen characters.\n");
        return;
    }
    StrToLower(user.username, input, USER_USERNAME_SIZE);

    /* Check uniqueness */
    if (FindUserByUsername(&g_db, user.username, &existing, &existing_id)) {
        printf("Username '%s' is already taken.\n", user.username);
        return;
    }

    /* Real Name */
    printf("Enter real name: ");
    fflush(stdout);
    ReadLine(input, sizeof(input));
    if (strlen(input) == 0) {
        printf("Real name cannot be empty.\n");
        return;
    }
    StrNCopy(user.real_name, input, USER_REALNAME_SIZE);

    /* Email */
    printf("Enter email address: ");
    fflush(stdout);
    ReadLine(input, sizeof(input));
    if (!ValidateEmail(input)) {
        printf("Invalid email address.\n");
        return;
    }
    StrToLower(user.email, input, USER_EMAIL_SIZE);

    /* Check email uniqueness */
    if (FindUserByEmail(&g_db, user.email, &existing, &existing_id)) {
        printf("Email '%s' is already in use.\n", user.email);
        return;
    }

    /* Password */
    printf("Enter password: ");
    fflush(stdout);
    ReadLine(password, sizeof(password));
    if (strlen(password) == 0) {
        printf("Password cannot be empty.\n");
        return;
    }
    HashPassword(password, user.password_hash);

    /* Access Level */
    printf("Enter access level (0-255): ");
    fflush(stdout);
    ReadLine(input, sizeof(input));
    {
        long level = atol(input);
        if (level < 0 || level > 255) {
            printf("Invalid access level.\n");
            return;
        }
        user.access_level = (byte)level;
    }

    /* Locked */
    printf("Lock account? (y/n): ");
    fflush(stdout);
    ReadLine(input, sizeof(input));
    user.locked = (input[0] == 'y' || input[0] == 'Y') ? TRUE : FALSE;

    /* Set timestamps */
    now = GetCurrentTime();
    user.created_date = now;
    user.updated_date = now;
    user.last_seen = now;

    /* Serialize and add to database */
    SerializeUser(&user, data);

    if (!AddRecord(&g_db, data, &record_id)) {
        printf("Error: Could not add record to database.\n");
        return;
    }

    /* Update secondary indexes */
    if (!AddUserToIndexes(&g_username_idx, &g_email_idx, &user, record_id)) {
        printf("Warning: Could not update indexes.\n");
    }

    printf("\nUser created successfully.\n");
    DisplayUser(&user, record_id);
}

/* ---- User Submenu ---- */

/*
 * DoUserSubmenu - Show edit/delete/return options for a displayed user
 */
static void DoUserSubmenu(UserRecord *user, int32 record_id)
{
    char input[8];

    while (1) {
        printf("\n1. Edit user\n");
        printf("2. Delete user\n");
        printf("3. Return to menu\n");
        printf("\nEnter choice: ");
        fflush(stdout);
        ReadLine(input, sizeof(input));

        if (input[0] == '1') {
            DoEditUser(user, record_id);
            /* Re-display after edit */
            if (FindUserByRecordID(&g_db, record_id, user)) {
                DisplayUser(user, record_id);
            } else {
                /* Record was somehow lost */
                printf("Record no longer exists.\n");
                return;
            }
        } else if (input[0] == '2') {
            DoDeleteUser(user, record_id);
            return;
        } else if (input[0] == '3') {
            return;
        } else {
            printf("Invalid choice.\n");
        }
    }
}

/*
 * DoEditUser - Edit a specific field of a user record
 */
static void DoEditUser(UserRecord *user, int32 record_id)
{
    char input[128];
    char old_username[USER_USERNAME_SIZE];
    char old_email[USER_EMAIL_SIZE];
    UserRecord existing;
    int32 existing_id;
    byte data[USER_RECORD_SIZE];
    int choice;

    printf("\nEdit which field?\n");
    printf("1. Username\n");
    printf("2. Real Name\n");
    printf("3. Email\n");
    printf("4. Password\n");
    printf("5. Access Level\n");
    printf("6. Locked\n");
    printf("7. Cancel\n");
    printf("\nEnter choice: ");
    fflush(stdout);
    ReadLine(input, sizeof(input));
    choice = atoi(input);

    /* Save old values for index updates */
    StrNCopy(old_username, user->username, USER_USERNAME_SIZE);
    StrNCopy(old_email, user->email, USER_EMAIL_SIZE);

    switch (choice) {
    case 1: /* Username */
        printf("Enter new username: ");
        fflush(stdout);
        ReadLine(input, sizeof(input));
        if (!ValidateUsername(input)) {
            printf("Invalid username.\n");
            return;
        }
        {
            char lower_name[USER_USERNAME_SIZE];
            StrToLower(lower_name, input, USER_USERNAME_SIZE);

            /* Check uniqueness (allow keeping same name) */
            if (StrCompareI(lower_name, old_username) != 0) {
                if (FindUserByUsername(&g_db, lower_name, &existing,
                                      &existing_id)) {
                    printf("Username '%s' is already taken.\n", lower_name);
                    return;
                }
            }
            StrNCopy(user->username, lower_name, USER_USERNAME_SIZE);
        }
        break;

    case 2: /* Real Name */
        printf("Enter new real name: ");
        fflush(stdout);
        ReadLine(input, sizeof(input));
        if (strlen(input) == 0) {
            printf("Real name cannot be empty.\n");
            return;
        }
        StrNCopy(user->real_name, input, USER_REALNAME_SIZE);
        break;

    case 3: /* Email */
        printf("Enter new email: ");
        fflush(stdout);
        ReadLine(input, sizeof(input));
        if (!ValidateEmail(input)) {
            printf("Invalid email address.\n");
            return;
        }
        {
            char lower_email[USER_EMAIL_SIZE];
            StrToLower(lower_email, input, USER_EMAIL_SIZE);

            if (StrCompareI(lower_email, old_email) != 0) {
                if (FindUserByEmail(&g_db, lower_email, &existing,
                                    &existing_id)) {
                    printf("Email '%s' is already in use.\n", lower_email);
                    return;
                }
            }
            StrNCopy(user->email, lower_email, USER_EMAIL_SIZE);
        }
        break;

    case 4: /* Password */
        printf("Enter new password: ");
        fflush(stdout);
        ReadLine(input, sizeof(input));
        if (strlen(input) == 0) {
            printf("Password cannot be empty.\n");
            return;
        }
        HashPassword(input, user->password_hash);
        break;

    case 5: /* Access Level */
        printf("Enter new access level (0-255): ");
        fflush(stdout);
        ReadLine(input, sizeof(input));
        {
            long level = atol(input);
            if (level < 0 || level > 255) {
                printf("Invalid access level.\n");
                return;
            }
            user->access_level = (byte)level;
        }
        break;

    case 6: /* Locked */
        printf("Lock account? (y/n): ");
        fflush(stdout);
        ReadLine(input, sizeof(input));
        user->locked = (input[0] == 'y' || input[0] == 'Y') ? TRUE : FALSE;
        break;

    case 7: /* Cancel */
        return;

    default:
        printf("Invalid choice.\n");
        return;
    }

    /* Update timestamp */
    user->updated_date = GetCurrentTime();

    /* Update secondary indexes if username or email changed */
    if (StrCompareI(user->username, old_username) != 0) {
        /* Remove old username key, add new one */
        BTreeDeleteValue(&g_username_idx,
                         StringKey(old_username), record_id);
        BTreeInsert(&g_username_idx,
                    StringKey(user->username), record_id);
    }
    if (StrCompareI(user->email, old_email) != 0) {
        /* Remove old email key, add new one */
        BTreeDeleteValue(&g_email_idx,
                         StringKey(old_email), record_id);
        BTreeInsert(&g_email_idx,
                    StringKey(user->email), record_id);
    }

    /* Serialize and update record */
    SerializeUser(user, data);
    if (!UpdateRecord(&g_db, record_id, data)) {
        printf("Error: Could not update record.\n");
        return;
    }

    printf("User updated successfully.\n");
}

/*
 * DoDeleteUser - Confirm and delete a user record
 */
static void DoDeleteUser(UserRecord *user, int32 record_id)
{
    char input[8];

    printf("\nAre you sure you want to delete user '%s'? (y/n): ",
           user->username);
    fflush(stdout);
    ReadLine(input, sizeof(input));

    if (input[0] != 'y' && input[0] != 'Y') {
        printf("Delete cancelled.\n");
        return;
    }

    /* Remove from secondary indexes */
    RemoveUserFromIndexes(&g_username_idx, &g_email_idx, user, record_id);

    /* Delete the record */
    if (!DeleteRecord(&g_db, record_id)) {
        printf("Error: Could not delete record.\n");
        return;
    }

    printf("User '%s' deleted.\n", user->username);
}

/* ---- Main ---- */

int main(void)
{
    char input[8];

    if (!InitDatabase()) {
        return 1;
    }

    while (1) {
        ClearScreen();
        ShowMainMenu();
        ReadLine(input, sizeof(input));

        switch (input[0]) {
        case '1':
            DoFindByUsername();
            break;
        case '2':
            DoFindByEmail();
            break;
        case '3':
            DoFindByID();
            break;
        case '4':
            DoListUsers();
            break;
        case '5':
            DoCreateUser();
            break;
        case '6':
            CleanupDatabase();
            printf("Goodbye.\n");
            return 0;
        default:
            printf("Invalid choice. Please enter 1-6.\n");
            break;
        }
    }

    /* Not reached */
    CleanupDatabase();
    return 0;
}
