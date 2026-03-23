/*
 * usersadm - Example user database administration tool
 *
 * Demonstrates the libvdb database library with CRUD operations,
 * secondary indexes, and SHA-1 password hashing.
 *
 * Usage: usersadm <command> [args]
 *
 * Commands:
 *   init              Create a new user database
 *   list              List all users
 *   create            Create a new user (interactive)
 *   view <id|str>     View user by ID, username, or email
 *   edit <id>         Edit a user (interactive)
 *   delete <id>       Delete a user
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "vdb.h"

/* Record field sizes */
#define USERNAME_LEN     32
#define REALNAME_LEN     64
#define EMAIL_LEN        64
#define PASSWORD_MAX     128

/* Record layout offsets and total size */
#define OFF_USERNAME     0
#define OFF_REALNAME     32
#define OFF_PASSHASH     96
#define OFF_EMAIL        116
#define OFF_CREATED      180
#define OFF_UPDATED      184
#define OFF_LASTSEEN     188
#define OFF_ACCESS       192
#define OFF_LOCKED       193
#define USER_RECORD_SIZE 194

/* Secondary index numbers */
#define IDX_USERNAME     0
#define IDX_EMAIL        1

/* Database name */
#define DB_NAME          "users"

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

static void serialize_user(const byte *username, const byte *real_name,
                           const byte *password_hash, const byte *email,
                           int32 created_at, int32 updated_at,
                           int32 last_seen, byte access_level,
                           byte locked, byte *buf)
{
    memset(buf, 0, USER_RECORD_SIZE);
    memcpy(buf + OFF_USERNAME, username, USERNAME_LEN);
    memcpy(buf + OFF_REALNAME, real_name, REALNAME_LEN);
    memcpy(buf + OFF_PASSHASH, password_hash, SHA1_DIGEST_SIZE);
    memcpy(buf + OFF_EMAIL,    email, EMAIL_LEN);
    PUT_LE32(buf + OFF_CREATED,  created_at);
    PUT_LE32(buf + OFF_UPDATED,  updated_at);
    PUT_LE32(buf + OFF_LASTSEEN, last_seen);
    buf[OFF_ACCESS] = access_level;
    buf[OFF_LOCKED] = locked;
}

static void print_hex(const byte *data, int len)
{
    int i;
    for (i = 0; i < len; i++)
        printf("%02x", data[i]);
}

static void format_time(int32 ts, char *out, int out_size)
{
    time_t t = (time_t)ts;
    struct tm *tm;
    if (ts == 0) {
        strncpy(out, "(none)", (size_t)out_size);
        out[out_size - 1] = '\0';
        return;
    }
    tm = localtime(&t);
    if (tm)
        strftime(out, (size_t)out_size, "%Y-%m-%d %H:%M:%S", tm);
    else
        strncpy(out, "(invalid)", (size_t)out_size);
}

static int read_line(const char *prompt, char *buf, int buf_size)
{
    printf("%s", prompt);
    fflush(stdout);
    if (!fgets(buf, buf_size, stdin))
        return -1;
    /* Strip trailing newline */
    {
        size_t len = strlen(buf);
        if (len > 0 && buf[len - 1] == '\n')
            buf[--len] = '\0';
        if (len > 0 && buf[len - 1] == '\r')
            buf[--len] = '\0';
        return (int)len;
    }
}

static bool open_indexes(BTree *idx_username, BTree *idx_email)
{
    if (!OpenIndexFile(idx_username, DB_NAME, IDX_USERNAME))
        return false;
    if (!OpenIndexFile(idx_email, DB_NAME, IDX_EMAIL)) {
        CloseBTree(idx_username);
        return false;
    }
    return true;
}

static void close_indexes(BTree *idx_username, BTree *idx_email)
{
    CloseBTree(idx_username);
    CloseBTree(idx_email);
}

static void print_user_summary(int32 id, const byte *buf)
{
    char username[USERNAME_LEN + 1];
    char real_name[REALNAME_LEN + 1];
    char email[EMAIL_LEN + 1];
    byte locked;

    memset(username, 0, sizeof(username));
    memset(real_name, 0, sizeof(real_name));
    memset(email, 0, sizeof(email));

    memcpy(username,  buf + OFF_USERNAME, USERNAME_LEN);
    memcpy(real_name, buf + OFF_REALNAME, REALNAME_LEN);
    memcpy(email,     buf + OFF_EMAIL,    EMAIL_LEN);
    locked = buf[OFF_LOCKED];

    printf("%6d  %-20s  %-30s  %-30s  %s\n",
           (int)id, username, real_name, email,
           locked ? "LOCKED" : "");
}

static void print_user_detail(int32 id, const byte *buf)
{
    char username[USERNAME_LEN + 1];
    char real_name[REALNAME_LEN + 1];
    char email[EMAIL_LEN + 1];
    char timebuf[32];
    int32 created_at, updated_at, last_seen;
    byte access_level, locked;

    memset(username, 0, sizeof(username));
    memset(real_name, 0, sizeof(real_name));
    memset(email, 0, sizeof(email));

    memcpy(username,  buf + OFF_USERNAME, USERNAME_LEN);
    memcpy(real_name, buf + OFF_REALNAME, REALNAME_LEN);
    memcpy(email,     buf + OFF_EMAIL,    EMAIL_LEN);
    created_at   = (int32)GET_LE32(buf + OFF_CREATED);
    updated_at   = (int32)GET_LE32(buf + OFF_UPDATED);
    last_seen    = (int32)GET_LE32(buf + OFF_LASTSEEN);
    access_level = buf[OFF_ACCESS];
    locked       = buf[OFF_LOCKED];

    printf("ID:           %d\n", (int)id);
    printf("Username:     %s\n", username);
    printf("Real Name:    %s\n", real_name);
    printf("Email:        %s\n", email);
    printf("Password:     ");
    print_hex(buf + OFF_PASSHASH, SHA1_DIGEST_SIZE);
    printf("\n");
    printf("Access Level: %d\n", (int)access_level);
    printf("Locked:       %s\n", locked ? "yes" : "no");

    format_time(created_at, timebuf, sizeof(timebuf));
    printf("Created:      %s\n", timebuf);
    format_time(updated_at, timebuf, sizeof(timebuf));
    printf("Updated:      %s\n", timebuf);
    format_time(last_seen, timebuf, sizeof(timebuf));
    printf("Last Seen:    %s\n", timebuf);
}

/* ------------------------------------------------------------------ */
/* Commands                                                           */
/* ------------------------------------------------------------------ */

static int cmd_init(void)
{
    Database db;

    if (!CreateDatabase(DB_NAME, USER_RECORD_SIZE)) {
        fprintf(stderr, "Error: failed to create database\n");
        return 1;
    }

    if (!OpenDatabase(DB_NAME, &db)) {
        fprintf(stderr, "Error: failed to open database\n");
        return 1;
    }

    if (!AddIndex(&db, "username", IT_STRING)) {
        fprintf(stderr, "Error: failed to create username index\n");
        CloseDatabase(&db);
        return 1;
    }

    if (!AddIndex(&db, "email", IT_STRING)) {
        fprintf(stderr, "Error: failed to create email index\n");
        CloseDatabase(&db);
        return 1;
    }

    CloseDatabase(&db);
    printf("Database initialized.\n");
    return 0;
}

static int cmd_list(void)
{
    Database db;
    DBPage page;
    byte buf[USER_RECORD_SIZE];
    long file_size;
    int32 page_num, total_pages;
    int count = 0;

    if (!OpenDatabase(DB_NAME, &db)) {
        fprintf(stderr, "Error: failed to open database\n");
        return 1;
    }

    fseek(db.data_file, 0, SEEK_END);
    file_size = ftell(db.data_file);
    total_pages = (int32)(file_size / DB_PAGE_SIZE);

    printf("%6s  %-20s  %-30s  %-30s  %s\n",
           "ID", "Username", "Real Name", "Email", "Status");
    printf("------  --------------------  "
           "------------------------------  "
           "------------------------------  ------\n");

    for (page_num = 2; page_num < total_pages; page_num++) {
        if (!ReadPage(&db, page_num, &page))
            continue;
        if (page.status != PS_ACTIVE)
            continue;
        if (!ReadRecord(&db, page_num, buf))
            continue;
        print_user_summary(page.id, buf);
        count++;
    }

    printf("\n%d user(s) total.\n", count);

    CloseDatabase(&db);
    return 0;
}

static int cmd_create(void)
{
    Database db;
    BTree idx_username, idx_email;
    char username[USERNAME_LEN];
    char real_name[REALNAME_LEN];
    char email[EMAIL_LEN];
    char password[PASSWORD_MAX];
    char level_str[16];
    byte password_hash[SHA1_DIGEST_SIZE];
    byte buf[USER_RECORD_SIZE];
    int32 record_id, now;
    int access_level;

    if (!OpenDatabase(DB_NAME, &db)) {
        fprintf(stderr, "Error: failed to open database\n");
        return 1;
    }

    if (!open_indexes(&idx_username, &idx_email)) {
        fprintf(stderr, "Error: failed to open indexes\n");
        CloseDatabase(&db);
        return 1;
    }

    memset(username, 0, sizeof(username));
    memset(real_name, 0, sizeof(real_name));
    memset(email, 0, sizeof(email));
    memset(password, 0, sizeof(password));

    if (read_line("Username: ", username, USERNAME_LEN) <= 0) {
        fprintf(stderr, "Error: username is required\n");
        close_indexes(&idx_username, &idx_email);
        CloseDatabase(&db);
        return 1;
    }

    read_line("Real Name: ", real_name, REALNAME_LEN);

    if (read_line("Email: ", email, EMAIL_LEN) <= 0) {
        fprintf(stderr, "Error: email is required\n");
        close_indexes(&idx_username, &idx_email);
        CloseDatabase(&db);
        return 1;
    }

    if (read_line("Password: ", password, PASSWORD_MAX) <= 0) {
        fprintf(stderr, "Error: password is required\n");
        close_indexes(&idx_username, &idx_email);
        CloseDatabase(&db);
        return 1;
    }

    if (read_line("Access Level (0-255) [0]: ", level_str, sizeof(level_str)) <= 0)
        access_level = 0;
    else
        access_level = atoi(level_str);
    if (access_level < 0) access_level = 0;
    if (access_level > 255) access_level = 255;

    SHA1(password, (int16)strlen(password), password_hash);

    now = (int32)time(NULL);
    serialize_user((const byte *)username, (const byte *)real_name,
                   password_hash, (const byte *)email,
                   now, now, now, (byte)access_level, 0, buf);

    if (!AddRecord(&db, buf, &record_id)) {
        fprintf(stderr, "Error: failed to add record\n");
        close_indexes(&idx_username, &idx_email);
        CloseDatabase(&db);
        return 1;
    }

    InsertIntoIndex(&idx_username, StringKey(username), record_id);
    InsertIntoIndex(&idx_email, StringKey(email), record_id);

    close_indexes(&idx_username, &idx_email);
    CloseDatabase(&db);

    printf("User created with ID %d.\n", (int)record_id);
    return 0;
}

static int cmd_view(const char *arg)
{
    Database db;
    BTree idx_username, idx_email;
    byte buf[USER_RECORD_SIZE];
    int32 record_id;
    char *endp;
    long val;
    bool found = false;

    if (!OpenDatabase(DB_NAME, &db)) {
        fprintf(stderr, "Error: failed to open database\n");
        return 1;
    }

    val = strtol(arg, &endp, 10);
    if (*endp == '\0' && endp != arg) {
        /* Numeric ID lookup */
        record_id = (int32)val;
        if (FindRecordByID(&db, record_id, buf)) {
            found = true;
        }
    } else {
        /* String lookup via indexes */
        int32 values[10];
        int16 count;
        int i;

        if (!open_indexes(&idx_username, &idx_email)) {
            fprintf(stderr, "Error: failed to open indexes\n");
            CloseDatabase(&db);
            return 1;
        }

        /* Try username index first */
        if (FindInIndex(&idx_username, StringKey(arg), values, 10, &count)
            && count > 0) {
            for (i = 0; i < count && !found; i++) {
                if (FindRecordByID(&db, values[i], buf)) {
                    char field[USERNAME_LEN + 1];
                    memset(field, 0, sizeof(field));
                    memcpy(field, buf + OFF_USERNAME, USERNAME_LEN);
                    if (strcmp(field, arg) == 0) {
                        record_id = values[i];
                        found = true;
                    }
                }
            }
        }

        /* Try email index */
        if (!found && FindInIndex(&idx_email, StringKey(arg),
                                  values, 10, &count)
            && count > 0) {
            for (i = 0; i < count && !found; i++) {
                if (FindRecordByID(&db, values[i], buf)) {
                    char field[EMAIL_LEN + 1];
                    memset(field, 0, sizeof(field));
                    memcpy(field, buf + OFF_EMAIL, EMAIL_LEN);
                    if (strcmp(field, arg) == 0) {
                        record_id = values[i];
                        found = true;
                    }
                }
            }
        }

        close_indexes(&idx_username, &idx_email);
    }

    if (!found) {
        fprintf(stderr, "User not found: %s\n", arg);
        CloseDatabase(&db);
        return 1;
    }

    print_user_detail(record_id, buf);
    CloseDatabase(&db);
    return 0;
}

static int cmd_edit(const char *arg)
{
    Database db;
    BTree idx_username, idx_email;
    byte buf[USER_RECORD_SIZE];
    char old_username[USERNAME_LEN + 1];
    char old_email[EMAIL_LEN + 1];
    char new_val[REALNAME_LEN + 1]; /* largest field */
    char password[PASSWORD_MAX];
    char level_str[16];
    byte password_hash[SHA1_DIGEST_SIZE];
    int32 record_id, now;
    int len;
    char *endp;
    long val;

    val = strtol(arg, &endp, 10);
    if (*endp != '\0' || endp == arg) {
        fprintf(stderr, "Error: edit requires a numeric ID\n");
        return 1;
    }
    record_id = (int32)val;

    if (!OpenDatabase(DB_NAME, &db)) {
        fprintf(stderr, "Error: failed to open database\n");
        return 1;
    }

    if (!open_indexes(&idx_username, &idx_email)) {
        fprintf(stderr, "Error: failed to open indexes\n");
        CloseDatabase(&db);
        return 1;
    }

    if (!FindRecordByID(&db, record_id, buf)) {
        fprintf(stderr, "Error: user %d not found\n", (int)record_id);
        close_indexes(&idx_username, &idx_email);
        CloseDatabase(&db);
        return 1;
    }

    /* Save old indexed fields */
    memset(old_username, 0, sizeof(old_username));
    memset(old_email, 0, sizeof(old_email));
    memcpy(old_username, buf + OFF_USERNAME, USERNAME_LEN);
    memcpy(old_email, buf + OFF_EMAIL, EMAIL_LEN);

    printf("Editing user %d (press Enter to keep current value)\n\n", (int)record_id);

    /* Username */
    printf("  Current username: %s\n", old_username);
    memset(new_val, 0, sizeof(new_val));
    len = read_line("  New username: ", new_val, USERNAME_LEN);
    if (len > 0) {
        memset(buf + OFF_USERNAME, 0, USERNAME_LEN);
        memcpy(buf + OFF_USERNAME, new_val, (size_t)len);
    }

    /* Real name */
    {
        char cur[REALNAME_LEN + 1];
        memset(cur, 0, sizeof(cur));
        memcpy(cur, buf + OFF_REALNAME, REALNAME_LEN);
        printf("  Current real name: %s\n", cur);
    }
    memset(new_val, 0, sizeof(new_val));
    len = read_line("  New real name: ", new_val, REALNAME_LEN);
    if (len > 0) {
        memset(buf + OFF_REALNAME, 0, REALNAME_LEN);
        memcpy(buf + OFF_REALNAME, new_val, (size_t)len);
    }

    /* Email */
    printf("  Current email: %s\n", old_email);
    memset(new_val, 0, sizeof(new_val));
    len = read_line("  New email: ", new_val, EMAIL_LEN);
    if (len > 0) {
        memset(buf + OFF_EMAIL, 0, EMAIL_LEN);
        memcpy(buf + OFF_EMAIL, new_val, (size_t)len);
    }

    /* Password */
    memset(password, 0, sizeof(password));
    len = read_line("  New password (blank to keep): ", password, PASSWORD_MAX);
    if (len > 0) {
        SHA1(password, (int16)strlen(password), password_hash);
        memcpy(buf + OFF_PASSHASH, password_hash, SHA1_DIGEST_SIZE);
    }

    /* Access level */
    printf("  Current access level: %d\n", (int)buf[OFF_ACCESS]);
    memset(level_str, 0, sizeof(level_str));
    len = read_line("  New access level: ", level_str, sizeof(level_str));
    if (len > 0) {
        int lv = atoi(level_str);
        if (lv < 0) lv = 0;
        if (lv > 255) lv = 255;
        buf[OFF_ACCESS] = (byte)lv;
    }

    /* Locked */
    printf("  Currently locked: %s\n", buf[OFF_LOCKED] ? "yes" : "no");
    memset(new_val, 0, sizeof(new_val));
    len = read_line("  Locked (y/n): ", new_val, 4);
    if (len > 0) {
        buf[OFF_LOCKED] = (new_val[0] == 'y' || new_val[0] == 'Y') ? 1 : 0;
    }

    /* Update timestamp */
    now = (int32)time(NULL);
    PUT_LE32(buf + OFF_UPDATED, now);

    /* Update secondary indexes if needed */
    {
        char new_username[USERNAME_LEN + 1];
        char new_email[EMAIL_LEN + 1];

        memset(new_username, 0, sizeof(new_username));
        memset(new_email, 0, sizeof(new_email));
        memcpy(new_username, buf + OFF_USERNAME, USERNAME_LEN);
        memcpy(new_email, buf + OFF_EMAIL, EMAIL_LEN);

        if (strcmp(old_username, new_username) != 0) {
            DeleteFromIndex(&idx_username,
                            StringKey(old_username), record_id);
            InsertIntoIndex(&idx_username,
                            StringKey(new_username), record_id);
        }

        if (strcmp(old_email, new_email) != 0) {
            DeleteFromIndex(&idx_email,
                            StringKey(old_email), record_id);
            InsertIntoIndex(&idx_email,
                            StringKey(new_email), record_id);
        }
    }

    if (!UpdateRecord(&db, record_id, buf)) {
        fprintf(stderr, "Error: failed to update record\n");
        close_indexes(&idx_username, &idx_email);
        CloseDatabase(&db);
        return 1;
    }

    close_indexes(&idx_username, &idx_email);
    CloseDatabase(&db);

    printf("User %d updated.\n", (int)record_id);
    return 0;
}

static int cmd_delete(const char *arg)
{
    Database db;
    BTree idx_username, idx_email;
    byte buf[USER_RECORD_SIZE];
    char username[USERNAME_LEN + 1];
    char email[EMAIL_LEN + 1];
    int32 record_id;
    char *endp;
    long val;

    val = strtol(arg, &endp, 10);
    if (*endp != '\0' || endp == arg) {
        fprintf(stderr, "Error: delete requires a numeric ID\n");
        return 1;
    }
    record_id = (int32)val;

    if (!OpenDatabase(DB_NAME, &db)) {
        fprintf(stderr, "Error: failed to open database\n");
        return 1;
    }

    if (!open_indexes(&idx_username, &idx_email)) {
        fprintf(stderr, "Error: failed to open indexes\n");
        CloseDatabase(&db);
        return 1;
    }

    if (!FindRecordByID(&db, record_id, buf)) {
        fprintf(stderr, "Error: user %d not found\n", (int)record_id);
        close_indexes(&idx_username, &idx_email);
        CloseDatabase(&db);
        return 1;
    }

    memset(username, 0, sizeof(username));
    memset(email, 0, sizeof(email));
    memcpy(username, buf + OFF_USERNAME, USERNAME_LEN);
    memcpy(email, buf + OFF_EMAIL, EMAIL_LEN);

    DeleteFromIndex(&idx_username, StringKey(username), record_id);
    DeleteFromIndex(&idx_email, StringKey(email), record_id);

    if (!DeleteRecord(&db, record_id)) {
        fprintf(stderr, "Error: failed to delete record\n");
        close_indexes(&idx_username, &idx_email);
        CloseDatabase(&db);
        return 1;
    }

    close_indexes(&idx_username, &idx_email);
    CloseDatabase(&db);

    printf("User %d deleted.\n", (int)record_id);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Usage and main                                                     */
/* ------------------------------------------------------------------ */

static void usage(void)
{
    printf("Usage: usersadm <command> [args]\n\n");
    printf("Commands:\n");
    printf("  init              Create a new user database\n");
    printf("  list              List all users\n");
    printf("  create            Create a new user (interactive)\n");
    printf("  view <id|str>     View user by ID, username, or email\n");
    printf("  edit <id>         Edit a user (interactive)\n");
    printf("  delete <id>       Delete a user\n");
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        usage();
        return 1;
    }

    if (strcmp(argv[1], "init") == 0)
        return cmd_init();

    if (strcmp(argv[1], "list") == 0)
        return cmd_list();

    if (strcmp(argv[1], "create") == 0)
        return cmd_create();

    if (strcmp(argv[1], "view") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: usersadm view <id|username|email>\n");
            return 1;
        }
        return cmd_view(argv[2]);
    }

    if (strcmp(argv[1], "edit") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: usersadm edit <id>\n");
            return 1;
        }
        return cmd_edit(argv[2]);
    }

    if (strcmp(argv[1], "delete") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: usersadm delete <id>\n");
            return 1;
        }
        return cmd_delete(argv[2]);
    }

    fprintf(stderr, "Unknown command: %s\n", argv[1]);
    usage();
    return 1;
}
