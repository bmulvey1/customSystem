#include "ast.h"
#include "util.h"
#include "tac.h"
#include "symtab.h"

#pragma once

int linearizeASMBlock(int currentTACIndex,
					  struct BasicBlock *currentBlock,
					  struct AST *it);

int linearizeDereference(struct Scope *scope,
						 int currentTACIndex,
						 struct BasicBlock *currentBlock,
						 struct AST *it,
						 int *tempNum);

int linearizePointerArithmetic(struct Scope *scope,
							   int currentTACIndex,
							   struct BasicBlock *currentBlock,
							   struct AST *it,
							   int *tempNum,
							   int depth);

int linearizeFunctionCall(struct Scope *scope,
						  int currentTACIndex,
						  struct BasicBlock *currentBlock,
						  struct AST *it,
						  int *tempNum);

int linearizeExpression(struct Scope *scope,
						int currentTACIndex,
						struct BasicBlock *currentBlock,
						struct AST *it,
						int *tempNum);

int linearizeAssignment(struct Scope *scope,
						int currentTACIndex,
						struct BasicBlock *currentBlock,
						struct AST *it,
						int *tempNum);

struct TACLine *linearizeConditionalJump(int currentTACIndex, char *cmpOp, struct AST *correspondingTree);

int linearizeDeclaration(struct Scope *scope,
						 int currentTACIndex,
						 struct BasicBlock *currentBlock,
						 struct AST *it);

struct Stack *linearizeIfStatement(struct Scope *scope,
								   int currentTACIndex,
								   struct BasicBlock *currentBlock,
								   struct BasicBlock *afterIfBlock,
								   struct AST *it,
								   int *tempNum,
								   int *labelCount,
								   struct Stack *scopenesting);

struct LinearizationResult *linearizeWhileLoop(struct Scope *scope,
											   int currentTACIndex,
											   struct BasicBlock *currentBlock,
											   struct BasicBlock *afterIfBlock,
											   struct AST *it,
											   int *tempNum,
											   int *labelCount,
											   struct Stack *scopenesting);

struct LinearizationResult *linearizeScope(struct Scope *scope,
										   int currentTACIndex,
										   struct BasicBlock *currentBlock,
										   struct BasicBlock *controlConvergesTo,
										   struct AST *it,
										   int *tempNum,
										   int *labelCount,
										   struct Stack *scopenesting);

void collapseScopes(struct Scope *scope, struct Dictionary *dict, int depth);

void linearizeProgram(struct AST *it, struct Scope *globalScope, struct Dictionary *dict);
