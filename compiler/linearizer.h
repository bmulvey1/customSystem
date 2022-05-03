#include "ast.h"
#include "util.h"
#include "tac.h"
#include "symtab.h"

#pragma once

int linearizeASMBlock(int currentTACIndex,
					  struct BasicBlock *currentBlock,
					  struct ASTNode *it);

int linearizeDereference(struct symbolTable *table,
						 int currentTACIndex,
						 struct LinkedList *blockList,
						 struct BasicBlock *currentBlock,
						 struct ASTNode *it,
						 int *tempNum,
						 struct tempList *tl);

int linearizePointerArithmetic(struct symbolTable *table,
							   int currentTACIndex,
							   struct LinkedList *blockList,
							   struct BasicBlock *currentBlock,
							   struct ASTNode *it,
							   int *tempNum,
							   struct tempList *tl,
							   int depth);

int linearizeFunctionCall(struct symbolTable *table,
						  int currentTACIndex,
						  struct LinkedList *blockList,
						  struct BasicBlock *currentBlock,
						  struct ASTNode *it,
						  int *tempNum,
						  struct tempList *tl);

int linearizeExpression(struct symbolTable *table,
						int currentTACIndex,
						struct LinkedList *blockList,
						struct BasicBlock *currentBlock,
						struct ASTNode *it,
						int *tempNum,
						struct tempList *tl);

int linearizeAssignment(struct symbolTable *table,
						int currentTACIndex,
						struct LinkedList *blockList,
						struct BasicBlock *currentBlock,
						struct ASTNode *it,
						int *tempNum,
						struct tempList *tl);

struct TACLine *linearizeConditionalJump(int currentTACIndex, char *cmpOp);

int linearizeDeclaration(struct symbolTable *table,
						 int currentTACIndex,
						 struct BasicBlock *currentBlock,
						 struct ASTNode *it);

struct Stack *linearizeIfStatement(struct symbolTable *table,
								   int currentTACIndex,
								   struct LinkedList *blockList,
								   struct BasicBlock *currentBlock,
								   struct BasicBlock *afterIfBlock,
								   struct ASTNode *it,
								   int *tempNum,
								   int *labelCount,
								   struct tempList *tl);

struct LinearizationResult *linearizeWhileLoop(struct symbolTable *table,
											   int currentTACIndex,
											   struct LinkedList *blockList,
											   struct BasicBlock *currentBlock,
											   struct BasicBlock *afterIfBlock,
											   struct ASTNode *it,
											   int *tempNum,
											   int *labelCount,
											   struct tempList *tl);

struct LinearizationResult *linearizeStatementList(struct symbolTable *table,
												   int currentTACIndex,
												   struct LinkedList *blockList,
												   struct BasicBlock *currentBlock,
												   struct BasicBlock *controlConvergesTo,
												   struct ASTNode *it,
												   int *tempNum,
												   int *labelCount,
												   struct tempList *tl);

void linearizeProgram(struct ASTNode *it, struct symbolTable *table);
