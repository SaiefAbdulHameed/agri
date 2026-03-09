#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <openssl/evp.h>
#include <stdarg.h>

#include "auth.h"

#define SAFE(x) ((x) ? (x) : "")
#define DEBUG 1

static void debug_print(const char *fmt, ...) {
    if (!DEBUG) return;
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[AUTH_DEBUG] ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

char* hash_password(const char *password) {
    if (!password || !*password) {
        debug_print("WARNING: Empty password passed to hash_password");
        return NULL;
    }
    
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    unsigned char md_value[EVP_MAX_MD_SIZE];
    unsigned int md_len;

    EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL);
    EVP_DigestUpdate(mdctx, password, strlen(password));
    EVP_DigestFinal_ex(mdctx, md_value, &md_len);
    EVP_MD_CTX_free(mdctx);

    char *hash = malloc(md_len * 2 + 1);
    if (!hash) return NULL;

    for (unsigned int i = 0; i < md_len; i++)
        sprintf(hash + (i * 2), "%02x", md_value[i]);

    hash[md_len * 2] = '\0';
    
    debug_print("Hashed password (first 8 chars): %.8s...", hash);
    return hash;
}

int json_get_value(const char *json, const char *key, char *out, size_t out_size) {
    char search_key[256];
    snprintf(search_key, sizeof(search_key), "\"%s\"", key);
    
    const char *key_pos = strstr(json, search_key);
    if (!key_pos) {
        out[0] = '\0';
        return 0;
    }
    
    const char *colon = strchr(key_pos + strlen(search_key), ':');
    if (!colon) {
        out[0] = '\0';
        return 0;
    }
    
    const char *val_start = colon + 1;
    while (*val_start && (*val_start == ' ' || *val_start == '\t' || *val_start == '"')) {
        val_start++;
    }
    
    const char *val_end = val_start;
    if (*(val_start - 1) == '"') {
        val_end = strchr(val_start, '"');
        if (!val_end) val_end = val_start + strlen(val_start);
    } else {
        while (*val_end && *val_end != ',' && *val_end != '}') {
            val_end++;
        }
    }
    
    size_t len = val_end - val_start;
    if (len >= out_size) len = out_size - 1;
    if (len > 0) {
        strncpy(out, val_start, len);
        out[len] = '\0';
    } else {
        out[0] = '\0';
    }
    
    return 1;
}

int init_auth_db(sqlite3 **db) {
    if (sqlite3_open("data/users.db", db) != SQLITE_OK) {
        fprintf(stderr, "[AUTH_ERROR] Cannot open database: %s\n", sqlite3_errmsg(*db));
        return 0;
    }

    const char *users_sql = 
        "CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "fullname TEXT NOT NULL,"
        "email TEXT UNIQUE NOT NULL,"
        "phone TEXT DEFAULT '',"
        "state TEXT DEFAULT '',"
        "district TEXT DEFAULT '',"
        "village TEXT DEFAULT '',"
        "land_size INTEGER DEFAULT 0,"
        "experience TEXT DEFAULT '',"
        "role TEXT DEFAULT 'farmer',"
        "status TEXT DEFAULT 'pending',"
        "password TEXT NOT NULL,"
        "approved_by INTEGER,"
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "approved_at TIMESTAMP,"
        "farmer_id TEXT UNIQUE,"
        "employee_id TEXT UNIQUE,"
        "department TEXT DEFAULT '',"
        "expertise TEXT DEFAULT '',"
        "latitude REAL,"
        "longitude REAL,"
        "location_accuracy REAL"
        ");";
    
    sqlite3_exec(*db, users_sql, NULL, NULL, NULL);

    // Migration: Add new columns if they don't exist
    sqlite3_exec(*db, "ALTER TABLE users ADD COLUMN latitude REAL;", NULL, NULL, NULL);
    sqlite3_exec(*db, "ALTER TABLE users ADD COLUMN longitude REAL;", NULL, NULL, NULL);
    sqlite3_exec(*db, "ALTER TABLE users ADD COLUMN location_accuracy REAL;", NULL, NULL, NULL);
    sqlite3_exec(*db, "ALTER TABLE users ADD COLUMN farmer_id TEXT UNIQUE;", NULL, NULL, NULL);
    sqlite3_exec(*db, "ALTER TABLE users ADD COLUMN employee_id TEXT UNIQUE;", NULL, NULL, NULL);
    sqlite3_exec(*db, "ALTER TABLE users ADD COLUMN department TEXT DEFAULT '';", NULL, NULL, NULL);
    sqlite3_exec(*db, "ALTER TABLE users ADD COLUMN expertise TEXT DEFAULT '';", NULL, NULL, NULL);
    
    // Legacy migrations
    sqlite3_exec(*db, "ALTER TABLE users ADD COLUMN status TEXT DEFAULT 'pending';", NULL, NULL, NULL);
    sqlite3_exec(*db, "ALTER TABLE users ADD COLUMN phone TEXT DEFAULT '';", NULL, NULL, NULL);
    sqlite3_exec(*db, "ALTER TABLE users ADD COLUMN approved_by INTEGER;", NULL, NULL, NULL);
    sqlite3_exec(*db, "ALTER TABLE users ADD COLUMN approved_at TIMESTAMP;", NULL, NULL, NULL);

    // Trigger for auto-generating IDs
    const char *trigger_sql = 
        "CREATE TRIGGER IF NOT EXISTS generate_user_id "
        "AFTER INSERT ON users "
        "BEGIN "
        "UPDATE users SET "
        "farmer_id = CASE WHEN NEW.role = 'farmer' AND NEW.farmer_id IS NULL "
        "THEN 'FARM' || printf('%05d', NEW.id) ELSE NEW.farmer_id END, "
        "employee_id = CASE WHEN NEW.role = 'employee' AND NEW.employee_id IS NULL "
        "THEN 'EMP' || printf('%05d', NEW.id) ELSE NEW.employee_id END "
        "WHERE id = NEW.id; "
        "END;";
    sqlite3_exec(*db, trigger_sql, NULL, NULL, NULL);

    // Admin users table
    const char *admin_sql = 
        "CREATE TABLE IF NOT EXISTS admin_users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "email TEXT UNIQUE NOT NULL,"
        "fullname TEXT,"
        "password_hash TEXT NOT NULL,"
        "role INTEGER DEFAULT 1,"
        "active INTEGER DEFAULT 1,"
        "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "last_login TIMESTAMP"
        ");";
    
    sqlite3_exec(*db, admin_sql, NULL, NULL, NULL);
    
    const char *insert_admin = 
        "INSERT OR IGNORE INTO admin_users (email, fullname, password_hash, role) "
        "VALUES ('admin@agrismart.com', 'System Administrator', "
        "'8c6976e5b5410415bde908bd4dee15dfb167a9c873fc4bb8a81f6f2ab448a918', 1);";
    
    sqlite3_exec(*db, insert_admin, NULL, NULL, NULL);

    const char *ml_sql = 
        "CREATE TABLE IF NOT EXISTS ml_training_data ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "data_type TEXT NOT NULL,"
        "input_data TEXT NOT NULL,"
        "output_data TEXT NOT NULL,"
        "source TEXT,"
        "verified INTEGER DEFAULT 0,"
        "processed INTEGER DEFAULT 0,"
        "created_by INTEGER,"
        "accuracy_improvement REAL,"
        "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "processed_at TIMESTAMP"
        ");";
    
    sqlite3_exec(*db, ml_sql, NULL, NULL, NULL);
    
    sqlite3_exec(*db, "ALTER TABLE ml_training_data ADD COLUMN processed INTEGER DEFAULT 0;", NULL, NULL, NULL);
    sqlite3_exec(*db, "ALTER TABLE ml_training_data ADD COLUMN processed_at TIMESTAMP;", NULL, NULL, NULL);
    sqlite3_exec(*db, "ALTER TABLE ml_training_data ADD COLUMN accuracy_improvement REAL;", NULL, NULL, NULL);

    const char *other_tables[] = {
        "CREATE TABLE IF NOT EXISTS admin_activity_logs ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "admin_id INTEGER,"
        "action TEXT,"
        "details TEXT,"
        "ip_address TEXT,"
        "timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "FOREIGN KEY (admin_id) REFERENCES admin_users(id)"
        ");",
        
        "CREATE TABLE IF NOT EXISTS soil_analysis_logs ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "user_id INTEGER,"
        "input_data TEXT,"
        "prediction_result TEXT,"
        "recommendation TEXT,"
        "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "verified INTEGER DEFAULT 0,"
        "verified_by INTEGER"
        ");",
        
        "CREATE TABLE IF NOT EXISTS disease_reports ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "user_id INTEGER,"
        "image_path TEXT,"
        "detected_disease TEXT,"
        "severity REAL,"
        "treatment TEXT,"
        "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "verified INTEGER DEFAULT 0,"
        "verified_by INTEGER"
        ");",
        
        "CREATE TABLE IF NOT EXISTS fertilizers ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL,"
        "type TEXT,"
        "usage_info TEXT,"
        "region TEXT,"
        "n_content REAL,"
        "p_content REAL,"
        "k_content REAL,"
        "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
        ");",
        
        "CREATE TABLE IF NOT EXISTS system_settings ("
        "key TEXT PRIMARY KEY,"
        "value TEXT,"
        "updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
        ");",
        
        "INSERT OR IGNORE INTO system_settings (key, value) VALUES ('ml_server_url', 'http://127.0.0.1:5000');",
        "INSERT OR IGNORE INTO system_settings (key, value) VALUES ('auto_retrain_threshold', '50');",
        NULL
    };

    for (int i = 0; other_tables[i]; i++) {
        sqlite3_exec(*db, other_tables[i], NULL, NULL, NULL);
    }

    return 1;
}

int register_user(sqlite3 *db, const char *fullname, const char *email,
                  const char *phone, const char *state, const char *district,
                  const char *village, int land_size, const char *experience,
                  const char *role, const char *password,
                  const char *department, const char *expertise) {
    
    debug_print("Register called: email=%s, password_len=%zu, role=%s", email, strlen(password), role);
    
    if (!password || strlen(password) < 1) {
        debug_print("ERROR: Empty password provided");
        return 0;
    }
    
    char *hashed = hash_password(password);
    if (!hashed) {
        debug_print("ERROR: Password hashing failed");
        return 0;
    }
    
    debug_print("Password hashed successfully, hash_len=%zu", strlen(hashed));
    
    sqlite3_stmt *stmt;
    const char *sql = 
        "INSERT INTO users (fullname, email, phone, state, district, village, "
        "land_size, experience, role, status, password, department, expertise) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, 'pending', ?, ?, ?);";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "[AUTH_ERROR] Prepare failed: %s\n", sqlite3_errmsg(db));
        free(hashed);
        return 0;
    }
    
    sqlite3_bind_text(stmt, 1, SAFE(fullname), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, SAFE(email), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, SAFE(phone), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, SAFE(state), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, SAFE(district), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, SAFE(village), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 7, land_size);
    sqlite3_bind_text(stmt, 8, SAFE(experience), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 9, SAFE(role), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 10, hashed, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 11, SAFE(department), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 12, SAFE(expertise), -1, SQLITE_STATIC);
    
    int ok = sqlite3_step(stmt) == SQLITE_DONE;
    if (!ok) {
        fprintf(stderr, "[AUTH_ERROR] Insert failed: %s\n", sqlite3_errmsg(db));
    } else {
        debug_print("User registered successfully: %s", email);
    }
    
    sqlite3_finalize(stmt);
    free(hashed);
    return ok;
}

int login_user(sqlite3 *db, const char *email, const char *password, char *status_out) {
    debug_print("Login attempt: email=%s, password_len=%zu", email, strlen(password));
    
    if (!password || strlen(password) < 1) {
        debug_print("ERROR: Empty password in login attempt");
        return 0;
    }

    sqlite3_stmt *stmt;
    const char *sql = "SELECT status, password FROM users WHERE email=?";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        debug_print("ERROR: SQL prepare failed: %s", sqlite3_errmsg(db));
        return 0;
    }
    
    sqlite3_bind_text(stmt, 1, email, -1, SQLITE_STATIC);
    
    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        debug_print("User not found: %s", email);
        sqlite3_finalize(stmt);
        return 0;
    }
    
    const char *status_ptr = (const char*)sqlite3_column_text(stmt, 0);
    char status_buffer[32] = {0};
    
    if (status_ptr) {
        strncpy(status_buffer, status_ptr, 31);
        status_buffer[31] = '\0';
    } else {
        strcpy(status_buffer, "pending");
    }
    
    const char *stored_hash = (const char*)sqlite3_column_text(stmt, 1);
    
    debug_print("Status from DB: '%s' (raw: %p)", status_buffer, (void*)status_ptr);
    
    if (status_out) {
        strcpy(status_out, status_buffer);
    }
    
    char *computed = hash_password(password);
    if (!computed) {
        debug_print("ERROR: Failed to hash input password");
        sqlite3_finalize(stmt);
        return 0;
    }
    
    int password_match = (stored_hash && strcmp(stored_hash, computed) == 0);
    free(computed);
    
    if (!password_match) {
        debug_print("PASSWORD MISMATCH for user: %s", email);
        sqlite3_finalize(stmt);
        return 0;
    }
    
    debug_print("Password match, status='%s'", status_buffer);
    
    sqlite3_finalize(stmt);
    
    if (strcmp(status_buffer, "pending") == 0) {
        debug_print("REJECTED: pending");
        return -1;
    }
    if (strcmp(status_buffer, "disabled") == 0) {
        debug_print("REJECTED: disabled");
        return -2;
    }
    if (strcmp(status_buffer, "active") != 0) {
        debug_print("REJECTED: unknown status '%s', defaulting to pending", status_buffer);
        if (status_out) strcpy(status_out, "pending");
        return -1;
    }
    
    debug_print("APPROVED: active");
    return 1;
}

int approve_user(sqlite3 *db, int user_id, int admin_id) {
    sqlite3_stmt *stmt;
    const char *sql = 
        "UPDATE users SET status='active', approved_by=?, approved_at=CURRENT_TIMESTAMP "
        "WHERE id=? AND status='pending';";
    
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, admin_id);
    sqlite3_bind_int(stmt, 2, user_id);
    
    int ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok && sqlite3_changes(db) > 0;
}

int reject_user(sqlite3 *db, int user_id) {
    sqlite3_stmt *stmt;
    const char *sql = "DELETE FROM users WHERE id=? AND status='pending';";
    
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, user_id);
    
    int ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok && sqlite3_changes(db) > 0;
}

int disable_user(sqlite3 *db, int user_id) {
    sqlite3_stmt *stmt;
    const char *sql = "UPDATE users SET status='disabled' WHERE id=?;";
    
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, user_id);
    
    int ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

int enable_user(sqlite3 *db, int user_id) {
    sqlite3_stmt *stmt;
    const char *sql = "UPDATE users SET status='active' WHERE id=?;";
    
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, user_id);
    
    int ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

int delete_user(sqlite3 *db, int user_id) {
    sqlite3_stmt *stmt;
    const char *sql = "DELETE FROM users WHERE id=?;";
    
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, user_id);
    
    int ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

int update_user_profile(sqlite3 *db, const char *email, const char *fullname,
                        const char *state, const char *district, const char *village,
                        double latitude, double longitude, double accuracy) {
    sqlite3_stmt *stmt;
    const char *sql = 
        "UPDATE users SET fullname=?, state=?, district=?, village=?, "
        "latitude=?, longitude=?, location_accuracy=? "
        "WHERE email=? AND status='active';";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return 0;
    }
    
    sqlite3_bind_text(stmt, 1, SAFE(fullname), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, SAFE(state), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, SAFE(district), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, SAFE(village), -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 5, latitude);
    sqlite3_bind_double(stmt, 6, longitude);
    sqlite3_bind_double(stmt, 7, accuracy);
    sqlite3_bind_text(stmt, 8, email, -1, SQLITE_STATIC);
    
    int ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok && sqlite3_changes(db) > 0;
}

// FIXED: Added location parameters
int get_user_profile(sqlite3 *db, const char *email, char *fullname, char *state,
                     char *district, char *village, int *land_size,
                     char *experience, char *role, char *user_id,
                     char *department, char *expertise,
                     double *latitude, double *longitude, double *accuracy) {
    sqlite3_stmt *stmt;
    
    const char *sql = 
        "SELECT fullname, state, district, village, land_size, experience, role, "
        "COALESCE(farmer_id, employee_id) as user_id, department, expertise, "
        "latitude, longitude, location_accuracy "
        "FROM users WHERE email=? AND status='active';";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return 0;
    
    sqlite3_bind_text(stmt, 1, email, -1, SQLITE_STATIC);
    
    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return 0;
    }
    
    const char *val;
    
    val = (const char*)sqlite3_column_text(stmt, 0);
    strncpy(fullname, SAFE(val), 127);
    fullname[127] = '\0';
    
    val = (const char*)sqlite3_column_text(stmt, 1);
    strncpy(state, SAFE(val), 63);
    state[63] = '\0';
    
    val = (const char*)sqlite3_column_text(stmt, 2);
    strncpy(district, SAFE(val), 63);
    district[63] = '\0';
    
    val = (const char*)sqlite3_column_text(stmt, 3);
    strncpy(village, SAFE(val), 63);
    village[63] = '\0';
    
    *land_size = sqlite3_column_int(stmt, 4);
    
    val = (const char*)sqlite3_column_text(stmt, 5);
    strncpy(experience, SAFE(val), 63);
    experience[63] = '\0';
    
    val = (const char*)sqlite3_column_text(stmt, 6);
    strncpy(role, SAFE(val), 63);
    role[63] = '\0';
    
    val = (const char*)sqlite3_column_text(stmt, 7);
    strncpy(user_id, SAFE(val), 31);
    user_id[31] = '\0';
    
    val = (const char*)sqlite3_column_text(stmt, 8);
    strncpy(department, SAFE(val), 63);
    department[63] = '\0';
    
    val = (const char*)sqlite3_column_text(stmt, 9);
    strncpy(expertise, SAFE(val), 127);
    expertise[127] = '\0';
    
    // ADDED: Location fields
    *latitude = sqlite3_column_double(stmt, 10);
    *longitude = sqlite3_column_double(stmt, 11);
    *accuracy = sqlite3_column_double(stmt, 12);
    
    sqlite3_finalize(stmt);
    return 1;
}

int admin_login(sqlite3 *db, const char *email, const char *password, int *role_out) {
    debug_print("Admin login: %s", email);
    
    sqlite3_stmt *stmt;
    
    const char *sql = 
        "SELECT id, role, password_hash FROM admin_users "
        "WHERE email=? AND active=1;";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return 0;
    
    sqlite3_bind_text(stmt, 1, email, -1, SQLITE_STATIC);
    
    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return 0;
    }
    
    int id = sqlite3_column_int(stmt, 0);
    int role = sqlite3_column_int(stmt, 1);
    const char *stored_hash = (const char*)sqlite3_column_text(stmt, 2);
    
    char *computed = hash_password(password);
    int ok = (computed && stored_hash && strcmp(stored_hash, computed) == 0);
    free(computed);
    
    if (!ok) {
        sqlite3_finalize(stmt);
        return 0;
    }
    
    *role_out = role;
    sqlite3_finalize(stmt);
    
    sqlite3_prepare_v2(db, 
        "UPDATE admin_users SET last_login=CURRENT_TIMESTAMP WHERE id=?;",
        -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return id;
}

int get_pending_users(sqlite3 *db, char **json_output) {
    sqlite3_stmt *stmt;
    
    const char *sql = 
        "SELECT id, fullname, email, phone, state, district, village, "
        "land_size, experience, role, created_at, farmer_id, employee_id, "
        "department, expertise "
        "FROM users WHERE status='pending' ORDER BY created_at DESC;";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return 0;
    
    char *buf = malloc(65536);
    if (!buf) return 0;
    
    strcpy(buf, "[");
    int first = 1;
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (!first) strcat(buf, ",");
        first = 0;
        
        const char *role = SAFE((const char*)sqlite3_column_text(stmt, 9));
        char extra_fields[512] = "";
        
        if (strcmp(role, "farmer") == 0) {
            snprintf(extra_fields, sizeof(extra_fields),
                ",\"experience\":\"%s\",\"farmer_id\":\"%s\"",
                SAFE((const char*)sqlite3_column_text(stmt, 8)),
                SAFE((const char*)sqlite3_column_text(stmt, 11)));
        } else if (strcmp(role, "employee") == 0) {
            snprintf(extra_fields, sizeof(extra_fields),
                ",\"department\":\"%s\",\"expertise\":\"%s\",\"employee_id\":\"%s\"",
                SAFE((const char*)sqlite3_column_text(stmt, 13)),
                SAFE((const char*)sqlite3_column_text(stmt, 14)),
                SAFE((const char*)sqlite3_column_text(stmt, 12)));
        }
        
        char entry[4096];
        snprintf(entry, sizeof(entry),
            "{\"id\":%d,\"fullname\":\"%s\",\"email\":\"%s\",\"phone\":\"%s\","
            "\"state\":\"%s\",\"district\":\"%s\",\"village\":\"%s\","
            "\"land_size\":%d,\"role\":\"%s\",\"created_at\":\"%s\"%s}",
            sqlite3_column_int(stmt, 0),
            SAFE((const char*)sqlite3_column_text(stmt, 1)),
            SAFE((const char*)sqlite3_column_text(stmt, 2)),
            SAFE((const char*)sqlite3_column_text(stmt, 3)),
            SAFE((const char*)sqlite3_column_text(stmt, 4)),
            SAFE((const char*)sqlite3_column_text(stmt, 5)),
            SAFE((const char*)sqlite3_column_text(stmt, 6)),
            sqlite3_column_int(stmt, 7),
            role,
            SAFE((const char*)sqlite3_column_text(stmt, 10)),
            extra_fields);
        
        strcat(buf, entry);
    }
    
    strcat(buf, "]");
    sqlite3_finalize(stmt);
    *json_output = buf;
    return 1;
}

int get_all_users(sqlite3 *db, char **json_output) {
    sqlite3_stmt *stmt;
    
    const char *sql = 
        "SELECT id, fullname, email, phone, state, district, village, "
        "land_size, experience, role, status, created_at, farmer_id, employee_id, "
        "department, expertise "
        "FROM users ORDER BY id DESC;";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return 0;
    
    char *buf = malloc(65536);
    if (!buf) return 0;
    
    strcpy(buf, "[");
    int first = 1;
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (!first) strcat(buf, ",");
        first = 0;
        
        const char *role = SAFE((const char*)sqlite3_column_text(stmt, 9));
        const char *status = SAFE((const char*)sqlite3_column_text(stmt, 10));
        char extra_fields[512] = "";
        
        if (strcmp(role, "farmer") == 0) {
            snprintf(extra_fields, sizeof(extra_fields),
                ",\"experience\":\"%s\",\"farmer_id\":\"%s\"",
                SAFE((const char*)sqlite3_column_text(stmt, 8)),
                SAFE((const char*)sqlite3_column_text(stmt, 12)));
        } else if (strcmp(role, "employee") == 0) {
            snprintf(extra_fields, sizeof(extra_fields),
                ",\"department\":\"%s\",\"expertise\":\"%s\",\"employee_id\":\"%s\"",
                SAFE((const char*)sqlite3_column_text(stmt, 14)),
                SAFE((const char*)sqlite3_column_text(stmt, 15)),
                SAFE((const char*)sqlite3_column_text(stmt, 13)));
        }
        
        char entry[4096];
        snprintf(entry, sizeof(entry),
            "{\"id\":%d,\"fullname\":\"%s\",\"email\":\"%s\",\"phone\":\"%s\","
            "\"state\":\"%s\",\"district\":\"%s\",\"village\":\"%s\","
            "\"land_size\":%d,\"role\":\"%s\",\"status\":\"%s\",\"created_at\":\"%s\"%s}",
            sqlite3_column_int(stmt, 0),
            SAFE((const char*)sqlite3_column_text(stmt, 1)),
            SAFE((const char*)sqlite3_column_text(stmt, 2)),
            SAFE((const char*)sqlite3_column_text(stmt, 3)),
            SAFE((const char*)sqlite3_column_text(stmt, 4)),
            SAFE((const char*)sqlite3_column_text(stmt, 5)),
            SAFE((const char*)sqlite3_column_text(stmt, 6)),
            sqlite3_column_int(stmt, 7),
            role,
            status,
            SAFE((const char*)sqlite3_column_text(stmt, 11)),
            extra_fields);
        
        strcat(buf, entry);
    }
    
    strcat(buf, "]");
    sqlite3_finalize(stmt);
    *json_output = buf;
    return 1;
}

int add_ml_training_data(sqlite3 *db, const char *data_type, const char *input_data,
                         const char *output_data, const char *source, int created_by) {
    sqlite3_stmt *stmt;
    
    const char *sql = 
        "INSERT INTO ml_training_data (data_type, input_data, output_data, source, created_by) "
        "VALUES (?, ?, ?, ?, ?);";
    
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, data_type, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, input_data, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, output_data, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, source, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 5, created_by);
    
    int ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

int get_unprocessed_ml_data(sqlite3 *db, const char *data_type, char **json_output) {
    sqlite3_stmt *stmt;
    
    const char *sql = 
        "SELECT id, input_data, output_data "
        "FROM ml_training_data "
        "WHERE data_type=? AND verified=1 AND processed=0 "
        "ORDER BY created_at DESC LIMIT 100;";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return 0;
    
    sqlite3_bind_text(stmt, 1, data_type, -1, SQLITE_STATIC);
    
    char *buf = malloc(65536);
    if (!buf) return 0;
    
    strcpy(buf, "[");
    int first = 1;
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (!first) strcat(buf, ",");
        first = 0;
        
        char entry[4096];
        snprintf(entry, sizeof(entry),
            "{\"id\":%d,\"input\":%s,\"output\":%s}",
            sqlite3_column_int(stmt, 0),
            SAFE((const char*)sqlite3_column_text(stmt, 1)),
            SAFE((const char*)sqlite3_column_text(stmt, 2)));
        
        strcat(buf, entry);
    }
    
    strcat(buf, "]");
    sqlite3_finalize(stmt);
    *json_output = buf;
    return 1;
}

int mark_ml_data_processed(sqlite3 *db, int data_id, float accuracy) {
    sqlite3_stmt *stmt;
    
    const char *sql = 
        "UPDATE ml_training_data SET processed=1, processed_at=CURRENT_TIMESTAMP, "
        "accuracy_improvement=? WHERE id=?;";
    
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_double(stmt, 1, accuracy);
    sqlite3_bind_int(stmt, 2, data_id);
    
    int ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

int get_dashboard_stats(sqlite3 *db, int *total, int *pending, int *active, int *disabled) {
    sqlite3_stmt *stmt;
    
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM users;", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW) *total = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM users WHERE status='pending';", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW) *pending = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM users WHERE status='active';", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW) *active = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM users WHERE status='disabled';", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW) *disabled = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    
    return 1;
}
