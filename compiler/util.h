#include "string.h"
#include "stdlib.h"
#include "stdio.h"

#pragma once

enum CompilerErrors
{
	ERROR_INVOCATION, 	// user has made an error with arguments or other parameters
	ERROR_CODE,			// there is an error in the code which prevents a complete compilation
	ERROR_INTERNAL,
};


#define ErrorAndExit(code, fmt, ...) printf(fmt, ##__VA_ARGS__); printf("Bailing from file %s, line %d\n\n", __FILE__, __LINE__); exit(code)

/*
 * Dictionary for tracking strings
 * Economizes heap space by only storing strings once each
 * Uses a simple hash table which supports different bucket counts
 */
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

char *DictionaryInsert(struct Dictionary *dict, char *value);

char *DictionaryLookup(struct Dictionary *dict, char *value);

char *DictionaryLookupOrInsert(struct Dictionary *dict, char *value);

void freeDictionary(struct Dictionary *dict);

/*
 * Stack data structure
 *
 */

struct Stack
{
	void **data;
	int size;
	int allocated;
};

struct Stack *Stack_new();

void Stack_free(struct Stack *s);

void Stack_push(struct Stack *s, void *data);

void *Stack_pop(struct Stack *s);

void *Stack_peek(struct Stack *s);


/*
 * Unordered List data structure
 *
 */

struct LinkedListNode
{
	struct LinkedListNode *next;
	struct LinkedListNode *prev;
	void *data;
};

struct LinkedList
{
	struct LinkedListNode *head;
	struct LinkedListNode *tail;
	int size;
};

struct LinkedList *LinkedList_new();

void LinkedList_free(struct LinkedList *l,  void (*dataFreeFunction)());

void LinkedList_append(struct LinkedList *l, void *element);

void LinkedList_prepend(struct LinkedList *l, void *element);

void *LinkedList_delete(struct LinkedList *l, char (*compareFunction)(), void *element);

void *LinkedList_find(struct LinkedList *l, char (*compareFunction)(), void *element);

char *strTrim(char *s, int l);

/*
 *
 *
 * 
 */

struct TempList
{
	struct Stack *temps;
};


char *TempList_getString(struct TempList *tempList, int tempNum);

struct TempList *TempList_new();

void TempList_free(struct TempList *it);
