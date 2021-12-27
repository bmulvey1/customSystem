#include <stdio.h>
#include <string.h>

#include "ast.h"
#include "tac.h"
#include "parser.h"

#pragma once

enum symTabEntryType
{
    e_variable,
    e_function
};


struct symTabEntry
{
    char *name;
    enum symTabEntryType type;
    void *entry;
};

struct variableEntry
{
    int lsStart, lsEnd;
};

struct functionEntry
{
    struct symbolTable *table;
    struct tacLine *codeBlock;
};

struct symbolTable
{
    char *name;
    struct symTabEntry **entries;
    int size;
};

struct variableEntry *newVariableEntry();

struct functionEntry *newFunctionEntry(struct symbolTable *table);

struct symbolTable *newSymbolTable(char *name);

int symbolTableContains(struct symbolTable *table, char *name);

struct symTabEntry *symbolTableLookup(struct symbolTable *table, char *name);

void symTabInsert(struct symbolTable *table, char *name, void *newEntry, enum symTabEntryType type);

void symTab_insertVariable(struct symbolTable *table, char *name);

void symTab_insertFunction(struct symbolTable *table, char *name, struct symbolTable *subTable);

void printSymTabRec(struct symbolTable *it, int depth);

void printSymTab(struct symbolTable *it);