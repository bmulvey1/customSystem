#include "ast.h"
#include "util.h"
#include "tac.h"
#include "symtab.h"

#pragma once

int linearizeASMBlock(int currentTACIndex, struct BasicBlock *currentBlock, struct astNode *it);

int linearizeDereference(int currentTACIndex, struct LinkedList *blockList, struct BasicBlock *currentBlock, struct astNode *it, int *tempNum, struct tempList *tl);

int linearizePointerArithmetic(int currentTACIndex, struct LinkedList *blockList, struct BasicBlock *currentBlock, struct astNode *it, int *tempNum, struct tempList *tl, int depth);

int linearizeFunctionCall(int currentTACIndex, struct LinkedList *blockList, struct BasicBlock *currentBlock, struct astNode *it, int *tempNum, struct tempList *tl);

int linearizeExpression(int currentTACIndex, struct LinkedList *blockList, struct BasicBlock *currentBlock, struct astNode *it, int *tempNum, struct tempList *tl);

int linearizeAssignment(int currentTACIndex, struct LinkedList *blockList, struct BasicBlock *currentBlock, struct astNode *it, int *tempNum, struct tempList *tl);

struct TACLine *linearizeConditionalJump(int currentTACIndex, char *cmpOp);

int linearizeDeclaration(int currentTACIndex, struct BasicBlock *currentBlock, struct astNode *it, enum token type);

struct Stack *linearizeIfStatement(int currentTACIndex, struct LinkedList *blockList, struct BasicBlock *currentBlock, struct BasicBlock *afterIfBlock, struct astNode *it, int *tempNum, int *labelCount, struct tempList *tl);

struct LinearizationResult *linearizeWhileLoop(int currentTACIndex, struct LinkedList *blockList, struct BasicBlock *currentBlock, struct BasicBlock *afterIfBlock, struct astNode *it, int *tempNum, int *labelCount, struct tempList *tl);

struct LinearizationResult *linearizeStatementList(int currentTACIndex, struct LinkedList *blockList, struct BasicBlock *currentBlock, struct BasicBlock *controlConvergesTo, struct astNode *it, int *tempNum, int *labelCount, struct tempList *tl);

void linearizeProgram(struct astNode *it, struct symbolTable *table);
