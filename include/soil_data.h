#ifndef SOIL_DATA_H
#define SOIL_DATA_H

#include<stdio.h>
#include<stdlib.h>
#include<string.h>

typedef struct SoilData{
	float pH;
	float nitrogen;
	float phosphorus;
	float potassium;
	float moisture;
	char date[20];
	struct SoilData* next;
}SoilData;

#define ALPHABET_SIZE 26

typedef struct TrieNode{
	struct TrieNode* children[ALPHABET_SIZE];
	SoilData* soilList;
	int isEndOfWord;
}TrieNode;

TrieNode* create_trie_node();
void insert_location(TrieNode* root, const char* location, SoilData* data );
SoilData* search_location(TrieNode* root, const char* location);
void free_trie(TrieNode* root);

#endif
