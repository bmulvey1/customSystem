#include "ast.h"
#include "util.h"
#include "tac.h"
#include "symtab.h"

#pragma once

struct LinearizationMetadata
{
	int currentTACIndex;
	struct BasicBlock *currentBlock;
	struct AST *ast;
	int *tempNum;
	struct Scope *scope;
};

int linearizeASMBlock(struct LinearizationMetadata m);

int linearizeDereference(struct LinearizationMetadata m);

int linearizeArgumentPushes(struct LinearizationMetadata m);

int linearizePointerArithmetic(struct LinearizationMetadata m,
							   int depth);

int linearizeFunctionCall(struct LinearizationMetadata m);

int linearizeSubExpression(struct LinearizationMetadata m,
						   struct TACLine *parentExpression,
						   int operandIndex);

int linearizeExpression(struct LinearizationMetadata m);

int linearizeAssignment(struct LinearizationMetadata m);

struct TACLine *linearizeConditionalJump(int currentTACIndex, char *cmpOp, struct AST *correspondingTree);

int linearizeDeclaration(struct LinearizationMetadata m);

int linearizeConditionCheck(struct LinearizationMetadata m,
							struct BasicBlock *ifFalse);

struct Stack *linearizeIfStatement(struct LinearizationMetadata m,
								   struct BasicBlock *afterIfBlock,
								   int *labelCount,
								   struct Stack *scopenesting);

struct LinearizationResult *linearizeWhileLoop(struct LinearizationMetadata m,
											   struct BasicBlock *afterIfBlock,
											   int *labelCount,
											   struct Stack *scopenesting);

struct LinearizationResult *linearizeScope(struct LinearizationMetadata m,
										   struct BasicBlock *controlConvergesTo,
										   int *labelCount,
										   struct Stack *scopenesting);

void collapseScopes(struct Scope *scope, struct Dictionary *dict, int depth);

void linearizeProgram(struct AST *it, struct Scope *globalScope, struct Dictionary *dict);
