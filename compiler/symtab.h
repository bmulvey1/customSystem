#include <stdio.h>
#include <string.h>

#include "ast.h"
#include "tac.h"
#include "parser.h"
#include "tac.h"

#pragma once

enum symTabEntryType
{
    e_variable,
    e_function,
    e_argument
};

struct tempList
{
    int size;
    char **temps;
};

struct symTabEntry
{
    char *name;
    enum symTabEntryType type;
    void *entry;
};

struct variableEntry
{
    int isAssigned;
    char global;
    int stackOffset;
    enum variableTypes type;
    int indirectionLevel;
};

struct functionEntry
{
    struct symbolTable *table;
};

struct symbolTable
{
    char *name;
    struct symTabEntry **entries;
    struct symbolTable *parentScope;
    int size;
    struct tempList *tl;
    int localStackSize;
    int argStackSize;
    struct TACLine *codeBlock;
};

char *getTempString(struct tempList *tempList, int tempNum);

struct tempList *newTempList();

void freeTempList(struct tempList *it);

struct variableEntry *newVariableEntry(int indirectionLevel);

struct functionEntry *newFunctionEntry(struct symbolTable *table);

struct symbolTable *newSymbolTable(char *name);

int symbolTableContains(struct symbolTable *table, char *name);

struct symTabEntry *symbolTableLookup(struct symbolTable *table, char *name);

void symTabInsert(struct symbolTable *table, char *name, void *newEntry, enum symTabEntryType type);

void symTab_insertVariable(struct symbolTable *table, char *name, enum variableTypes type, int indirectionLevel);

void symTab_insertArgument(struct symbolTable *table, char *name, enum variableTypes type, int indirectionLevel);

void symTab_insertFunction(struct symbolTable *table, char *name, struct symbolTable *subTable);

void printSymTabRec(struct symbolTable *it, int depth, char printTAC);

void printSymTab(struct symbolTable *it, char printTAC);

void freeSymTab(struct symbolTable *it);

// AST walk functions
void walkStatement(struct astNode *it, struct symbolTable *wip);

void walkFunction(struct astNode *it, struct symbolTable *wip);

struct symbolTable *walkAST(struct astNode *it);