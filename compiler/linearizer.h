#include "ast.h"
#include "dict.h"
#include "tac.h"
#include "symtab.h"

#pragma once

struct tacLine *linearizeFunctionCall(struct astNode *it, int *tempNum, struct tempList *tl);

struct tacLine *linearizeExpression(struct astNode *it, int *tempNum, struct tempList *tl);

struct tacLine *linearizeAssignment(struct astNode *it, int *tempNum, struct tempList *tl);

struct tacLine *linearizeStatementList(struct astNode *it, int* tempNum, int* labelCount, struct tempList *tl);

void linearizeProgram(struct astNode *it, struct symbolTable *table);