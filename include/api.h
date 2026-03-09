#ifndef API_H
#define API_H

#include "soil_data.h"
#include "fertilizer.h"


#define MAX_QUEUE 100

// Struct to store a request
typedef struct {
    char location[100];
    float pH;
    float nitrogen;
    float phosphorus;
    float potassium;
    float moisture;
} SoilRequest;

// Ring buffer for incoming requests
typedef struct {
    SoilRequest requests[MAX_QUEUE];
    int front;
    int rear;
} RequestQueue;

// Function prototypes
void init_queue(RequestQueue* queue);
int enqueue(RequestQueue* queue, SoilRequest request);
int dequeue(RequestQueue* queue, SoilRequest* request);

// API handling
void handle_request(
    SoilRequest* request,
    TrieNode* soilRoot,
    FertilizerNode* fertMap[]
);

#endif
