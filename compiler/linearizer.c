#include "linearizer.h"

/*
 * These functions walk the AST and convert it to three-address code
 */

int linearizeASMBlock(int currentTACIndex,
					  struct BasicBlock *currentBlock,
					  struct ASTNode *it)
{
	struct ASTNode *asmRunner = it->child;
	while (asmRunner != NULL)
	{
		struct TACLine *asmLine = newTACLine(currentTACIndex++, tt_asm);
		asmLine->operands[0] = asmRunner->value;
		BasicBlock_append(currentBlock, asmLine);
		asmRunner = asmRunner->sibling;
	}
	return currentTACIndex;
}
void bkpt() {}
int linearizeDereference(struct symbolTable *table,
						 int currentTACIndex,
						 struct LinkedList *blockList,
						 struct BasicBlock *currentBlock,
						 struct ASTNode *it,
						 int *tempNum,
						 struct tempList *tl)
{
	printf("CALL TO LINEARIZE DEREFERENCE WITH TREE OF:\n\t");
	ASTNode_printHorizontal(it);
	printf("\n");
	bkpt();
	struct TACLine *thisDereference = newTACLine(currentTACIndex, tt_memr_1);
	thisDereference->correspondingTree = it;

	thisDereference->operands[0] = getTempString(tl, *tempNum);
	thisDereference->operandTypes[0] = vt_temp;
	(*tempNum)++;

	switch (it->type)
	{
		// directly dereference variables
	case t_name:
		thisDereference->operands[1] = it->value;
		thisDereference->operandTypes[1] = vt_var;
		thisDereference->indirectionLevels[1] = symbolTableLookup_var(table, it->value)->indirectionLevel;
		break;

		// recursively dereference nested dereferences
	case t_dereference:
		thisDereference->operands[1] = getTempString(tl, *tempNum);
		thisDereference->operandTypes[1] = vt_temp;
		currentTACIndex = linearizeDereference(table, currentTACIndex, blockList, currentBlock, it->child, tempNum, tl);
		thisDereference->indirectionLevels[1] = ((struct TACLine *)currentBlock->TACList->tail->data)->indirectionLevels[0];

		break;

		// handle pointer arithmetic to evalute the correct adddress to dereference
	case t_un_add:
	case t_un_sub:
		thisDereference->operands[1] = it->child->value; // base
		int LHSSize;
		switch (it->child->type)
		{
		case t_name:
			struct variableEntry *theVariable = symbolTableLookup_var(table, it->child->value);
			if (theVariable == NULL)
			{
				printf("Error - use of variable [%s] before declaration\n", it->child->value);
			}
			thisDereference->operandTypes[1] = theVariable->type;
			LHSSize = symbolTable_getSizeOfVariable(table, theVariable);
			break;

		case t_literal:
			thisDereference->operandTypes[1] = vt_literal;
			LHSSize = 2;
			break;

		case t_dereference:
			thisDereference->operandTypes[1] = vt_temp;
			thisDereference->operands[1] = getTempString(tl, *tempNum);
			currentTACIndex = linearizeDereference(table, currentTACIndex, blockList, currentBlock, it->child->child, tempNum, tl);
			thisDereference->indirectionLevels[1] = ((struct TACLine *)currentBlock->TACList->tail->data)->indirectionLevels[0];
			break;

		default:
			ASTNode_printHorizontal(it);
			perror("Illegal type on LHS of dereferenced expression\n");
			exit(2);
		}
		thisDereference->operation = tt_memr_3;
		thisDereference->operands[3] = (char *)(long int)LHSSize; // scale
		switch (it->child->sibling->type)						  // offset
		{
		case t_name:
			if (it->type == t_un_sub)
			{
				struct TACLine *subtractInvert = newTACLine(currentTACIndex++, tt_mul);
				subtractInvert->operands[0] = getTempString(tl, *tempNum);
				(*tempNum)++;
				subtractInvert->operandTypes[0] = vt_temp;
				subtractInvert->operands[1] = it->child->sibling->value;
				subtractInvert->operandTypes[1] = vt_var;
				subtractInvert->operands[2] = "-1";
				subtractInvert->operandTypes[2] = vt_literal;

				thisDereference->operands[2] = subtractInvert->operands[0];
				thisDereference->operandTypes[2] = vt_temp;
				BasicBlock_append(currentBlock, subtractInvert);
			}
			else
			{
				thisDereference->operands[2] = it->child->sibling->value;
				thisDereference->operandTypes[2] = vt_var;
			}

			break;

		// if literal, just use addressing mode base + offset
		case t_literal:
			// thisDereference->operands[1] = thisDereference->operands[2];
			// thisDereference->operandTypes[1] = thisDereference->operandTypes[2];
			thisDereference->operation = tt_memr_2;
			int offset = atoi(it->child->sibling->value);
			thisDereference->operands[2] = (char *)(long int)((offset * 2) * ((it->type == t_un_sub) ? -1 : 1));
			thisDereference->operandTypes[2] = vt_literal;
			break;

		case t_un_add:
		case t_un_sub:
			// parent expression type requiers inversion of entire (right) child expression if subtracting
			if (it->type == t_un_sub)
			{
				struct TACLine *subtractInvert = newTACLine(currentTACIndex++, tt_mul);
				subtractInvert->operands[0] = getTempString(tl, *tempNum);
				(*tempNum)++;
				currentTACIndex = linearizeExpression(table, currentTACIndex, blockList, currentBlock, it->child->sibling, tempNum, tl);
				subtractInvert->operandTypes[0] = vt_temp;
				subtractInvert->operands[1] = it->child->sibling->value;
				subtractInvert->operandTypes[1] = vt_var;
				subtractInvert->operands[2] = "-1";
				subtractInvert->operandTypes[2] = vt_literal;

				thisDereference->operands[2] = subtractInvert->operands[0];
				thisDereference->operandTypes[2] = vt_temp;
				BasicBlock_append(currentBlock, subtractInvert);
			}
			else
			{
				thisDereference->operands[2] = getTempString(tl, *tempNum);
				thisDereference->operandTypes[2] = vt_temp;
				currentTACIndex = linearizeExpression(table, currentTACIndex, blockList, currentBlock, it->child->sibling, tempNum, tl);
			}
			break;

		default:
			perror("Malformed parse tree in RHS of dereference arithmetic!\n");
			exit(1);
		}
		break;

	default:
		perror("Malformed parse tree when linearizing dereference!\n");
		exit(1);
	}

	thisDereference->index = currentTACIndex++;
	int newIndirection = thisDereference->indirectionLevels[1];
	if (newIndirection > 0)
	{
		newIndirection--;
	}

	thisDereference->indirectionLevels[0] = newIndirection;
	BasicBlock_append(currentBlock, thisDereference);
	return currentTACIndex;
}
/*
int linearizePointerArithmetic(struct symbolTable *table,
							   int currentTACIndex,
							   struct LinkedList *blockList,
							   struct BasicBlock *currentBlock,
							   struct ASTNode *it,
							   int *tempNum,
							   struct tempList *tl,
							   int depth)
{
	// set as tt_assign, assign properly in switch body
	struct TACLine *thisOperation = newTACLine(currentTACIndex, tt_assign);
	thisOperation->correspondingTree = it;
	struct TACLine *scaleMultiplication = NULL;

	thisOperation->operands[0] = getTempString(tl, *tempNum);
	thisOperation->operandTypes[0] = vt_temp;
	(*tempNum)++;
	char fallingThrough = 0;
	switch (it->type)
	{
		// assign the correct operation based on the operator node
	case t_un_add:
		thisOperation->reorderable = 1;
		thisOperation->operation = tt_add;
		fallingThrough = 1;
	case t_un_sub:
		if (!fallingThrough)
		{
			thisOperation->operation = tt_subtract;
			fallingThrough = 1;
		}

		// see what the LHS of the tree is
		switch (it->child->type)
		{
		// recursively handle more operations
		case t_un_add:
		case t_un_sub:
			thisOperation->operands[1] = getTempString(tl, *tempNum);
			thisOperation->operandTypes[1] = vt_temp;
			currentTACIndex = linearizePointerArithmetic(table, currentTACIndex, blockList, currentBlock, it->child, tempNum, tl, depth);
			thisOperation->indirectionLevels[1] = ((struct TACLine *)currentBlock->TACList->tail->data)->indirectionLevels[0];
			break;

		// handle dereferences explicitly
		case t_dereference:
			thisOperation->operands[1] = getTempString(tl, *tempNum);
			thisOperation->operandTypes[1] = vt_temp;
			currentTACIndex = linearizeDereference(table, currentTACIndex, blockList, currentBlock, it->child->child, tempNum, tl);
			thisOperation->indirectionLevels[1] = ((struct TACLine *)currentBlock->TACList->tail->data)->indirectionLevels[0];
			break;

		// handle variables directly
		case t_name:
			thisOperation->operands[1] = it->child->value;
			thisOperation->operandTypes[1] = vt_var;
			struct variableEntry *e = symbolTableLookup_var(table, it->child->value);
			if (e == NULL)
			{
				printf("Error - use of variable [%s] before declaration\n", it->child->value);
				exit(1);
			}
			thisOperation->indirectionLevels[1] = e->indirectionLevel;
			break;

		// disallow literals in the LHS of expressions - will never feasibly be doing something like *(1 - the_ptr)
		case t_literal:
			printf("Error - literal in LHS of pointer arithmetic expression!\n");
			exit(1);
	default:
			printf("Error - unexpected LHS of pointer arithmetic!\n");
			exit(1);
		}

		// much the same as the LHS evaluation above
		// except for performing scaling multiplication operations when depth == 0 to account for type sizes
		// TODO:
		//  - explicit scaling multiplication could be handled instead with different addressing modes
		//  - size lookups using symbol table (when implementing types)
		//

		switch (it->child->sibling->type)
		{
		case t_un_add:
		case t_un_sub:
			// scale iff depth 0
			if (depth == 0)
			{
				scaleMultiplication = newTACLine(currentTACIndex, tt_mul);
				scaleMultiplication->correspondingTree = it->child->sibling;
				scaleMultiplication->operands[0] = getTempString(tl, *tempNum);
				scaleMultiplication->operandTypes[0] = vt_temp;

				thisOperation->operands[2] = getTempString(tl, *tempNum);
				thisOperation->operandTypes[2] = vt_temp;

				(*tempNum)++;

				scaleMultiplication->operands[1] = getTempString(tl, *tempNum);
				scaleMultiplication->operandTypes[1] = vt_temp;

				scaleMultiplication->operands[2] = "2";
				scaleMultiplication->operandTypes[2] = vt_literal;

				currentTACIndex = linearizePointerArithmetic(table, currentTACIndex, blockList, currentBlock, it->child->sibling, tempNum, tl, depth + 1);
				int indirectionLevel = thisOperation->indirectionLevels[1];
				thisOperation->indirectionLevels[2] = indirectionLevel;
				scaleMultiplication->indirectionLevels[0] = indirectionLevel;
				scaleMultiplication->indirectionLevels[1] = indirectionLevel;
				scaleMultiplication->indirectionLevels[2] = indirectionLevel;
			}
			else
			{
				thisOperation->operands[2] = getTempString(tl, *tempNum);
				thisOperation->operandTypes[2] = vt_temp;
				currentTACIndex = linearizePointerArithmetic(table, currentTACIndex, blockList, currentBlock, it->child->sibling, tempNum, tl, depth + 1);
				thisOperation->indirectionLevels[2] = ((struct TACLine *)currentBlock->TACList->tail->data)->indirectionLevels[0];
			}
			break;

		case t_dereference:
			thisOperation->operands[2] = getTempString(tl, *tempNum);
			thisOperation->operandTypes[2] = vt_temp;
			(*tempNum)++;
			currentTACIndex = linearizeDereference(table, currentTACIndex, blockList, currentBlock, it->child->sibling->child, tempNum, tl);
			thisOperation->indirectionLevels[2] = ((struct TACLine *)currentBlock->TACList->tail->data)->indirectionLevels[0];
			break;

		case t_name:
		{
			// scale iff depth 0
			if (depth == 0)
			{
				scaleMultiplication = newTACLine(currentTACIndex, tt_mul);
				scaleMultiplication->correspondingTree = it->child->sibling;
				scaleMultiplication->operands[0] = getTempString(tl, *tempNum);
				scaleMultiplication->operandTypes[0] = vt_temp;

				scaleMultiplication->operands[1] = it->child->sibling->value;
				scaleMultiplication->operandTypes[1] = vt_var;

				thisOperation->operands[2] = getTempString(tl, *tempNum);
				thisOperation->operandTypes[2] = vt_temp;

				// TODO - scale multiplication based on object size (when implementing types)
				scaleMultiplication->operands[2] = "2";
				scaleMultiplication->operandTypes[2] = vt_literal;
				(*tempNum)++;
				struct variableEntry *e = symbolTableLookup_var(table, it->child->sibling->value);
				if (e == NULL)
				{
					printf("Error - use of undeclared variable [%s]\n", it->child->sibling->value);
					exit(1);
				}
				int indirectionLevel = e->indirectionLevel;
				thisOperation->indirectionLevels[2] = indirectionLevel + thisOperation->indirectionLevels[1];
				scaleMultiplication->indirectionLevels[0] = indirectionLevel + 1;
				scaleMultiplication->indirectionLevels[1] = indirectionLevel;
				scaleMultiplication->indirectionLevels[2] = indirectionLevel;
			}
			else
			{
				thisOperation->operands[2] = it->child->sibling->value;
				thisOperation->operandTypes[2] = vt_var;
				thisOperation->indirectionLevels[2] = symbolTableLookup_var(table, it->child->sibling->value)->indirectionLevel;
			}
		}
		break;

		case t_literal:
		{
			// scale iff depth 0
			if (depth == 0)
			{
				// TODO: use the dict to store these multiplied literals
				// this quick and dirty method leaks memory!
				char *scaledLiteral = malloc(8 * sizeof(char));

				sprintf(scaledLiteral, "%d", 2 * atoi(it->child->sibling->value));
				thisOperation->operands[2] = scaledLiteral;
			}
			else
			{
				thisOperation->operands[2] = it->child->sibling->value;
			}
			thisOperation->operandTypes[2] = vt_literal;
			thisOperation->indirectionLevels[2] = thisOperation->indirectionLevels[1];
		}
		break;

		default:
			perror("Bad RHS of pointer assignment expression:\n");
			exit(1);
		}
		break;

	default:
		perror("malformed parse tree or bad call to linearize pointer arithmetic!\n");
		exit(1);
		break;
	}
	if (scaleMultiplication != NULL)
	{
		scaleMultiplication->index = currentTACIndex++;
		BasicBlock_append(currentBlock, scaleMultiplication);
	}

	if (thisOperation->indirectionLevels[1] != thisOperation->indirectionLevels[2])
	{
		printf("Warning - pointer arithmetic between different indirection levels!\n\tExpression Tree: ");
		ASTNode_printHorizontal(it);
		printf("\n\t");
		printTACLine(thisOperation);
		printf("\n\n");
	}

	thisOperation->indirectionLevels[0] = thisOperation->indirectionLevels[1];

	thisOperation->index = currentTACIndex++;
	BasicBlock_append(currentBlock, thisOperation);
	return currentTACIndex;
}
*/
int linearizeArgumentPushes(struct symbolTable *table,
							int currentTACIndex,
							struct LinkedList *blockList,
							struct BasicBlock *currentBlock,
							struct ASTNode *argRunner,
							int *tempNum,
							struct tempList *tl)
{

	if (argRunner->sibling != NULL)
	{
		currentTACIndex = linearizeArgumentPushes(table, currentTACIndex, blockList, currentBlock, argRunner->sibling, tempNum, tl);
	}
	struct TACLine *thisArgument = NULL;
	switch (argRunner->type)
	{
	case t_name:
		thisArgument = newTACLine(currentTACIndex++, tt_push);
		thisArgument->operandTypes[0] = vt_var;
	case t_literal:
		if (thisArgument == NULL)
		{
			thisArgument = newTACLine(currentTACIndex++, tt_push);
			thisArgument->operandTypes[0] = vt_literal;
		}
		thisArgument->operands[0] = argRunner->value;
		break;

	case t_dereference:
		currentTACIndex = linearizeDereference(table, currentTACIndex, blockList, currentBlock, argRunner->child, tempNum, tl);
		thisArgument = newTACLine(currentTACIndex++, tt_push);
		thisArgument->operands[0] = ((struct TACLine *)currentBlock->TACList->tail->data)->operands[0];
		thisArgument->operandTypes[0] = ((struct TACLine *)currentBlock->TACList->tail->data)->operandTypes[0];
		thisArgument->indirectionLevels[0] = ((struct TACLine *)currentBlock->TACList->tail->data)->indirectionLevels[0];
		break;

	case t_un_add:
	case t_un_sub:
		char *pushOperand0 = getTempString(tl, *tempNum);

		currentTACIndex = linearizeExpression(table, currentTACIndex, blockList, currentBlock, argRunner, tempNum, tl);
		thisArgument = newTACLine(currentTACIndex++, tt_push);
		thisArgument->correspondingTree = argRunner;
		thisArgument->operands[0] = pushOperand0;
		thisArgument->operandTypes[0] = vt_temp;
		break;

	default:
		perror("Error - Unexpected argument node type\n");
		exit(2);
	}
	if (thisArgument != NULL)
		BasicBlock_append(currentBlock, thisArgument);

	return currentTACIndex;
}

// given an AST node of a function call, generate TAC to evaluate and push the arguments, then call it
int linearizeFunctionCall(struct symbolTable *table,
						  int currentTACIndex,
						  struct LinkedList *blockList,
						  struct BasicBlock *currentBlock,
						  struct ASTNode *it,
						  int *tempNum,
						  struct tempList *tl)
{
	char *operand0 = getTempString(tl, *tempNum);
	(*tempNum)++;

	// push arguments iff they exist
	if (it->child->child != NULL)
	{
		currentTACIndex = linearizeArgumentPushes(table, currentTACIndex, blockList, currentBlock, it->child->child, tempNum, tl);
	}

	struct TACLine *calltac = newTACLine(currentTACIndex++, tt_call);
	calltac->correspondingTree = it;
	calltac->operands[0] = operand0;
	calltac->operandTypes[0] = vt_temp;
	calltac->operands[1] = it->child->value;
	calltac->operandTypes[1] = vt_returnval;

	BasicBlock_append(currentBlock, calltac);
	return currentTACIndex;
}

// given an AST node of an expression, figure out how to break it down into multiple lines of three address code
int linearizeExpression(struct symbolTable *table,
						int currentTACIndex,
						struct LinkedList *blockList,
						struct BasicBlock *currentBlock,
						struct ASTNode *it,
						int *tempNum,
						struct tempList *tl)
{
	// set to tt_assign, reassign in switch body based on operator later
	// also set true TAC index at bottom of function, after child expression linearizations take place
	struct TACLine *thisExpression = newTACLine(currentTACIndex, tt_assign);
	thisExpression->correspondingTree = it;

	// since 'cmp' doesn't generate a result, it just sets flags, no need to consume a temp
	if (it->type != t_compOp)
	{
		thisExpression->operands[0] = getTempString(tl, *tempNum);
		thisExpression->operandTypes[0] = vt_temp;
		// increment count of temp variables, the parse of this expression will be written to a temp
		(*tempNum)++;
	}
	// support dereference and reference operations separately
	// since these have only one operand
	if (it->type == t_dereference)
	{
		thisExpression->operation = tt_memr_1;
		// if simply dereferencing a name
		if (it->child->type == t_name)
		{
			thisExpression->operands[1] = it->child->value;
			thisExpression->operandTypes[1] = vt_var;
		}
		// otherwise there's pointer arithmetic involved
		else
		{
			thisExpression->operands[1] = getTempString(tl, *tempNum);
			thisExpression->operandTypes[1] = vt_temp;

			currentTACIndex = linearizeDereference(table, currentTACIndex, blockList, currentBlock, it->child, tempNum, tl);
			thisExpression->indirectionLevels[1] = ((struct TACLine *)currentBlock->TACList->tail->data)->indirectionLevels[0];
		}

		thisExpression->index = currentTACIndex++;
		BasicBlock_append(currentBlock, thisExpression);
		return currentTACIndex;
	}

	// if we fall through to here, the expression is some sort of unary operation
	// handle the LHS of the operation
	switch (it->child->type)
	{

	// handle recording return types from function calls here
	case t_call:
		thisExpression->operands[1] = getTempString(tl, *tempNum);
		thisExpression->operandTypes[1] = vt_temp;

		currentTACIndex = linearizeFunctionCall(table, currentTACIndex, blockList, currentBlock, it->child, tempNum, tl);
		break;

	case t_compOp:
	case t_un_add:
	case t_un_sub:
		thisExpression->operands[1] = getTempString(tl, *tempNum);
		thisExpression->operandTypes[1] = vt_temp;

		currentTACIndex = linearizeExpression(table, currentTACIndex, blockList, currentBlock, it->child, tempNum, tl);
		thisExpression->indirectionLevels[1] = ((struct TACLine *)currentBlock->TACList->tail->data)->indirectionLevels[0];
		break;

	case t_name:
		thisExpression->operands[1] = it->child->value;
		thisExpression->operandTypes[1] = vt_var;
		struct variableEntry *theVariable = symbolTableLookup_var(table, it->child->value);
		if (theVariable == NULL)
		{
			printf("Error - use of variable [%s] before declaration\n", it->child->value);
			exit(1);
		}
		thisExpression->indirectionLevels[1] = theVariable->indirectionLevel;
		break;

	case t_literal:
		thisExpression->operands[1] = it->child->value;
		thisExpression->operandTypes[1] = vt_literal;
		// indirection levels set to 0 by default
		break;

	case t_dereference:
		thisExpression->operands[1] = getTempString(tl, *tempNum);
		thisExpression->operandTypes[1] = vt_temp;

		currentTACIndex = linearizeDereference(table, currentTACIndex, blockList, currentBlock, it->child->child, tempNum, tl);
		thisExpression->indirectionLevels[0] = ((struct TACLine *)currentBlock->TACList->tail->data)->indirectionLevels[0];
		break;

	default:
		perror("Unexpected type seen while linearizing expression! Expected variable or literal\n");
		exit(2);
	}

	// assign the TAC operation based on the operator at hand
	switch (it->value[0])
	{
	case '+':
		thisExpression->reorderable = 1;
		thisExpression->operation = tt_add;
		break;

	case '-':
		thisExpression->operation = tt_subtract;
		break;

	case '>':
	case '<':
	case '!':
	case '=':
		thisExpression->operation = tt_cmp;
		break;
	}

	// handle the RHS of the expression, same as LHS but at third operand of TAC
	switch (it->child->sibling->type)
	{
	case t_call:
		thisExpression->operands[2] = getTempString(tl, *tempNum);
		thisExpression->operandTypes[2] = vt_temp;

		currentTACIndex = linearizeFunctionCall(table, currentTACIndex, blockList, currentBlock, it->child->sibling, tempNum, tl);
		break;

	case t_compOp:
	case t_un_add:
	case t_un_sub:
		thisExpression->operands[2] = getTempString(tl, *tempNum);
		thisExpression->operandTypes[2] = vt_temp;
		// figure out what to prepend to, then set the return expression TACLine to the head of the block
		currentTACIndex = linearizeExpression(table, currentTACIndex, blockList, currentBlock, it->child->sibling, tempNum, tl);
		thisExpression->indirectionLevels[2] = ((struct TACLine *)currentBlock->TACList->tail->data)->indirectionLevels[0];
		break;

	case t_name:
		thisExpression->operands[2] = it->child->sibling->value;
		thisExpression->operandTypes[2] = vt_var;
		thisExpression->indirectionLevels[2] = symbolTableLookup_var(table, it->child->sibling->value)->indirectionLevel;

		break;

	case t_literal:
		thisExpression->operands[2] = it->child->sibling->value;
		thisExpression->operandTypes[2] = vt_literal;
		// indirection levels set to 0 by default
		break;

	case t_dereference:
		thisExpression->operands[2] = getTempString(tl, *tempNum);
		thisExpression->operandTypes[2] = vt_temp;

		currentTACIndex = linearizeDereference(table, currentTACIndex, blockList, currentBlock, it->child->sibling->child, tempNum, tl);
		thisExpression->indirectionLevels[2] = ((struct TACLine *)currentBlock->TACList->tail->data)->indirectionLevels[0];
		break;

	default:
		perror("Unexpected type seen while linearizing expression!\n Expected variable or literal\n");
		exit(2);
	}

	thisExpression->index = currentTACIndex++;
	BasicBlock_append(currentBlock, thisExpression);
	return currentTACIndex;
}

// given an AST node of an assignment, return the TAC block necessary to generate the correct value
int linearizeAssignment(struct symbolTable *table,
						int currentTACIndex,
						struct LinkedList *blockList,
						struct BasicBlock *currentBlock,
						struct ASTNode *it,
						int *tempNum,
						struct tempList *tl)
{
	struct TACLine *assignment = NULL;

	// if this assignment is simply setting one thing to another
	if (it->child->sibling->child == NULL)
	{
		// pull out the relevant values and generate a single line of TAC to return
		assignment = newTACLine(currentTACIndex++, tt_assign);
		assignment->correspondingTree = it;
		assignment->operands[1] = it->child->sibling->value;

		switch (it->child->sibling->type)
		{
		case t_literal:
			assignment->operandTypes[1] = vt_literal;
			break;

		case t_name:
			assignment->operandTypes[1] = vt_var;
			break;

		default:
			perror("Error parsing simple assignment - unexpected type on RHS\n");
			exit(2);
		}

		BasicBlock_append(currentBlock, assignment);
	}

	// otherwise there is some sort of expression, which will need to be broken down into multiple lines of TAC
	else
	{
		switch (it->child->sibling->type)
		{
		case t_dereference:
			currentTACIndex = linearizeDereference(table, currentTACIndex, blockList, currentBlock, it->child->sibling->child, tempNum, tl);
			break;

		case t_un_add:
		case t_un_sub:
			currentTACIndex = linearizeExpression(table, currentTACIndex, blockList, currentBlock, it->child->sibling, tempNum, tl);
			break;

		case t_call:
			currentTACIndex = linearizeFunctionCall(table, currentTACIndex, blockList, currentBlock, it->child->sibling, tempNum, tl);
			break;

		default:
			perror("Error linearizing expression - malformed parse tree (expected unOp or call)\n");
			exit(2);
		}
	}

	// LHS of the assignment expression - handle pointer writes and potential pointer arithmetic
	struct TACLine *lastLine = currentBlock->TACList->tail->data;
	if (it->child->type == t_dereference)
	{
		lastLine->operands[0] = getTempString(tl, *tempNum);
		lastLine->operandTypes[0] = vt_temp;
		(*tempNum)++;

		struct TACLine *LHS;
		struct TACLine *finalWrite;

		// base address
		switch (it->child->child->type)
		{
		case t_dereference:
			currentTACIndex = linearizeDereference(table, currentTACIndex++, blockList, currentBlock, it->child->child->child, tempNum, tl);

			finalWrite = newTACLine(currentTACIndex++, tt_memw_1);
			finalWrite->correspondingTree = it->child->child;
			finalWrite->operands[1] = lastLine->operands[0];
			finalWrite->operandTypes[1] = lastLine->operandTypes[0];
			finalWrite->indirectionLevels[1] = lastLine->indirectionLevels[0];

			finalWrite->operands[0] = getTempString(tl, *tempNum);
			finalWrite->operandTypes[0] = vt_temp;
			BasicBlock_append(currentBlock, finalWrite);
			break;

		// use base + offset * scale
		case t_un_add:
		case t_un_sub:
		{
			struct ASTNode *subExpression = it->child->child;
			finalWrite = newTACLine(currentTACIndex++, tt_memw_3);

			// source
			finalWrite->operands[3] = lastLine->operands[0];
			finalWrite->operandTypes[3] = lastLine->operandTypes[0];
			finalWrite->indirectionLevels[3] = lastLine->indirectionLevels[0];

			// destination base
			finalWrite->operands[0] = subExpression->child->value;

			// LHS of subexpression (base)
			switch (subExpression->child->type)
			{
			case t_name:
				struct ASTNode *lhs = subExpression->child;
				struct variableEntry *theVariable = symbolTableLookup_var(table, lhs->value);
				if (theVariable == NULL)
				{
					printf("Error - use of variable [%s] before declaration\n\tLine %d, Col %d\n", lhs->value, lhs->sourceLine, lhs->sourceCol);
					exit(1);
				}
				finalWrite->operandTypes[0] = theVariable->type;
				finalWrite->indirectionLevels[0] = theVariable->indirectionLevel;
				// set scale value
				finalWrite->operands[2] = (char *)(long int)symbolTable_getSizeOfVariable(table, theVariable);
				break;

			case t_literal:
				finalWrite->operandTypes[0] = vt_literal;
				finalWrite->indirectionLevels[0] = 0;
				finalWrite->operands[2] = (char *)1;
				break;

			case t_un_add:
			case t_un_sub:
				printf("Error - only rightwards binding within dereferenced expressions is supported\n\tLine %d, Col %d\n", subExpression->sourceLine, subExpression->sourceCol);
				exit(1);

			default:
				printf("Error in LHS of pointer write expression\n\tLine %d, Col %d", subExpression->sourceLine, subExpression->sourceCol);
				exit(2);
			}

			// RHS of subexpression (offset)
			struct ASTNode *rhs = subExpression->child->sibling;
			switch (rhs->type)
			{
			case t_name:
				struct variableEntry *theVariable = symbolTableLookup_var(table, rhs->value);
				if (theVariable == NULL)
				{
					printf("Error - use of variable [%s] before declaration\n\tLine %d, Col %d\n", rhs->value, rhs->sourceLine, rhs->sourceCol);
					exit(1);
				}
				finalWrite->operands[1] = rhs->value;
				finalWrite->indirectionLevels[1] = theVariable->indirectionLevel;
				finalWrite->operandTypes[1] = theVariable->type;
				break;

			case t_un_add:
			case t_un_sub:
				finalWrite->operands[1] = getTempString(tl, *tempNum);
				finalWrite->operandTypes[1] = vt_temp;
				// undo the previous increment of TAC index to reorder
				// finalWrite must come... final!
				currentTACIndex = linearizeExpression(table, --currentTACIndex, blockList, currentBlock, rhs, tempNum, tl);
				finalWrite->index = currentTACIndex++;
				finalWrite->indirectionLevels[1] = ((struct TACLine *)currentBlock->TACList->tail->data)->indirectionLevels[0];
				break;

			case t_literal: // no need for scaling mode, just scale the literal here
				finalWrite->operation = tt_memw_2;
				finalWrite->operands[1] = (char *)(long int)(atoi(rhs->value) * (long int)finalWrite->operands[2]);
				finalWrite->indirectionLevels[1] = 0;

				finalWrite->operands[2] = finalWrite->operands[3];

				break;

			default:
				ASTNode_printHorizontal(rhs);
				perror("Illegal type in pointer write subexpression\n");
				exit(1);
			}
		}
			BasicBlock_append(currentBlock, finalWrite);

			break;

		case t_name:
			LHS = newTACLine(currentTACIndex++, tt_memw_1);
			LHS->correspondingTree = it->child->child;
			LHS->operands[0] = it->child->child->value;
			LHS->operandTypes[0] = vt_var;

			LHS->operands[1] = lastLine->operands[0];
			LHS->operandTypes[1] = lastLine->operandTypes[0];
			LHS->indirectionLevels[1] = lastLine->indirectionLevels[0];
			BasicBlock_append(currentBlock, LHS);

			break;

		default:
			perror("Malformed parse tree - expected unary operator or dereference as child of pointer write!\n");
			exit(1);
		}
	}
	else
	{
		// set the final line's operand 0 to the variable actually being assigned
		lastLine->operands[0] = it->child->value;
		lastLine->operandTypes[0] = vt_var;
		struct variableEntry *e = symbolTableLookup_var(table, it->child->value);
		if (e == NULL)
		{
			printf("Error - use of variable [%s] before assignment\n", it->child->value);
			exit(1);
		}
		lastLine->indirectionLevels[0] = e->indirectionLevel;
	}

	return currentTACIndex;
}

struct TACLine *linearizeConditionalJump(int currentTACIndex, char *cmpOp)
{
	struct TACLine *notMetJump;
	switch (cmpOp[0])
	{
	case '<':
		switch (cmpOp[1])
		{
		case '=':
			notMetJump = newTACLine(currentTACIndex, tt_jg);
			break;

		default:
			notMetJump = newTACLine(currentTACIndex, tt_jge);
			break;
		}
		break;

	case '>':
		switch (cmpOp[1])
		{
		case '=':
			notMetJump = newTACLine(currentTACIndex, tt_jl);
			break;

		default:
			notMetJump = newTACLine(currentTACIndex, tt_jle);
			break;
		}
		break;

	case '!':
		notMetJump = newTACLine(currentTACIndex, tt_je);
		break;

	case '=':
		notMetJump = newTACLine(currentTACIndex, tt_jne);
		break;
	}
	return notMetJump;
}

int linearizeDeclaration(struct symbolTable *table,
						 int currentTACIndex,
						 struct BasicBlock *currentBlock,
						 struct ASTNode *it)
{
	struct TACLine *declarationLine = newTACLine(currentTACIndex++, tt_declare);
	declarationLine->correspondingTree = it;
	enum variableTypes declaredType;
	switch (it->type)
	{
	case t_var:
		declaredType = vt_var;
		break;

	default:
		perror("Unexpected type seen while linearizing declaration!");
		exit(1);
	}

	while (it->child->type == t_dereference)
	{
		it = it->child;
	}

	declarationLine->operands[0] = it->child->value;
	declarationLine->operandTypes[0] = declaredType;
	BasicBlock_append(currentBlock, declarationLine);
	return currentTACIndex;
}

struct Stack *linearizeIfStatement(struct symbolTable *table,
								   int currentTACIndex,
								   struct LinkedList *blockList,
								   struct BasicBlock *currentBlock,
								   struct BasicBlock *afterIfBlock,
								   struct ASTNode *it,
								   int *tempNum,
								   int *labelCount,
								   struct tempList *tl)
{
	// a stack is overkill but allows to store either 1 or 2 resulting blocks depending on if there is or isn't an else block
	struct Stack *results = Stack_new();

	// linearize the expression we will test
	currentTACIndex = linearizeExpression(table, currentTACIndex, blockList, currentBlock, it->child, tempNum, tl);

	// save state before branch
	struct TACLine *pushState = newTACLine(currentTACIndex, tt_pushstate);
	BasicBlock_append(currentBlock, pushState);

	// generate a label and figure out condition to jump when the if statement isn't executed
	struct TACLine *noifJump = linearizeConditionalJump(currentTACIndex++, it->child->value);

	BasicBlock_append(currentBlock, noifJump);

	// track the highest TAC index of both branches
	int ifTACIndex = currentTACIndex;
	int elseTACIndex = currentTACIndex;

	struct LinearizationResult *r = linearizeStatementList(table, ifTACIndex, blockList, currentBlock, afterIfBlock, it->child->sibling->child, tempNum, labelCount, tl);

	Stack_push(results, r);

	// if there is an else statement to the if
	if (it->child->sibling->sibling != NULL)
	{
		// create a basicblock for the else statement
		struct BasicBlock *elseBlock = BasicBlock_new(++(*labelCount));
		elseBlock->hintLabel = "else block";
		LinkedList_append(blockList, elseBlock);

		// need to ensure the else block starts from the same state as the if
		struct TACLine *resetLine = newTACLine(elseTACIndex, tt_resetstate);
		BasicBlock_append(elseBlock, resetLine);

		// bit hax (⌐▨_▨)
		// store the label index using the char* for this TAC line to avoid more fields in the struct
		noifJump->operands[0] = (char *)((long int)elseBlock->labelNum);

		// linearize the else block, it returns to the same afterIfBlock as the ifBlock does
		r = linearizeStatementList(table, elseTACIndex, blockList, elseBlock, afterIfBlock, it->child->sibling->sibling->child->child, tempNum, labelCount, tl);

		Stack_push(results, r);
	}
	else
	{
		noifJump->operands[0] = (char *)((long int)afterIfBlock->labelNum);
	}

	return results;
}

struct LinearizationResult *linearizeWhileLoop(struct symbolTable *table,
											   int currentTACIndex,
											   struct LinkedList *blockList,
											   struct BasicBlock *currentBlock,
											   struct BasicBlock *afterWhileBlock,
											   struct ASTNode *it,
											   int *tempNum,
											   int *labelCount,
											   struct tempList *tl)
{
	int entryTACIndex = currentTACIndex;
	// save state before while block
	struct TACLine *pushState = newTACLine(currentTACIndex, tt_pushstate);
	BasicBlock_append(currentBlock, pushState);

	struct BasicBlock *whileBlock = BasicBlock_new(++(*labelCount));
	whileBlock->hintLabel = "while block";
	LinkedList_append(blockList, whileBlock);

	struct TACLine *enterWhileJump = newTACLine(currentTACIndex++, tt_jmp);
	enterWhileJump->operands[0] = (char *)((long int)whileBlock->labelNum);
	BasicBlock_append(currentBlock, enterWhileJump);

	struct TACLine *whileDo = newTACLine(currentTACIndex, tt_do);
	BasicBlock_append(whileBlock, whileDo);

	// linearize the expression we will test
	currentTACIndex = linearizeExpression(table, currentTACIndex, blockList, whileBlock, it->child, tempNum, tl);

	// restore state before the conditional jump that exits the loop
	struct TACLine *beforeNoWhileRestore = newTACLine(currentTACIndex, tt_restorestate);
	beforeNoWhileRestore->operands[0] = (char *)((long int)entryTACIndex);
	BasicBlock_append(whileBlock, beforeNoWhileRestore);

	struct TACLine *noWhileJump = linearizeConditionalJump(currentTACIndex++, it->child->value);
	noWhileJump->operands[0] = (char *)((long int)afterWhileBlock->labelNum);
	BasicBlock_append(whileBlock, noWhileJump);

	struct LinearizationResult *r = linearizeStatementList(table, currentTACIndex, blockList, whileBlock, whileBlock, it->child->sibling->child, tempNum, labelCount, tl);

	for (struct LinkedListNode *TACRunner = r->block->TACList->tail; TACRunner != NULL; TACRunner = TACRunner->prev)
	{
		struct TACLine *examinedTAC = TACRunner->data;
		if (examinedTAC->operation == tt_restorestate)
		{
			examinedTAC->operands[0] = beforeNoWhileRestore->operands[0];
			break;
		}
	}

	struct TACLine *whileEndDo = newTACLine(r->endingTACIndex, tt_enddo);
	BasicBlock_append(r->block, whileEndDo);

	return r;
}

// given the AST for a function, generate TAC and return a pointer to the head of the generated block
struct LinearizationResult *linearizeStatementList(struct symbolTable *table,
												   int currentTACIndex,
												   struct LinkedList *blockList,
												   struct BasicBlock *currentBlock,
												   struct BasicBlock *controlConvergesTo,
												   struct ASTNode *it,
												   int *tempNum,
												   int *labelCount,
												   struct tempList *tl)
{
	// scrape along the function
	struct ASTNode *runner = it;
	while (runner != NULL)
	{
		switch (runner->type)
		{
		case t_asm:
			currentTACIndex = linearizeASMBlock(currentTACIndex, currentBlock, runner);
			break;

		// if we see a variable being declared and then assigned
		// generate the code and stick it on to the end of the block
		case t_var:
		{
			switch (runner->child->type)
			{
			case t_assign:
				currentTACIndex = linearizeAssignment(table, currentTACIndex, blockList, currentBlock, runner->child, tempNum, tl);
				break;

			case t_dereference:
				struct ASTNode *dereferenceScraper = runner->child;
				while (dereferenceScraper->type == t_dereference)
					dereferenceScraper = dereferenceScraper->child;

				if (dereferenceScraper->type == t_assign)
					currentTACIndex = linearizeAssignment(table, currentTACIndex, blockList, currentBlock, dereferenceScraper, tempNum, tl);
				else
					currentTACIndex = linearizeDeclaration(table, currentTACIndex, currentBlock, runner);

				break;

			// if just a declaration, do nothing
			case t_name:
				currentTACIndex = linearizeDeclaration(table, currentTACIndex, currentBlock, runner);
				break;

			default:
				perror("Error linearizing statement - malformed parse tree under 'var' node\n");
				exit(1);
			}
		}
		break;

		// if we see an assignment, generate the code and stick it on to the end of the block
		case t_assign:
			currentTACIndex = linearizeAssignment(table, currentTACIndex, blockList, currentBlock, runner, tempNum, tl);
			break;

		case t_call:
		{
			// for a raw call, return value is not used, null out that operand
			currentTACIndex = linearizeFunctionCall(table, currentTACIndex, blockList, currentBlock, runner, tempNum, tl);
			struct TACLine *lastLine = currentBlock->TACList->tail->data;
			lastLine->operands[0] = NULL;
			lastLine->operandTypes[0] = vt_null;
		}
		break;

		case t_return:
		{
			char *returned;
			enum variableTypes returnedType;

			switch (runner->child->type)
			{
			case t_name:
				returned = runner->child->value;
				returnedType = vt_var;
				break;

			case t_literal:
				returned = runner->child->value;
				returnedType = vt_literal;
				break;

			case t_dereference:
				returned = getTempString(tl, *tempNum);
				returnedType = vt_temp;
				currentTACIndex = linearizeDereference(table, currentTACIndex, blockList, currentBlock, runner->child->child, tempNum, tl);
				break;

			case t_un_add:
			case t_un_sub:
				returned = getTempString(tl, *tempNum);
				returnedType = vt_temp;
				currentTACIndex = linearizeExpression(table, currentTACIndex, blockList, currentBlock, runner->child, tempNum, tl);
				break;

			default:
				perror("Error - Unexpected type within return expression\n");
				exit(2);
				break;
			}
			struct TACLine *returnTac = newTACLine(currentTACIndex++, tt_return);
			returnTac->correspondingTree = runner;
			returnTac->operands[0] = returned;
			returnTac->operandTypes[0] = returnedType;
			BasicBlock_append(currentBlock, returnTac);
		}
		break;

		case t_if:
		{
			// this is the block that control will be passed to after the branch, regardless of what happens
			struct BasicBlock *afterIfBlock = BasicBlock_new(++(*labelCount));
			afterIfBlock->hintLabel = "after if block";

			// linearize the if statement and attached else if it exists
			struct Stack *results = linearizeIfStatement(table, currentTACIndex, blockList, currentBlock, afterIfBlock, runner, tempNum, labelCount, tl);
			LinkedList_append(blockList, afterIfBlock);

			struct Stack *beforeConvergeRestores = Stack_new();

			// grab all generated basic blocks generated by the if statement's linearization
			while (results->size > 0)
			{
				struct LinearizationResult *poppedResult = Stack_pop(results);
				for (struct LinkedListNode *TACRunner = poppedResult->block->TACList->tail; TACRunner != NULL; TACRunner = TACRunner->prev)
				{
					struct TACLine *examinedTAC = TACRunner->data;
					if (examinedTAC->operation == tt_restorestate)
					{
						Stack_push(beforeConvergeRestores, examinedTAC);
						break;
					}
				}
				// skip the current TAC index as far forward as possible so code generated from here on out is always after previous code

				if (poppedResult->endingTACIndex > currentTACIndex)
					currentTACIndex = poppedResult->endingTACIndex + 1;

				free(poppedResult);
			}

			Stack_free(results);

			while (beforeConvergeRestores->size > 0)
			{
				struct TACLine *expireLine = Stack_pop(beforeConvergeRestores);
				expireLine->operands[0] = (char *)((long int)currentTACIndex);
			}

			Stack_free(beforeConvergeRestores);

			// we are now linearizing code into the block which the branches converge to
			currentBlock = afterIfBlock;
			struct TACLine *endPop = newTACLine(currentTACIndex, tt_popstate);
			BasicBlock_append(currentBlock, endPop);
		}
		break;

		case t_while:
		{
			struct BasicBlock *afterWhileBlock = BasicBlock_new(++(*labelCount));
			afterWhileBlock->hintLabel = "after while block";

			struct LinearizationResult *r = linearizeWhileLoop(table, currentTACIndex, blockList, currentBlock, afterWhileBlock, runner, tempNum, labelCount, tl);
			currentTACIndex = r->endingTACIndex;
			LinkedList_append(blockList, afterWhileBlock);
			free(r);

			currentBlock = afterWhileBlock;
			struct TACLine *endPop = newTACLine(currentTACIndex, tt_popstate);
			BasicBlock_append(currentBlock, endPop);
		}
		break;

		default:
			perror("Something broke :(\n");
			exit(1);
		}
		runner = runner->sibling;
	}

	if (controlConvergesTo != NULL)
	{
		struct TACLine *lastLine = currentBlock->TACList->tail->data;
		if (lastLine->operation != tt_return)
		{
			struct TACLine *beforeConvergeRestore = newTACLine(currentTACIndex, tt_restorestate);
			BasicBlock_append(currentBlock, beforeConvergeRestore);

			struct TACLine *convergeControlJump = newTACLine(currentTACIndex++, tt_jmp);
			convergeControlJump->operands[0] = (char *)((long int)controlConvergesTo->labelNum);
			BasicBlock_append(currentBlock, convergeControlJump);
		}
	}

	struct LinearizationResult *r = malloc(sizeof(struct LinearizationResult));
	r->block = currentBlock;
	r->endingTACIndex = currentTACIndex;
	return r;
}

// given an AST and a populated symbol table, generate three address code for the function entries
void linearizeProgram(struct ASTNode *it, struct symbolTable *table)
{
	struct LinkedList *globalBlockList = LinkedList_new();
	struct BasicBlock *globalBlock = BasicBlock_new(0);
	LinkedList_append(globalBlockList, globalBlock);
	// scrape along the top level of the AST
	table->BasicBlockList = globalBlockList;
	struct ASTNode *runner = it;
	int tempNum = 0;
	int currentTACIndex = 0;
	while (runner != NULL)
	{
		switch (runner->type)
		{
		// if we encounter a function, lookup its symbol table entry
		// then generate the TAC for it and add a reference to the start of the generated code to the function entry
		case t_fun:
		{
			int funTempNum = 0; // track the number of temporary variables used
			int labelCount = 0;
			struct functionEntry *theFunction = symbolTableLookup(table, runner->child->value)->entry;
			struct LinkedList *functionBlockList = LinkedList_new();
			theFunction->table->BasicBlockList = functionBlockList;
			struct BasicBlock *functionBlock = BasicBlock_new(funTempNum);
			functionBlock->hintLabel = table->name;

			LinkedList_append(functionBlockList, functionBlock);
			struct LinearizationResult *r = linearizeStatementList(theFunction->table, 0, functionBlockList, functionBlock, NULL, runner->child->sibling, &funTempNum, &labelCount, table->tl);
			free(r);
			break;
		}
		break;

		case t_asm:
			currentTACIndex = linearizeASMBlock(currentTACIndex, globalBlock, runner);
			break;

		case t_var:
			struct ASTNode *declarationScraper = runner;

			// scrape down all pointer levels if necessary, then linearize if the variable is actually assigned
			while (declarationScraper->child->type == t_dereference)
				declarationScraper = declarationScraper->child;

			declarationScraper = declarationScraper->child;
			if (declarationScraper->type == t_assign)
				currentTACIndex = linearizeAssignment(table, currentTACIndex, globalBlockList, globalBlock, declarationScraper, &tempNum, table->tl);

			break;

		case t_assign:
			currentTACIndex = linearizeAssignment(table, currentTACIndex, globalBlockList, globalBlock, runner, &tempNum, table->tl);
			break;

		// ignore everything else (for now) - no global vars, etc...
		default:
			break;
		}
		runner = runner->sibling;
	}
}
