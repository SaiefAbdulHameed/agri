#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "../include/ml_integration.h"

/* ================= CURL RESPONSE BUFFER ================= */

struct curl_buffer {
    char *data;
    size_t size;
    size_t capacity;
};

static size_t write_cb(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    struct curl_buffer *buf = (struct curl_buffer *)userdata;
    size_t incoming = size * nmemb;

    if (buf->size + incoming >= buf->capacity) {
        incoming = buf->capacity - buf->size - 1;
        if ((int)incoming <= 0)
            return size * nmemb;
    }

    memcpy(buf->data + buf->size, ptr, incoming);
    buf->size += incoming;
    buf->data[buf->size] = '\0';

    return size * nmemb;
}

/* ================= IMAGE → ML ================= */

int forward_image_to_ml(const char *image_path, char *response, size_t response_size)
{
    if (!image_path || !response || response_size == 0) return 0;
    response[0] = '\0';

    CURL *curl = curl_easy_init();
    if (!curl) return 0;

    struct curl_buffer buf = { .data = response, .size = 0, .capacity = response_size };

    curl_mime *mime = curl_mime_init(curl);
    curl_mimepart *part = curl_mime_addpart(mime);
    curl_mime_name(part, "image");
    curl_mime_filedata(part, image_path);

    curl_easy_setopt(curl, CURLOPT_URL, "http://127.0.0.1:5000/predict");
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);

    CURLcode res = curl_easy_perform(curl);

    curl_mime_free(mime);
    curl_easy_cleanup(curl);

    return (res == CURLE_OK);
}

int forward_image_to_plant_ml(const char *image_path, char *response, size_t response_size)
{
    if (!image_path || !response || response_size == 0) return 0;
    response[0] = '\0';

    CURL *curl = curl_easy_init();
    if (!curl) return 0;

    struct curl_buffer buf = { .data = response, .size = 0, .capacity = response_size };

    curl_mime *mime = curl_mime_init(curl);
    curl_mimepart *part = curl_mime_addpart(mime);
    curl_mime_name(part, "image");
    curl_mime_filedata(part, image_path);

    curl_easy_setopt(curl, CURLOPT_URL, "http://127.0.0.1:5000/predict-plant");
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);

    CURLcode res = curl_easy_perform(curl);

    curl_mime_free(mime);
    curl_easy_cleanup(curl);

    return (res == CURLE_OK);
}

/* ================= SOIL → ML SERVER ================= */

int get_crop_prediction(MLInput *in, MLOutput *out)
{
    if (!in || !out) return -1;

    CURL *curl = curl_easy_init();
    if (!curl) return -1;

    struct curl_buffer buf;
    buf.data = malloc(2048); // adjust if needed
    if (!buf.data) return -1;
    buf.size = 0;
    buf.capacity = 2048;
    buf.data[0] = '\0';

    char postfields[512];
    snprintf(postfields, sizeof(postfields),
             "{\"ph\": %.2f, \"nitrogen\": %d, \"phosphorus\": %d, \"potassium\": %d, \"moisture\": %d}",
             in->pH, in->nitrogen, in->phosphorus, in->potassium, in->moisture);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

 curl_easy_setopt(curl, CURLOPT_URL, "http://127.0.0.1:5000/predict-soil-params");
     // ML server endpoint
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postfields);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);

    CURLcode res = curl_easy_perform(curl);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        free(buf.data);
        return -1;
    }

    // Copy response into out struct
    // Assuming ML server returns JSON like: {"success":true,"predictedSoil":"Neutral","recommendedCrops":"Wheat, Maize"}
    sscanf(buf.data, "{\"success\":true,\"predictedSoil\":\"%63[^\"]\",\"recommendedCrops\":\"%255[^\"]\"}",
           out->predictedSoil, out->recommendedCrops);

    free(buf.data);
    return 0;
}
