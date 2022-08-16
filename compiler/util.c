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
		char *pointer = DictionaryInsert(dict, value);
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

struct Stack *Stack_new()
{
	struct Stack *wip = malloc(sizeof(struct Stack));
	wip->data = malloc(20 * sizeof(void *));
	wip->size = 0;
	wip->allocated = 20;
	return wip;
}

void Stack_free(struct Stack *s)
{
	free(s->data);
	free(s);
}

void Stack_push(struct Stack *s, void *data)
{
	if (s->size >= s->allocated)
	{
		void **newData = malloc((int)(s->allocated * 1.5) * sizeof(void *));
		memcpy(newData, s->data, s->allocated * sizeof(void *));
		free(s->data);
		s->data = newData;
		s->allocated = (int)(s->allocated * 1.5);
	}

	s->data[s->size++] = data;
}

void *Stack_pop(struct Stack *s)
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

void *Stack_peek(struct Stack *s)
{
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

/*
 * LINKED LIST FUNCTIONS
 *
 */

struct LinkedList *LinkedList_new()
{
	struct LinkedList *wip = malloc(sizeof(struct LinkedList));
	wip->head = NULL;
	wip->tail = NULL;
	wip->size = 0;
	return wip;
}

void LinkedList_free(struct LinkedList *l, void (*dataFreeFunction)())
{
	struct LinkedListNode *runner = l->head;
	while (runner != NULL)
	{
		if (dataFreeFunction != NULL)
		{
			dataFreeFunction(runner->data);
		}
		struct LinkedListNode *old = runner;
		runner = runner->next;
		free(old);
	}
	free(l);
}

void LinkedList_append(struct LinkedList *l, void *element)
{
	if (element == NULL)
	{
		perror("Attempt to append data with null pointer into LinkedList!");
		exit(1);
	}

	struct LinkedListNode *newNode = malloc(sizeof(struct LinkedListNode));
	newNode->data = element;
	if (l->size == 0)
	{
		newNode->next = NULL;
		newNode->prev = NULL;
		l->head = newNode;
		l->tail = newNode;
	}
	else
	{
		l->tail->next = newNode;
		newNode->prev = l->tail;
		newNode->next = NULL;
		l->tail = newNode;
	}
	l->size++;
}

void LinkedList_prepend(struct LinkedList *l, void *element)
{
	if (element == NULL)
	{
		perror("Attempt to prepend data with null pointer into LinkedList!");
		exit(1);
	}

	struct LinkedListNode *newNode = malloc(sizeof(struct LinkedListNode));
	newNode->data = element;
	if (l->size == 0)
	{
		newNode->next = NULL;
		newNode->prev = NULL;
		l->head = newNode;
		l->tail = newNode;
	}
	else
	{
		l->head->prev = newNode;
		newNode->next = l->head;
		l->head = newNode;
	}
	l->size++;
}

void *LinkedList_delete(struct LinkedList *l, char (*compareFunction)(), void *element)
{
	for (struct LinkedListNode *runner = l->head; runner != NULL; runner = runner->next)
	{
		if (compareFunction(runner->data, element))
		{
			if (l->size > 1)
			{
				if (runner == l->head)
				{
					l->head = runner->next;
					runner->next->prev = NULL;
				}
				else
				{
					if (runner == l->tail)
					{
						l->tail = runner->prev;
						runner->prev->next = NULL;
					}
					else
					{
						runner->prev->next = runner->next;
						runner->next->prev = runner->prev;
					}
				}
			}
			void *data = runner->data;
			free(runner);
			return data;
		}
	}
	fprintf(stderr, "Error - could not delete element from linked list!\n");
	exit(1);
}

void *LinkedList_find(struct LinkedList *l, char (*compareFunction)(), void *element)
{
	for (struct LinkedListNode *runner = l->head; runner != NULL; runner = runner->next)
	{
		if (!compareFunction(runner->data, element))
		{
			return runner->data;
		}
	}
	return NULL;
}

/*
 * string trimming - overallocate for sprintf and let this funciton trim it down
*/

char *strTrim(char *s, int l){
	char *newStr = malloc(l + 1);
	strcpy(newStr, s);
	return newStr;
}

/*
 *
 *
 *
 * 
 */

char *TempList_getString(struct TempList *tempList, int tempNum)
{
	int sizeDiff = tempNum - tempList->temps->size;
	while(sizeDiff-- > 0)
	{
		char *thisTemp = malloc(6 * sizeof(char));
		sprintf(thisTemp, ".t%d", tempList->temps->size);
		Stack_push(tempList->temps, thisTemp);
	}
	return tempList->temps->data[tempNum];
}

struct TempList *TempList_new()
{
	struct TempList *wip = malloc(sizeof(struct TempList));
	wip->temps = Stack_new();
	TempList_getString(wip, 10);
	return wip;
}

void TempList_free(struct TempList *it)
{
	for(int i = 0; i < it->temps->size; i++)
	{
		free(it->temps->data[i]);
	}
	Stack_free(it->temps);
	free(it);
}
