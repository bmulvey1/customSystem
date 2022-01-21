#include "symtab.h"

char *symbolNames[] = {
    "variable",
    "function"};

char *getTempString(struct tempList *tempList, int tempNum)
{
    if (tempList->size < tempNum)
    {
        int newSize = tempList->size + 10;
        char **newTemps = malloc(newSize * sizeof(char *));
        for (int i = 0; i < tempList->size; i++)
        {
            newTemps[i] = tempList->temps[i];
        }
        for (int i = tempList->size; i < newSize; i++)
        {
            newTemps[i] = malloc(5 * sizeof(char));
            sprintf(newTemps[i], ".t%d", i);
        }
        free(tempList->temps);
        tempList->temps = newTemps;
        tempList->size = newSize;
    }
    return tempList->temps[tempNum];
}

struct tempList *newTempList()
{
    struct tempList *wip = malloc(sizeof(struct tempList));
    wip->size = 10;
    wip->temps = malloc(10 * sizeof(char *));
    for (int i = 0; i < 10; i++)
    {
        wip->temps[i] = malloc(5 * sizeof(char));
        sprintf(wip->temps[i], ".t%d", i);
    }
    return wip;
}

void freeTempList(struct tempList *it)
{
    for (int i = 0; i < it->size; i++)
    {
        free(it->temps[i]);
    }
    free(it->temps);
    free(it);
}

struct symTabEntry *newEntry(char *name, enum symTabEntryType type)
{
    struct symTabEntry *wip = malloc(sizeof(struct symTabEntry));
    wip->name = name;
    wip->type = type;
    wip->entry = NULL;
    return wip;
}

struct variableEntry *newVariableEntry(int index)
{
    struct variableEntry *wip = malloc(sizeof(struct variableEntry));
    wip->lsStart = 0;
    wip->lsEnd = 0;
    wip->isAssigned = 0;
    wip->index = index;
    return wip;
}

struct argumentEntry *newArgumentEntry(int index)
{
    struct argumentEntry *wip = malloc(sizeof(struct argumentEntry));
    wip->lsStart = 0;
    wip->lsEnd = 0;
    wip->index = index;
    return wip;
}

struct functionEntry *newFunctionEntry(struct symbolTable *table)
{
    struct functionEntry *wip = malloc(sizeof(struct functionEntry));
    wip->table = table;
    wip->codeBlock = NULL;
    return wip;
}

struct symbolTable *newSymbolTable(char *name)
{
    struct symbolTable *wip = malloc(sizeof(struct symbolTable));
    wip->size = 0;
    wip->entries = NULL;
    wip->name = name;
    // generate a single templist at the top level symTab
    // leave all others as NULL to avoid duplication
    wip->tl = NULL;
    return wip;
}

int symbolTableContains(struct symbolTable *table, char *name)
{
    for (int i = 0; i < table->size; i++)
        if (!strcmp(table->entries[i]->name, name))
            return 1;

    return 0;
}

struct symTabEntry *symbolTableLookup(struct symbolTable *table, char *name)
{
    for (int i = 0; i < table->size; i++)
        if (!strcmp(table->entries[i]->name, name))
            return table->entries[i];

    // return NULL if not found
    return NULL;
}

void symTabInsert(struct symbolTable *table, char *name, void *newEntry, enum symTabEntryType type)
{
    if (symbolTableContains(table, name))
    {
        printf("Error defining symbol [%s] - name already exists!\n", name);
        exit(1);
    }
    struct symTabEntry *wip = malloc(sizeof(struct symTabEntry));
    wip->name = name;
    wip->entry = newEntry;
    wip->type = type;

    struct symTabEntry **newList = malloc((table->size + 1) * sizeof(struct symTabEntry *));
    for (int i = 0; i < table->size; i++)
    {
        newList[i] = table->entries[i];
    }

    newList[table->size] = wip;
    free(table->entries);
    table->entries = newList;
    table->size++;
}

void symTab_insertVariable(struct symbolTable *table, char *name, int index)
{
    struct variableEntry *newVariable = newVariableEntry(index);
    symTabInsert(table, name, newVariable, e_variable);
}

void symTab_insertArgument(struct symbolTable *table, char *name, int index)
{
    struct argumentEntry *newArgument = newArgumentEntry(index);
    symTabInsert(table, name, newArgument, e_argument);
}

void symTab_insertFunction(struct symbolTable *table, char *name, struct symbolTable *subTable)
{
    struct functionEntry *newFunction = newFunctionEntry(subTable);
    symTabInsert(table, name, newFunction, e_function);
}

void printSymTabRec(struct symbolTable *it, int depth)
{
    for (int i = 0; i < depth; i++)
        printf("\t");

    printf("~~~~~Symbol table for [%s]\n", it->name);
    for (int i = 0; i < it->size; i++)
    {
        for (int i = 0; i < depth; i++)
            printf("\t");

        switch (it->entries[i]->type)
        {
        case e_argument:
        {
            struct argumentEntry *theArgument = it->entries[i]->entry;
            printf("> argument [%s]: lifespan %d - %d\n", it->entries[i]->name, theArgument->lsStart, theArgument->lsEnd);
        }
        break;

        case e_variable:
        {
            struct variableEntry *theVariable = it->entries[i]->entry;
            printf("> variable [%s]: lifespan %d - %d\n", it->entries[i]->name, theVariable->lsStart, theVariable->lsEnd);
        }
        break;

        case e_function:
        {
            struct functionEntry *theFunction = it->entries[i]->entry;
            printf("> function [%s]: %d symbols\n", it->entries[i]->name, theFunction->table->size);
            printSymTabRec(theFunction->table, depth + 1);
            printTacLine(theFunction->codeBlock);
        }
        break;
        }
    }
    for (int i = 0; i < depth; i++)
        printf("\t");
    printf("\n");
}

void printSymTab(struct symbolTable *it)
{
    printSymTabRec(it, 0);
}

void freeSymTab(struct symbolTable *it)
{
    for (int i = 0; i < it->size; i++)
    {
        struct symTabEntry *currentEntry = it->entries[i];
        switch (currentEntry->type)
        {
        case e_variable:
            free((struct variableEntry *)currentEntry->entry);
            break;

        case e_argument:
            free((struct argumentEntry *)currentEntry->entry);
            break;

        case e_function:
            freeSymTab(((struct functionEntry *)currentEntry->entry)->table);
            freeTAC(((struct functionEntry *)currentEntry->entry)->codeBlock);
            free((struct functionEntry *)currentEntry->entry);
            break;
        }
        //free(it->name);
        free(currentEntry);
    }
    if (it->tl != NULL)
    {
        freeTempList(it->tl);
    }
    free(it->entries);
    free(it);
}