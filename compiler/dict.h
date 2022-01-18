#include "string.h"
#include "stdlib.h"
#include "stdio.h"

#pragma once

struct DictionaryEntry
{
    char *data;
    struct DictionaryEntry *next;
};

struct Dictionary
{
    struct DictionaryEntry **buckets;
    int nBuckets;
};

unsigned int hash(char *str);

struct Dictionary *newDictionary(int nBuckets);

char* DictionaryInsert(struct Dictionary *dict, char *value);

char *DictionaryLookup(struct Dictionary *dict, char *value);

char *DictionaryLookupOrInsert(struct Dictionary *dict, char *value);

void freeDictionary(struct Dictionary* dict);