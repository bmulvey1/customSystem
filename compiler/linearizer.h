#include "ast.h"
#include "util.h"
#include "tac.h"
#include "symtab.h"

#pragma once

struct TACLine *linearizeASMBlock(struct astNode *it);

struct TACLine *linearizeDereference(struct astNode *it, int *tempNum, struct tempList *tl);

struct TACLine *linearizePointerArithmetic(struct astNode *it, int *tempNum, struct tempList *tl, int depth);

struct TACLine *linearizePointerWrite(struct astNode *it, int *tempNum, struct tempList *tl);

struct TACLine *linearizeFunctionCall(struct astNode *it, int *tempNum, struct tempList *tl);

struct TACLine *linearizeExpression(struct astNode *it, int *tempNum, struct tempList *tl);

struct TACLine *linearizeAssignment(struct astNode *it, int *tempNum, struct tempList *tl);

struct TACLine *linearizeStatementList(struct astNode *it, int* tempNum, int* labelCount, struct tempList *tl);

void linearizeProgram(struct astNode *it, struct symbolTable *table);