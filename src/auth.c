#include "auth.h"
#include <stdio.h>
#include <string.h>
#include <openssl/sha.h>

void hash_password(const char *password, char *output_hash) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char *)password, strlen(password), hash);

    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(output_hash + (i * 2), "%02x", hash[i]);
    }
    output_hash[64] = '\0';
}

int init_auth_db(sqlite3 **db) {
    if (sqlite3_open("data/users.db", db)) {
        return 0;
    }

    const char *sql =
        "CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "username TEXT UNIQUE,"
        "password_hash TEXT,"
        "role TEXT,"
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP);";

    return sqlite3_exec(*db, sql, 0, 0, 0) == SQLITE_OK;
}

int register_user(sqlite3 *db, const char *username, const char *password, const char *role) {
    char hash[65];
    hash_password(password, hash);

    const char *sql =
        "INSERT INTO users (username, password_hash, role) VALUES (?, ?, ?);";

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, hash, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, role, -1, SQLITE_STATIC);

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return result == SQLITE_DONE;
}

int login_user(sqlite3 *db, const char *username, const char *password, char *out_role) {
    char hash[65];
    hash_password(password, hash);

    const char *sql =
        "SELECT role FROM users WHERE username=? AND password_hash=?;";

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, hash, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        strcpy(out_role, (const char *)sqlite3_column_text(stmt, 0));
        sqlite3_finalize(stmt);
        return 1;
    }

    sqlite3_finalize(stmt);
    return 0;
}
