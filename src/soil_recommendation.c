#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

struct Memory {
    char *response;
    size_t size;
};

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t real_size = size * nmemb;
    struct Memory *mem = (struct Memory *)userp;

    char *ptr = realloc(mem->response, mem->size + real_size + 1);
    if (!ptr) return 0;

    mem->response = ptr;
    memcpy(&(mem->response[mem->size]), contents, real_size);
    mem->size += real_size;
    mem->response[mem->size] = 0;

    return real_size;
}

int main() {
    CURL *curl;
    CURLcode res;

    struct Memory chunk;
    chunk.response = malloc(1);
    chunk.size = 0;

    const char *json_payload =
        "{"
        "\"nitrogen\":90,"
        "\"phosphorus\":42,"
        "\"potassium\":43,"
        "\"ph\":6.5,"
        "\"moisture\":30,"
        "\"temperature\":28,"
        "\"rainfall\":120"
        "}";

    curl = curl_easy_init();
    if (!curl) {
        printf("Failed to init curl\n");
        return 1;
    }

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, "http://127.0.0.1:5000/predict-soil-params");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_payload);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

    res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        fprintf(stderr, "curl error: %s\n", curl_easy_strerror(res));
    } else {
        printf("ML RESPONSE:\n%s\n", chunk.response);
    }

    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
    free(chunk.response);

    return 0;
}
