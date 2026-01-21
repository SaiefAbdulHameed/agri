#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/fertilizer.h"
#include "../include/utils.h"

// Hash function for fertilizer name
unsigned int hash_function(const char* key) {
    unsigned int hash = 0;
    while (*key) {
        hash = (hash << 5) + (unsigned char)(*key); // cast to unsigned char
        key++;
    }
    return hash % HASH_MAP_SIZE;
}

// Insert fertilizer into hash map
void insert_fertilizer(FertilizerNode* hashMap[], const char* name, float price) {
    if (!hashMap || !name) return;

    unsigned int index = hash_function(name);

    // Allocate memory for new node
    FertilizerNode* node = (FertilizerNode*)safe_malloc(sizeof(FertilizerNode));
    
    // Copy the name safely (assuming node->name has at least 50 bytes)
    str_copy(node->name, name, sizeof(node->name));
    
    node->price = price;
    node->next = hashMap[index]; // prepend to chain
    hashMap[index] = node;
}

// Get fertilizer price by name
float get_fertilizer_price(FertilizerNode* hashMap[], const char* name) {
    if (!hashMap || !name) return -1.0f;

    unsigned int index = hash_function(name);
    FertilizerNode* curr = hashMap[index];

    while (curr) {
        if (strcmp(curr->name, name) == 0)
            return curr->price;
        curr = curr->next;
    }

    return -1.0f; // fertilizer not found
}

// Optional: Free the hash map memory
void free_fertilizer_hash_map(FertilizerNode* hashMap[]) {
    if (!hashMap) return;

    for (int i = 0; i < HASH_MAP_SIZE; i++) {
        FertilizerNode* curr = hashMap[i];
        while (curr) {
            FertilizerNode* temp = curr;
            curr = curr->next;
            free(temp);
        }
        hashMap[i] = NULL;
    }
}
