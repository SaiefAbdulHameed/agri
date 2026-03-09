#ifndef AUTH_H
#define AUTH_H

#include <sqlite3.h>

// Debug and JSON helpers
char* hash_password(const char *password);
int json_get_value(const char *json, const char *key, char *out, size_t out_size);

int init_auth_db(sqlite3 **db);

// UPDATED: Added department, expertise parameters
int register_user(sqlite3 *db, const char *fullname, const char *email,
                  const char *phone, const char *state, const char *district,
                  const char *village, int land_size, const char *experience,
                  const char *role, const char *password,
                  const char *department, const char *expertise);

// Returns: 1=success, 0=invalid, -1=pending, -2=disabled
int login_user(sqlite3 *db, const char *email, const char *password, char *status_out);

int approve_user(sqlite3 *db, int user_id, int admin_id);
int reject_user(sqlite3 *db, int user_id);
int disable_user(sqlite3 *db, int user_id);
int enable_user(sqlite3 *db, int user_id);
int delete_user(sqlite3 *db, int user_id);

// UPDATED: Added coordinates and accuracy
int update_user_profile(sqlite3 *db, const char *email, const char *fullname,
                        const char *state, const char *district, const char *village,
                        double latitude, double longitude, double accuracy);

// FIXED: Added location parameters (15 total)
int get_user_profile(sqlite3 *db, const char *email, char *fullname, char *state,
                     char *district, char *village, int *land_size,
                     char *experience, char *role, char *user_id,
                     char *department, char *expertise,
                     double *latitude, double *longitude, double *accuracy);

int admin_login(sqlite3 *db, const char *email, const char *password, int *role_out);
int get_pending_users(sqlite3 *db, char **json_output);
int get_all_users(sqlite3 *db, char **json_output);
int get_dashboard_stats(sqlite3 *db, int *total, int *pending, int *active, int *disabled);

// ML functions
int add_ml_training_data(sqlite3 *db, const char *data_type, const char *input_data,
                         const char *output_data, const char *source, int created_by);
int get_unprocessed_ml_data(sqlite3 *db, const char *data_type, char **json_output);
int mark_ml_data_processed(sqlite3 *db, int data_id, float accuracy);

#endif
