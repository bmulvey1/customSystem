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
		struct TACLine *asmLine = newTACLine(currentTACIndex++, tt_asm, asmRunner);
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
	struct TACLine *thisDereference = newTACLine(currentTACIndex, tt_memr_1, it);

	thisDereference->operands[0] = getTempString(tl, *tempNum);
	thisDereference->operandPermutations[0] = vp_temp;
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
		thisDereference->operandPermutations[1] = vp_temp;

		currentTACIndex = linearizeDereference(table, currentTACIndex, blockList, currentBlock, it->child, tempNum, tl);
		struct TACLine *recursiveDereference = currentBlock->TACList->tail->data;
		thisDereference->operandTypes[1] = recursiveDereference->operandTypes[0];
		thisDereference->indirectionLevels[1] = recursiveDereference->indirectionLevels[0];

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
			LHSSize = symbolTable_getSizeOfVariable(table, theVariable->type);
			break;

		case t_literal:
			thisDereference->operandTypes[1] = vt_var;
			thisDereference->operandPermutations[1] = vp_literal;
			LHSSize = 2;
			break;

		case t_dereference:
			bkpt();
			thisDereference->operands[1] = getTempString(tl, *tempNum);
			thisDereference->operandPermutations[1] = vp_temp;

			currentTACIndex = linearizeDereference(table, currentTACIndex, blockList, currentBlock, it->child->child, tempNum, tl);
			struct TACLine *recursiveDereference = currentBlock->TACList->tail->data;
			thisDereference->operandTypes[1] = recursiveDereference->operandTypes[0];
			thisDereference->indirectionLevels[1] = recursiveDereference->indirectionLevels[0];
			LHSSize = symbolTable_getSizeOfVariable(table, recursiveDereference->operandTypes[0]);
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
				struct TACLine *subtractInvert = newTACLine(currentTACIndex++, tt_mul, it);
				subtractInvert->operands[0] = getTempString(tl, *tempNum);
				subtractInvert->operandPermutations[0] = vp_temp;
				(*tempNum)++;
				char *invertedVariableName = it->child->sibling->value;
				enum variableTypes invertedVariableType = symbolTableLookup_var(table, invertedVariableName)->type;

				subtractInvert->operandTypes[0] = invertedVariableType;
				subtractInvert->operands[1] = invertedVariableName;
				subtractInvert->operandTypes[1] = invertedVariableType;

				subtractInvert->operands[2] = "-1";
				subtractInvert->operandTypes[2] = vt_var;
				subtractInvert->operandPermutations[2] = vp_literal;

				thisDereference->operands[2] = subtractInvert->operands[0];
				thisDereference->operandTypes[2] = invertedVariableType;
				BasicBlock_append(currentBlock, subtractInvert);
			}
			else
			{
				char *variableName = it->child->sibling->value;
				thisDereference->operands[2] = variableName;
				thisDereference->operandTypes[2] = symbolTableLookup_var(table, variableName)->type;
			}

			break;

		// if literal, just use addressing mode base + offset
		case t_literal:
			// thisDereference->operands[1] = thisDereference->operands[2];
			// thisDereference->operandTypes[1] = thisDereference->operandTypes[2];
			thisDereference->operation = tt_memr_2;
			int offset = atoi(it->child->sibling->value);
			thisDereference->operands[2] = (char *)(long int)((offset * 2) * ((it->type == t_un_sub) ? -1 : 1));
			thisDereference->operandTypes[2] = vt_var;
			thisDereference->operandPermutations[2] = vp_literal;
			break;

		case t_un_add:
		case t_un_sub:
			// parent expression type requiers inversion of entire (right) child expression if subtracting
			if (it->type == t_un_sub)
			{
				struct TACLine *subtractInvert = newTACLine(currentTACIndex++, tt_mul, it);
				subtractInvert->operands[0] = getTempString(tl, *tempNum);
				subtractInvert->operandPermutations[0] = vp_temp;
				(*tempNum)++;
				currentTACIndex = linearizeExpression(table, currentTACIndex, blockList, currentBlock, it->child->sibling, tempNum, tl);
				struct TACLine *recursiveExpression = currentBlock->TACList->tail->data;

				// set inverted types based on the expression result type
				subtractInvert->operandTypes[0] = recursiveExpression->operandTypes[0];
				subtractInvert->operands[1] = it->child->sibling->value;
				subtractInvert->operandTypes[1] = recursiveExpression->operandTypes[0];

				subtractInvert->operands[2] = "-1";
				subtractInvert->operandTypes[2] = vt_var;
				subtractInvert->operandPermutations[2] = vp_literal;

				thisDereference->operands[2] = subtractInvert->operands[0];
				thisDereference->operandTypes[2] = subtractInvert->operandTypes[0];
				BasicBlock_append(currentBlock, subtractInvert);
			}
			else
			{
				thisDereference->operands[2] = getTempString(tl, *tempNum);
				thisDereference->operandTypes[2] = vp_temp;
				currentTACIndex = linearizeExpression(table, currentTACIndex, blockList, currentBlock, it->child->sibling, tempNum, tl);
			}
			thisDereference->operandPermutations[2] = vp_temp;
			break;

		default:
			perror("Malformed parse tree in RHS of dereference arithmetic!\n");
			exit(1);
		}
		break;

	default:
		perror("Malformed parse tree when linearizing dereference!\n");
		exit(2);
	}

	thisDereference->operandTypes[0] = thisDereference->operandTypes[1];
	thisDereference->operandPermutations[0] = thisDereference->operandPermutations[1];

	thisDereference->index = currentTACIndex++;
	int newIndirection = thisDereference->indirectionLevels[1];
	if (newIndirection > 0)
	{
		newIndirection--;
	}
	else
	{
		printf("Warning - dereference of non-indirect expression or statement\n\t");
		ASTNode_printHorizontal(it);
		printf("\n\tLine %d, Col %d\n", it->sourceLine, it->sourceCol);
	}

	thisDereference->indirectionLevels[0] = newIndirection;
	BasicBlock_append(currentBlock, thisDereference);
	return currentTACIndex;
}

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
		thisArgument = newTACLine(currentTACIndex++, tt_push, argRunner);
		thisArgument->operandTypes[0] = vt_var;
	case t_literal:
	{
		if (thisArgument == NULL)
		{
			thisArgument = newTACLine(currentTACIndex++, tt_push, argRunner);
			thisArgument->operandTypes[0] = vt_var;
			thisArgument->operandPermutations[0] = vp_literal;
		}
		thisArgument->operands[0] = argRunner->value;
	}
	break;

	case t_dereference:
	{
		currentTACIndex = linearizeDereference(table, currentTACIndex, blockList, currentBlock, argRunner->child, tempNum, tl);
		thisArgument = newTACLine(currentTACIndex++, tt_push, argRunner);
		struct TACLine *recursiveDereference = currentBlock->TACList->tail->data;
		thisArgument->operands[0] = recursiveDereference->operands[0];
		thisArgument->operandTypes[0] = recursiveDereference->operandTypes[0];
		thisArgument->indirectionLevels[0] = recursiveDereference->indirectionLevels[0];
		thisArgument->operandPermutations[0] = recursiveDereference->operandPermutations[0];
	}
	break;

	case t_un_add:
	case t_un_sub:
	{
		currentTACIndex = linearizeExpression(table, currentTACIndex, blockList, currentBlock, argRunner, tempNum, tl);
		struct TACLine *recursiveExpression = currentBlock->TACList->tail->data;

		thisArgument = newTACLine(currentTACIndex++, tt_push, argRunner);
		thisArgument->operands[0] = recursiveExpression->operands[0];
		thisArgument->operandTypes[0] = recursiveExpression->operandTypes[0];
		thisArgument->indirectionLevels[0] = recursiveExpression->indirectionLevels[0];
		thisArgument->operandPermutations[0] = recursiveExpression->operandPermutations[0];
	}
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

	struct TACLine *calltac = newTACLine(currentTACIndex++, tt_call, it);
	calltac->operands[0] = operand0;
	calltac->operandPermutations[0] = vp_temp;

	// no type check because it contains the name of the function itself

	char *functionName = it->child->value;
	calltac->operands[1] = functionName;

	calltac->operandTypes[0] = symbolTableLookup_fun(table, functionName)->returnType;
	// TODO: set variabletype based on function return type, error if void function

	// struct functionEntry *calledFunction = symbolTableLookup_fun(table, functionName);
	// calltac->operandTypes[1] = calledFunction->returnType;

	BasicBlock_append(currentBlock, calltac);
	return currentTACIndex;
}

// linearize any subexpression which results in the use of a temporary variable
int linearizeSubExpression(struct symbolTable *table,
						   int currentTACIndex,
						   struct LinkedList *blockList,
						   struct BasicBlock *currentBlock,
						   struct ASTNode *it,
						   int *tempNum,
						   struct tempList *tl,
						   struct TACLine *parentExpression,
						   int operandIndex)
{
	parentExpression->operands[operandIndex] = getTempString(tl, *tempNum);
	parentExpression->operandPermutations[operandIndex] = vp_temp;
	switch (it->type)
	{

	// handle recording return types from function calls here
	case t_call:
	{
		currentTACIndex = linearizeFunctionCall(table, currentTACIndex, blockList, currentBlock, it, tempNum, tl);
		struct TACLine *recursiveFunctionCall = currentBlock->TACList->tail->data;

		parentExpression->operandTypes[operandIndex] = recursiveFunctionCall->operandTypes[0];
		parentExpression->indirectionLevels[operandIndex] = recursiveFunctionCall->indirectionLevels[1];
	}
	break;

	case t_compOp:
	case t_un_add:
	case t_un_sub:
	{
		currentTACIndex = linearizeExpression(table, currentTACIndex, blockList, currentBlock, it, tempNum, tl);
		struct TACLine *recursiveExpression = currentBlock->TACList->tail->data;

		parentExpression->operandTypes[operandIndex] = recursiveExpression->operandTypes[1];
		parentExpression->indirectionLevels[operandIndex] = recursiveExpression->indirectionLevels[0];
	}
	break;

	case t_dereference:
	{
		currentTACIndex = linearizeDereference(table, currentTACIndex, blockList, currentBlock, it->child, tempNum, tl);
		struct TACLine *recursiveDereference = currentBlock->TACList->tail->data;

		parentExpression->operandTypes[operandIndex] = recursiveDereference->operandTypes[0];
		parentExpression->indirectionLevels[operandIndex] = recursiveDereference->indirectionLevels[0];
	}
	break;

	default:
		perror("Unexpected type seen while linearizing subexpression!\n");
		exit(2);
	}
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
	struct TACLine *thisExpression = newTACLine(currentTACIndex, tt_assign, it);

	// since 'cmp' doesn't generate a result, it just sets flags, no need to consume a temp
	if (it->type != t_compOp)
	{
		thisExpression->operands[0] = getTempString(tl, *tempNum);
		thisExpression->operandPermutations[0] = vp_temp;
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
			thisExpression->operandPermutations[1] = vp_temp;

			currentTACIndex = linearizeDereference(table, currentTACIndex, blockList, currentBlock, it->child, tempNum, tl);
			struct TACLine *recursiveDereference = currentBlock->TACList->tail->data;

			thisExpression->indirectionLevels[1] = recursiveDereference->indirectionLevels[0];
			thisExpression->operandTypes[1] = recursiveDereference->operandTypes[0];
		}

		thisExpression->index = currentTACIndex++;
		BasicBlock_append(currentBlock, thisExpression);
		return currentTACIndex;
	}

	// if we fall through to here, the expression is some sort of unary operation
	// handle the LHS of the operation
	switch (it->child->type)
	{
	case t_call:
	case t_compOp:
	case t_un_add:
	case t_un_sub:
	case t_dereference:
	{
		currentTACIndex = linearizeSubExpression(table, currentTACIndex, blockList, currentBlock, it->child, tempNum, tl, thisExpression, 1);
	}
	break;

	case t_name:
	{
		thisExpression->operands[1] = it->child->value;
		struct variableEntry *theVariable = symbolTableLookup_var(table, it->child->value);
		thisExpression->operandTypes[1] = theVariable->type;
		thisExpression->indirectionLevels[1] = theVariable->indirectionLevel;
	}
	break;

	case t_literal:
	{
		thisExpression->operands[1] = it->child->value;
		thisExpression->operandTypes[1] = vt_var;
		thisExpression->operandPermutations[1] = vp_literal;
		// indirection levels set to 0 by default
	}
	break;

	default:
		perror("Unexpected type seen while linearizing expression LHS! Expected variable, literal, or subexpression\n");
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
	case t_compOp:
	case t_un_add:
	case t_un_sub:
	case t_dereference:
	{
		currentTACIndex = linearizeSubExpression(table, currentTACIndex, blockList, currentBlock, it->child->sibling, tempNum, tl, thisExpression, 2);
	}
	break;

	case t_name:
	{
		thisExpression->operands[2] = it->child->sibling->value;
		struct variableEntry *theVariable = symbolTableLookup_var(table, it->child->sibling->value);
		thisExpression->operandTypes[2] = theVariable->type;
		thisExpression->indirectionLevels[2] = theVariable->indirectionLevel;
	}
	break;

	case t_literal:
	{
		thisExpression->operands[2] = it->child->sibling->value;
		thisExpression->operandTypes[2] = vt_var;
		thisExpression->operandPermutations[2] = vp_literal;
		// indirection levels set to 0 by default
	}
	break;

	default:
		perror("Unexpected type seen while linearizing expression RHS! Expected variable, literal, or subexpression\n");
		exit(2);
	}

	if (thisExpression->operation != tt_cmp)
	{
		// TODO (with type system) - properly determine type of expression when different operands
		thisExpression->operandTypes[0] = vt_var;
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
	ASTNode_printHorizontal(it);
	printf("\n");
	// if this assignment is simply setting one thing to another
	if (it->child->sibling->child == NULL)
	{
		// pull out the relevant values and generate a single line of TAC to return
		struct TACLine *assignment = newTACLine(currentTACIndex++, tt_assign, it);
		assignment->operands[1] = it->child->sibling->value;

		switch (it->child->sibling->type)
		{
		case t_literal:
			assignment->operandTypes[1] = vt_var;
			assignment->operandPermutations[1] = vp_literal;
			break;

		case t_name:
		{
			struct variableEntry *theVariable = symbolTableLookup_var(table, it->child->sibling->value);
			assignment->operandTypes[1] = theVariable->type;
			assignment->indirectionLevels[1] = theVariable->indirectionLevel;
		}
		break;

		default:
			perror("Error parsing simple assignment - unexpected type on RHS\n");
			exit(2);
		}

		BasicBlock_append(currentBlock, assignment);
	}
	else
	// otherwise there is some sort of expression, which will need to be broken down into multiple lines of TAC
	{
		// TODO: use linearizesubexpression?
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

	struct TACLine *RHS = currentBlock->TACList->tail->data;
	if (it->child->type == t_name)
	{
		printf("lhs is name\n");
		struct variableEntry *assignedVariable = symbolTableLookup_var(table, it->child->value);
		RHS->operands[0] = it->child->value;
		RHS->operandTypes[0] = assignedVariable->type;
		RHS->indirectionLevels[0] = assignedVariable->indirectionLevel;
	}
	else
	{
		struct TACLine *finalAssignment = NULL;
		RHS->operands[0] = getTempString(tl, *tempNum);
		RHS->operandPermutations[0] = vp_temp;
		(*tempNum)++;
		RHS->operandTypes[0] = RHS->operandTypes[0];
		RHS->indirectionLevels[0] = RHS->indirectionLevels[1];
		switch (it->child->type)
		{
		case t_dereference:
		{
			struct ASTNode *dereferencedExpression = it->child->child;

			switch (dereferencedExpression->type)
			{
			case t_name:
			case t_dereference:
			{
				currentTACIndex = linearizeDereference(table, currentTACIndex, blockList, currentBlock, dereferencedExpression, tempNum, tl);
				struct TACLine *thisDereference = currentBlock->TACList->tail->data;
				finalAssignment = newTACLine(currentTACIndex++, tt_memw_1, dereferencedExpression);
				finalAssignment->operands[1] = RHS->operands[0];
				finalAssignment->operandPermutations[1] = RHS->operandPermutations[0];
				finalAssignment->operandTypes[1] = RHS->operandTypes[0];
				finalAssignment->indirectionLevels[1] = RHS->indirectionLevels[0];

				finalAssignment->operands[0] = thisDereference->operands[0];
				finalAssignment->operandPermutations[0] = thisDereference->operandPermutations[0];
				finalAssignment->operandTypes[0] = thisDereference->operandTypes[0];
				finalAssignment->indirectionLevels[0] = thisDereference->indirectionLevels[0];
			}
			break;

			case t_un_add:
			case t_un_sub:
			{

				// linearize the RHS of the dereferenced arithmetic
				struct ASTNode *dereferencedRHS = dereferencedExpression->child->sibling;
				switch (dereferencedRHS->type)
				{
				case t_literal:
				{
					finalAssignment = newTACLine(currentTACIndex++, tt_memw_2, dereferencedRHS);
					finalAssignment->operands[1] = (char *)(long int)atoi(dereferencedRHS->value);
					finalAssignment->operandPermutations[1] = vp_literal;
					finalAssignment->operandTypes[1] = vt_var;
					finalAssignment->indirectionLevels[1] = 0;
				}
				break;

				case t_name:
				{
					finalAssignment = newTACLine(currentTACIndex++, tt_memw_3, dereferencedRHS);
					struct variableEntry *theVariable = symbolTableLookup_var(table, dereferencedRHS->value);
					finalAssignment->operands[1] = dereferencedRHS->value;
					finalAssignment->operandTypes[1] = theVariable->type;
					finalAssignment->indirectionLevels[1] = theVariable->indirectionLevel;
				}
				break;

				// all other arithmetic goes here
				case t_un_add:
				case t_un_sub:
				{
					currentTACIndex = linearizeExpression(table, currentTACIndex, blockList, currentBlock, dereferencedRHS, tempNum, tl);
					struct TACLine *finalArithmeticLine = currentBlock->TACList->tail->data;
					finalAssignment = newTACLine(currentTACIndex++, tt_memw_3, dereferencedRHS);

					// struct variableEntry *theVariable = symbolTableLookup_var(table, dereferencedRHS->value);
					finalAssignment->operands[1] = finalArithmeticLine->operands[0];
					finalAssignment->operandPermutations[1] = finalArithmeticLine->operandPermutations[0];
					finalAssignment->operandTypes[1] = finalArithmeticLine->operandTypes[0];
					finalAssignment->indirectionLevels[1] = finalArithmeticLine->indirectionLevels[0];
				}
				break;

				default:
					perror("Error - Unexpected type on RHS of pointer write arithmetic\n");
					exit(2);
				}

				int lhsSize = 0;
				// check the LHS of the add
				switch (dereferencedExpression->child->type)
				{
				case t_name:
				{
					struct variableEntry *lhsVariable = symbolTableLookup_var(table, dereferencedExpression->child->value);
					finalAssignment->operands[0] = dereferencedExpression->child->value;
					finalAssignment->operandTypes[0] = lhsVariable->type;
					finalAssignment->indirectionLevels[0] = lhsVariable->indirectionLevel;
					lhsSize = symbolTable_getSizeOfVariable(table, lhsVariable->type);
				}
				break;

				default:
					perror("Error - Illegal type on LHS of assignment expression\n\tPointer write operations can only bind rightwards\n");
					exit(2);
				}

				switch (finalAssignment->operation)
				{
				case tt_memw_2:
				{
					finalAssignment->operands[1] = (char *)((long int)lhsSize * (long int)(finalAssignment->operands[1]));
					finalAssignment->operandPermutations[1] = vp_literal;
					finalAssignment->operandTypes[1] = vt_var;
					finalAssignment->indirectionLevels[1] = 0;

					// make offset value negative if subtracting
					if (dereferencedExpression->type == t_un_sub)
					{
						finalAssignment->operands[1] = (char *)((long int)finalAssignment->operands[1] * -1);
					}

					finalAssignment->operands[2] = RHS->operands[0];
					finalAssignment->operandPermutations[2] = RHS->operandPermutations[0];
					finalAssignment->operandTypes[2] = RHS->operandTypes[0];
					finalAssignment->indirectionLevels[2] = RHS->indirectionLevels[0];
				}
				break;

				case tt_memw_3:
				{
					finalAssignment->operands[2] = (char *)(long int)lhsSize;
					finalAssignment->operandPermutations[2] = vp_literal;
					finalAssignment->operandTypes[2] = vt_var;
					finalAssignment->indirectionLevels[2] = 0;

					// make scale value negative if subtracting
					if (dereferencedExpression->type == t_un_sub)
					{
						finalAssignment->operands[2] = (char *)((long int)finalAssignment->operands[2] * -1);
					}

					finalAssignment->operands[3] = RHS->operands[0];
					finalAssignment->operandPermutations[3] = RHS->operandPermutations[0];
					finalAssignment->operandTypes[3] = RHS->operandTypes[0];
					finalAssignment->indirectionLevels[3] = RHS->indirectionLevels[0];
				}
				break;

				default:
					perror("Error - Unexpected final assignment TAC type\n");
					exit(2);
				}
			}
			break;

			default:
				printf("Error - Unexpected type within dereference on LHS of assignment expression\n\t");
				ASTNode_printHorizontal(dereferencedExpression);
				printf("\n");
				exit(2);
			}
		}
		break;

		default:
			printf("Error - Unexpected type on LHS of assignment expression\n\t");
			ASTNode_printHorizontal(it->child);
			printf("\n");
			exit(2);
		}
		BasicBlock_append(currentBlock, finalAssignment);
	}

	return currentTACIndex;
}

struct TACLine *linearizeConditionalJump(int currentTACIndex, char *cmpOp, struct ASTNode *correspondingTree)
{
	struct TACLine *notMetJump;
	switch (cmpOp[0])
	{
	case '<':
		switch (cmpOp[1])
		{
		case '=':
			notMetJump = newTACLine(currentTACIndex, tt_jg, correspondingTree);
			break;

		case '\0':
			notMetJump = newTACLine(currentTACIndex, tt_jge, correspondingTree);
			break;

		default:
			perror("Error - Unexpected value in comparison operator node\n");
			exit(2);
			break;
		}
		break;

	case '>':
		switch (cmpOp[1])
		{
		case '=':
			notMetJump = newTACLine(currentTACIndex, tt_jl, correspondingTree);
			break;

		case '\0':
			notMetJump = newTACLine(currentTACIndex, tt_jle, correspondingTree);
			break;

		default:
			perror("Error - Unexpected value in comparison operator node\n");
			exit(2);
			break;
		}
		break;

	case '!':
		notMetJump = newTACLine(currentTACIndex, tt_je, correspondingTree);
		break;

	case '=':
		notMetJump = newTACLine(currentTACIndex, tt_jne, correspondingTree);
		break;
	}
	return notMetJump;
}

int linearizeDeclaration(struct symbolTable *table,
						 int currentTACIndex,
						 struct BasicBlock *currentBlock,
						 struct ASTNode *it)
{
	struct TACLine *declarationLine = newTACLine(currentTACIndex++, tt_declare, it);
	enum variableTypes declaredType;
	switch (it->type)
	{
	case t_var:
		declaredType = vt_var;
		break;

	default:
		perror("Unexpected type seen while linearizing declaration!");
		exit(2);
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
	struct TACLine *pushState = newTACLine(currentTACIndex, tt_pushstate, it);
	BasicBlock_append(currentBlock, pushState);

	// generate a label and figure out condition to jump when the if statement isn't executed
	struct TACLine *noifJump = linearizeConditionalJump(currentTACIndex++, it->child->value, it->child);

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
		struct TACLine *resetLine = newTACLine(elseTACIndex, tt_resetstate, it);
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
	struct TACLine *pushState = newTACLine(currentTACIndex, tt_pushstate, it);
	BasicBlock_append(currentBlock, pushState);

	struct BasicBlock *whileBlock = BasicBlock_new(++(*labelCount));
	whileBlock->hintLabel = "while block";
	LinkedList_append(blockList, whileBlock);

	struct TACLine *enterWhileJump = newTACLine(currentTACIndex++, tt_jmp, it);
	enterWhileJump->operands[0] = (char *)((long int)whileBlock->labelNum);
	BasicBlock_append(currentBlock, enterWhileJump);

	struct TACLine *whileDo = newTACLine(currentTACIndex, tt_do, it);
	BasicBlock_append(whileBlock, whileDo);

	// linearize the expression we will test
	currentTACIndex = linearizeExpression(table, currentTACIndex, blockList, whileBlock, it->child, tempNum, tl);

	// restore state before the conditional jump that exits the loop
	struct TACLine *beforeNoWhileRestore = newTACLine(currentTACIndex, tt_restorestate, it);
	beforeNoWhileRestore->operands[0] = (char *)((long int)entryTACIndex);
	BasicBlock_append(whileBlock, beforeNoWhileRestore);

	struct TACLine *noWhileJump = linearizeConditionalJump(currentTACIndex++, it->child->value, it->child);
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

	struct TACLine *whileEndDo = newTACLine(r->endingTACIndex, tt_enddo, it);
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
				exit(2);
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
			enum variablePermutations returnedPermutation;

			switch (runner->child->type)
			{
			case t_name:
			{
				returned = runner->child->value;
				returnedType = symbolTableLookup_var(table, returned)->type;
				returnedPermutation = vp_standard;
			}
			break;

			case t_literal:
			{
				returned = runner->child->value;
				returnedType = vt_var;
				returnedPermutation = vp_literal;
			}
			break;

			case t_dereference:
			{
				returned = getTempString(tl, *tempNum);
				currentTACIndex = linearizeDereference(table, currentTACIndex, blockList, currentBlock, runner->child->child, tempNum, tl);
				struct TACLine *recursiveDereference = currentBlock->TACList->tail->data;
				returnedType = recursiveDereference->operandTypes[0];
				returnedPermutation = recursiveDereference->operandPermutations[0];
			}
			break;

			case t_un_add:
			case t_un_sub:
			{
				returned = getTempString(tl, *tempNum);
				currentTACIndex = linearizeExpression(table, currentTACIndex, blockList, currentBlock, runner->child, tempNum, tl);
				struct TACLine *recursiveExpression = currentBlock->TACList->tail->data;
				returnedType = recursiveExpression->operandTypes[0];
				returnedPermutation = recursiveExpression->operandPermutations[0];
			}
			break;

			default:
				perror("Error - Unexpected type within return expression\n");
				exit(2);
				break;
			}
			struct TACLine *returnTac = newTACLine(currentTACIndex++, tt_return, runner);
			returnTac->operands[0] = returned;
			returnTac->operandPermutations[0] = returnedPermutation;
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
			struct TACLine *endPop = newTACLine(currentTACIndex, tt_popstate, runner);
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
			struct TACLine *endPop = newTACLine(currentTACIndex, tt_popstate, runner);
			BasicBlock_append(currentBlock, endPop);
		}
		break;

		default:
			perror("Error - Unexpected node type when linearizing statement\n");
			exit(2);
		}
		runner = runner->sibling;
	}

	if (controlConvergesTo != NULL)
	{
		struct TACLine *lastLine = currentBlock->TACList->tail->data;
		if (lastLine->operation != tt_return)
		{
			struct TACLine *beforeConvergeRestore = newTACLine(currentTACIndex, tt_restorestate, runner);
			BasicBlock_append(currentBlock, beforeConvergeRestore);

			struct TACLine *convergeControlJump = newTACLine(currentTACIndex++, tt_jmp, runner);
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
			perror("Error - Unexpected statement type at global scope\n");
			exit(2);
			break;
		}
		runner = runner->sibling;
	}
}
