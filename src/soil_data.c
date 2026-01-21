#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/soil_data.h"
#include "../include/utils.h"

TrieNode* create_trie_node() {
    TrieNode* node = (TrieNode*)safe_malloc(sizeof(TrieNode));
    node->isEndOfWord = 0;
    node->soilList = NULL;

    for (int i = 0; i < ALPHABET_SIZE; i++)
        node->children[i] = NULL;

    return node;
}

void insert_location(TrieNode* root, const char* location, SoilData* data) {
    TrieNode* curr = root;

    for (int i = 0; location[i]; i++) {
        int index = location[i] - 'a';
        if (index < 0 || index >= ALPHABET_SIZE)
            continue;

        if (!curr->children[index])
            curr->children[index] = create_trie_node();

        curr = curr->children[index];
    }

    curr->isEndOfWord = 1;

    data->next = curr->soilList;
    curr->soilList = data;
}

SoilData* search_location(TrieNode* root, const char* location) {
    TrieNode* curr = root;

    for (int i = 0; location[i]; i++) {
        int index = location[i] - 'a';
        if (index < 0 || index >= ALPHABET_SIZE)
            continue;

        if (!curr->children[index])
            return NULL;

        curr = curr->children[index];
    }

    return (curr->isEndOfWord) ? curr->soilList : NULL;
}

void free_trie(TrieNode* root) {
    if (!root) return;

    for (int i = 0; i < ALPHABET_SIZE; i++)
        free_trie(root->children[i]);

    free(root);
}
