#include "linearizer.h"

struct TempList *temps = NULL;

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

int linearizeDereference(struct Scope *scope,
						 int currentTACIndex,
						 struct BasicBlock *currentBlock,
						 struct ASTNode *it,
						 int *tempNum)
{
	struct TACLine *thisDereference = newTACLine(currentTACIndex, tt_memr_1, it);

	thisDereference->operands[0] = TempList_getString(temps, *tempNum);
	thisDereference->operandPermutations[0] = vp_temp;
	(*tempNum)++;

	switch (it->type)
	{
		// directly dereference variables
	case t_name:
	{
		thisDereference->operands[1] = it->value;
		thisDereference->operandTypes[1] = vt_var;
		thisDereference->indirectionLevels[1] = Scope_lookupVar(scope, it->value)->indirectionLevel;
	}
	break;

		// recursively dereference nested dereferences
	case t_dereference:
	{
		thisDereference->operands[1] = TempList_getString(temps, *tempNum);
		thisDereference->operandPermutations[1] = vp_temp;

		currentTACIndex = linearizeDereference(scope, currentTACIndex, currentBlock, it->child, tempNum);
		struct TACLine *recursiveDereference = currentBlock->TACList->tail->data;
		thisDereference->operandTypes[1] = recursiveDereference->operandTypes[0];
		thisDereference->indirectionLevels[1] = recursiveDereference->indirectionLevels[0];
	}
	break;

		// handle pointer arithmetic to evalute the correct adddress to dereference
	case t_un_add:
	case t_un_sub:
	{
		thisDereference->operands[1] = it->child->value; // base
		int LHSSize;
		switch (it->child->type)
		{
		case t_name:
		{
			struct VariableEntry *theVariable = Scope_lookupVar(scope, it->child->value);
			thisDereference->operandTypes[1] = theVariable->type;
			thisDereference->indirectionLevels[1] = theVariable->indirectionLevel;
			LHSSize = Scope_getSizeOfVariable(scope, it->child->value);
		}
		break;

		case t_literal:
		{
			thisDereference->operandTypes[1] = vt_var;
			thisDereference->operandPermutations[1] = vp_literal;
			LHSSize = 2;
		}
		break;

		case t_dereference:
		{
			thisDereference->operands[1] = TempList_getString(temps, *tempNum);
			thisDereference->operandPermutations[1] = vp_temp;

			currentTACIndex = linearizeDereference(scope, currentTACIndex, currentBlock, it->child->child, tempNum);
			struct TACLine *recursiveDereference = currentBlock->TACList->tail->data;
			thisDereference->operandTypes[1] = recursiveDereference->operandTypes[0];
			thisDereference->indirectionLevels[1] = recursiveDereference->indirectionLevels[0];
			LHSSize = Scope_getSizeOfVariable(scope, recursiveDereference->operands[0]);
		}
		break;

		default:
			ASTNode_printHorizontal(it);
			ErrorAndExit(ERROR_INTERNAL, "Illegal type on LHS of dereferenced expression\n");
		}
		thisDereference->operation = tt_memr_3;
		thisDereference->operands[3] = (char *)(long int)LHSSize; // scale
		thisDereference->operandTypes[3] = vt_var;
		thisDereference->operandPermutations[3] = vp_literal;
		switch (it->child->sibling->type) // offset
		{
		case t_name:
			if (it->type == t_un_sub)
			{
				struct TACLine *subtractInvert = newTACLine(currentTACIndex++, tt_mul, it);
				subtractInvert->operands[0] = TempList_getString(temps, *tempNum);
				subtractInvert->operandPermutations[0] = vp_temp;
				(*tempNum)++;
				char *invertedVariableName = it->child->sibling->value;
				struct VariableEntry *invertedVariable = Scope_lookupVar(scope, invertedVariableName);

				subtractInvert->operandTypes[0] = invertedVariable->type;
				subtractInvert->operands[1] = invertedVariableName;
				subtractInvert->operandTypes[1] = invertedVariable->type;

				subtractInvert->operands[2] = "-1";
				subtractInvert->operandTypes[2] = vt_var;
				subtractInvert->operandPermutations[2] = vp_literal;

				thisDereference->operands[2] = subtractInvert->operands[0];
				thisDereference->operandTypes[2] = invertedVariable->type;
				thisDereference->indirectionLevels[2] = invertedVariable->indirectionLevel;
				BasicBlock_append(currentBlock, subtractInvert);
			}
			else
			{
				char *variableName = it->child->sibling->value;
				thisDereference->operands[2] = variableName;
				struct VariableEntry *theVariable = Scope_lookupVar(scope, variableName);
				thisDereference->operandTypes[2] = theVariable->type;
				thisDereference->indirectionLevels[2] = theVariable->indirectionLevel;
			}

			break;

		// if literal, just use addressing mode base + offset
		case t_literal:
		{
			// thisDereference->operands[1] = thisDereference->operands[2];
			// thisDereference->operandTypes[1] = thisDereference->operandTypes[2];
			thisDereference->operation = tt_memr_2;

			// invalidate the previously set 4th operand, will be unused
			thisDereference->operandTypes[3] = vt_null;

			int offset = atoi(it->child->sibling->value);
			thisDereference->operands[2] = (char *)(long int)((offset * 2) * ((it->type == t_un_sub) ? -1 : 1));
			thisDereference->operandTypes[2] = vt_var;
			thisDereference->operandPermutations[2] = vp_literal;
		}
		break;

		case t_un_add:
		case t_un_sub:
			// parent expression type requiers inversion of entire (right) child expression if subtracting
			if (it->type == t_un_sub)
			{
				struct TACLine *subtractInvert = newTACLine(currentTACIndex++, tt_mul, it);
				subtractInvert->operands[0] = TempList_getString(temps, *tempNum);
				subtractInvert->operandPermutations[0] = vp_temp;
				(*tempNum)++;
				currentTACIndex = linearizeExpression(scope, currentTACIndex, currentBlock, it->child->sibling, tempNum);
				struct TACLine *recursiveExpression = currentBlock->TACList->tail->data;

				// set inverted types based on the expression result type
				subtractInvert->operandTypes[0] = recursiveExpression->operandTypes[0];
				// subtractInvert->operands[1] = it->child->sibling->value; // use expression operand instead?
				subtractInvert->operands[1] = recursiveExpression->operands[0];
				subtractInvert->operandTypes[1] = recursiveExpression->operandTypes[0];
				subtractInvert->indirectionLevels[1] = recursiveExpression->indirectionLevels[0];

				subtractInvert->operands[2] = "-1";
				subtractInvert->operandTypes[2] = vt_var;
				subtractInvert->operandPermutations[2] = vp_literal;

				thisDereference->operands[2] = subtractInvert->operands[0];
				thisDereference->operandTypes[2] = subtractInvert->operandTypes[0];
				BasicBlock_append(currentBlock, subtractInvert);
			}
			else
			{
				thisDereference->operands[2] = TempList_getString(temps, *tempNum);
				thisDereference->operandTypes[2] = vp_temp;
				currentTACIndex = linearizeExpression(scope, currentTACIndex, currentBlock, it->child->sibling, tempNum);
			}
			thisDereference->operandPermutations[2] = vp_temp;
			break;

		default:
			ErrorAndExit(ERROR_INTERNAL, "Malformed parse tree in RHS of dereference arithmetic!\n");
		}
	}
	break;

	default:
		ErrorAndExit(ERROR_INTERNAL, "Malformed parse tree when linearizing dereference!\n");
	}

	thisDereference->operandTypes[0] = thisDereference->operandTypes[1];
	// thisDereference->operandPermutations[0] = thisDereference->operandPermutations[1];

	thisDereference->index = currentTACIndex++;
	int newIndirection = thisDereference->indirectionLevels[1];
	if (newIndirection > 0)
	{
		newIndirection--;
	}
	else
	{
		printf("\n%s - ", thisDereference->operands[1]);
		printf("Warning - dereference of non-indirect expression or statement\n\t");
		ASTNode_printHorizontal(it);
		printf("\n\tLine %d, Col %d\n", it->sourceLine, it->sourceCol);
		printTACLine(thisDereference);
		printf("\n");
	}

	thisDereference->indirectionLevels[0] = newIndirection;
	BasicBlock_append(currentBlock, thisDereference);
	return currentTACIndex;
}

int linearizeArgumentPushes(struct Scope *scope,
							int currentTACIndex,
							struct BasicBlock *currentBlock,
							struct ASTNode *argRunner,
							int *tempNum)
{

	if (argRunner->sibling != NULL)
	{
		currentTACIndex = linearizeArgumentPushes(scope, currentTACIndex, currentBlock, argRunner->sibling, tempNum);
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
		currentTACIndex = linearizeDereference(scope, currentTACIndex, currentBlock, argRunner->child, tempNum);
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
		currentTACIndex = linearizeExpression(scope, currentTACIndex, currentBlock, argRunner, tempNum);
		struct TACLine *recursiveExpression = currentBlock->TACList->tail->data;

		thisArgument = newTACLine(currentTACIndex++, tt_push, argRunner);
		thisArgument->operands[0] = recursiveExpression->operands[0];
		thisArgument->operandTypes[0] = recursiveExpression->operandTypes[0];
		thisArgument->indirectionLevels[0] = recursiveExpression->indirectionLevels[0];
		thisArgument->operandPermutations[0] = recursiveExpression->operandPermutations[0];
	}
	break;

	default:
		ErrorAndExit(ERROR_INTERNAL, "Error - Unexpected argument node type\n");
	}
	if (thisArgument != NULL)
		BasicBlock_append(currentBlock, thisArgument);

	return currentTACIndex;
}

// given an AST node of a function call, generate TAC to evaluate and push the arguments, then call it
int linearizeFunctionCall(struct Scope *scope,
						  int currentTACIndex,
						  struct BasicBlock *currentBlock,
						  struct ASTNode *it,
						  int *tempNum)
{
	char *operand0 = TempList_getString(temps, *tempNum);
	char *functionName = it->child->value;
	struct FunctionEntry *calledFunction = Scope_lookupFun(scope, functionName);

	if (calledFunction->returnType != vt_null)
	{
		(*tempNum)++;
	}

	// push arguments iff they exist
	if (it->child->child != NULL)
	{
		currentTACIndex = linearizeArgumentPushes(scope, currentTACIndex, currentBlock, it->child->child, tempNum);
	}

	struct TACLine *calltac = newTACLine(currentTACIndex++, tt_call, it);
	calltac->operands[0] = operand0;

	// always set the return permutation to temp
	// null vs non-null type will be the handler for whether the return value exists
	calltac->operandPermutations[0] = vp_temp;

	// no type check because it contains the name of the function itself

	calltac->operands[1] = functionName;

	if (calledFunction->returnType != vt_null)
	{
		calltac->operandTypes[0] = calledFunction->returnType;
	}
	// TODO: set variabletype based on function return type, error if void function

	// struct FunctionEntry *calledFunction = symbolTableLookup_fun(table, functionName);
	// calltac->operandTypes[1] = calledFunction->returnType;

	BasicBlock_append(currentBlock, calltac);
	return currentTACIndex;
}

// linearize any subexpression which results in the use of a temporary variable
int linearizeSubExpression(struct Scope *scope,
						   int currentTACIndex,
						   struct BasicBlock *currentBlock,
						   struct ASTNode *it,
						   int *tempNum,
						   struct TACLine *parentExpression,
						   int operandIndex)
{
	parentExpression->operands[operandIndex] = TempList_getString(temps, *tempNum);
	parentExpression->operandPermutations[operandIndex] = vp_temp;

	switch (it->type)
	{
	// handle recording return types from function calls here
	case t_call:
	{
		currentTACIndex = linearizeFunctionCall(scope, currentTACIndex, currentBlock, it, tempNum);
		struct TACLine *recursiveFunctionCall = currentBlock->TACList->tail->data;

		// TODO: check return type (or at least that function returns something)

		// using a returned value will always be a temp
		parentExpression->operandPermutations[operandIndex] = vp_temp;
		parentExpression->operandTypes[operandIndex] = recursiveFunctionCall->operandTypes[0];
		parentExpression->indirectionLevels[operandIndex] = recursiveFunctionCall->indirectionLevels[1];
	}
	break;

	case t_compOp:
	case t_un_add:
	case t_un_sub:
	{
		currentTACIndex = linearizeExpression(scope, currentTACIndex, currentBlock, it, tempNum);
		struct TACLine *recursiveExpression = currentBlock->TACList->tail->data;

		parentExpression->operandTypes[operandIndex] = recursiveExpression->operandTypes[1];
		parentExpression->indirectionLevels[operandIndex] = recursiveExpression->indirectionLevels[0];
	}
	break;

	case t_dereference:
	{
		currentTACIndex = linearizeDereference(scope, currentTACIndex, currentBlock, it->child, tempNum);
		struct TACLine *recursiveDereference = currentBlock->TACList->tail->data;

		parentExpression->operandTypes[operandIndex] = recursiveDereference->operandTypes[0];
		parentExpression->indirectionLevels[operandIndex] = recursiveDereference->indirectionLevels[0];
	}
	break;

	default:
		ErrorAndExit(ERROR_INTERNAL, "Unexpected type seen while linearizing subexpression!\n");
	}
	return currentTACIndex;
}

// given an AST node of an expression, figure out how to break it down into multiple lines of three address code
int linearizeExpression(struct Scope *scope,
						int currentTACIndex,
						struct BasicBlock *currentBlock,
						struct ASTNode *it,
						int *tempNum)
{
	// set to tt_assign, reassign in switch body based on operator later
	// also set true TAC index at bottom of function, after child expression linearizations take place
	struct TACLine *thisExpression = newTACLine(currentTACIndex, tt_assign, it);

	// since 'cmp' doesn't generate a result, it just sets flags, no need to consume a temp
	if (it->type != t_compOp)
	{
		thisExpression->operands[0] = TempList_getString(temps, *tempNum);
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
			thisExpression->operands[1] = TempList_getString(temps, *tempNum);
			thisExpression->operandPermutations[1] = vp_temp;

			currentTACIndex = linearizeDereference(scope, currentTACIndex, currentBlock, it->child, tempNum);
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
		currentTACIndex = linearizeSubExpression(scope, currentTACIndex, currentBlock, it->child, tempNum, thisExpression, 1);
	}
	break;

	case t_name:
	{
		thisExpression->operands[1] = it->child->value;
		struct VariableEntry *theVariable = Scope_lookupVar(scope, it->child->value);
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
		ErrorAndExit(ERROR_INTERNAL, "Unexpected type seen while linearizing expression LHS! Expected variable, literal, or subexpression\n");
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
		currentTACIndex = linearizeSubExpression(scope, currentTACIndex, currentBlock, it->child->sibling, tempNum, thisExpression, 2);
	}
	break;

	case t_name:
	{
		thisExpression->operands[2] = it->child->sibling->value;
		struct VariableEntry *theVariable = Scope_lookupVar(scope, it->child->sibling->value);
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
		// TODO: should this really be an internal error?
		ErrorAndExit(ERROR_INTERNAL, "Unexpected type seen while linearizing expression RHS! Expected variable, literal, or subexpression\n");
	}

	if (thisExpression->operation != tt_cmp)
	{
		// TODO (with type system) - properly determine type of expression when different operands
		thisExpression->operandTypes[0] = vt_var;
	}

	// automatically scale pointer arithmetic
	// but only for relevant operations
	switch (thisExpression->operation)
	{
	case tt_add:
	case tt_subtract:
	{
		// op1 is a pointer, op2 isn't
		if (thisExpression->indirectionLevels[1] > 0 && thisExpression->indirectionLevels[2] == 0)
		{
			switch (thisExpression->operandPermutations[2])
			{
			case vp_literal:
				// TODO: dynamically multiply here, fix memory leak
				char *scaledLiteral = malloc(16);
				sprintf(scaledLiteral, "%d", atoi(thisExpression->operands[2]) * 2);
				thisExpression->operands[2] = scaledLiteral;
				thisExpression->indirectionLevels[2] = thisExpression->indirectionLevels[1];
				break;

			case vp_standard:
			case vp_temp:
				struct TACLine *scaleMultiply = newTACLine(currentTACIndex++, tt_mul, it);
				scaleMultiply->operands[0] = TempList_getString(temps, *tempNum);
				(*tempNum)++;
				scaleMultiply->operandPermutations[0] = vp_temp;
				scaleMultiply->operandTypes[0] = thisExpression->operandTypes[2];

				// scale multiplication result has same indirection level as the operand being scaled to
				scaleMultiply->indirectionLevels[0] = thisExpression->indirectionLevels[1];

				// transfer the scaled operand out of the main expression
				scaleMultiply->operands[1] = thisExpression->operands[2];
				scaleMultiply->operandPermutations[1] = thisExpression->operandPermutations[2];
				scaleMultiply->operandTypes[1] = thisExpression->operandTypes[2];

				// transfer the temp into the main expression
				thisExpression->operands[2] = scaleMultiply->operands[0];
				thisExpression->operandTypes[2] = scaleMultiply->operandTypes[0];
				thisExpression->operandPermutations[2] = scaleMultiply->operandPermutations[0];
				thisExpression->indirectionLevels[2] = scaleMultiply->indirectionLevels[0];

				// TODO: auto scale by size of pointer and operand with types
				// TODO: scaling memory leak
				char *scalingLiteral = malloc(16);
				sprintf(scalingLiteral, "%d", 2);
				scaleMultiply->operands[2] = scalingLiteral;
				scaleMultiply->operandPermutations[2] = vp_literal;
				scaleMultiply->operandTypes[2] = vt_var;
				BasicBlock_append(currentBlock, scaleMultiply);
				break;
			}
		}
		else
		{
			if (thisExpression->indirectionLevels[2] > 0 && thisExpression->indirectionLevels[1] == 0)
			{
				switch (thisExpression->operandPermutations[1])
				{
				case vp_literal:
					// TODO: dynamically multiply here, fix memory leak
					char *scaledLiteral = malloc(16);
					sprintf(scaledLiteral, "%d", atoi(thisExpression->operands[1]) * 2);
					thisExpression->operands[1] = scaledLiteral;
					thisExpression->indirectionLevels[1] = thisExpression->indirectionLevels[2];
					break;

				case vp_standard:
				case vp_temp:
					struct TACLine *scaleMultiply;
					scaleMultiply = newTACLine(currentTACIndex++, tt_mul, it);
					scaleMultiply->operands[0] = TempList_getString(temps, *tempNum);
					(*tempNum)++;
					scaleMultiply->operandPermutations[0] = vp_temp;
					scaleMultiply->operandTypes[0] = thisExpression->operandTypes[2];

					// scale multiplication result has same indirection level as the operand being scaled to
					scaleMultiply->indirectionLevels[0] = thisExpression->indirectionLevels[2];

					// transfer the scaled operand out of the main expression
					scaleMultiply->operands[1] = thisExpression->operands[1];
					scaleMultiply->operandPermutations[1] = thisExpression->operandPermutations[1];
					scaleMultiply->operandTypes[1] = thisExpression->operandTypes[1];

					// transfer the temp into the main expression
					thisExpression->operands[1] = scaleMultiply->operands[0];
					thisExpression->operandTypes[1] = scaleMultiply->operandTypes[0];
					thisExpression->operandPermutations[1] = scaleMultiply->operandPermutations[0];
					thisExpression->indirectionLevels[1] = scaleMultiply->indirectionLevels[0];

					// TODO: auto scale by size of pointer and operand with types
					// TODO: scaling memory leak
					char *scalingLiteral = malloc(16);
					sprintf(scalingLiteral, "%d", 2);
					scaleMultiply->operands[2] = scalingLiteral;
					scaleMultiply->operandPermutations[2] = vp_literal;
					scaleMultiply->operandTypes[2] = vt_var;
					BasicBlock_append(currentBlock, scaleMultiply);
				}
			}
		}
	}
	break;

	default:
		break;
	}

	thisExpression->index = currentTACIndex++;
	BasicBlock_append(currentBlock, thisExpression);
	return currentTACIndex;
}

// given an AST node of an assignment, return the TAC block necessary to generate the correct value
int linearizeAssignment(struct Scope *scope,
						int currentTACIndex,
						struct BasicBlock *currentBlock,
						struct ASTNode *it,
						int *tempNum)
{
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
			assignment->operandTypes[0] = vt_var;
			assignment->operandPermutations[1] = vp_literal;
			break;

		case t_name:
		{
			struct VariableEntry *theVariable = Scope_lookupVar(scope, it->child->sibling->value);
			assignment->operandTypes[1] = theVariable->type;
			assignment->operandTypes[0] = theVariable->type;
			assignment->indirectionLevels[1] = theVariable->indirectionLevel;
		}
		break;

		default:
			ErrorAndExit(ERROR_INTERNAL, "Error parsing simple assignment - unexpected type on RHS\n");
		}

		BasicBlock_append(currentBlock, assignment);
	}
	else
	// otherwise there is some sort of expression, which will need to be broken down into multiple lines of TAC
	{
		switch (it->child->sibling->type)
		{
		case t_dereference:
			currentTACIndex = linearizeDereference(scope, currentTACIndex, currentBlock, it->child->sibling->child, tempNum);
			break;

		case t_un_add:
		case t_un_sub:
			currentTACIndex = linearizeExpression(scope, currentTACIndex, currentBlock, it->child->sibling, tempNum);
			break;

		case t_call:
			currentTACIndex = linearizeFunctionCall(scope, currentTACIndex, currentBlock, it->child->sibling, tempNum);
			break;

		default:
			ErrorAndExit(ERROR_INTERNAL, "Error linearizing expression - malformed parse tree (expected unOp or call)\n");
		}
	}

	struct TACLine *RHS = currentBlock->TACList->tail->data;
	if (it->child->type == t_name)
	{
		struct VariableEntry *assignedVariable = Scope_lookupVar(scope, it->child->value);
		RHS->operands[0] = it->child->value;
		RHS->operandTypes[0] = assignedVariable->type;
		RHS->indirectionLevels[0] = assignedVariable->indirectionLevel;
		RHS->operandPermutations[0] = vp_standard;
	}
	else
	{
		struct TACLine *finalAssignment = NULL;
		RHS->operands[0] = TempList_getString(temps, *tempNum);
		RHS->operandPermutations[0] = vp_temp;
		(*tempNum)++;
		RHS->operandTypes[0] = RHS->operandTypes[1];
		RHS->indirectionLevels[0] = RHS->indirectionLevels[1];
		switch (it->child->type)
		{
		case t_dereference:
		{
			struct ASTNode *dereferencedExpression = it->child->child;

			switch (dereferencedExpression->type)
			{
			case t_name:
			{
				finalAssignment = newTACLine(currentTACIndex++, tt_memw_1, it->child);
				finalAssignment->operands[0] = dereferencedExpression->value;
				finalAssignment->operandTypes[0] = Scope_lookupVar(scope, dereferencedExpression->value)->type;

				finalAssignment->operands[1] = RHS->operands[0];
				finalAssignment->operandPermutations[1] = RHS->operandPermutations[0];
				finalAssignment->indirectionLevels[1] = RHS->indirectionLevels[0];
				finalAssignment->operandTypes[1] = RHS->operandTypes[0];
			}
			break;

			case t_dereference:
			{
				currentTACIndex = linearizeDereference(scope, currentTACIndex, currentBlock, dereferencedExpression, tempNum);
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
					struct VariableEntry *theVariable = Scope_lookupVar(scope, dereferencedRHS->value);
					finalAssignment->operands[1] = dereferencedRHS->value;
					finalAssignment->operandTypes[1] = theVariable->type;
					finalAssignment->indirectionLevels[1] = theVariable->indirectionLevel;
				}
				break;

				// all other arithmetic goes here
				case t_un_add:
				case t_un_sub:
				{
					currentTACIndex = linearizeExpression(scope, currentTACIndex, currentBlock, dereferencedRHS, tempNum);
					struct TACLine *finalArithmeticLine = currentBlock->TACList->tail->data;
					finalAssignment = newTACLine(currentTACIndex++, tt_memw_3, dereferencedRHS);

					// struct VariableEntry *theVariable = Scope_lookupVar(scope, dereferencedRHS->value);
					finalAssignment->operands[1] = finalArithmeticLine->operands[0];
					finalAssignment->operandPermutations[1] = finalArithmeticLine->operandPermutations[0];
					finalAssignment->operandTypes[1] = finalArithmeticLine->operandTypes[0];
					finalAssignment->indirectionLevels[1] = finalArithmeticLine->indirectionLevels[0];
				}
				break;

				default:
					ErrorAndExit(ERROR_INTERNAL, "Error - Unexpected type on RHS of pointer write arithmetic\n");
				}

				int lhsSize = 0;
				// check the LHS of the add
				switch (dereferencedExpression->child->type)
				{
				case t_name:
				{
					struct VariableEntry *lhsVariable = Scope_lookupVar(scope, dereferencedExpression->child->value);
					finalAssignment->operands[0] = dereferencedExpression->child->value;
					finalAssignment->operandTypes[0] = lhsVariable->type;
					finalAssignment->indirectionLevels[0] = lhsVariable->indirectionLevel;
					lhsSize = Scope_getSizeOfVariable(scope, dereferencedExpression->child->value);
				}
				break;

				default:
					ErrorAndExit(ERROR_CODE, "Error - Illegal type on LHS of assignment expression\n\tPointer write operations can only bind rightwards\n");
				}

				switch (finalAssignment->operation)
				{
				case tt_memw_2:
				{
					finalAssignment->operands[1] = (char *)((long int)lhsSize * (long int)(finalAssignment->operands[1]));
					finalAssignment->operandPermutations[1] = vp_literal;
					finalAssignment->operandTypes[1] = vt_var;
					// finalAssignment->indirectionLevels[1] = 0; // extraneous

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
					ErrorAndExit(ERROR_INTERNAL, "Error - Unexpected final assignment TAC type\n");
				}
			}
			break;

			default:
				ErrorAndExit(ERROR_INTERNAL, "Error - Unexpected type within dereference on LHS of assignment expression\n\t");
			}
		}
		break;

		default:
			ErrorAndExit(ERROR_INTERNAL, "Error - Unexpected type on LHS of assignment expression\n\t");
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
			ErrorAndExit(ERROR_INTERNAL, "Error - Unexpected value in comparison operator node\n");
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
			ErrorAndExit(ERROR_INTERNAL, "Error - Unexpected value in comparison operator node\n");
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

int linearizeDeclaration(struct Scope *scope,
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
		ErrorAndExit(ERROR_INTERNAL, "Unexpected type seen while linearizing declaration!");
	}

	int dereferenceLevel = 0;
	while (it->child->type == t_dereference)
	{
		dereferenceLevel++;
		it = it->child;
	}

	// if we are declaring an array, set the string with the size as the second operand
	if (it->child->type == t_array)
	{
		it = it->child;
		declarationLine->operands[1] = it->child->sibling->value;
		declarationLine->operandPermutations[1] = vp_literal;
		declarationLine->operandTypes[1] = vt_var;
	}

	declarationLine->operands[0] = it->child->value;
	declarationLine->operandTypes[0] = declaredType;
	declarationLine->indirectionLevels[0] = dereferenceLevel;

	BasicBlock_append(currentBlock, declarationLine);
	return currentTACIndex;
}

struct Stack *linearizeIfStatement(struct Scope *scope,
								   int currentTACIndex,
								   struct BasicBlock *currentBlock,
								   struct BasicBlock *afterIfBlock,
								   struct ASTNode *it,
								   int *tempNum,
								   int *labelCount,
								   struct Stack *scopeNesting)
{
	// a stack is overkill but allows to store either 1 or 2 resulting blocks depending on if there is or isn't an else block
	struct Stack *results = Stack_new();

	// linearize the expression we will test
	currentTACIndex = linearizeExpression(scope, currentTACIndex, currentBlock, it->child, tempNum);

	// generate a label and figure out condition to jump when the if statement isn't executed
	struct TACLine *noifJump = linearizeConditionalJump(currentTACIndex++, it->child->value, it->child);

	BasicBlock_append(currentBlock, noifJump);

	// track the highest TAC index of both branches
	int ifTACIndex = currentTACIndex;
	int elseTACIndex = currentTACIndex;

	struct LinearizationResult *r = linearizeScope(scope, ifTACIndex, currentBlock, afterIfBlock, it->child->sibling->child, tempNum, labelCount, scopeNesting);
	Stack_push(results, r);

	// if there is an else statement to the if
	if (it->child->sibling->sibling != NULL)
	{
		// create a basicblock for the else statement
		struct BasicBlock *elseBlock = BasicBlock_new(++(*labelCount));
		Scope_addBasicBlock(scope, elseBlock);
		Function_addBasicBlock(scope->parentFunction, elseBlock);

		// bit hax (⌐▨_▨)
		// store the label index using the char* for this TAC line to avoid more fields in the struct
		noifJump->operands[0] = (char *)((long int)elseBlock->labelNum);

		// linearize the else block, it returns to the same afterIfBlock as the ifBlock does
		struct Stack *scopeStack = Stack_new();
		r = linearizeScope(scope, elseTACIndex, elseBlock, afterIfBlock, it->child->sibling->sibling->child->child, tempNum, labelCount, scopeStack);
		Stack_free(scopeStack);
		Stack_push(results, r);
	}
	else
	{
		noifJump->operands[0] = (char *)((long int)afterIfBlock->labelNum);
	}

	return results;
}

struct LinearizationResult *linearizeWhileLoop(struct Scope *scope,
											   int currentTACIndex,
											   struct BasicBlock *currentBlock,
											   struct BasicBlock *afterWhileBlock,
											   struct ASTNode *it,
											   int *tempNum,
											   int *labelCount,
											   struct Stack *scopeNesting)
{
	struct BasicBlock *whileBlock = BasicBlock_new(++(*labelCount));
	int whileSubScopeIndex = scope->subScopeCount - 1;
	Function_addBasicBlock(scope->parentFunction, whileBlock);

	struct TACLine *enterWhileJump = newTACLine(currentTACIndex++, tt_jmp, it);
	enterWhileJump->operands[0] = (char *)((long int)whileBlock->labelNum);
	BasicBlock_append(currentBlock, enterWhileJump);

	struct TACLine *whileDo = newTACLine(currentTACIndex, tt_do, it);
	BasicBlock_append(whileBlock, whileDo);

	// linearize the expression we will test
	currentTACIndex = linearizeExpression(scope, currentTACIndex, whileBlock, it->child, tempNum);

	struct TACLine *noWhileJump = linearizeConditionalJump(currentTACIndex++, it->child->value, it->child);
	noWhileJump->operands[0] = (char *)((long int)afterWhileBlock->labelNum);
	BasicBlock_append(whileBlock, noWhileJump);

	// create the scope for the while loop
	struct LinearizationResult *r = linearizeScope(scope, currentTACIndex, whileBlock, whileBlock, it->child->sibling->child, tempNum, labelCount, scopeNesting);
	// insert the conditional checks into that scope
	Scope_addBasicBlock(Scope_lookupSubScopeByNumber(scope, whileSubScopeIndex), whileBlock);

	struct TACLine *whileEndDo = newTACLine(r->endingTACIndex, tt_enddo, it);
	BasicBlock_append(r->block, whileEndDo);

	return r;
}

// given the AST for a function, generate TAC and return a pointer to the head of the generated block
struct LinearizationResult *linearizeScope(struct Scope *scope,
										   int currentTACIndex,
										   struct BasicBlock *currentBlock,
										   struct BasicBlock *controlConvergesTo,
										   struct ASTNode *it,
										   int *tempNum,
										   int *labelCount,
										   struct Stack *scopeNesting)
{
	// if we are descending into a nested scope, look up the correct scope
	if (scopeNesting->size > 0)
	{
		scope = Scope_lookupSubScopeByNumber(scope, *(long int *)Stack_peek(scopeNesting));
	}

	// locally scope a variable for scope count at this depth, push it to the stack
	int depthThisScope = 0;
	Stack_push(scopeNesting, &depthThisScope);

	// scrape along the function
	struct ASTNode *runner = it->child;
	while (runner != NULL)
	{
		switch (runner->type)
		{
		case t_scope:
			linearizeScope(scope, currentTACIndex, currentBlock, controlConvergesTo, runner, tempNum, labelCount, scopeNesting);
			break;

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
				currentTACIndex = linearizeAssignment(scope, currentTACIndex, currentBlock, runner->child, tempNum);
				break;

			case t_dereference:
				struct ASTNode *dereferenceScraper = runner->child;
				while (dereferenceScraper->type == t_dereference)
					dereferenceScraper = dereferenceScraper->child;

				if (dereferenceScraper->type == t_assign)
					currentTACIndex = linearizeAssignment(scope, currentTACIndex, currentBlock, dereferenceScraper, tempNum);
				else
					currentTACIndex = linearizeDeclaration(scope, currentTACIndex, currentBlock, runner);

				break;

			// if just a declaration, do nothing
			case t_array:
			case t_name:
				currentTACIndex = linearizeDeclaration(scope, currentTACIndex, currentBlock, runner);
				break;

			default:
				ErrorAndExit(ERROR_INTERNAL, "Error linearizing statement - malformed parse tree under 'var' node\n");
			}
		}
		break;

		// if we see an assignment, generate the code and stick it on to the end of the block
		case t_assign:
			currentTACIndex = linearizeAssignment(scope, currentTACIndex, currentBlock, runner, tempNum);
			break;

		case t_call:
		{
			// for a raw call, return value is not used, null out that operand
			currentTACIndex = linearizeFunctionCall(scope, currentTACIndex, currentBlock, runner, tempNum);
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
				returnedType = Scope_lookupVar(scope, returned)->type;
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
				returned = TempList_getString(temps, *tempNum);
				returnedPermutation = vp_temp;
				currentTACIndex = linearizeDereference(scope, currentTACIndex, currentBlock, runner->child->child, tempNum);
				struct TACLine *recursiveDereference = currentBlock->TACList->tail->data;
				returnedType = recursiveDereference->operandTypes[0];
			}
			break;

			case t_un_add:
			case t_un_sub:
			{
				returned = TempList_getString(temps, *tempNum);
				returnedPermutation = vp_temp;
				currentTACIndex = linearizeExpression(scope, currentTACIndex, currentBlock, runner->child, tempNum);
				struct TACLine *recursiveExpression = currentBlock->TACList->tail->data;
				returnedType = recursiveExpression->operandTypes[0];
			}
			break;

			default:
				ErrorAndExit(ERROR_INTERNAL, "Error - Unexpected type within return expression\n");
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

			// linearize the if statement and attached else if it exists
			struct Stack *results = linearizeIfStatement(scope, currentTACIndex, currentBlock, afterIfBlock, runner, tempNum, labelCount, scopeNesting);
			Scope_addBasicBlock(scope, afterIfBlock);
			Function_addBasicBlock(scope->parentFunction, afterIfBlock);

			struct Stack *beforeConvergeRestores = Stack_new();

			// grab all generated basic blocks generated by the if statement's linearization
			while (results->size > 0)
			{
				struct LinearizationResult *poppedResult = Stack_pop(results);
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
		}
		break;

		case t_while:
		{
			struct BasicBlock *afterWhileBlock = BasicBlock_new(++(*labelCount));

			struct LinearizationResult *r = linearizeWhileLoop(scope, currentTACIndex, currentBlock, afterWhileBlock, runner, tempNum, labelCount, scopeNesting);
			currentTACIndex = r->endingTACIndex;
			Scope_addBasicBlock(scope, afterWhileBlock);
			Function_addBasicBlock(scope->parentFunction, afterWhileBlock);
			free(r);

			currentBlock = afterWhileBlock;
		}
		break;

		default:
			ErrorAndExit(ERROR_INTERNAL, "Error - Unexpected node type when linearizing statement\n");
		}
		runner = runner->sibling;
	}

	if (controlConvergesTo != NULL)
	{
		if (currentBlock->TACList->tail != NULL)
		{
			struct TACLine *lastLine = currentBlock->TACList->tail->data;
			if (lastLine->operation != tt_return)
			{
				struct TACLine *convergeControlJump = newTACLine(currentTACIndex++, tt_jmp, runner);
				convergeControlJump->operands[0] = (char *)((long int)controlConvergesTo->labelNum);
				BasicBlock_append(currentBlock, convergeControlJump);
			}
		}
	}

	struct LinearizationResult *r = malloc(sizeof(struct LinearizationResult));
	r->block = currentBlock;
	r->endingTACIndex = currentTACIndex;
	Stack_pop(scopeNesting);
	if (scopeNesting->size > 0)
		*((int *)Stack_peek(scopeNesting)) += 1;
	return r;
}

void collapseScopes(struct Scope *scope, struct Dictionary *dict, int depth)
{
	// first pass: recurse depth-first so everything we do at this call depth will be 100% correct
	for (int i = 0; i < scope->entries->size; i++)
	{
		struct ScopeMember *thisMember = scope->entries->data[i];
		switch (thisMember->type)
		{
		case e_scope: // recurse to subscopes
			collapseScopes(thisMember->entry, dict, depth + 1);
			break;

		case e_function: // recurse to functions
		{
			struct FunctionEntry *thisFunction = thisMember->entry;
			collapseScopes(thisFunction->mainScope, dict, 0);
		}
		break;

		// skip everything else
		case e_variable:
		case e_argument:
		case e_basicblock:
		case e_stackobj:
			break;
		}
	}

	// second pass: rename basic block operands relevant to the current scope
	for (int i = 0; i < scope->entries->size; i++)
	{
		struct ScopeMember *thisMember = scope->entries->data[i];
		switch (thisMember->type)
		{
		case e_scope:
		case e_function:
			break;

		case e_basicblock:
		{
			// if we are in a nested scope
			if (depth > 1)
			{
				// go through all TAC lines in this block
				struct BasicBlock *thisBlock = thisMember->entry;
				for (struct LinkedListNode *TACRunner = thisBlock->TACList->head; TACRunner != NULL; TACRunner = TACRunner->next)
				{
					struct TACLine *thisTAC = TACRunner->data;
					for (int j = 0; j < 4; j++)
					{
						// check only TAC operands that both exist and refer to a named variable from the source code (ignore temps etc)
						if (thisTAC->operandTypes[j] != vt_null && thisTAC->operandPermutations[j] == vp_standard)
						{
							char *originalName = thisTAC->operands[j];
							// if this operand refers to a variable declared at this scope
							if (Scope_contains(scope, originalName))
							{
								char *mangledName = malloc(strlen(originalName) + 4);
								// tack on the name of this scope to the variable name since the same will happen to the variable entry itself in third pass
								sprintf(mangledName, "%s%s", originalName, scope->name);
								thisTAC->operands[j] = DictionaryLookupOrInsert(dict, mangledName);
								free(mangledName);
							}
						}
					}
				}
			}
		}
		break;

		case e_variable:
		case e_argument:
		case e_stackobj:
			break;
		}
	}

	// third pass: move nested members (basic blocks and variables) to parent scope since names have been mangled
	for (int i = 0; i < scope->entries->size; i++)
	{
		struct ScopeMember *thisMember = scope->entries->data[i];
		switch (thisMember->type)
		{
		case e_scope:
		case e_function:
			break;

		case e_basicblock:
			if (depth > 0 && scope->parentScope != NULL)
			{
				Scope_insert(scope->parentScope, thisMember->name, thisMember->entry, thisMember->type);
				free(scope->entries->data[i]);
				for (int j = i; j < scope->entries->size; j++)
				{
					scope->entries->data[j] = scope->entries->data[j + 1];
				}
				scope->entries->size--;
				i--;
			}
			break;

		case e_stackobj:
		case e_variable:
		case e_argument:
			if (depth > 0)
			{
				char *originalName = thisMember->name;
				char *scopeName = scope->name;

				char *mangledName = malloc(strlen(originalName) + strlen(scopeName) + 1);
				sprintf(mangledName, "%s%s", originalName, scopeName);
				char *newName = DictionaryLookupOrInsert(dict, mangledName);
				free(mangledName);
				Scope_insert(scope->parentScope, newName, thisMember->entry, thisMember->type);
				free(scope->entries->data[i]);
				for (int j = i; j < scope->entries->size; j++)
				{
					scope->entries->data[j] = scope->entries->data[j + 1];
				}
				scope->entries->size--;
				i--;
			}
			break;

		default:
			break;
		}
	}
}

// given an AST and a populated symbol table, generate three address code for the function entries
void linearizeProgram(struct ASTNode *it, struct Scope *globalScope, struct Dictionary *dict)
{
	temps = TempList_new();
	struct BasicBlock *globalBlock = Scope_lookup(globalScope, "globalblock")->entry;

	// scrape along the top level of the AST
	struct ASTNode *runner = it;
	int tempNum = 0;
	// start currentTAC index for both program and functions at 1
	// this allows anything involved in setup to occur at index 0
	// the primary example of this is stating that function arguments exist at index 0, even if they aren't used in the rest of the function
	// (particularly applicable for functions that use only asm)
	int currentTACIndex = 1;
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
			struct FunctionEntry *theFunction = Scope_lookupFun(globalScope, runner->child->value);

			struct BasicBlock *functionBlock = BasicBlock_new(funTempNum);

			Scope_addBasicBlock(theFunction->mainScope, functionBlock);
			Function_addBasicBlock(theFunction, functionBlock);
			struct Stack *scopeStack = Stack_new();
			struct LinearizationResult *r = linearizeScope(theFunction->mainScope, 1, functionBlock, NULL, runner->child->sibling, &funTempNum, &labelCount, scopeStack);
			free(r);
			Stack_free(scopeStack);
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
				currentTACIndex = linearizeAssignment(globalScope, currentTACIndex, globalBlock, declarationScraper, &tempNum);

			break;

		case t_assign:
			currentTACIndex = linearizeAssignment(globalScope, currentTACIndex, globalBlock, runner, &tempNum);
			break;

		// ignore everything else (for now) - no global vars, etc...
		default:
			ErrorAndExit(ERROR_INTERNAL, "Error - Unexpected statement type at global scope\n");
		}
		runner = runner->sibling;
	}

	collapseScopes(globalScope, dict, 1);
}

/*

Generating symbol table from AST..
Linearizing code to basic blocks
Symbol Table Program
> Function print (returns 1)
	> Argument n (stack offset -4)
> Function scopeTest (returns 1)
	> Argument a (stack offset -4)
	> Variable i (stack offset 0)
	> Subscope _00
		> Variable j (stack offset 2)
		> Subscope _00_00
			> Variable k (stack offset 4)

*/