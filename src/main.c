#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sqlite3.h>
#include <curl/curl.h>
#include <time.h>
#include <openssl/evp.h>

#include "auth.h"

#define PORT 8080
#define BUFFER_SIZE 131072
#define DEBUG_MAIN 1

#define debug_main(fmt, ...) \
    do { if (DEBUG_MAIN) fprintf(stderr, "[MAIN_DEBUG] " fmt "\n", ##__VA_ARGS__); } while(0)

struct CurlBuf {
    char *data;
    size_t size;
};

size_t curl_write_cb(void *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t total = size * nmemb;
    struct CurlBuf *buf = userdata;
    char *tmp = realloc(buf->data, buf->size + total + 1);
    if (!tmp) return 0;
    buf->data = tmp;
    memcpy(buf->data + buf->size, ptr, total);
    buf->size += total;
    buf->data[buf->size] = '\0';
    return total;
}

int call_ml_json(const char *url, const char *json_payload, char **out_response) {
    CURL *curl = curl_easy_init();
    if (!curl) return 0;
    struct CurlBuf buf = {0};
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_payload);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    if (res != CURLE_OK) {
        free(buf.data);
        return 0;
    }
    *out_response = buf.data;
    return 1;
}

int call_ml_image(const char *url, const char *img_ptr, size_t img_len, char **out_response) {
    CURL *curl = curl_easy_init();
    if (!curl) return 0;
    struct CurlBuf buf = {0};
    curl_mime *mime = curl_mime_init(curl);
    curl_mimepart *part = curl_mime_addpart(mime);
    curl_mime_name(part, "image");
    curl_mime_filename(part, "upload.jpg");
    curl_mime_data(part, img_ptr, img_len);
    curl_mime_type(part, "image/jpeg");
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    CURLcode res = curl_easy_perform(curl);
    curl_mime_free(mime);
    curl_easy_cleanup(curl);
    if (res != CURLE_OK) {
        free(buf.data);
        return 0;
    }
    *out_response = buf.data;
    return 1;
}

void send_response(int sock, const char *status, const char *body, const char *origin) {
    if (!body) body = "";
    if (!origin) origin = "*";
    dprintf(sock,
        "HTTP/1.1 %s\r\n"
        "Content-Type: application/json\r\n"
        "Access-Control-Allow-Origin: %s\r\n"
        "Access-Control-Allow-Credentials: true\r\n"
        "Access-Control-Allow-Methods: GET, POST, OPTIONS, DELETE\r\n"
        "Access-Control-Allow-Headers: Content-Type, X-User, X-Admin-Email\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n\r\n%s",
        status, origin, strlen(body), body);
}

ssize_t read_http_request(int fd, char *buf, size_t max) {
    ssize_t total = 0;
    while (total < (ssize_t)max) {
        ssize_t n = recv(fd, buf + total, max - total, 0);
        if (n <= 0) return -1;
        total += n;
        if (memmem(buf, total, "\r\n\r\n", 4)) break;
    }
    ssize_t content_length = 0;
    char *cl = memmem(buf, total, "Content-Length:", 15);
    if (cl) sscanf(cl, "Content-Length: %zd", &content_length);
    char *body = memmem(buf, total, "\r\n\r\n", 4);
    if (!body) return total;
    body += 4;
    ssize_t body_read = total - (body - buf);
    while (body_read < content_length && total < (ssize_t)max) {
        ssize_t n = recv(fd, buf + total, max - total, 0);
        if (n <= 0) break;
        total += n;
        body_read += n;
    }
    return total;
}

int extract_image_multipart(const char *buffer, size_t buf_len __attribute__((unused)), 
                            const char **img_ptr, size_t *img_len) {
    const char *boundary = strstr(buffer, "boundary=");
    if (!boundary) return 0;
    boundary += 9;
    char bnd[128];
    sscanf(boundary, "%127s", bnd);
    char delim[256];
    snprintf(delim, sizeof(delim), "--%s", bnd);
    const char *part = strstr(buffer, delim);
    if (!part) return 0;
    part += strlen(delim);
    const char *content_type = strstr(part, "Content-Type: image/");
    if (!content_type) return 0;
    const char *data_start = strstr(content_type, "\r\n\r\n");
    if (!data_start) return 0;
    data_start += 4;
    const char *next_boundary = strstr(data_start, delim);
    if (!next_boundary) return 0;
    *img_ptr = data_start;
    *img_len = next_boundary - data_start - 2;
    return 1;
}

const char* get_role_name(int role) {
    switch(role) {
        case 1: return "Super Admin";
        case 2: return "Manager";
        case 3: return "Expert";
        default: return "Unknown";
    }
}

int main() {
    sqlite3 *db;
    if (!init_auth_db(&db)) {
        fprintf(stderr, "[FATAL] DB init failed\n");
        return 1;
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(PORT)
    };

    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 10);

    printf("[SERVER] Running on port %d (Debug mode enabled)\n", PORT);

    while (1) {
        char *buffer = malloc(BUFFER_SIZE);
        int client = accept(server_fd, NULL, NULL);
        if (client < 0) { free(buffer); continue; }
        
        ssize_t bytes = read_http_request(client, buffer, BUFFER_SIZE);
        if (bytes <= 0) {
            close(client);
            free(buffer);
            continue;
        }

        char origin[256] = "*";
        char *o = memmem(buffer, bytes, "Origin:", 7);
        if (o) sscanf(o, "Origin: %255s", origin);

        if (!memcmp(buffer, "OPTIONS", 7)) {
            dprintf(client,
                "HTTP/1.1 204 No Content\r\n"
                "Access-Control-Allow-Origin: %s\r\n"
                "Access-Control-Allow-Methods: GET, POST, OPTIONS, DELETE\r\n"
                "Access-Control-Allow-Headers: Content-Type, X-User, X-Admin-Email\r\n"
                "Content-Length: 0\r\n\r\n",
                origin);
            close(client);
            free(buffer);
            continue;
        }

        char *body = strstr(buffer, "\r\n\r\n");
        if (!body) {
            send_response(client, "400 Bad Request", "{\"success\":false}", origin);
            close(client); free(buffer); continue;
        }
        body += 4;

        // ===== UPDATE PROFILE =====
        if (!strncmp(buffer, "POST /update-profile", 20)) {
            char email[128]={0}, fullname[128]={0}, state[64]={0}, district[64]={0}, village[64]={0};
            char lat_str[32]={0}, lng_str[32]={0}, acc_str[32]={0};
            
            char *e = strstr(buffer, "X-User:");
            if (e) sscanf(e, "X-User: %127[^\r\n]", email);
            
            json_get_value(body, "fullname", fullname, sizeof(fullname));
            json_get_value(body, "state", state, sizeof(state));
            json_get_value(body, "district", district, sizeof(district));
            json_get_value(body, "village", village, sizeof(village));
            json_get_value(body, "latitude", lat_str, sizeof(lat_str));
            json_get_value(body, "longitude", lng_str, sizeof(lng_str));
            json_get_value(body, "accuracy", acc_str, sizeof(acc_str));
            
            double latitude = atof(lat_str);
            double longitude = atof(lng_str);
            double accuracy = atof(acc_str);
            
            if (update_user_profile(db, email, fullname, state, district, village, 
                                   latitude, longitude, accuracy)) {
                // FIXED: Proper C string formatting
                char response[256];
                if (accuracy > 0) {
                    snprintf(response, sizeof(response), 
                        "{\"success\":true,\"message\":\"Profile updated\",\"accuracy\":%.2f}", accuracy);
                } else {
                    snprintf(response, sizeof(response), 
                        "{\"success\":true,\"message\":\"Profile updated\",\"accuracy\":null}");
                }
                send_response(client, "200 OK", response, origin);
            } else {
                send_response(client, "400 Error", "{\"success\":false,\"message\":\"Update failed\"}", origin);
            }
            close(client); free(buffer); continue;
        }

        // ===== ADMIN LOGIN =====
        if (!strncmp(buffer, "POST /admin/login", 17)) {
            char email[128]={0}, password[128]={0};
            json_get_value(body, "email", email, sizeof(email));
            json_get_value(body, "password", password, sizeof(password));
            
            debug_main("Admin login attempt: %s", email);
            
            int role = 0;
            int admin_id = admin_login(db, email, password, &role);
            
            if (admin_id > 0) {
                char json[1024];
                snprintf(json, sizeof(json),
                    "{\"success\":true,\"id\":%d,\"email\":\"%s\",\"role\":%d,\"role_name\":\"%s\"}",
                    admin_id, email, role, get_role_name(role));
                send_response(client, "200 OK", json, origin);
            } else {
                send_response(client, "401 Unauthorized", "{\"success\":false,\"error\":\"Invalid credentials\"}", origin);
            }
            close(client); free(buffer); continue;
        }

        // ===== ADMIN DASHBOARD =====
        if (!strncmp(buffer, "GET /admin/dashboard", 20)) {
            int total = 0, pending = 0, active = 0, disabled = 0;
            get_dashboard_stats(db, &total, &pending, &active, &disabled);
            
            char json[2048];
            snprintf(json, sizeof(json),
                "{\"total_users\":%d,\"pending_users\":%d,\"active_users\":%d,\"disabled_users\":%d,\"ml_accuracy\":98.2}",
                total, pending, active, disabled);
            send_response(client, "200 OK", json, origin);
            close(client); free(buffer); continue;
        }

        // ===== PENDING USERS =====
        if (!strncmp(buffer, "GET /admin/pending-users", 24)) {
            char *json = NULL;
            if (get_pending_users(db, &json)) {
                send_response(client, "200 OK", json, origin);
                free(json);
            } else {
                send_response(client, "500 Error", "{\"error\":\"Failed to fetch\"}", origin);
            }
            close(client); free(buffer); continue;
        }

        // ===== ALL USERS =====
        if (!strncmp(buffer, "GET /admin/users", 16)) {
            char *json = NULL;
            if (get_all_users(db, &json)) {
                send_response(client, "200 OK", json, origin);
                free(json);
            } else {
                send_response(client, "500 Error", "{\"error\":\"Failed to fetch\"}", origin);
            }
            close(client); free(buffer); continue;
        }

        // ===== APPROVE USER =====
        if (!strncmp(buffer, "POST /admin/approve", 19)) {
            char id_str[16]={0};
            json_get_value(body, "id", id_str, sizeof(id_str));
            int user_id = atoi(id_str);
            
            if (approve_user(db, user_id, 1)) {
                send_response(client, "200 OK", "{\"success\":true}", origin);
            } else {
                send_response(client, "400 Error", "{\"success\":false}", origin);
            }
            close(client); free(buffer); continue;
        }

        // ===== REJECT USER =====
        if (!strncmp(buffer, "POST /admin/reject", 18)) {
            char id_str[16]={0};
            json_get_value(body, "id", id_str, sizeof(id_str));
            int user_id = atoi(id_str);
            
            if (reject_user(db, user_id)) {
                send_response(client, "200 OK", "{\"success\":true}", origin);
            } else {
                send_response(client, "400 Error", "{\"success\":false}", origin);
            }
            close(client); free(buffer); continue;
        }

        // ===== DISABLE USER =====
        if (!strncmp(buffer, "POST /admin/disable", 19)) {
            char id_str[16]={0};
            json_get_value(body, "id", id_str, sizeof(id_str));
            int user_id = atoi(id_str);
            
            if (disable_user(db, user_id)) {
                send_response(client, "200 OK", "{\"success\":true}", origin);
            } else {
                send_response(client, "400 Error", "{\"success\":false}", origin);
            }
            close(client); free(buffer); continue;
        }

        // ===== ENABLE USER =====
        if (!strncmp(buffer, "POST /admin/enable", 17)) {
            char id_str[16]={0};
            json_get_value(body, "id", id_str, sizeof(id_str));
            int user_id = atoi(id_str);
            
            if (enable_user(db, user_id)) {
                send_response(client, "200 OK", "{\"success\":true}", origin);
            } else {
                send_response(client, "400 Error", "{\"success\":false}", origin);
            }
            close(client); free(buffer); continue;
        }

        // ===== DELETE USER =====
        if (!strncmp(buffer, "DELETE /admin/user", 18)) {
            int user_id = 0;
            sscanf(buffer, "DELETE /admin/user?id=%d", &user_id);
            if (delete_user(db, user_id)) {
                send_response(client, "200 OK", "{\"success\":true}", origin);
            } else {
                send_response(client, "400 Error", "{\"success\":false}", origin);
            }
            close(client); free(buffer); continue;
        }

        // ===== ADD ML TRAINING DATA =====
        if (!strncmp(buffer, "POST /admin/ml-train", 20)) {
            char data_type[64]={0}, input_data[4096]={0}, output_data[4096]={0}, source[64]={0};
            json_get_value(body, "type", data_type, sizeof(data_type));
            json_get_value(body, "input", input_data, sizeof(input_data));
            json_get_value(body, "output", output_data, sizeof(output_data));
            json_get_value(body, "source", source, sizeof(source));
            
            if (add_ml_training_data(db, data_type, input_data, output_data, source, 1)) {
                send_response(client, "200 OK", "{\"success\":true}", origin);
            } else {
                send_response(client, "400 Error", "{\"success\":false}", origin);
            }
            close(client); free(buffer); continue;
        }

        // ===== HEALTH CHECKS =====
        if (!strncmp(buffer, "GET /health", 11)) {
            send_response(client, "200 OK", "{\"status\":\"ok\"}", origin);
            close(client); free(buffer); continue;
        }

        if (!strncmp(buffer, "GET /ml/health", 14)) {
            CURL *curl = curl_easy_init();
            if (curl) {
                curl_easy_setopt(curl, CURLOPT_URL, "http://127.0.0.1:5000/health");
                curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L);
                CURLcode res = curl_easy_perform(curl);
                curl_easy_cleanup(curl);
                if (res == CURLE_OK) {
                    send_response(client, "200 OK", "{\"status\":\"ok\"}", origin);
                } else {
                    send_response(client, "503 Error", "{\"status\":\"offline\"}", origin);
                }
            }
            close(client); free(buffer); continue;
        }

        // ===== USER REGISTRATION =====
        if (!strncmp(buffer, "POST /register", 14)) {
            char fullname[128]={0}, email[128]={0}, phone[32]={0}, password[128]={0};
            char state[64]={0}, district[64]={0}, village[64]={0}, experience[64]={0}, role[32]={0};
            char department[64]={0}, expertise[128]={0};
            char land_size_str[16]={0};
            int land_size = 0;
            
            json_get_value(body, "fullname", fullname, sizeof(fullname));
            json_get_value(body, "email", email, sizeof(email));
            json_get_value(body, "phone", phone, sizeof(phone));
            json_get_value(body, "password", password, sizeof(password));
            json_get_value(body, "state", state, sizeof(state));
            json_get_value(body, "district", district, sizeof(district));
            json_get_value(body, "village", village, sizeof(village));
            json_get_value(body, "experience", experience, sizeof(experience));
            json_get_value(body, "role", role, sizeof(role));
            json_get_value(body, "land_size", land_size_str, sizeof(land_size_str));
            json_get_value(body, "department", department, sizeof(department));
            json_get_value(body, "expertise", expertise, sizeof(expertise));
            land_size = atoi(land_size_str);
            
            fprintf(stderr, "[REGISTER_DEBUG] email='%s' role='%s' fullname='%s'\n", email, role, fullname);
            
            if (strlen(role) == 0) {
                strcpy(role, "farmer");
                fprintf(stderr, "[REGISTER_DEBUG] Defaulted role to 'farmer'\n");
            }
            
            if (strlen(password) == 0) {
                send_response(client, "400 Bad Request", 
                    "{\"success\":false,\"message\":\"Password is required\"}", origin);
                close(client); free(buffer); continue;
            }
            
            int ok = register_user(db, fullname, email, phone, state, district, village, 
                                  land_size, experience, role, password, department, expertise);
            
            if (ok) {
                send_response(client, "200 OK", 
                    "{\"success\":true,\"message\":\"Registration submitted for approval\"}", origin);
            } else {
                send_response(client, "400 Bad Request", 
                    "{\"success\":false,\"message\":\"Email already exists or registration failed\"}", origin);
            }
            close(client); free(buffer); continue;
        }

        // ===== USER LOGIN =====
        if (!strncmp(buffer, "POST /login", 11)) {
            char email[128] = {0};
            char password[128] = {0};
            char status[32] = {0};
            
            char *p = body;
            while (*p) {
                if (*p == '"' && strncmp(p+1, "email", 5) == 0) {
                    p += 8;
                    while (*p == ' ' || *p == '"' || *p == ':') p++;
                    int i = 0;
                    while (*p && *p != '"' && i < 127) {
                        email[i++] = *p++;
                    }
                    email[i] = '\0';
                }
                if (*p == '"' && strncmp(p+1, "password", 8) == 0) {
                    p += 11;
                    while (*p == ' ' || *p == '"' || *p == ':') p++;
                    int i = 0;
                    while (*p && *p != '"' && i < 127) {
                        password[i++] = *p++;
                    }
                    password[i] = '\0';
                }
                p++;
            }
            
            fprintf(stderr, "[LOGIN_DEBUG] email='%s' password='%s'\n", email, password);
            
            if (strlen(email) == 0 || strlen(password) == 0) {
                send_response(client, "400 Bad Request", 
                    "{\"success\":false,\"message\":\"Email and password required\"}", origin);
                close(client); free(buffer); continue;
            }
            
            int result = login_user(db, email, password, status);
            
            if (result == 1) {
                char fullname[128]={0}, state[64]={0}, district[64]={0}, village[64]={0};
                char experience[64]={0}, role[64]={0};
                char user_id[32]={0}, department[64]={0}, expertise[128]={0};
                int land_size = 0;
                double latitude = 0, longitude = 0, accuracy = 0;
                
                // FIXED: Added location parameters
                get_user_profile(db, email, fullname, state, district, village, &land_size, 
                                experience, role, user_id, department, expertise,
                                &latitude, &longitude, &accuracy);
                
                char json[2048];
                if (strcmp(role, "farmer") == 0) {
                    snprintf(json, sizeof(json),
                        "{\"success\":true,\"user\":{\"email\":\"%s\",\"fullname\":\"%s\",\"state\":\"%s\",\"district\":\"%s\",\"village\":\"%s\",\"land_size\":%d,\"experience\":\"%s\",\"role\":\"%s\",\"farmer_id\":\"%s\",\"location\":{\"lat\":%.6f,\"lng\":%.6f,\"accuracy\":%.1f}}}",
                        email, fullname, state, district, village, land_size, experience, role, user_id,
                        latitude, longitude, accuracy);
                } else if (strcmp(role, "employee") == 0) {
                    snprintf(json, sizeof(json),
                        "{\"success\":true,\"user\":{\"email\":\"%s\",\"fullname\":\"%s\",\"state\":\"%s\",\"district\":\"%s\",\"village\":\"%s\",\"role\":\"%s\",\"employee_id\":\"%s\",\"department\":\"%s\",\"expertise\":\"%s\",\"location\":{\"lat\":%.6f,\"lng\":%.6f,\"accuracy\":%.1f}}}",
                        email, fullname, state, district, village, role, user_id, department, expertise,
                        latitude, longitude, accuracy);
                } else {
                    snprintf(json, sizeof(json),
                        "{\"success\":true,\"user\":{\"email\":\"%s\",\"fullname\":\"%s\",\"state\":\"%s\",\"district\":\"%s\",\"village\":\"%s\",\"land_size\":%d,\"experience\":\"%s\",\"role\":\"%s\",\"location\":{\"lat\":%.6f,\"lng\":%.6f,\"accuracy\":%.1f}}}",
                        email, fullname, state, district, village, land_size, experience, role,
                        latitude, longitude, accuracy);
                }
                send_response(client, "200 OK", json, origin);
            } else if (result == -1) {
                send_response(client, "403 Forbidden", 
                    "{\"success\":false,\"status\":\"pending\",\"message\":\"Account pending admin approval\"}", origin);
            } else if (result == -2) {
                send_response(client, "403 Forbidden", 
                    "{\"success\":false,\"status\":\"disabled\",\"message\":\"Account has been disabled\"}", origin);
            } else {
                send_response(client, "401 Unauthorized", 
                    "{\"success\":false,\"message\":\"Invalid credentials\"}", origin);
            }
            close(client); free(buffer); continue;
        }

        // ===== GET PROFILE =====
        if (!strncmp(buffer, "GET /me", 7)) {
            char email[128]={0};
            char *e = strstr(buffer, "X-User:");
            if (!e) { 
                send_response(client, "401 Unauthorized", "{}", origin); 
                close(client); free(buffer); continue; 
            }
            sscanf(e, "X-User: %127[^\r\n]", email);

            char fullname[128]={0}, state[64]={0}, district[64]={0}, village[64]={0};
            char experience[64]={0}, role[64]={0};
            char user_id[32]={0}, department[64]={0}, expertise[128]={0};
            int land_size = 0;
            double latitude = 0, longitude = 0, accuracy = 0;

            // FIXED: Added location parameters
            if (get_user_profile(db, email, fullname, state, district, village, &land_size, 
                                experience, role, user_id, department, expertise,
                                &latitude, &longitude, &accuracy)) {
                char json[2048];
                if (strcmp(role, "farmer") == 0) {
                    snprintf(json, sizeof(json),
                        "{\"fullname\":\"%s\",\"email\":\"%s\",\"state\":\"%s\",\"district\":\"%s\",\"village\":\"%s\",\"land_size\":%d,\"experience\":\"%s\",\"role\":\"%s\",\"farmer_id\":\"%s\",\"location\":{\"lat\":%.6f,\"lng\":%.6f,\"accuracy\":%.1f}}",
                        fullname, email, state, district, village, land_size, experience, role, user_id,
                        latitude, longitude, accuracy);
                } else if (strcmp(role, "employee") == 0) {
                    snprintf(json, sizeof(json),
                        "{\"fullname\":\"%s\",\"email\":\"%s\",\"state\":\"%s\",\"district\":\"%s\",\"village\":\"%s\",\"role\":\"%s\",\"employee_id\":\"%s\",\"department\":\"%s\",\"expertise\":\"%s\",\"location\":{\"lat\":%.6f,\"lng\":%.6f,\"accuracy\":%.1f}}",
                        fullname, email, state, district, village, role, user_id, department, expertise,
                        latitude, longitude, accuracy);
                } else {
                    snprintf(json, sizeof(json),
                        "{\"fullname\":\"%s\",\"email\":\"%s\",\"state\":\"%s\",\"district\":\"%s\",\"village\":\"%s\",\"land_size\":%d,\"experience\":\"%s\",\"role\":\"%s\",\"location\":{\"lat\":%.6f,\"lng\":%.6f,\"accuracy\":%.1f}}",
                        fullname, email, state, district, village, land_size, experience, role,
                        latitude, longitude, accuracy);
                }
                send_response(client, "200 OK", json, origin);
            } else {
                send_response(client, "404 Not Found", "{}", origin);
            }
            close(client); free(buffer); continue;
        }

        // ===== SOIL PREDICTION =====
        if (!strncmp(buffer, "POST /soil/recommend", 20)) {
            char *ml_resp = NULL;
            if (!call_ml_json("http://127.0.0.1:5000/predict-soil-params", body, &ml_resp)) {
                send_response(client, "500 Error", "{\"success\":false}", origin);
            } else {
                send_response(client, "200 OK", ml_resp, origin);
                free(ml_resp);
            }
            close(client); free(buffer); continue;
        }

        // ===== SOIL IMAGE =====
        if (!strncmp(buffer, "POST /soil/image", 16)) {
            const char *img_ptr;
            size_t img_len;
            if (!extract_image_multipart(buffer, bytes, &img_ptr, &img_len)) {
                send_response(client, "400 Bad Request", "{\"error\":\"Invalid image\"}", origin);
                close(client); free(buffer); continue;
            }
            char *ml_resp = NULL;
            if (!call_ml_image("http://127.0.0.1:5000/predict-soil", img_ptr, img_len, &ml_resp)) {
                send_response(client, "500 Error", "{\"success\":false}", origin);
            } else {
                send_response(client, "200 OK", ml_resp, origin);
                free(ml_resp);
            }
            close(client); free(buffer); continue;
        }

        // ===== DEBUG USER STATUS =====
        if (!strncmp(buffer, "GET /debug/user-status", 22)) {
            char email[128] = {0};
            char *email_start = strstr(buffer, "email=");
            if (email_start) {
                sscanf(email_start, "email=%127[^& \r\n]", email);
            }
            
            sqlite3_stmt *stmt;
            const char *sql = "SELECT status, role, password FROM users WHERE email=?";
            sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
            sqlite3_bind_text(stmt, 1, email, -1, SQLITE_STATIC);
            
            char json[512] = "{\"found\":false}";
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                const char *status = (const char*)sqlite3_column_text(stmt, 0);
                const char *role = (const char*)sqlite3_column_text(stmt, 1);
                const char *pwd = (const char*)sqlite3_column_text(stmt, 2);
                int is_hashed = (pwd && strlen(pwd) == 64);
                
                snprintf(json, sizeof(json), 
                    "{\"found\":true,\"email\":\"%s\",\"status\":\"%s\",\"role\":\"%s\",\"password_is_hash\":%s,\"password_len\":%zu}",
                    email, status ? status : "null", role ? role : "null",
                    is_hashed ? "true" : "false", pwd ? strlen(pwd) : 0);
            }
            sqlite3_finalize(stmt);
            
            send_response(client, "200 OK", json, origin);
            close(client); free(buffer); continue;
        }

        // ===== PLANT DISEASE =====
        char method[8] = {0}, path[128] = {0};
        sscanf(buffer, "%7s %127s", method, path);
        
        if (strcmp(method, "POST") == 0 && strcmp(path, "/plant/disease") == 0) {
            const char *img_ptr;
            size_t img_len;
            if (!extract_image_multipart(buffer, bytes, &img_ptr, &img_len)) {
                send_response(client, "400 Bad Request", "{\"error\":\"Invalid image\"}", origin);
            } else {
                char *ml_resp = NULL;
                if (!call_ml_image("http://127.0.0.1:5000/predict-plant", img_ptr, img_len, &ml_resp)) {
                    send_response(client, "500 Error", "{\"success\":false}", origin);
                } else {
                    send_response(client, "200 OK", ml_resp, origin);
                    free(ml_resp);
                }
            }
            close(client); free(buffer); continue;
        }

        send_response(client, "404 Not Found", "{\"error\":\"Not found\"}", origin);
        close(client);
        free(buffer);
    }
}
