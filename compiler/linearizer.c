#include "linearizer.h"

struct TempList *temps = NULL;

/*
 * These functions walk the AST and convert it to three-address code
 */

int linearizeASMBlock(struct LinearizationMetadata m)
{
	struct AST *asmRunner = m.ast->child;
	while (asmRunner != NULL)
	{
		struct TACLine *asmLine = newTACLine(m.currentTACIndex++, tt_asm, asmRunner);
		asmLine->operands[0].name.str = asmRunner->value;
		BasicBlock_append(m.currentBlock, asmLine);
		asmRunner = asmRunner->sibling;
	}
	return m.currentTACIndex;
}

int linearizeDereference(struct LinearizationMetadata m)
{
	// TAC index set at bottom of function so it aligns properly with any lines resulting from recursive calls
	struct TACLine *thisDereference = newTACLine(m.currentTACIndex, tt_memr_1, m.ast);

	thisDereference->operands[0].name.str = TempList_Get(temps, *m.tempNum);
	thisDereference->operands[0].permutation = vp_temp;
	(*m.tempNum)++;

	switch (m.ast->type)
	{
		// directly dereference variables
	case t_name:
	{
		thisDereference->operands[1].name.str = m.ast->value;
		thisDereference->operands[1].type = vt_var;
		thisDereference->operands[1].indirectionLevel = Scope_lookupVar(m.scope, m.ast->value)->indirectionLevel;
	}
	break;

		// recursively dereference nested dereferences
	case t_dereference:
	{
		thisDereference->operands[1].name.str = TempList_Get(temps, *m.tempNum);
		thisDereference->operands[1].permutation = vp_temp;

		struct LinearizationMetadata recursiveDereferenceMetadata;
		memcpy(&recursiveDereferenceMetadata, &m, sizeof(struct LinearizationMetadata));
		recursiveDereferenceMetadata.ast = m.ast->child;
		m.currentTACIndex = linearizeDereference(recursiveDereferenceMetadata);
		struct TACLine *recursiveDereference = m.currentBlock->TACList->tail->data;
		thisDereference->operands[1].type = recursiveDereference->operands[0].type;
		thisDereference->operands[1].indirectionLevel = recursiveDereference->operands[0].indirectionLevel;
	}
	break;

		// handle pointer arithmetic to evalute the correct adddress to dereference
	case t_bin_add:
	case t_bin_sub:
	{
		thisDereference->operands[1].name.str = m.ast->child->value; // base
		int LHSSize;

		// figure out what the LHS is
		switch (m.ast->child->type)
		{
		case t_name:
		{
			struct VariableEntry *theVariable = Scope_lookupVar(m.scope, m.ast->child->value);
			thisDereference->operands[1].type = theVariable->type;
			thisDereference->operands[1].indirectionLevel = theVariable->indirectionLevel;
			LHSSize = Scope_getSizeOfVariable(m.scope, m.ast->child->value);
		}
		break;

		case t_literal:
		{
			thisDereference->operands[1].type = vt_var;
			thisDereference->operands[1].permutation = vp_literal;
			LHSSize = 4; // hardcode lhs as uint size if a literal
		}
		break;

		case t_dereference:
		{
			thisDereference->operands[1].name.str = TempList_Get(temps, *m.tempNum);
			thisDereference->operands[1].permutation = vp_temp;

			struct LinearizationMetadata recursiveDereferenceMetadata;
			memcpy(&recursiveDereferenceMetadata, &m, sizeof(struct LinearizationMetadata));
			recursiveDereferenceMetadata.ast = m.ast->child->child;

			m.currentTACIndex = linearizeDereference(recursiveDereferenceMetadata);
			struct TACLine *recursiveDereference = m.currentBlock->TACList->tail->data;
			thisDereference->operands[1].type = recursiveDereference->operands[0].type;
			thisDereference->operands[1].indirectionLevel = recursiveDereference->operands[0].indirectionLevel;
			LHSSize = Scope_getSizeOfVariable(m.scope, recursiveDereference->operands[0].name.str);
		}
		break;

		default:
			AST_PrintHorizontal(m.ast);
			ErrorAndExit(ERROR_INTERNAL, "Illegal type on LHS of dereferenced expression\n");
		}
		thisDereference->operation = tt_memr_3;
		thisDereference->operands[3].name.val = LHSSize; // scale
		thisDereference->operands[3].type = vt_var;
		thisDereference->operands[3].permutation = vp_literal;

		// deal with the RHS (offset)
		switch (m.ast->child->sibling->type)
		{
		case t_name:
		{
			if (m.ast->type == t_bin_sub)
			{
				struct TACLine *subtractInvert = newTACLine(m.currentTACIndex++, tt_mul, m.ast);
				subtractInvert->operands[0].name.str = TempList_Get(temps, *m.tempNum);
				subtractInvert->operands[0].permutation = vp_temp;
				(*m.tempNum)++;
				char *invertedVariableName = m.ast->child->sibling->value;
				struct VariableEntry *invertedVariable = Scope_lookupVar(m.scope, invertedVariableName);

				subtractInvert->operands[0].type = invertedVariable->type;
				subtractInvert->operands[1].name.str = invertedVariableName;
				subtractInvert->operands[1].type = invertedVariable->type;

				subtractInvert->operands[2].name.str = "-1";
				subtractInvert->operands[2].type = vt_var;
				subtractInvert->operands[2].permutation = vp_literal;

				thisDereference->operands[2].name.str = subtractInvert->operands[0].name.str;
				thisDereference->operands[2].type = invertedVariable->type;
				thisDereference->operands[2].indirectionLevel = invertedVariable->indirectionLevel;
				BasicBlock_append(m.currentBlock, subtractInvert);
			}
			else
			{
				char *variableName = m.ast->child->sibling->value;
				thisDereference->operands[2].name.str = variableName;
				struct VariableEntry *theVariable = Scope_lookupVar(m.scope, variableName);
				thisDereference->operands[2].type = theVariable->type;
				thisDereference->operands[2].indirectionLevel = theVariable->indirectionLevel;
			}
		}
		break;

		// if literal, just use addressing mode base + offset
		case t_literal:
		{
			thisDereference->operation = tt_memr_2;

			// invalidate the previously set 4th operand, will be unused
			thisDereference->operands[3].type = vt_null;

			int offset = atoi(m.ast->child->sibling->value);
			// multiply offset by 4 for word size
			thisDereference->operands[2].name.val = (offset * 4) * ((m.ast->type == t_bin_sub) ? -1 : 1);
			thisDereference->operands[2].type = vt_var;
			thisDereference->operands[2].permutation = vp_literal;
		}
		break;

		case t_bin_add:
		case t_bin_sub:
		{
			// parent expression type requires inversion of entire (right) child expression if subtracting
			if (m.ast->type == t_bin_sub)
			{
				struct TACLine *subtractInvert = newTACLine(m.currentTACIndex++, tt_mul, m.ast);
				subtractInvert->operands[0].name.str = TempList_Get(temps, *m.tempNum);
				subtractInvert->operands[0].permutation = vp_temp;
				(*m.tempNum)++;

				struct LinearizationMetadata expressionMetadata;
				memcpy(&expressionMetadata, &m, sizeof(struct LinearizationMetadata));
				expressionMetadata.ast = m.ast->child->sibling;

				m.currentTACIndex = linearizeExpression(expressionMetadata);
				struct TACLine *recursiveExpression = m.currentBlock->TACList->tail->data;

				// set inverted types based on the expression result type
				subtractInvert->operands[0].type = recursiveExpression->operands[0].type;

				memcpy(&subtractInvert->operands[1], &recursiveExpression->operands[0], sizeof(struct TACOperand));

				subtractInvert->operands[2].name.str = "-1";
				subtractInvert->operands[2].type = vt_var;
				subtractInvert->operands[2].permutation = vp_literal;

				thisDereference->operands[2].name.str = subtractInvert->operands[0].name.str;
				thisDereference->operands[2].type = subtractInvert->operands[0].type;
				BasicBlock_append(m.currentBlock, subtractInvert);
			}
			else
			{
				// thisDereference->operands[2].name.str = TempList_Get(temps, *m.tempNum);

				struct LinearizationMetadata expressionMetadata;
				memcpy(&expressionMetadata, &m, sizeof(struct LinearizationMetadata));
				expressionMetadata.ast = m.ast->child->sibling;

				m.currentTACIndex = linearizeExpression(expressionMetadata);

				struct TACLine *lastSubExpressionLine = m.currentBlock->TACList->tail->data;
				memcpy(&thisDereference->operands[2], &lastSubExpressionLine->operands[0], sizeof(struct TACOperand));
				// thisDereference->operands[2].type = lastSubExpressionLine->operands[0].type;
			}
			thisDereference->operands[2].permutation = vp_temp;
		}
		break;

		default:
			ErrorAndExit(ERROR_INTERNAL, "Malformed parse tree in RHS of dereference arithmetic!\n");
		}
	}
	break;

	default:
		ErrorAndExit(ERROR_INTERNAL, "Malformed parse tree when linearizing dereference!\n");
	}

	thisDereference->operands[0].type = thisDereference->operands[1].type;
	// thisDereference->operands[0].permutation = thisDereference->operands[1].permutation;

	thisDereference->index = m.currentTACIndex++;
	int newIndirection = thisDereference->operands[1].indirectionLevel;
	if (newIndirection > 0)
	{
		newIndirection--;
	}
	else
	{
		printf("\n%s - ", thisDereference->operands[1].name.str);
		printf("Warning - dereference of non-indirect expression or statement\n\t");
		AST_PrintHorizontal(m.ast);
		printf("\n\tLine %d, Col %d\n", m.ast->sourceLine, m.ast->sourceCol);
		printTACLine(thisDereference);
		printf("\n");
	}

	thisDereference->operands[0].indirectionLevel = newIndirection;
	BasicBlock_append(m.currentBlock, thisDereference);
	return m.currentTACIndex;
}

int linearizeArgumentPushes(struct LinearizationMetadata m)
{
	if (m.ast->sibling != NULL)
	{
		struct LinearizationMetadata argumentMetadata;
		memcpy(&argumentMetadata, &m, sizeof(struct LinearizationMetadata));
		argumentMetadata.ast = m.ast->sibling;

		m.currentTACIndex = linearizeArgumentPushes(argumentMetadata);
	}
	struct TACLine *thisArgument = NULL;
	switch (m.ast->type)
	{
	case t_name:
	{
		thisArgument = newTACLine(m.currentTACIndex++, tt_push, m.ast);
		thisArgument->operands[0].type = vt_var;
	}
	// fall through to assign operand[0] name
	case t_literal:
	{
		if (thisArgument == NULL)
		{
			thisArgument = newTACLine(m.currentTACIndex++, tt_push, m.ast);
			thisArgument->operands[0].type = vt_var;
			thisArgument->operands[0].permutation = vp_literal;
		}
		thisArgument->operands[0].name.str = m.ast->value;
	}
	break;

	case t_dereference:
	{
		struct LinearizationMetadata dereferenceMetadata;
		memcpy(&dereferenceMetadata, &m, sizeof(struct LinearizationMetadata));
		dereferenceMetadata.ast = m.ast->child;

		m.currentTACIndex = linearizeDereference(dereferenceMetadata);
		thisArgument = newTACLine(m.currentTACIndex++, tt_push, m.ast);
		struct TACLine *recursiveDereference = m.currentBlock->TACList->tail->data;

		// copy destination of dererence to source of argument push
		memcpy(&thisArgument->operands[0], &recursiveDereference->operands[0], sizeof(struct TACOperand));
	}
	break;

	case t_bin_add:
	case t_bin_sub:
	{
		struct LinearizationMetadata expressionMetadata;
		memcpy(&expressionMetadata, &m, sizeof(struct LinearizationMetadata));
		expressionMetadata.ast = m.ast;

		m.currentTACIndex = linearizeExpression(expressionMetadata);
		struct TACLine *recursiveExpression = m.currentBlock->TACList->tail->data;

		thisArgument = newTACLine(m.currentTACIndex++, tt_push, m.ast);

		// copy destination of expression to source of argument push
		memcpy(&thisArgument->operands[0], &recursiveExpression->operands[0], sizeof(struct TACOperand));
	}
	break;

	default:
		ErrorAndExit(ERROR_INTERNAL, "Error - Unexpected argument node type\n");
	}
	if (thisArgument != NULL)
		BasicBlock_append(m.currentBlock, thisArgument);

	return m.currentTACIndex;
}

// given an AST node of a function call, generate TAC to evaluate and push the arguments, then call it
int linearizeFunctionCall(struct LinearizationMetadata m)
{
	char *operand0 = TempList_Get(temps, *m.tempNum);
	char *functionName = m.ast->child->value;
	struct FunctionEntry *calledFunction = Scope_lookupFun(m.scope, functionName);

	if (calledFunction->returnType != vt_null)
	{
		(*m.tempNum)++;
	}

	// push arguments iff they exist
	if (m.ast->child->child != NULL)
	{
		struct LinearizationMetadata argumentMetadata;
		memcpy(&argumentMetadata, &m, sizeof(struct LinearizationMetadata));
		argumentMetadata.ast = m.ast->child->child;

		m.currentTACIndex = linearizeArgumentPushes(argumentMetadata);
	}

	struct TACLine *calltac = newTACLine(m.currentTACIndex++, tt_call, m.ast);
	calltac->operands[0].name.str = operand0;

	// always set the return permutation to temp
	// null vs non-null type will be the handler for whether the return value exists
	calltac->operands[0].permutation = vp_temp;

	// no type check because it contains the name of the function itself

	calltac->operands[1].name.str = functionName;

	if (calledFunction->returnType != vt_null)
	{
		calltac->operands[0].type = calledFunction->returnType;
	}
	// TODO: set variabletype based on function return type, error if void function

	// struct FunctionEntry *calledFunction = symbolTableLookup_fun(table, functionName);
	// calltac->operands[1].type = calledFunction->returnType;

	BasicBlock_append(m.currentBlock, calltac);
	return m.currentTACIndex;
}

// linearize any subexpression which results in the use of a temporary variable
int linearizeSubExpression(struct LinearizationMetadata m,
						   struct TACLine *parentExpression,
						   int operandIndex)
{
	parentExpression->operands[operandIndex].name.str = TempList_Get(temps, *m.tempNum);
	parentExpression->operands[operandIndex].permutation = vp_temp;

	switch (m.ast->type)
	{
	// handle recording return types from function calls here
	case t_call:
	{
		struct LinearizationMetadata callMetadata;
		memcpy(&callMetadata, &m, sizeof(struct LinearizationMetadata));
		callMetadata.ast = m.ast;

		m.currentTACIndex = linearizeFunctionCall(callMetadata);
		struct TACLine *recursiveFunctionCall = m.currentBlock->TACList->tail->data;

		// TODO: check return type (or at least that function returns something)

		// using a returned value will always be a temp
		parentExpression->operands[operandIndex].permutation = vp_temp;
		parentExpression->operands[operandIndex].type = recursiveFunctionCall->operands[0].type;
		parentExpression->operands[operandIndex].indirectionLevel = recursiveFunctionCall->operands[1].indirectionLevel;
	}
	break;

	case t_bin_add:
	case t_bin_sub:
	case t_bin_lThan:
	case t_bin_lThanE:
	case t_bin_gThan:
	case t_bin_gThanE:
	case t_bin_equals:
	case t_bin_notEquals:
	{
		struct LinearizationMetadata expressionMetadata;
		memcpy(&expressionMetadata, &m, sizeof(struct LinearizationMetadata));
		expressionMetadata.ast = m.ast;

		m.currentTACIndex = linearizeExpression(expressionMetadata);
		struct TACLine *recursiveExpression = m.currentBlock->TACList->tail->data;

		parentExpression->operands[operandIndex].type = recursiveExpression->operands[1].type;
		parentExpression->operands[operandIndex].indirectionLevel = recursiveExpression->operands[0].indirectionLevel;
	}
	break;

	case t_dereference:
	{
		struct LinearizationMetadata dereferenceMetadata;
		memcpy(&dereferenceMetadata, &m, sizeof(struct LinearizationMetadata));
		dereferenceMetadata.ast = m.ast->child;

		m.currentTACIndex = linearizeDereference(dereferenceMetadata);
		struct TACLine *recursiveDereference = m.currentBlock->TACList->tail->data;

		parentExpression->operands[operandIndex].type = recursiveDereference->operands[0].type;
		parentExpression->operands[operandIndex].indirectionLevel = recursiveDereference->operands[0].indirectionLevel;
	}
	break;

	default:
		ErrorAndExit(ERROR_INTERNAL, "Unexpected type seen while linearizing subexpression!\n");
	}
	return m.currentTACIndex;
}

// given an AST node of an expression, figure out how to break it down into multiple lines of three address code
int linearizeExpression(struct LinearizationMetadata m)
{
	// set to tt_assign, reassign in switch body based on operator later
	// also set true TAC index at bottom of function, after child expression linearizations take place
	struct TACLine *thisExpression = newTACLine(m.currentTACIndex, tt_assign, m.ast);

	// since 'cmp' doesn't generate a result, it just sets flags, no need to consume a temp for operations that become cmp's
	switch (m.ast->type)
	{
	case t_bin_add:
	case t_bin_sub:
	case t_dereference:
		thisExpression->operands[0].name.str = TempList_Get(temps, *m.tempNum);
		thisExpression->operands[0].permutation = vp_temp;
		// increment count of temp variables, the parse of this expression will be written to a temp
		(*m.tempNum)++;
		break;

	case t_bin_lThan:
	case t_bin_lThanE:
	case t_bin_gThan:
	case t_bin_gThanE:
	case t_bin_equals:
	case t_bin_notEquals:
		break;

	default:
		break;
	}
	// support dereference and reference operations separately
	// since these have only one operand
	if (m.ast->type == t_dereference)
	{
		thisExpression->operation = tt_memr_1;
		// if simply dereferencing a name
		if (m.ast->child->type == t_name)
		{
			thisExpression->operands[1].name.str = m.ast->child->value;
			thisExpression->operands[1].type = vt_var;
		}
		// otherwise there's pointer arithmetic involved
		else
		{
			thisExpression->operands[1].name.str = TempList_Get(temps, *m.tempNum);
			thisExpression->operands[1].permutation = vp_temp;

			struct LinearizationMetadata dereferenceMetadata;
			memcpy(&dereferenceMetadata, &m, sizeof(struct LinearizationMetadata));
			dereferenceMetadata.ast = m.ast->child;

			m.currentTACIndex = linearizeDereference(dereferenceMetadata);
			struct TACLine *recursiveDereference = m.currentBlock->TACList->tail->data;

			thisExpression->operands[1].indirectionLevel = recursiveDereference->operands[0].indirectionLevel;
			thisExpression->operands[1].type = recursiveDereference->operands[0].type;
		}

		thisExpression->index = m.currentTACIndex++;
		BasicBlock_append(m.currentBlock, thisExpression);
		return m.currentTACIndex;
	}

	// if we fall through to here, the expression contains a subexpression
	// handle the LHS of the operation
	switch (m.ast->child->type)
	{
	case t_call:
	case t_bin_add:
	case t_bin_sub:
	case t_bin_lThan:
	case t_bin_lThanE:
	case t_bin_gThan:
	case t_bin_gThanE:
	case t_bin_equals:
	case t_bin_notEquals:
	case t_dereference:
	{
		struct LinearizationMetadata subexpressionMetadata;
		memcpy(&subexpressionMetadata, &m, sizeof(struct LinearizationMetadata));
		subexpressionMetadata.ast = m.ast->child;

		m.currentTACIndex = linearizeSubExpression(subexpressionMetadata, thisExpression, 1);
	}
	break;

	case t_name:
	{
		thisExpression->operands[1].name.str = m.ast->child->value;
		struct VariableEntry *theVariable = Scope_lookupVar(m.scope, m.ast->child->value);
		thisExpression->operands[1].type = theVariable->type;
		thisExpression->operands[1].indirectionLevel = theVariable->indirectionLevel;
	}
	break;

	case t_literal:
	{
		thisExpression->operands[1].name.str = m.ast->child->value;
		thisExpression->operands[1].type = vt_var;
		thisExpression->operands[1].permutation = vp_literal;
		// indirection levels set to 0 by default
	}
	break;

	default:
		ErrorAndExit(ERROR_INTERNAL, "Unexpected type seen while linearizing expression LHS! Expected variable, literal, or subexpression\n");
	}

	// assign the TAC operation based on the operator at hand
	switch (m.ast->type)
	{
	case t_bin_add:
	{
		thisExpression->reorderable = 1;
		thisExpression->operation = tt_add;
	}
	break;

	case t_bin_sub:
	{
		thisExpression->operation = tt_subtract;
	}
	break;

	case t_bin_lThan:
	case t_bin_lThanE:
	case t_bin_gThan:
	case t_bin_gThanE:
	case t_bin_equals:
	case t_bin_notEquals:
	{
		thisExpression->operation = tt_cmp;
	}
	break;

	default:
	{
		ErrorAndExit(ERROR_INTERNAL, "Unexpected operator type found in linearizeExpression!\n");
		break;
	}
	}

	// handle the RHS of the expression, same as LHS but at third operand of TAC
	switch (m.ast->child->sibling->type)
	{
	case t_call:
	case t_bin_add:
	case t_bin_sub:
	case t_bin_lThan:
	case t_bin_lThanE:
	case t_bin_gThan:
	case t_bin_gThanE:
	case t_bin_equals:
	case t_bin_notEquals:
	case t_dereference:
	{

		struct LinearizationMetadata subexpressionMetadata;
		memcpy(&subexpressionMetadata, &m, sizeof(struct LinearizationMetadata));
		subexpressionMetadata.ast = m.ast->child->sibling;

		m.currentTACIndex = linearizeSubExpression(subexpressionMetadata, thisExpression, 2);
	}
	break;

	case t_name:
	{
		thisExpression->operands[2].name.str = m.ast->child->sibling->value;
		struct VariableEntry *theVariable = Scope_lookupVar(m.scope, m.ast->child->sibling->value);
		thisExpression->operands[2].type = theVariable->type;
		thisExpression->operands[2].indirectionLevel = theVariable->indirectionLevel;
	}
	break;

	case t_literal:
	{
		thisExpression->operands[2].name.str = m.ast->child->sibling->value;
		thisExpression->operands[2].type = vt_var;
		thisExpression->operands[2].permutation = vp_literal;
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
		thisExpression->operands[0].type = vt_var;
	}

	// automatically scale pointer arithmetic
	// but only for relevant operations
	switch (thisExpression->operation)
	{
	case tt_add:
	case tt_subtract:
	{
		// op1 is a pointer, op2 isn't
		if (thisExpression->operands[1].indirectionLevel > 0 && thisExpression->operands[2].indirectionLevel == 0)
		{
			switch (thisExpression->operands[2].permutation)
			{
			case vp_literal:
			{
				// TODO: dynamically multiply here, fix memory leak
				char *scaledLiteral = malloc(16);
				sprintf(scaledLiteral, "%d", atoi(thisExpression->operands[2].name.str) * 4);
				thisExpression->operands[2].name.str = scaledLiteral;
				thisExpression->operands[2].indirectionLevel = thisExpression->operands[1].indirectionLevel;
			}
			break;

			case vp_standard:
			case vp_temp:
			{
				struct TACLine *scaleMultiply = newTACLine(m.currentTACIndex++, tt_mul, m.ast);
				scaleMultiply->operands[0].name.str = TempList_Get(temps, *m.tempNum);
				(*m.tempNum)++;
				scaleMultiply->operands[0].permutation = vp_temp;
				scaleMultiply->operands[0].type = thisExpression->operands[2].type;

				// scale multiplication result has same indirection level as the operand being scaled to
				scaleMultiply->operands[0].indirectionLevel = thisExpression->operands[1].indirectionLevel;

				// transfer the scaled operand out of the main expression
				memcpy(&scaleMultiply->operands[1], &thisExpression->operands[0], sizeof(struct TACOperand));

				// transfer the temp into the main expression
				memcpy(&thisExpression->operands[2], &scaleMultiply->operands[0], sizeof(struct TACOperand));

				// TODO: auto scale by size of pointer and operand with types
				// TODO: scaling memory leak
				char *scalingLiteral = malloc(16);
				sprintf(scalingLiteral, "%d", 4);
				scaleMultiply->operands[2].name.str = scalingLiteral;
				scaleMultiply->operands[2].permutation = vp_literal;
				scaleMultiply->operands[2].type = vt_var;
				BasicBlock_append(m.currentBlock, scaleMultiply);
			}
			break;
			}
		}
		else
		{
			if (thisExpression->operands[2].indirectionLevel > 0 && thisExpression->operands[1].indirectionLevel == 0)
			{
				switch (thisExpression->operands[1].permutation)
				{
				case vp_literal:
				{
					// TODO: dynamically multiply here, fix memory leak
					char *scaledLiteral = malloc(16);
					sprintf(scaledLiteral, "%d", atoi(thisExpression->operands[1].name.str) * 4);
					thisExpression->operands[1].name.str = scaledLiteral;
					thisExpression->operands[1].indirectionLevel = thisExpression->operands[2].indirectionLevel;
				}
				break;

				case vp_standard:
				case vp_temp:
				{
					struct TACLine *scaleMultiply;
					scaleMultiply = newTACLine(m.currentTACIndex++, tt_mul, m.ast);
					scaleMultiply->operands[0].name.str = TempList_Get(temps, *m.tempNum);
					(*m.tempNum)++;
					scaleMultiply->operands[0].permutation = vp_temp;
					scaleMultiply->operands[0].type = thisExpression->operands[2].type;

					// scale multiplication result has same indirection level as the operand being scaled to
					scaleMultiply->operands[0].indirectionLevel = thisExpression->operands[2].indirectionLevel;

					// transfer the scaled operand out of the main expression
					memcpy(&scaleMultiply->operands[1], &thisExpression->operands[1], sizeof(struct TACOperand));

					// transfer the temp into the main expression
					memcpy(&thisExpression->operands[1], &scaleMultiply->operands[0], sizeof(struct TACOperand));

					// TODO: auto scale by size of pointer and operand with types
					// TODO: scaling memory leak
					char *scalingLiteral = malloc(16);
					sprintf(scalingLiteral, "%d", 4);
					scaleMultiply->operands[2].name.str = scalingLiteral;
					scaleMultiply->operands[2].permutation = vp_literal;
					scaleMultiply->operands[2].type = vt_var;
					BasicBlock_append(m.currentBlock, scaleMultiply);
				}
				}
			}
		}
	}
	break;

	default:
		break;
	}

	thisExpression->index = m.currentTACIndex++;
	BasicBlock_append(m.currentBlock, thisExpression);
	return m.currentTACIndex;
}

// given an AST node of an assignment, return the TAC block necessary to generate the correct value
int linearizeAssignment(struct LinearizationMetadata m)
{
	// if this assignment is simply setting one thing to another
	if (m.ast->child->sibling->child == NULL)
	{
		// pull out the relevant values and generate a single line of TAC to return
		struct TACLine *assignment = newTACLine(m.currentTACIndex++, tt_assign, m.ast);
		assignment->operands[1].name.str = m.ast->child->sibling->value;

		switch (m.ast->child->sibling->type)
		{
		case t_literal:
		{
			assignment->operands[1].type = vt_var;
			assignment->operands[0].type = vt_var;
			assignment->operands[1].permutation = vp_literal;
		}
		break;

		case t_name:
		{
			struct VariableEntry *theVariable = Scope_lookupVar(m.scope, m.ast->child->sibling->value);
			assignment->operands[1].type = theVariable->type;
			assignment->operands[0].type = theVariable->type;
			assignment->operands[1].indirectionLevel = theVariable->indirectionLevel;
		}
		break;

		default:
			ErrorAndExit(ERROR_INTERNAL, "Error parsing simple assignment - unexpected type on RHS\n");
		}

		BasicBlock_append(m.currentBlock, assignment);
	}
	else
	// otherwise there is some sort of expression, which will need to be broken down into multiple lines of TAC
	{
		switch (m.ast->child->sibling->type)
		{
		case t_dereference:
		{
			struct LinearizationMetadata dereferenceMetadata;
			memcpy(&dereferenceMetadata, &m, sizeof(struct LinearizationMetadata));
			dereferenceMetadata.ast = m.ast->child->sibling->child;

			m.currentTACIndex = linearizeDereference(dereferenceMetadata);
		}
		break;

		case t_bin_add:
		case t_bin_sub:
		{
			struct LinearizationMetadata expressionMetadata;
			memcpy(&expressionMetadata, &m, sizeof(struct LinearizationMetadata));
			expressionMetadata.ast = m.ast->child->sibling;

			m.currentTACIndex = linearizeExpression(expressionMetadata);
		}
		break;

		case t_call:
		{
			struct LinearizationMetadata callMetadata;
			memcpy(&callMetadata, &m, sizeof(struct LinearizationMetadata));
			callMetadata.ast = m.ast->child->sibling;

			m.currentTACIndex = linearizeFunctionCall(callMetadata);
		}
		break;

		default:
			ErrorAndExit(ERROR_INTERNAL, "Error linearizing expression - malformed parse tree (expected unOp or call)\n");
		}
	}

	struct TACLine *RHS = m.currentBlock->TACList->tail->data;
	if (m.ast->child->type == t_name)
	{
		struct VariableEntry *assignedVariable = Scope_lookupVar(m.scope, m.ast->child->value);
		RHS->operands[0].name.str = m.ast->child->value;
		RHS->operands[0].type = assignedVariable->type;
		RHS->operands[0].indirectionLevel = assignedVariable->indirectionLevel;
		RHS->operands[0].permutation = vp_standard;
	}
	else
	{
		struct TACLine *finalAssignment = NULL;
		RHS->operands[0].name.str = TempList_Get(temps, *m.tempNum);
		RHS->operands[0].permutation = vp_temp;
		(*m.tempNum)++;
		RHS->operands[0].type = RHS->operands[1].type;
		RHS->operands[0].indirectionLevel = RHS->operands[1].indirectionLevel;
		switch (m.ast->child->type)
		{
		case t_dereference:
		{
			struct AST *dereferencedExpression = m.ast->child->child;

			switch (dereferencedExpression->type)
			{
			case t_name:
			{
				finalAssignment = newTACLine(m.currentTACIndex++, tt_memw_1, m.ast->child);
				finalAssignment->operands[0].name.str = dereferencedExpression->value;
				finalAssignment->operands[0].type = Scope_lookupVar(m.scope, dereferencedExpression->value)->type;

				// copy operand from RHS dest to final assignment operand
				memcpy(&finalAssignment->operands[1], &RHS->operands[0], sizeof(struct TACOperand));
			}
			break;

			case t_dereference:
			{
				struct LinearizationMetadata dereferenceMetadata;
				memcpy(&dereferenceMetadata, &m, sizeof(struct LinearizationMetadata));
				dereferenceMetadata.ast = dereferencedExpression;

				m.currentTACIndex = linearizeDereference(dereferenceMetadata);

				struct TACLine *recursiveDereference = m.currentBlock->TACList->tail->data;
				finalAssignment = newTACLine(m.currentTACIndex++, tt_memw_1, dereferencedExpression);

				// transfer RHS dest to this expression operand
				memcpy(&finalAssignment->operands[1], &RHS->operands[0], sizeof(struct TACOperand));

				// transfer recursive dereference dest to final assignment operand
				memcpy(&finalAssignment->operands[0], &recursiveDereference->operands[0], sizeof(struct TACOperand));
			}
			break;

			case t_bin_add:
			case t_bin_sub:
			{

				// linearize the RHS of the dereferenced arithmetic
				struct AST *dereferencedRHS = dereferencedExpression->child->sibling;
				switch (dereferencedRHS->type)
				{
				case t_literal:
				{
					finalAssignment = newTACLine(m.currentTACIndex++, tt_memw_2, dereferencedRHS);
					finalAssignment->operands[1].name.str = (char *)(long int)atoi(dereferencedRHS->value);
					finalAssignment->operands[1].permutation = vp_literal;
					finalAssignment->operands[1].type = vt_var;
					finalAssignment->operands[1].indirectionLevel = 0;
				}
				break;

				case t_name:
				{
					finalAssignment = newTACLine(m.currentTACIndex++, tt_memw_3, dereferencedRHS);
					struct VariableEntry *theVariable = Scope_lookupVar(m.scope, dereferencedRHS->value);
					finalAssignment->operands[1].name.str = dereferencedRHS->value;
					finalAssignment->operands[1].type = theVariable->type;
					finalAssignment->operands[1].indirectionLevel = theVariable->indirectionLevel;
				}
				break;

				// all other arithmetic goes here
				case t_bin_add:
				case t_bin_sub:
				{
					struct LinearizationMetadata expressionMetadata;
					memcpy(&expressionMetadata, &m, sizeof(struct LinearizationMetadata));
					expressionMetadata.ast = dereferencedRHS;

					m.currentTACIndex = linearizeExpression(expressionMetadata);

					struct TACLine *finalArithmeticLine = m.currentBlock->TACList->tail->data;
					finalAssignment = newTACLine(m.currentTACIndex++, tt_memw_3, dereferencedRHS);

					memcpy(&finalAssignment->operands[1], &finalArithmeticLine->operands[0], sizeof(struct TACOperand));
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
					struct VariableEntry *lhsVariable = Scope_lookupVar(m.scope, dereferencedExpression->child->value);
					finalAssignment->operands[0].name.str = dereferencedExpression->child->value;
					finalAssignment->operands[0].type = lhsVariable->type;
					finalAssignment->operands[0].indirectionLevel = lhsVariable->indirectionLevel;
					lhsSize = Scope_getSizeOfVariable(m.scope, dereferencedExpression->child->value);
				}
				break;

				default:
					ErrorAndExit(ERROR_CODE, "Error - Illegal type on LHS of assignment expression\n\tPointer write operations can only bind rightwards\n");
				}

				switch (finalAssignment->operation)
				{
				case tt_memw_2:
				{
					finalAssignment->operands[1].name.val = lhsSize * (finalAssignment->operands[1].name.val);
					finalAssignment->operands[1].permutation = vp_literal;
					finalAssignment->operands[1].type = vt_var;
					// finalAssignment->operands[1].indirectionLevel = 0; // extraneous

					// make offset value negative if subtracting
					if (dereferencedExpression->type == t_bin_sub)
					{
						finalAssignment->operands[1].name.val = finalAssignment->operands[1].name.val * -1;
					}

					// copy RHS dest to final assignment operand
					memcpy(&finalAssignment->operands[2], &RHS->operands[0], sizeof(struct TACOperand));
				}
				break;

				case tt_memw_3:
				{
					finalAssignment->operands[2].name.val = lhsSize;
					finalAssignment->operands[2].permutation = vp_literal;
					finalAssignment->operands[2].type = vt_var;
					finalAssignment->operands[2].indirectionLevel = 0;

					// make scale value negative if subtracting
					if (dereferencedExpression->type == t_bin_sub)
					{
						finalAssignment->operands[2].name.val = finalAssignment->operands[2].name.val * -1;
					}

					// copy RHS dest to final assignment source
					memcpy(&finalAssignment->operands[3], &RHS->operands[0], sizeof(struct TACOperand));
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
		BasicBlock_append(m.currentBlock, finalAssignment);
	}

	return m.currentTACIndex;
}

struct TACLine *linearizeConditionalJump(int currentTACIndex, char *cmpOp, struct AST *correspondingTree)
{
	struct TACLine *notMetJump;
	switch (cmpOp[0])
	{
	case '<':
	{
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
	}
	break;

	case '>':
	{
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
	}
	break;

	case '!':
	{
		notMetJump = newTACLine(currentTACIndex, tt_je, correspondingTree);
	}
	break;

	case '=':
	{
		notMetJump = newTACLine(currentTACIndex, tt_jne, correspondingTree);
	}
	break;

	default:
		ErrorAndExit(ERROR_INTERNAL, "Error linearizing conditional jump - malformed parse tree: expected comparison operator, got [%s] instead!\n", cmpOp);

	}
	return notMetJump;
}

int linearizeDeclaration(struct LinearizationMetadata m)
{
	struct TACLine *declarationLine = newTACLine(m.currentTACIndex++, tt_declare, m.ast);
	enum variableTypes declaredType;
	switch (m.ast->type)
	{
	case t_var:
	{
		declaredType = vt_var;
	}
	break;

	default:
		ErrorAndExit(ERROR_INTERNAL, "Unexpected type seen while linearizing declaration!");
	}

	int dereferenceLevel = 0;
	while (m.ast->child->type == t_dereference)
	{
		dereferenceLevel++;
		m.ast = m.ast->child;
	}

	// if we are declaring an array, set the string with the size as the second operand
	if (m.ast->child->type == t_array)
	{
		m.ast = m.ast->child;
		declarationLine->operands[1].name.str = m.ast->child->sibling->value;
		declarationLine->operands[1].permutation = vp_literal;
		declarationLine->operands[1].type = vt_var;
	}

	declarationLine->operands[0].name.str = m.ast->child->value;
	declarationLine->operands[0].type = declaredType;
	declarationLine->operands[0].indirectionLevel = dereferenceLevel;

	BasicBlock_append(m.currentBlock, declarationLine);
	return m.currentTACIndex;
}

int linearizeConditionCheck(struct LinearizationMetadata m,
							struct BasicBlock *ifFalse)
{
	printf("LinearizeConditionCheck %s\n", getTokenName(m.ast->type));
	switch (m.ast->type)
	{
	case t_bin_log_and:
	{
		struct LinearizationMetadata LHS;
		memcpy(&LHS, &m, sizeof(struct LinearizationMetadata));
		LHS.ast = m.ast->child;
		m.currentTACIndex = linearizeConditionCheck(LHS, ifFalse);

		struct LinearizationMetadata RHS;
		memcpy(&RHS, &m, sizeof(struct LinearizationMetadata));
		RHS.ast = m.ast->child->sibling;
		m.currentTACIndex = linearizeConditionCheck(RHS, ifFalse);

		// no need for extra logic - if either condition is false the whole condition is false
	}
	break;

	case t_bin_lThan:
	case t_bin_gThan:
	case t_bin_lThanE:
	case t_bin_gThanE:
	case t_bin_equals:
	case t_bin_notEquals:
	{
		m.currentTACIndex = linearizeExpression(m);

		// generate a label and figure out condition to jump when the if statement isn't executed
		struct TACLine *ifFalseJump = linearizeConditionalJump(m.currentTACIndex++, m.ast->value, m.ast);
		ifFalseJump->operands[0].name.val = ifFalse->labelNum;
		BasicBlock_append(m.currentBlock, ifFalseJump);
	}
	break;

	default:
		ErrorAndExit(ERROR_INTERNAL, "Error linearizing statement - malformed parse tree: expected comparison or logical operator!\n");
	}
	return m.currentTACIndex;
}

struct Stack *linearizeIfStatement(struct LinearizationMetadata m,
								   struct BasicBlock *afterIfBlock,
								   int *labelCount,
								   struct Stack *scopenesting)
{
	// a stack is overkill but allows to store either 1 or 2 resulting blocks depending on if there is or isn't an else block
	struct Stack *results = Stack_New();

	// linearize the expression we will test
	struct LinearizationMetadata conditionCheckMetadata;
	memcpy(&conditionCheckMetadata, &m, sizeof(struct LinearizationMetadata));
	conditionCheckMetadata.ast = m.ast->child;

	// if we need to generate an else statement, jump there on false
	// otherwise jump to afterIfBlock on false
	struct BasicBlock *elseBlock = NULL;
	if (m.ast->child->sibling->sibling != NULL)
	{
		elseBlock = BasicBlock_new(++(*labelCount));
		m.currentTACIndex = linearizeConditionCheck(conditionCheckMetadata, elseBlock);
	}
	else
	{
		m.currentTACIndex = linearizeConditionCheck(conditionCheckMetadata, afterIfBlock);
	}

	struct LinearizationMetadata ifMetadata;
	memcpy(&ifMetadata, &m, sizeof(struct LinearizationMetadata));
	ifMetadata.ast = m.ast->child->sibling->child;

	struct LinearizationResult *r = linearizeScope(ifMetadata, afterIfBlock, labelCount, scopenesting);
	Stack_Push(results, r);

	// we need to generate an else statement, do so now
	if (elseBlock != NULL)
	{
		// add the else block to the scope and function table
		Scope_addBasicBlock(m.scope, elseBlock);
		Function_addBasicBlock(m.scope->parentFunction, elseBlock);

		// linearize the else block, it returns to the same afterIfBlock as the ifBlock does
		struct LinearizationMetadata elseMetadata;
		memcpy(&elseMetadata, &m, sizeof(struct LinearizationMetadata));
		elseMetadata.ast = m.ast->child->sibling->sibling->child->child;
		elseMetadata.currentBlock = elseBlock;

		r = linearizeScope(elseMetadata, afterIfBlock, labelCount, scopenesting);
		Stack_Push(results, r);
	}

	return results;
}

struct LinearizationResult *linearizeWhileLoop(struct LinearizationMetadata m,
											   struct BasicBlock *afterWhileBlock,
											   int *labelCount,
											   struct Stack *scopenesting)
{
	struct BasicBlock *beforeWhileBlock = m.currentBlock;

	m.currentBlock = BasicBlock_new(++(*labelCount));
	int whileSubScopeIndex = m.scope->subScopeCount - 1;
	Function_addBasicBlock(m.scope->parentFunction, m.currentBlock);

	struct TACLine *enterWhileJump = newTACLine(m.currentTACIndex++, tt_jmp, m.ast);
	enterWhileJump->operands[0].name.val = m.currentBlock->labelNum;
	BasicBlock_append(beforeWhileBlock, enterWhileJump);

	struct TACLine *whileDo = newTACLine(m.currentTACIndex, tt_do, m.ast);
	BasicBlock_append(m.currentBlock, whileDo);

	// linearize the expression we will test
	struct LinearizationMetadata conditionCheckMetadata;
	memcpy(&conditionCheckMetadata, &m, sizeof(struct LinearizationMetadata));
	conditionCheckMetadata.ast = m.ast->child;

	m.currentTACIndex = linearizeConditionCheck(conditionCheckMetadata, afterWhileBlock);

	// create the scope for the while loop
	struct LinearizationMetadata whileBodyScopeMetadata;
	memcpy(&whileBodyScopeMetadata, &m, sizeof(struct LinearizationMetadata));
	whileBodyScopeMetadata.ast = m.ast->child->sibling->child;

	struct LinearizationResult *r = linearizeScope(whileBodyScopeMetadata, m.currentBlock, labelCount, scopenesting);

	// insert the conditional checks into that scope
	Scope_addBasicBlock(Scope_lookupSubScopeByNumber(m.scope, whileSubScopeIndex), m.currentBlock);

	struct TACLine *whileEndDo = newTACLine(r->endingTACIndex, tt_enddo, m.ast);
	BasicBlock_append(r->block, whileEndDo);

	return r;
}

// given the AST for a function, generate TAC and return a pointer to the head of the generated block
struct LinearizationResult *linearizeScope(struct LinearizationMetadata m,
										   struct BasicBlock *controlConvergesTo,
										   int *labelCount,
										   struct Stack *scopeNesting)
{
	// if we are descending into a nested scope, look up the correct scope and use it
	// the subscope will be used in this call and any calls generated from this one, allowing the scopes to recursively nest properly
	if (scopeNesting->size > 0)
	{
		printf("Linearize scope at subscope number %d\n", *(int *)Stack_Peek(scopeNesting));
		m.scope = Scope_lookupSubScopeByNumber(m.scope, *(int *)Stack_Peek(scopeNesting));
	}
	// otherwise the stack is empty so we should set it up to start at index 0
	else
	{
		printf("Not descending into subscope, start off by making a new subscope index\n");
		int newSubscopeIndex = 0;
		Stack_Push(scopeNesting, &newSubscopeIndex);
	}

	// locally scope a variable for scope count at this depth, push it to the stack
	int depthThisScope = 0;
	Stack_Push(scopeNesting, &depthThisScope);

	// scrape along the function
	struct AST *runner = m.ast->child;
	while (runner != NULL)
	{
		switch (runner->type)
		{
		case t_scope:
		{
			struct LinearizationMetadata scopeMetadata;
			memcpy(&scopeMetadata, &m, sizeof(struct LinearizationMetadata));
			scopeMetadata.ast = runner;

			// TODO: find what case generates a real return value from here - why is the return not used in this?
			linearizeScope(scopeMetadata, controlConvergesTo, labelCount, scopeNesting);
		}
		break;

		case t_asm:
		{
			struct LinearizationMetadata asmMetadata;
			memcpy(&asmMetadata, &m, sizeof(struct LinearizationMetadata));
			asmMetadata.ast = runner;

			m.currentTACIndex = linearizeASMBlock(asmMetadata);
		}
		break;

		// if we see a variable being declared and then assigned
		// generate the code and stick it on to the end of the block
		case t_var:
		{
			switch (runner->child->type)
			{
			case t_assign:
			{
				struct LinearizationMetadata assignmentMetadata;
				memcpy(&assignmentMetadata, &m, sizeof(struct LinearizationMetadata));
				assignmentMetadata.ast = runner->child;

				m.currentTACIndex = linearizeAssignment(assignmentMetadata);
			}
			break;

			case t_dereference:
			{
				struct AST *dereferenceScraper = runner->child;
				while (dereferenceScraper->type == t_dereference)
					dereferenceScraper = dereferenceScraper->child;

				if (dereferenceScraper->type == t_assign)
				{
					struct LinearizationMetadata assignmentMetadata;
					memcpy(&assignmentMetadata, &m, sizeof(struct LinearizationMetadata));
					assignmentMetadata.ast = dereferenceScraper;

					m.currentTACIndex = linearizeAssignment(assignmentMetadata);
				}
				else
				{
					struct LinearizationMetadata declarationMetadata;
					memcpy(&declarationMetadata, &m, sizeof(struct LinearizationMetadata));
					declarationMetadata.ast = runner;

					m.currentTACIndex = linearizeDeclaration(declarationMetadata);
				}
			}
			break;

			// if just a declaration, do nothing
			case t_array:
			case t_name:
			{
				struct LinearizationMetadata declarationMetadata;
				memcpy(&declarationMetadata, &m, sizeof(struct LinearizationMetadata));
				declarationMetadata.ast = runner;

				m.currentTACIndex = linearizeDeclaration(declarationMetadata);
			}
			break;

			default:
				ErrorAndExit(ERROR_INTERNAL, "Error linearizing statement - malformed parse tree under 'var' node\n");
			}
		}
		break;

		// if we see an assignment, generate the code and stick it on to the end of the block
		case t_assign:
		{
			struct LinearizationMetadata assignmentMetadata;
			memcpy(&assignmentMetadata, &m, sizeof(struct LinearizationMetadata));
			assignmentMetadata.ast = runner;

			m.currentTACIndex = linearizeAssignment(assignmentMetadata);
		}
		break;

		case t_call:
		{
			struct LinearizationMetadata callMetadata;
			memcpy(&callMetadata, &m, sizeof(struct LinearizationMetadata));
			callMetadata.ast = runner;

			// for a raw call, return value is not used, null out that operand
			m.currentTACIndex = linearizeFunctionCall(callMetadata);

			struct TACLine *lastLine = m.currentBlock->TACList->tail->data;
			lastLine->operands[0].name.str = NULL;
			lastLine->operands[0].type = vt_null;
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
				returnedType = Scope_lookupVar(m.scope, returned)->type;
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
				returned = TempList_Get(temps, *m.tempNum);
				returnedPermutation = vp_temp;

				struct LinearizationMetadata dereferenceMetadata;
				memcpy(&dereferenceMetadata, &m, sizeof(struct LinearizationMetadata));
				dereferenceMetadata.ast = runner->child->child;

				m.currentTACIndex = linearizeDereference(dereferenceMetadata);
				struct TACLine *recursiveDereference = m.currentBlock->TACList->tail->data;
				returnedType = recursiveDereference->operands[0].type;
			}
			break;

			case t_bin_add:
			case t_bin_sub:
			{
				returned = TempList_Get(temps, *m.tempNum);
				returnedPermutation = vp_temp;

				struct LinearizationMetadata expressionMetadata;
				memcpy(&expressionMetadata, &m, sizeof(struct LinearizationMetadata));
				expressionMetadata.ast = runner->child->child;

				m.currentTACIndex = linearizeExpression(expressionMetadata);
				struct TACLine *recursiveExpression = m.currentBlock->TACList->tail->data;
				returnedType = recursiveExpression->operands[0].type;
			}
			break;

			default:
				ErrorAndExit(ERROR_INTERNAL, "Error - Unexpected type within return expression\n");
			}
			struct TACLine *returnTac = newTACLine(m.currentTACIndex++, tt_return, runner);
			returnTac->operands[0].name.str = returned;
			returnTac->operands[0].permutation = returnedPermutation;
			returnTac->operands[0].type = returnedType;
			BasicBlock_append(m.currentBlock, returnTac);
		}
		break;

		case t_if:
		{
			// this is the block that control will be passed to after the branch, regardless of what happens
			struct BasicBlock *afterIfBlock = BasicBlock_new(++(*labelCount));

			struct LinearizationMetadata ifMetadata;
			memcpy(&ifMetadata, &m, sizeof(struct LinearizationMetadata));
			ifMetadata.ast = runner;

			// linearize the if statement and attached else if it exists
			struct Stack *results = linearizeIfStatement(ifMetadata, afterIfBlock, labelCount, scopeNesting);

			Scope_addBasicBlock(m.scope, afterIfBlock);
			Function_addBasicBlock(m.scope->parentFunction, afterIfBlock);

			struct Stack *beforeConvergeRestores = Stack_New();

			// grab all generated basic blocks generated by the if statement's linearization
			while (results->size > 0)
			{
				struct LinearizationResult *poppedResult = Stack_Pop(results);
				// skip the current TAC index as far forward as possible so code generated from here on out is always after previous code
				if (poppedResult->endingTACIndex > m.currentTACIndex)
					m.currentTACIndex = poppedResult->endingTACIndex + 1;

				free(poppedResult);
			}

			Stack_Free(results);

			while (beforeConvergeRestores->size > 0)
			{
				struct TACLine *expireLine = Stack_Pop(beforeConvergeRestores);
				expireLine->operands[0].name.val = m.currentTACIndex;
			}

			Stack_Free(beforeConvergeRestores);

			// we are now linearizing code into the block which the branches converge to
			m.currentBlock = afterIfBlock;
		}
		break;

		case t_while:
		{
			struct BasicBlock *afterWhileBlock = BasicBlock_new(++(*labelCount));

			struct LinearizationMetadata whileMetadata;
			memcpy(&whileMetadata, &m, sizeof(struct LinearizationMetadata));
			whileMetadata.ast = runner;

			struct LinearizationResult *r = linearizeWhileLoop(whileMetadata, afterWhileBlock, labelCount, scopeNesting);
			m.currentTACIndex = r->endingTACIndex;
			Scope_addBasicBlock(m.scope, afterWhileBlock);
			Function_addBasicBlock(m.scope->parentFunction, afterWhileBlock);
			free(r);

			m.currentBlock = afterWhileBlock;
		}
		break;

		default:
			ErrorAndExit(ERROR_INTERNAL, "Error - Unexpected node type when linearizing statement\n");
		}
		runner = runner->sibling;
	}

	if (controlConvergesTo != NULL)
	{
		if (m.currentBlock->TACList->tail != NULL)
		{
			struct TACLine *lastLine = m.currentBlock->TACList->tail->data;
			if (lastLine->operation != tt_return)
			{
				struct TACLine *convergeControlJump = newTACLine(m.currentTACIndex++, tt_jmp, runner);
				convergeControlJump->operands[0].name.val = controlConvergesTo->labelNum;
				BasicBlock_append(m.currentBlock, convergeControlJump);
			}
		}
	}

	struct LinearizationResult *r = malloc(sizeof(struct LinearizationResult));
	r->block = m.currentBlock;
	r->endingTACIndex = m.currentTACIndex;
	Stack_Pop(scopeNesting);
	if (scopeNesting->size > 0)
		*((int *)Stack_Peek(scopeNesting)) += 1;

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
		{
			collapseScopes(thisMember->entry, dict, depth + 1);
		}
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
						if (thisTAC->operands[j].type != vt_null && thisTAC->operands[j].permutation == vp_standard)
						{
							char *originalName = thisTAC->operands[j].name.str;
							// if this operand refers to a variable declared at this scope
							if (Scope_contains(scope, originalName))
							{
								char *mangledName = malloc(strlen(originalName) + 4);
								// tack on the name of this scope to the variable name since the same will happen to the variable entry itself in third pass
								sprintf(mangledName, "%s%s", originalName, scope->name);
								thisTAC->operands[j].name.str = Dictionary_LookupOrInsert(dict, mangledName);
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
		{
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
		}
		break;

		case e_stackobj:
		case e_variable:
		case e_argument:
		{
			if (depth > 0)
			{
				char *originalName = thisMember->name;
				char *scopeName = scope->name;

				char *mangledName = malloc(strlen(originalName) + strlen(scopeName) + 1);
				sprintf(mangledName, "%s%s", originalName, scopeName);
				char *newName = Dictionary_LookupOrInsert(dict, mangledName);
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
		}
		break;

		default:
			break;
		}
	}
}

// given an AST and a populated symbol table, generate three address code for the function entries
void linearizeProgram(struct AST *it, struct Scope *globalScope, struct Dictionary *dict)
{
	temps = TempList_New();
	struct BasicBlock *globalBlock = Scope_lookup(globalScope, "globalblock")->entry;

	// scrape along the top level of the AST
	struct AST *runner = it;
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
			struct Stack *scopeStack = Stack_New();
			struct LinearizationMetadata functionMetadata;
			functionMetadata.ast = runner->child->sibling;
			functionMetadata.currentBlock = functionBlock;
			functionMetadata.currentTACIndex = 1;
			functionMetadata.scope = theFunction->mainScope;
			functionMetadata.tempNum = &funTempNum;
			struct LinearizationResult *r = linearizeScope(functionMetadata, NULL, &labelCount, scopeStack);
			free(r);
			Stack_Free(scopeStack);
			break;
		}
		break;

		case t_asm:
		{
			struct LinearizationMetadata asmMetadata;
			asmMetadata.ast = runner;
			asmMetadata.currentBlock = globalBlock;
			asmMetadata.currentTACIndex = currentTACIndex;
			asmMetadata.scope = NULL;
			asmMetadata.tempNum = NULL;
			currentTACIndex = linearizeASMBlock(asmMetadata);
			// currentTACIndex = linearizeASMBlock(currentTACIndex, globalBlock, runner);
		}
		break;

		case t_var:
		{
			struct AST *declarationScraper = runner;

			// scrape down all pointer levels if necessary, then linearize if the variable is actually assigned
			while (declarationScraper->child->type == t_dereference)
				declarationScraper = declarationScraper->child;

			declarationScraper = declarationScraper->child;
			if (declarationScraper->type == t_assign)
			{
				struct LinearizationMetadata assignmentMetadata;
				assignmentMetadata.ast = declarationScraper;
				assignmentMetadata.currentBlock = globalBlock;
				assignmentMetadata.currentTACIndex = currentTACIndex;
				assignmentMetadata.scope = globalScope;
				assignmentMetadata.tempNum = &tempNum;
				currentTACIndex = linearizeAssignment(assignmentMetadata);
			}
		}
		break;

		case t_assign:
		{
			struct LinearizationMetadata assignmentMetadata;
			assignmentMetadata.ast = runner;
			assignmentMetadata.currentBlock = globalBlock;
			assignmentMetadata.currentTACIndex = currentTACIndex;
			assignmentMetadata.scope = globalScope;
			assignmentMetadata.tempNum = &tempNum;
			currentTACIndex = linearizeAssignment(assignmentMetadata);
		}
		break;

		// ignore everything else (for now) - no global vars, etc...
		default:
			ErrorAndExit(ERROR_INTERNAL, "Error - Unexpected statement type at global scope\n");
		}
		runner = runner->sibling;
	}
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