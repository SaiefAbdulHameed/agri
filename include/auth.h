#ifndef AUTH_H
#define AUTH_H

#include <sqlite3.h>

int init_auth_db(sqlite3 **db);
int register_user(sqlite3 *db, const char *username, const char *password, const char *role);
int login_user(sqlite3 *db, const char *username, const char *password, char *out_role);

void hash_password(const char *password, char *output_hash);

#endif
