#ifndef FERTILIZER_H
#define FERTILIZER_H

#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#define HASH_MAP_SIZE 100

typedef struct FertilizerNode{
	char name[50];
	float price;
	struct FertilizerNode* next;
}FertilizerNode;

unsigned int hash_function(const char* key);
void insert_fertilizer(FertilizerNode* hashMap[],const char* name, float price);
float get_fertilizer_price(FertilizerNode* hashMap[], const char* name);

#endif
