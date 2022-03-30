#include "ast.h"
#include "util.h"
#include "tac.h"
#include "symtab.h"

#pragma once

void linearizeASMBlock(struct BasicBlock *currentBlock, struct astNode *it);

void linearizeDereference(struct LinkedList *blockList, struct BasicBlock *currentBlock, struct astNode *it, int *tempNum, struct tempList *tl);

void linearizePointerArithmetic(struct LinkedList *blockList, struct BasicBlock *currentBlock, struct astNode *it, int *tempNum, struct tempList *tl, int depth);


void linearizeFunctionCall(struct LinkedList *blockList, struct BasicBlock *currentBlock, struct astNode *it, int *tempNum, struct tempList *tl);

void linearizeExpression(struct LinkedList *blockList, struct BasicBlock *currentBlock, struct astNode *it, int *tempNum, struct tempList *tl);

void linearizeAssignment(struct LinkedList *blockList, struct BasicBlock *currentBlock, struct astNode *it, int *tempNum, struct tempList *tl);

void linearizeDeclaration(struct BasicBlock *currentBlock, struct astNode *it, enum token type);

void linearizeStatementList(struct LinkedList *blockList, struct BasicBlock *currentBlock, struct astNode *it, int* tempNum, int* labelCount, struct tempList *tl);

void linearizeProgram(struct astNode *it, struct symbolTable *table);