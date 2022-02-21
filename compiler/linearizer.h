#include "ast.h"
#include "util.h"
#include "tac.h"
#include "symtab.h"

#pragma once

struct TACLine *linearizeFunctionCall(struct astNode *it, int *tempNum, struct tempList *tl);

struct TACLine *linearizeExpression(struct astNode *it, int *tempNum, struct tempList *tl);

struct TACLine *linearizeAssignment(struct astNode *it, int *tempNum, struct tempList *tl);

struct TACLine *linearizeStatementList(struct astNode *it, int* tempNum, int* labelCount, struct tempList *tl);

void linearizeProgram(struct astNode *it, struct symbolTable *table);