#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "api.h"
#include "soil_data.h"
#include "fertilizer.h"
#include "ml_integration.h"
#include "utils.h"

#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE];

    // Initialize data structures
    TrieNode* soilRoot = create_trie_node();
    FertilizerNode* fertMap[HASH_MAP_SIZE] = {0};

    insert_fertilizer(fertMap, "Urea", 266.5);
    insert_fertilizer(fertMap, "DAP", 1350.0);

    SoilData* data = (SoilData*)safe_malloc(sizeof(SoilData));
    data->pH = 6.2;
    data->nitrogen = 40;
    data->phosphorus = 30;
    data->potassium = 20;
    data->moisture = 65;
    data->next = NULL;
    insert_location(soilRoot, "karnataka", data);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 5) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server running on port %d\n", PORT);

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);

        new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        if (new_socket < 0) {
            perror("Accept failed");
            continue;
        }

        int bytes = read(new_socket, buffer, BUFFER_SIZE - 1);
        if (bytes <= 0) {
            close(new_socket);
            continue;
        }
        buffer[bytes] = '\0';

        /* ===== CORS PREFLIGHT ===== */
        if (strncmp(buffer, "OPTIONS", 7) == 0) {
            const char* optionsResponse =
                "HTTP/1.1 200 OK\r\n"
                "Access-Control-Allow-Origin: *\r\n"
                "Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n"
                "Access-Control-Allow-Headers: Content-Type\r\n"
                "Content-Length: 0\r\n\r\n";

            write(new_socket, optionsResponse, strlen(optionsResponse));
            close(new_socket);
            continue;
        }

        /* ===== PARSE REQUEST BODY ===== */
        char location[100];
        float ph;
        int n, p, k, moisture;
      char *body = strstr(buffer, "\r\n\r\n");
        if (body) {
            body += 4; // skip HTTP headers
        } else {
            body = buffer; // fallback: assume raw CSV
        }
        
        sscanf(body, "%99[^,],%f,%d,%d,%d,%d",
               location, &ph, &n, &p, &k, &moisture);
        printf("Received request: %s\n", buffer);
        printf("Parsed: location=%s, pH=%.2f, N=%d, P=%d, K=%d, moisture=%d\n",
               location, ph, n, p, k, moisture);
        

        SoilData* soil = search_location(soilRoot, location);

      char predictedCrop[50] = "Unknown";
        
      if (soil) {
            if (soil->pH >= 6.0 && soil->moisture > 60) {
                strcpy(predictedCrop, "Rice");
            } else if (soil->pH >= 5.5 && soil->nitrogen > 30) {
                strcpy(predictedCrop, "Wheat");
            } else if (soil->pH >= 5.0) {
                strcpy(predictedCrop, "Millet");
            } else {
                strcpy(predictedCrop, "Unknown");
            }
        }
        

        float ureaPrice = get_fertilizer_price(fertMap, "Urea");

        /* ===== JSON RESPONSE BODY ===== */
        char response[256];
        snprintf(response, sizeof(response),
                 "{\"crop\":\"%s\",\"ureaPrice\":%.2f}",
                 predictedCrop, ureaPrice);

        /* ===== HTTP RESPONSE ===== */
        char httpResponse[512];
        snprintf(httpResponse, sizeof(httpResponse),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n"
            "Access-Control-Allow-Headers: Content-Type\r\n"
            "Content-Length: %lu\r\n\r\n%s",
            strlen(response), response);

        write(new_socket, httpResponse, strlen(httpResponse));
        close(new_socket);
    }

    free_trie(soilRoot);
    return 0;
}
