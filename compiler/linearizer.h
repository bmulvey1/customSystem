#include "ast.h"
#include "util.h"
#include "tac.h"
#include "symtab.h"

#pragma once

int linearizeASMBlock(int currentTACIndex,
					  struct BasicBlock *currentBlock,
					  struct ASTNode *it);

int linearizeDereference(struct Scope *scope,
						 int currentTACIndex,
						 struct LinkedList *blockList,
						 struct BasicBlock *currentBlock,
						 struct ASTNode *it,
						 int *tempNum);

int linearizePointerArithmetic(struct Scope *scope,
							   int currentTACIndex,
							   struct LinkedList *blockList,
							   struct BasicBlock *currentBlock,
							   struct ASTNode *it,
							   int *tempNum,
							   int depth);

int linearizeFunctionCall(struct Scope *scope,
						  int currentTACIndex,
						  struct LinkedList *blockList,
						  struct BasicBlock *currentBlock,
						  struct ASTNode *it,
						  int *tempNum);

int linearizeExpression(struct Scope *scope,
						int currentTACIndex,
						struct LinkedList *blockList,
						struct BasicBlock *currentBlock,
						struct ASTNode *it,
						int *tempNum);

int linearizeAssignment(struct Scope *scope,
						int currentTACIndex,
						struct LinkedList *blockList,
						struct BasicBlock *currentBlock,
						struct ASTNode *it,
						int *tempNum);

struct TACLine *linearizeConditionalJump(int currentTACIndex, char *cmpOp, struct ASTNode *correspondingTree);

int linearizeDeclaration(struct Scope *scope,
						 int currentTACIndex,
						 struct BasicBlock *currentBlock,
						 struct ASTNode *it);

struct Stack *linearizeIfStatement(struct Scope *scope,
								   int currentTACIndex,
								   struct LinkedList *blockList,
								   struct BasicBlock *currentBlock,
								   struct BasicBlock *afterIfBlock,
								   struct ASTNode *it,
								   int *tempNum,
								   int *labelCount);

struct LinearizationResult *linearizeWhileLoop(struct Scope *scope,
											   int currentTACIndex,
											   struct LinkedList *blockList,
											   struct BasicBlock *currentBlock,
											   struct BasicBlock *afterIfBlock,
											   struct ASTNode *it,
											   int *tempNum,
											   int *labelCount);

struct LinearizationResult *linearizeScope(struct Scope *scope,
										   int currentTACIndex,
										   struct LinkedList *blockList,
										   struct BasicBlock *currentBlock,
										   struct BasicBlock *controlConvergesTo,
										   struct ASTNode *it,
										   int *tempNum,
										   int *labelCount,
										   struct Stack *scopeNestings);

void linearizeProgram(struct ASTNode *it, struct Scope *scope);
