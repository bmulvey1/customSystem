#include "util.h"

/*
 * DICTIONARY FUNCTIONS
 * This string hashing algorithm is the djb2 algorithm
 * further information can be found at http://www.cse.yorku.ca/~oz/hash.html
 */
unsigned int hash(char *str)
{
    unsigned int hash = 5381;
    unsigned int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

struct Dictionary *newDictionary(int nBuckets)
{
    struct Dictionary *wip = malloc(sizeof(struct Dictionary));
    wip->nBuckets = nBuckets;
    wip->buckets = malloc(nBuckets * sizeof(struct DictionaryNode *));

    for (int i = 0; i < nBuckets; i++)
    {
        wip->buckets[i] = NULL;
    }

    return wip;
}

char *DictionaryInsert(struct Dictionary *dict, char *value)
{

    char *string = malloc(strlen(value) + 1);
    strncpy(string, value, strlen(value));
    string[strlen(value)] = '\0';
    unsigned int strHash = hash(value);
    strHash = strHash % dict->nBuckets;

    struct DictionaryEntry *newEntry = malloc(sizeof(struct DictionaryEntry));
    newEntry->data = string;
    newEntry->next = NULL;

    struct DictionaryEntry *runner = dict->buckets[strHash];
    if (runner != NULL)
    {
        while (runner->next != NULL)
        {
            runner = runner->next;
        }
        runner->next = newEntry;
    }
    else
    {
        dict->buckets[strHash] = newEntry;
    }

    return string;
}

char *DictionaryLookup(struct Dictionary *dict, char *value)
{
    unsigned int strHash = hash(value);
    strHash = strHash % dict->nBuckets;

    struct DictionaryEntry *runner = dict->buckets[strHash];
    if (runner == NULL)
    {
        return NULL;
    }

    while (runner != NULL)
    {
        if (!strcmp(runner->data, value))
        {
            return runner->data;
        }

        runner = runner->next;
    }
    return NULL;
}

char *DictionaryLookupOrInsert(struct Dictionary *dict, char *value)
{
    char *lookupResult = DictionaryLookup(dict, value);
    if (lookupResult != NULL)
    {
        return lookupResult;
    }
    else
    {
        char* pointer = DictionaryInsert(dict, value);
        return pointer;
    }
}

void freeDictionary(struct Dictionary *dict)
{
    for (int i = 0; i < dict->nBuckets; i++)
    {
        struct DictionaryEntry *runner = dict->buckets[i];
        while (runner != NULL)
        {
            struct DictionaryEntry *old = runner;
            free(runner->data);
            runner = runner->next;
            free(old);
        }
    }
    free(dict->buckets);
    free(dict);
}

/*
 * STACK FUNCTIONS
 *
 */

struct Stack *newStack()
{
    struct Stack *wip = malloc(sizeof(struct Stack));
    wip->data = malloc(5 * sizeof(void *));
    wip->size = 0;
    wip->allocated = 5;
    return wip;
}

void StackPush(struct Stack *s, void *data)
{
    if (s->size >= s->allocated)
    {
        void **newData = malloc((s->allocated + 5) * sizeof(void *));
        memcpy(newData, s->data, s->allocated * sizeof(void *));
        s->data = newData;
        s->allocated += 5;
    }

    s->data[s->size++] = data;
}

void *StackPop(struct Stack *s)
{
    if (s->size > 0)
    {
        return s->data[--s->size];
    }
    else
    {
        printf("Error - attempted to pop from empty stack!\n");
        exit(1);
    }
}

void* StackPeek(struct Stack* s){
    if (s->size > 0)
    {
        return s->data[s->size - 1];
    }
    else
    {
        printf("Error - attempted to peek empty stack!\n");
        exit(1);
    }
}