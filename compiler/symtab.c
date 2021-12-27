#include "symtab.h"
 
char* symbolNames[] = {
    "variable",
    "function"};

struct symTabEntry *newEntry(char *name, enum symTabEntryType type)
{
    struct symTabEntry *wip = malloc(sizeof(struct symTabEntry));
    wip->name = name;
    wip->type = type;
    wip->entry = NULL;
    return wip;
}

struct variableEntry *newVariableEntry()
{
    struct variableEntry *wip = malloc(sizeof(struct variableEntry));
    wip->lsStart = 0;
    wip->lsEnd = 0;
    return wip;
}

struct functionEntry *newFunctionEntry(struct symbolTable *table)
{
    struct functionEntry *wip = malloc(sizeof(struct functionEntry));
    wip->table = table;
    return wip;
}

struct symbolTable *newSymbolTable(char *name)
{
    struct symbolTable *wip = malloc(sizeof(struct symbolTable));
    wip->size = 0;
    wip->entries = NULL;
    wip->name = name;
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
    printf("Unable to find symbol [%s]! Something is broken in the compiler :(\n", name);
    exit(1);
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
    struct symTabEntry **newList = malloc(++table->size * sizeof(struct symTabEntry *));
    for (int i = 0; i < table->size - 1; i++)
        newList[i] = table->entries[i];

    newList[table->size - 1] = wip;
    free(table->entries);
    table->entries = newList;
}

void symTab_insertVariable(struct symbolTable *table, char *name)
{
    symTabInsert(table, name, newVariableEntry(name), e_variable);
}

void symTab_insertFunction(struct symbolTable *table, char *name, struct symbolTable *subTable)
{
    symTabInsert(table, name, newFunctionEntry(subTable), e_function);
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
        case e_variable:
        {
            struct variableEntry *theVariable = it->entries[i]->entry;
            printf("> variable [%s]: (lifespan %d:%d)\n", it->entries[i]->name, theVariable->lsStart, theVariable->lsEnd);
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

