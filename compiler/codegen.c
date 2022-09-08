#include "codegen.h"

struct Stack *generateCode(struct SymbolTable *table, FILE *outFile)
{
	printf("generate code for [%s]\n", table->name);
	return generateCodeForScope(table->globalScope, outFile);
};

struct Stack *generateCodeForScope(struct Scope *scope, FILE *outFile)
{
	struct Stack *scopeBlocks = Stack_new();
	printf("generate code for scope [%s] (size %d)\n", scope->name, scope->entries->size);
	for (int i = 0; i < scope->entries->size; i++)
	{
		struct ScopeMember *thisMember = scope->entries->data[i];
		switch (thisMember->type)
		{
		case e_function:
			Stack_push(scopeBlocks, generateCodeForFunction(thisMember->entry, outFile));
			break;

		case e_basicblock:
		{
			struct ASMblock *blockBlock = newASMblock();
			GenerateCodeForBasicBlock(thisMember->entry, NULL, blockBlock, "global");
			Stack_push(scopeBlocks, blockBlock);
		}
		break;

		default:
			break;
		}
	}
	return scopeBlocks;
}

int calculateRegisterLoading(struct LinkedList *activeLifetimes, int index)
{
	int trueLoad = 0;
	for(struct LinkedListNode *runner = activeLifetimes->head; runner != NULL; runner = runner->next)
	{
		struct Lifetime *thisLifetime = runner->data;
		if(thisLifetime->start <= index && index < thisLifetime->end)
		{
			trueLoad++;
		}
		else if(thisLifetime->end >= index)
		{
			trueLoad--;
		}
	}
	return trueLoad;
}

struct ASMblock *generateCodeForFunction(struct FunctionEntry *function, FILE *outFile)
{
	printf("Generate code for function %s", function->name);
	struct LinkedList *allLifetimes = findLifetimes(function);

	// found lifetimes
	printf(".");

	struct Stack *spilledLifetimes = Stack_new();

	// find all overlapping lifetimes, to figure out which variables can live in registers vs being spilled
	int largestTacIndex = 0;
	for (struct LinkedListNode *runner = allLifetimes->head; runner != NULL; runner = runner->next)
	{
		struct Lifetime *thisLifetime = runner->data;
		if (thisLifetime->end > largestTacIndex)
		{
			largestTacIndex = thisLifetime->end;
		}
	}

	// generate an array of lists corresponding to which lifetimes are active at a given TAC step by index in the array
	struct LinkedList **lifetimeOverlaps = malloc((largestTacIndex + 1) * sizeof(struct LinkedList *));
	for (int i = 0; i <= largestTacIndex; i++)
	{
		lifetimeOverlaps[i] = LinkedList_new();
	}

	int mostConcurrentLifetimes = 0;
	// populate the array of active lifetimes
	for (struct LinkedListNode *runner = allLifetimes->head; runner != NULL; runner = runner->next)
	{
		struct Lifetime *thisLifetime = runner->data;
		for (int i = thisLifetime->start; i <= thisLifetime->end; i++)
		{
			LinkedList_append(lifetimeOverlaps[i], thisLifetime);
			if (lifetimeOverlaps[i]->size > mostConcurrentLifetimes)
			{
				mostConcurrentLifetimes = lifetimeOverlaps[i]->size;
			}
		}
	}
	
	// placed lifetimes into array, ready to allocate registers
	printf(".");

	int MAXREG = REGISTER_COUNT;
	// if we have just enough room, simply use all registers
	// if we need to spill, ensure 2 scratch registers
	if (mostConcurrentLifetimes > REGISTER_COUNT)
	{
		MAXREG -= 2;
	}

	// look through the populated array of active lifetimes
	// if a given index has too many active lifetimes, figure out which lifetime(s) to spill
	// then allocate registers for any lifetimes without a home
	for (int i = 0; i <= largestTacIndex; i++)
	{
		while (calculateRegisterLoading(lifetimeOverlaps[i], i) > MAXREG)
		{
			float bestHeuristic = -1.0;
			struct Lifetime *bestLifetime = NULL;
			for (struct LinkedListNode *overlapRunner = lifetimeOverlaps[i]->head; overlapRunner != NULL; overlapRunner = overlapRunner->next)
			{
				struct Lifetime *thisLifetime = overlapRunner->data;
				float thisHeuristic = ((thisLifetime->end - thisLifetime->start) + thisLifetime->nreads) * (thisLifetime->nwrites + 1);
				// printf("%s has heuristic of %f\n", thisLifetime->variable, thisHeuristic);
				if (thisHeuristic > bestHeuristic)
				{
					bestHeuristic = thisHeuristic;
					bestLifetime = thisLifetime;
				}
			}
			for (int j = bestLifetime->start; j <= bestLifetime->end; j++)
			{
				LinkedList_delete(lifetimeOverlaps[j], compareLifetimes, bestLifetime->variable);
			}
			bestLifetime->isSpilled = 1;
			Stack_push(spilledLifetimes, bestLifetime);
		}
	}

	// sort the list of spilled lifetimes by size of the variable so they can be laid out cleanly on the stack
	for (int i = 0; i < spilledLifetimes->size; i++)
	{
		for (int j = 0; j < spilledLifetimes->size - i - 1; j++)
		{
			int thisSize = Scope_getSizeOfVariable(function->mainScope, ((struct Lifetime *)spilledLifetimes->data[j])->variable);
			int compSize = Scope_getSizeOfVariable(function->mainScope, ((struct Lifetime *)spilledLifetimes->data[j + 1])->variable);
			if (thisSize < compSize)
			{
				struct Lifetime *swap = spilledLifetimes->data[j];
				spilledLifetimes->data[j] = spilledLifetimes->data[j + 1];
				spilledLifetimes->data[j + 1] = swap;
			}
		}
	}

	int stackOffset = 0;
	for (int i = 0; i < spilledLifetimes->size; i++)
	{
		struct Lifetime *thisLifetime = spilledLifetimes->data[i];
		int thisSize = Scope_getSizeOfVariable(function->mainScope, thisLifetime->variable);
		struct ScopeMember *thisVariableEntry = Scope_lookup(function->mainScope, thisLifetime->variable);
		if (thisVariableEntry != NULL && thisVariableEntry->type == e_argument)
		{
			struct VariableEntry *thisVariable = thisVariableEntry->entry;
			thisLifetime->stackOrRegLocation = thisVariable->stackOffset;
		}
		else
		{
			thisLifetime->stackOrRegLocation = (-1 * stackOffset) - 2;
			stackOffset += thisSize;
		}
	}

	// variables placed into registers or stack offsets
	printf(".");

	// flag registers in use at any given TAC index so we can easily assign
	char registers[REGISTER_COUNT];
	struct Lifetime *occupiedBy[REGISTER_COUNT];
	// flag registers which have *ever* been used so we know what to callee-save
	char touchedRegisters[REGISTER_COUNT];
	for (int i = 0; i < REGISTER_COUNT; i++)
	{
		registers[i] = 0;
		touchedRegisters[i] = 0;
		occupiedBy[i] = NULL;
	}

	if (MAXREG < REGISTER_COUNT)
	{
		// reserve r0 and r1 as scratch registers for arithmetic because we have spilled variables
		registers[0] = 1;
		registers[1] = 1;
		touchedRegisters[0] = 1;
		touchedRegisters[1] = 1;
	}

	for (int i = 0; i < largestTacIndex; i++)
	{
		// free any registers inhabited by expired lifetimes
		for (int j = 2; j < REGISTER_COUNT; j++)
		{
			if (occupiedBy[j] != NULL && occupiedBy[j]->end <= i)
			{
				registers[j] = 0;
				occupiedBy[j] = NULL;
			}
		}

		// second pass, assign any new lifetimes
		for (struct LinkedListNode *ltRunner = allLifetimes->head; ltRunner != NULL; ltRunner = ltRunner->next)
		{
			struct Lifetime *thisLifetime = ltRunner->data;
			if (thisLifetime->isSpilled == 0 && thisLifetime->stackOrRegLocation == 0)
			{
				if (thisLifetime->start == i)
				{
					char registerFound = 0;
					for (int j = 0; j < REGISTER_COUNT; j++)
					{
						if (registers[j] == 0)
						{
							thisLifetime->stackOrRegLocation = j;
							registers[j] = 1;
							occupiedBy[j] = thisLifetime;
							touchedRegisters[j] = 1;
							registerFound = 1;
							break;
						}
					}
					if (!registerFound)
					{
						ErrorAndExit(ERROR_INTERNAL, "Unable to find register for variable %s!\n", thisLifetime->variable);
					}
				}
			}
		}
	}

	// actual registers have been assigned to variables
	printf(".");

	struct ASMblock *functionBlock = newASMblock();
	char *functionSetup;

	// move any applicable arguments into registers if we are expecting them not to be spilled
	for (struct LinkedListNode *ltRunner = allLifetimes->head; ltRunner != NULL; ltRunner = ltRunner->next)
	{
		struct Lifetime *thisLifetime = ltRunner->data;

		// (short-circuit away from looking up temps since they can't be arguments)
		if (thisLifetime->variable[0] != '.')
		{
			struct ScopeMember *thisEntry = Scope_lookup(function->mainScope, thisLifetime->variable);
			// if this lifetime is an argument, and the argument lives in a register, retrieve it from the stack and place it there
			if (thisEntry->type == e_argument && !thisLifetime->isSpilled)
			{
				struct VariableEntry *theArgument = thisEntry->entry;
				functionSetup = malloc(64);
				sprintf(functionSetup, "mov %%r%d, %d(%%bp) ;place %s", thisLifetime->stackOrRegLocation, theArgument->stackOffset, thisLifetime->variable);
				touchedRegisters[thisLifetime->stackOrRegLocation] = 1;
				ASMblock_append(functionBlock, functionSetup);
			}
		}
	}

	// arguments placed into registers
	printf(".");

	for (struct LinkedListNode *blockRunner = function->BasicBlockList->head; blockRunner != NULL; blockRunner = blockRunner->next)
	{
		struct BasicBlock *thisBlock = blockRunner->data;
		GenerateCodeForBasicBlock(thisBlock, allLifetimes, functionBlock, function->name);
	}

	// meaningful code generated
	printf(".");

	functionSetup = malloc(64);
	sprintf(functionSetup, "%s_done:", function->name);
	ASMblock_append(functionBlock, functionSetup);

	for (int i = REGISTER_COUNT - 1; i >= 0; i--)
	{
		if (touchedRegisters[i])
		{
			functionSetup = malloc(64);
			sprintf(functionSetup, "pop %%r%d", i);
			ASMblock_append(functionBlock, functionSetup);

			functionSetup = malloc(64);
			sprintf(functionSetup, "push %%r%d", i);
			ASMblock_prepend(functionBlock, functionSetup);
		}
	}

	if (stackOffset > 0)
	{
		functionSetup = malloc(64);
		sprintf(functionSetup, "sub %%sp, %%sp, $%d", stackOffset);
		ASMblock_prepend(functionBlock, functionSetup);
	}

	functionSetup = malloc(64);
	sprintf(functionSetup, "%s:", function->name);
	ASMblock_prepend(functionBlock, functionSetup);

	if (stackOffset > 0)
	{
		functionSetup = malloc(64);
		sprintf(functionSetup, "add %%sp, %%sp, $%d", stackOffset);
		ASMblock_append(functionBlock, functionSetup);
	}

	functionSetup = malloc(64);
	if (function->argStackSize > 0)
	{
		sprintf(functionSetup, "ret %d", function->argStackSize);
	}
	else
	{
		sprintf(functionSetup, "ret");
	}
	ASMblock_append(functionBlock, functionSetup);

	// function setup and teardown code generated
	printf(".");

	Stack_free(spilledLifetimes);
	LinkedList_free(allLifetimes, free);

	for (int i = 0; i <= largestTacIndex; i++)
	{
		LinkedList_free(lifetimeOverlaps[i], NULL);
	}
	free(lifetimeOverlaps);

	printf("\n");
	return functionBlock;
}

void GenerateCodeForBasicBlock(struct BasicBlock *thisBlock, struct LinkedList *allLifetimes, struct ASMblock *asmBlock, char *functionName)
{
	char *blockLabel = malloc(64);
	if (thisBlock->labelNum > 0)
	{
		sprintf(blockLabel, "%s_%d:", functionName, thisBlock->labelNum);
	}
	ASMblock_append(asmBlock, blockLabel);

	for (struct LinkedListNode *TACRunner = thisBlock->TACList->head; TACRunner != NULL; TACRunner = TACRunner->next)
	{
		struct TACLine *thisTAC = TACRunner->data;
		char *thisLine;
		// char *thisLine = malloc(64);
		// char *printedTAC = sPrintTACLine(thisTAC);
		// sprintf(thisLine, ";%s", printedTAC);
		// ASMblock_append(asmBlock, thisLine);
		// free(printedTAC);

		switch (thisTAC->operation)
		{
		case tt_asm:
			ASMblock_append(asmBlock, thisTAC->operands[0]);
			break;

		case tt_assign:
		{
			struct Lifetime *assignedLifetime = LinkedList_find(allLifetimes, compareLifetimes, thisTAC->operands[0]);
			struct Lifetime *assignerLifetime = LinkedList_find(allLifetimes, compareLifetimes, thisTAC->operands[1]);
			thisLine = malloc(64);
			// assign to register
			if (assignedLifetime->isSpilled == 0)
			{
				switch (thisTAC->operandPermutations[1])
				{
				case vp_standard:
				case vp_temp:
					if (!assignerLifetime->isSpilled)
					{
						sprintf(thisLine, "mov %%r%d, %%r%d", assignedLifetime->stackOrRegLocation, assignerLifetime->stackOrRegLocation);
					}
					else
					{
						sprintf(thisLine, "mov %%r%d, %d(%%bp)", assignedLifetime->stackOrRegLocation, assignerLifetime->stackOrRegLocation);
					}
					break;

				case vp_literal:
					sprintf(thisLine, "mov %%r%d, $%s", assignedLifetime->stackOrRegLocation, thisTAC->operands[1]);
					break;
				}
			}
			// assign to spilled
			else
			{
				switch (thisTAC->operandPermutations[1])
				{
				case vp_standard:
				case vp_temp:
					if (!assignerLifetime->isSpilled)
					{
						sprintf(thisLine, "mov %d(%%bp), %%r%d", assignedLifetime->stackOrRegLocation, assignerLifetime->stackOrRegLocation);
					}
					else
					{
						// in this case, the value being read needs to be pulled down to a register
						// then put back up onto the stack to assign to the spilled variable
						ErrorAndExit(ERROR_INTERNAL, "assign from spilled to spilled!\n");
					}
					break;

				case vp_literal:
					sprintf(thisLine, "mov %d(%%bp), $%s", assignedLifetime->stackOrRegLocation, thisTAC->operands[1]);
					break;
				}
			}
			ASMblock_append(asmBlock, thisLine);
		}
		break;

		// ignore
		case tt_declare:
			break;

		case tt_add:
		case tt_subtract:
		case tt_mul:
		case tt_div:
		{
			thisLine = malloc(64);

			struct Lifetime *assignedLifetime = LinkedList_find(allLifetimes, compareLifetimes, thisTAC->operands[0]);
			struct Lifetime *operand1Lifetime = LinkedList_find(allLifetimes, compareLifetimes, thisTAC->operands[1]);
			struct Lifetime *operand2Lifetime = LinkedList_find(allLifetimes, compareLifetimes, thisTAC->operands[2]);
			int destinationRegister;
			char op1[12];
			char op2[12];
			char reordering = 0;
			if (assignedLifetime->isSpilled)
			{
				destinationRegister = 0;
			}
			else
			{
				destinationRegister = assignedLifetime->stackOrRegLocation;
			}

			if (thisTAC->operandPermutations[1] == vp_literal && thisTAC->operandPermutations[2] == vp_literal)
			{
				ErrorAndExit(ERROR_INTERNAL, "Arithmetic between two literals!\n");
			}

			// examine the first operand, place it in regisetr if necessary, and handle reordering (if possible/beneficial)
			switch (thisTAC->operandPermutations[1])
			{
			case vp_standard:
			case vp_temp:
				if (!operand1Lifetime->isSpilled)
				{
					sprintf(op1, "%%r%d", operand1Lifetime->stackOrRegLocation);
				}
				else
				{
					placeOperandInRegister(allLifetimes, operand1Lifetime->variable, asmBlock, 0);
					sprintf(op1, "%%r0");
				}
				break;

			case vp_literal:
				if (thisTAC->reorderable)
				{
					reordering = 1;
					sprintf(op1, "$%s", thisTAC->operands[1]);
				}
				break;
			}

			// examine the second operand, place it in register
			switch (thisTAC->operandPermutations[2])
			{
			case vp_standard:
			case vp_temp:
				if (!operand2Lifetime->isSpilled)
				{
					sprintf(op2, "%%r%d", operand2Lifetime->stackOrRegLocation);
				}
				else
				{
					placeOperandInRegister(allLifetimes, operand2Lifetime->variable, asmBlock, 1);
					sprintf(op2, "%%r1");
				}
				break;

			case vp_literal:
				sprintf(op2, "$%s", thisTAC->operands[2]);
				break;
			}

			if (reordering)
			{
				sprintf(thisLine, "%s %%r%d, %s, %s", getAsmOp(thisTAC->operation), destinationRegister, op2, op1);
			}
			else
			{
				sprintf(thisLine, "%s %%r%d, %s, %s", getAsmOp(thisTAC->operation), destinationRegister, op1, op2);
			}
			ASMblock_append(asmBlock, thisLine);

			if (assignedLifetime->isSpilled)
			{
				thisLine = malloc(64);
				sprintf(thisLine, "mov %d(%%bp), %%r%d;replace %s", assignedLifetime->stackOrRegLocation, destinationRegister, assignedLifetime->variable);
				ASMblock_append(asmBlock, thisLine);
			}
		}
		break;

		case tt_dereference:
			ErrorAndExit(ERROR_INTERNAL, "Code generation not implemented for this operation (%s) yet!\n", getAsmOp(thisTAC->operation));
			break;

		case tt_reference:
			ErrorAndExit(ERROR_INTERNAL, "Code generation not implemented for this operation (%s) yet!\n", getAsmOp(thisTAC->operation));
			break;

		case tt_memw_1:
			ErrorAndExit(ERROR_INTERNAL, "Code generation not implemented for this operation (%s) yet!\n", getAsmOp(thisTAC->operation));
			break;

		case tt_memw_2:
			ErrorAndExit(ERROR_INTERNAL, "Code generation not implemented for this operation (%s) yet!\n", getAsmOp(thisTAC->operation));
			break;

		case tt_memw_3:
		{
			thisLine = malloc(64);
			int baseReg = placeOperandInRegister(allLifetimes, thisTAC->operands[0], asmBlock, 0);
			int offsetReg = placeOperandInRegister(allLifetimes, thisTAC->operands[1], asmBlock, 1);
			int sourceReg = placeOperandInRegister(allLifetimes, thisTAC->operands[3], asmBlock, 16);
			sprintf(thisLine, "mov %%r%d(%%r%d, $%d), %%r%d", offsetReg, baseReg, (int)(long int)thisTAC->operands[2], sourceReg);
			ASMblock_append(asmBlock, thisLine);
		}
		break;

		case tt_memr_1:
			ErrorAndExit(ERROR_INTERNAL, "Code generation not implemented for this operation (%s) yet!\n", getAsmOp(thisTAC->operation));
			break;

		case tt_memr_2:
			ErrorAndExit(ERROR_INTERNAL, "Code generation not implemented for this operation (%s) yet!\n", getAsmOp(thisTAC->operation));
			break;

		case tt_memr_3:
		{
			thisLine = malloc(64);
			int destReg = placeOperandInRegister(allLifetimes, thisTAC->operands[0], asmBlock, 0);
			int baseReg = placeOperandInRegister(allLifetimes, thisTAC->operands[1], asmBlock, 1);
			int offsetReg = placeOperandInRegister(allLifetimes, thisTAC->operands[2], asmBlock, 16);
			sprintf(thisLine, "mov %%r%d, %%r%d(%%r%d, $%d)", destReg, offsetReg, baseReg, (int)(long int)thisTAC->operands[3]);
			ASMblock_append(asmBlock, thisLine);
		}
		break;

		case tt_cmp:
		{
			thisLine = malloc(64);

			struct Lifetime *operand1Lifetime = LinkedList_find(allLifetimes, compareLifetimes, thisTAC->operands[1]);
			struct Lifetime *operand2Lifetime = LinkedList_find(allLifetimes, compareLifetimes, thisTAC->operands[2]);
			char op1[12];
			char op2[12];

			if (thisTAC->operandPermutations[1] == vp_literal && thisTAC->operandPermutations[2] == vp_literal)
			{
				ErrorAndExit(ERROR_INTERNAL, "Cmp between two literals!\n");
			}

			// examine the first operand, place it in regisetr if necessary, and handle reordering (if possible/beneficial)
			switch (thisTAC->operandPermutations[1])
			{
			case vp_standard:
			case vp_temp:
				if (!operand1Lifetime->isSpilled)
				{
					sprintf(op1, "%%r%d", operand1Lifetime->stackOrRegLocation);
				}
				else
				{
					placeOperandInRegister(allLifetimes, operand1Lifetime->variable, asmBlock, 0);
					sprintf(op1, "%%r0");
				}
				break;

			case vp_literal:
				ErrorAndExit(ERROR_INTERNAL, "First operand of comparison is a literal\n");
				break;
			}

			// examine the second operand, place it in register
			switch (thisTAC->operandPermutations[2])
			{
			case vp_standard:
			case vp_temp:
				if (!operand2Lifetime->isSpilled)
				{
					sprintf(op2, "%%r%d", operand2Lifetime->stackOrRegLocation);
				}
				else
				{
					placeOperandInRegister(allLifetimes, operand2Lifetime->variable, asmBlock, 1);
					sprintf(op2, "%%r1");
				}
				break;

			case vp_literal:
				sprintf(op2, "$%s", thisTAC->operands[2]);
				break;
			}

			sprintf(thisLine, "%s %s, %s", getAsmOp(thisTAC->operation), op1, op2);
			ASMblock_append(asmBlock, thisLine);
		}
		break;

		case tt_jg:
		case tt_jge:
		case tt_jl:
		case tt_jle:
		case tt_je:
		case tt_jne:
		{
			thisLine = malloc(64);
			sprintf(thisLine, "%s %s_%ld", getAsmOp(thisTAC->operation), functionName, (long int)thisTAC->operands[0]);
			ASMblock_append(asmBlock, thisLine);
		}
		break;

		case tt_jmp:
		{
			thisLine = malloc(64);
			sprintf(thisLine, "jmp %s_%d", functionName, (int)(long int)thisTAC->operands[0]);
			ASMblock_append(asmBlock, thisLine);
		}
		break;

		case tt_push:
		{
			thisLine = malloc(64);
			switch (thisTAC->operandPermutations[0])
			{
			case vp_standard:
			case vp_temp:
			{
				int registerIndex = placeOperandInRegister(allLifetimes, thisTAC->operands[0], asmBlock, 0);
				sprintf(thisLine, "push %%r%d", registerIndex);
			}
			break;

			case vp_literal:
				sprintf(thisLine, "push $%s", thisTAC->operands[0]);
				break;
			}
			ASMblock_append(asmBlock, thisLine);
		}
		break;

		case tt_call:
		{
			thisLine = malloc(64);
			sprintf(thisLine, "call %s", thisTAC->operands[1]);
			ASMblock_append(asmBlock, thisLine);
			thisLine = malloc(64);

			// the call returns a value
			if (thisTAC->operands[0] != NULL)
			{
				struct Lifetime *returnedLifetime = LinkedList_find(allLifetimes, compareLifetimes, thisTAC->operands[0]);
				if (!returnedLifetime->isSpilled)
				{
					sprintf(thisLine, "mov %%r%d, %%rr", returnedLifetime->stackOrRegLocation);
				}
				else
				{
					sprintf(thisLine, "mov %d(%%bp), %%rr", returnedLifetime->stackOrRegLocation);
				}
				ASMblock_append(asmBlock, thisLine);
			}
		}
		break;

		case tt_label:
			ErrorAndExit(ERROR_INTERNAL, "Code generation not implemented for this operation (%s) yet!\n", getAsmOp(thisTAC->operation));
			break;

		case tt_return:
		{
			thisLine = malloc(64);
			switch (thisTAC->operandPermutations[0])
			{
			case vp_literal:
			{
				sprintf(thisLine, "mov %%rr, $%s", thisTAC->operands[0]);
				ASMblock_append(asmBlock, thisLine);
			}
			break;

			case vp_standard:
			case vp_temp:
			{
				int destReg = placeOperandInRegister(allLifetimes, thisTAC->operands[0], asmBlock, 0);
				sprintf(thisLine, "mov %%rr, %%r%d", destReg);
				ASMblock_append(asmBlock, thisLine);
				// ASMblock_append(outputBlock, trimmedStr);
			}
			break;

			default:
				perror("unexpected type in return TAC!\n");
			}
		}
		break;

		case tt_do:
		case tt_enddo:
			break;

		}
	}
}

/*
while(allLifetimes->size > 0 || activeLifetimes->size > 0)
{
	for(struct LinkedListNode *regRunner = registerLifetimes->head; regRunner != NULL; regRunner = regRunner->next)
	{
		struct Lifetime *thisRegisterLifetime = regRunner->data;
		if(thisRegisterLifetime->end < tacIndex)
		{
			LinkedList_delete(registerLifetimes, compareLifetimes, thisRegisterLifetime);
		}
	}

	for(struct LinkedListNode *spillRunner = spilledLifetimes->head; spillRunner != NULL; spillRunner = spillRunner->next)
	{
		struct Lifetime *thisSpilledLifetime = spillRunner->data;
		if(thisSpilledLifetime->end < tacIndex)
		{
			LinkedList_delete(registerLifetimes, compareLifetimes, thisSpilledLifetime);
		}
	}

	for(struct LinkedListNode *unprocessedRunner = allLifetimes->head; unprocessedRunner != NULL; unprocessedRunner = unprocessedRunner->next)
	{

	}

}
*/

/*

struct ASMblock *outputBlock = newASMblock();
// convert the linked list of lifetimes to an array for easy indexing
struct Lifetime **lifetimeArray = malloc(lifetimes->size * sizeof(struct Lifetime *));
int lifetimeCount = 0;
for (struct LinkedListNode *runner = lifetimes->head; runner != NULL; runner = runner->next)
	lifetimeArray[lifetimeCount++] = runner->data;

// print out the variable lifetimes as a horizontal graph
// printf("\n");
// printLifetimesGraph(lifetimes);
printf("Generate code for function %s", function->name);
// printLifetimesGraph(lifetimes);
// printf("\n");

int *registerLoads = malloc((REGISTER_COUNT + 1) * sizeof(int)); // count of TAC steps with different numbers of registers free
for (int i = 0; i < REGISTER_COUNT; i++)
	registerLoads[i] = 0;

int *stackLoads = malloc((lifetimeCount + 1) * sizeof(int)); // count of TAC steps with different numbers of variables spilled
for (int i = 0; i <= lifetimeCount; i++)
	stackLoads[i] = 0;

struct Stack *inactiveList = Stack_new(); // registers not currently in use
struct Stack *activeList = Stack_new();	  // registers containing variables
struct Stack *spilledList = Stack_new();  // list of live variables which have been spilled to stack
int maxSpillSpace = 0;

// create all register instances, add to inactive list
for (int i = 0; i < REGISTER_COUNT; i++)
{
	struct Register *wip = malloc(sizeof(struct Register));
	wip->lifetime = NULL;
	// remember not to use the zero register
	wip->index = (REGISTER_COUNT)-i;
	Stack_push(inactiveList, wip);
}

// iterate each TAC line
int TACIndex = 0;
int currentLifetimeIndex = 0;
char printBuf[129];
char *trimmedStr;
struct Stack *savedStateStack = Stack_new();
char touchedRegisters[REGISTER_COUNT];
for (int i = 0; i < REGISTER_COUNT; i++)
{
	touchedRegisters[i] = 0;
}

struct LinkedListNode *blockRunner = function->BasicBlockList->head;
while (blockRunner != NULL)
{
	struct BasicBlock *thisBlock = blockRunner->data;
	trimmedStr = strTrim(printBuf, sprintf(printBuf, "%s_%d:", function->name, thisBlock->labelNum));
	ASMblock_append(outputBlock, trimmedStr);

	// find all variables active at TAC index 0
	// this will allow function arguments to be introduced correctly
	while (currentLifetimeIndex < lifetimeCount && lifetimeArray[currentLifetimeIndex]->start <= 0)
	{

		struct Lifetime *this = lifetimeArray[currentLifetimeIndex++];
		// only introduce variables if they are function arguments or actually used in this TAC address
		// spill a register if one isn't available
		if (inactiveList->size == 0)
		{
			spillRegister(activeList, inactiveList, spilledList, outputBlock, table);

			if (spilledList->size * 2 > maxSpillSpace)
				maxSpillSpace = spilledList->size * 2;
		}

		// assign this variable to the next free register
		int destinationIndex = assignRegister(activeList, inactiveList, this);
		// trimmedStr = strTrim(printBuf, sprintf(outputLine, ";introduce var %s to %%r%d", this->variable, destinationIndex));
		// ASMblock_append(outputBlock, outputLine);
		// place the value into the register if this is an argument (value starts on the stack)
		if (this->variable[0] != '.' && Scope_lookup(function->mainScope, this->variable)->type == e_argument)
		{
			struct variableEntry *theArgument = SymbolTableLookup(table, this->variable)->entry;
			trimmedStr = strTrim(printBuf, sprintf(printBuf, "mov %%r%d, %d(%%bp)", destinationIndex, theArgument->stackOffset));
			ASMblock_append(outputBlock, trimmedStr);
		}
	}

	// iterate all TAC lines in the current Basic Block
	struct LinkedListNode *TACRunner = thisBlock->TACList->head;
	while (TACRunner != NULL)
	{
		struct TACLine *currentTAC = TACRunner->data;
		TACIndex = currentTAC->index;
		expireOldIntervals(activeList, inactiveList, spilledList, TACIndex);

		// increment last used values of all active registers or reset if variable used in this step
		for (int i = 0; i < activeList->size; i++)
		{
			struct Register *thisReg = activeList->data[i];
			thisReg->lastUsed++;
			for (int i = 0; i < 3; i++)
			{
				if (currentTAC->operandPermutations[i] != vp_literal)
				{
					switch (currentTAC->operandTypes[i])
					{
					case vt_var:
						if (!strcmp(thisReg->lifetime->variable, currentTAC->operands[i]))
							thisReg->lastUsed = 0;

						break;

					default:
						break;
					}
				}
			}
		}

		switch (currentTAC->operation)
		{
			// function calls can use or discard a returned value
		case tt_call:
			if (currentTAC->operands[0] == NULL)
				break;

		case tt_declare:
		case tt_assign:
		case tt_add:
		case tt_subtract:
		case tt_div:
		case tt_mul:
		case tt_memr_1:
		case tt_memr_2:
		case tt_memr_3:

			while (currentLifetimeIndex < lifetimeCount && lifetimeArray[currentLifetimeIndex]->start <= TACIndex)
			{
				struct Lifetime *this = lifetimeArray[currentLifetimeIndex++];
				if (!strcmp(this->variable, currentTAC->operands[0]))
				{
					// spill a register if one isn't available
					if (inactiveList->size == 0)
					{
						spillRegister(activeList, inactiveList, spilledList, outputBlock, table);

						if (spilledList->size * 2 > maxSpillSpace)
							maxSpillSpace = spilledList->size * 2;
					}

					// assign this variable to the next free register
					int destinationIndex = assignRegister(activeList, inactiveList, this);
					trimmedStr = strTrim(printBuf, sprintf(printBuf, "\t;introduce var %s to %%r%d", this->variable, destinationIndex));
					ASMblock_append(outputBlock, trimmedStr);
					// place the value into the register if this is an argument (value starts on the stack)
					if (this->variable[0] != '.' && SymbolTableLookup(table, this->variable)->type == e_argument)
					{
						struct variableEntry *theArgument = SymbolTableLookup(table, this->variable)->entry;
						trimmedStr = strTrim(printBuf, sprintf(printBuf, "mov %%r%d, %d(%%bp)", destinationIndex, theArgument->stackOffset));
						// trimmedStr = strTrim(printBuf, sprintf(printBuf, "%s ;place argument %s", outputLine, this->variable);
						table
					}
				}

			default:
				break;
			}
*/
/*
 * double check spill space because some functions in regalloc may spill variables on their own
 * TODO: consider just making this the only check, any reason it wouldn't work?
 *
 */
/*
				if (spilledList->size * 2 > maxSpillSpace)
					maxSpillSpace = spilledList->size * 2;

				int destinationRegister;
				int firstSourceRegister;
				int secondSourceRegister;
				switch (currentTAC->operation)
				{
				case tt_assign:
				{
					destinationRegister = findOrPlaceAssignedVariable(activeList, inactiveList, spilledList, currentTAC->operands[0], outputBlock, table);
					if (currentTAC->operandPermutations[1] == vp_literal)
						table firstSourceRegister = findActiveVariable(activeList, currentTAC->operands[1]);
					if (firstSourceRegister != -1)
					{
						trimmedStr = strTrim(printBuf, sprintf(printBuf, "mov %%r%d, %%r%d", destinationRegister, firstSourceRegister));
					}
					else
					{
						trimmedStr = strTrim(printBuf, sprintf(printBuf, "mov %%r%d, %d(%%bp)", destinationRegister, findSpilledVariable(spilledList, currentTAC->operands[1])));
					}
				}
					// ASMblock_append(outputBlock, outputLine);
					ASMblock_append(outputBlock, trimmedStr);
				}
				break;

			case tt_add:
			case tt_subtract:
			case tt_mul:
			case tt_div:
			{
				// operand 1 is a literal
				if (currentTAC->operandPermutations[1] == vp_literal)
				{
					// operand 1 and 2 are literals
					if (currentTAC->operandPermutations[2] == vp_literal)
					{
						perror("Error - arithmetic between 2 literals\n");
						exit(2);
					}
					// only operand 1 is a literal
					else
					{
						perror("Error - arithmetic with literal as first operand\n");
						exit(2);
					}
				}
				// operand 1 is not a literal
				else
				{
					// operand 2 is a literal
					if (currentTAC->operandPermutations[2] == vp_literal)
					{
						int source1Register = findOrPlaceOperand(activeList, inactiveList, spilledList, currentTAC->operands[1], outputBlock, table);
						destinationRegister = findOrPlaceAssignedVariable(activeList, inactiveList, spilledList, currentTAC->operands[0], outputBlock, table);

						trimmedStr = strTrim(printBuf, sprintf(printBuf, "%si %%r%d, %%r%d, $%s", getAsmOp(currentTAC->operation), destinationRegister, source1Register, currentTAC->operands[2]));
					}
					// neither operand is a literal
					else
					{
						int source1Register = findOrPlaceOperand(activeList, inactiveList, spilledList, currentTAC->operands[1], outputBlock, table);
						int source2Register = findOrPlaceOperand(activeList, inactiveList, spilledList, currentTAC->operands[2], outputBlock, table);
						destinationRegister = findOrPlaceAssignedVariable(activeList, inactiveList, spilledList, currentTAC->operands[0], outputBlock, table);

						trimmedStr = strTrim(printBuf, sprintf(printBuf, "%s %%r%d, %%r%d, %%r%d", getAsmOp(currentTAC->operation), destinationRegister, source1Register, source2Register));
					}
				}

				ASMblock_append(outputBlock, trimmedStr);
			}
			break;

			case tt_memr_1:
			{
				int destinationIndex = findOrPlaceAssignedVariable(activeList, inactiveList, spilledList, currentTAC->operands[0], outputBlock, table);
				int baseIndex = findOrPlaceAssignedVariable(activeList, inactiveList, spilledList, currentTAC->operands[1], outputBlock, table);

				trimmedStr = strTrim(printBuf, sprintf(printBuf, "mov %%r%d, (%%r%d)", destinationIndex, baseIndex));

				// printedTAC = sPrintTACLine(currentTAC);
				// sprintf(finalOutputLine, "%s;%s", outputLine, printedTAC);
				// free(outputLine);
				// free(printedTAC);

				ASMblock_append(outputBlock, trimmedStr);
			}
			break;

			case tt_memr_2:
			{
				int dest = findOrPlaceAssignedVariable(activeList, inactiveList, spilledList, currentTAC->operands[0], outputBlock, table);
				int baseIndex = findOrPlaceAssignedVariable(activeList, inactiveList, spilledList, currentTAC->operands[1], outputBlock, table);
				trimmedStr = strTrim(printBuf, sprintf(printBuf, "mov %%r%d, %d(%%r%d)", dest, (int)(long int)currentTAC->operands[2], baseIndex));
				ASMblock_append(outputBlock, trimmedStr);
			}
			break;

			case tt_memr_3:
			{
				int dest = findOrPlaceAssignedVariable(activeList, inactiveList, spilledList, currentTAC->operands[0], outputBlock, table);
				int baseIndex = findOrPlaceAssignedVariable(activeList, inactiveList, spilledList, currentTAC->operands[1], outputBlock, table);
				int offsetIndex = findOrPlaceAssignedVariable(activeList, inactiveList, spilledList, currentTAC->operands[2], outputBlock, table);
				int scale = (int)(long int)currentTAC->operands[3];
				trimmedStr = strTrim(printBuf, sprintf(printBuf, "mov %%r%d, %%r%d(%%r%d, $%d)", dest, offsetIndex, baseIndex, scale));
				ASMblock_append(outputBlock, trimmedStr);
			}
			break;

			case tt_memw_1:
			{

				int destinationIndex = findOrPlaceAssignedVariable(activeList, inactiveList, spilledList, currentTAC->operands[0], outputBlock, table);
				int sourceIndex = findOrPlaceAssignedVariable(activeList, inactiveList, spilledList, currentTAC->operands[1], outputBlock, table);

				trimmedStr = strTrim(printBuf, sprintf(printBuf, "mov (%%r%d), %%r%d", destinationIndex, sourceIndex));
				ASMblock_append(outputBlock, trimmedStr);
			}
			break;

			case tt_memw_2:
			{
				int baseIndex = findOrPlaceAssignedVariable(activeList, inactiveList, spilledList, currentTAC->operands[0], outputBlock, table);
				int sourceIndex = findOrPlaceAssignedVariable(activeList, inactiveList, spilledList, currentTAC->operands[2], outputBlock, table);

				trimmedStr = strTrim(printBuf, sprintf(printBuf, "mov %d(%%r%d), %%r%d", (int)(long int)currentTAC->operands[1], baseIndex, sourceIndex));

				ASMblock_append(outputBlock, trimmedStr);
			}
			break;

			case tt_memw_3:
			{
				int baseIndex = findOrPlaceAssignedVariable(activeList, inactiveList, spilledList, currentTAC->operands[0], outputBlock, table);
				int offsetIndex = findOrPlaceAssignedVariable(activeList, inactiveList, spilledList, currentTAC->operands[1], outputBlock, table);
				int sourceIndex = findOrPlaceAssignedVariable(activeList, inactiveList, spilledList, currentTAC->operands[3], outputBlock, table);
				trimmedStr = strTrim(printBuf, sprintf(printBuf, "mov %%r%d(%%r%d, $%d), %%r%d", offsetIndex, baseIndex, (int)(long int)currentTAC->operands[2], sourceIndex));
				ASMblock_append(outputBlock, trimmedStr);
			}
			break;

			case tt_push:
			{
				switch (currentTAC->operandPermutations[0])
				{
				case vp_literal:
					trimmedStr = strTrim(printBuf, sprintf(printBuf, "push $%s", currentTAC->operands[0]));
					break;

				case vp_standard:
				case vp_temp:
				{
					int sourceRegister = findActiveVariable(activeList, currentTAC->operands[0]);
					if (sourceRegister == -1)
					{
						sourceRegister = unSpillVariable(activeList, inactiveList, spilledList, currentTAC->operands[0], outputBlock, table);
					}
					trimmedStr = strTrim(printBuf, sprintf(printBuf, "push %%r%d", sourceRegister));
				}
				break;

				default:
					perror("Unexpected TAC type in push TAC!");
					exit(2);
				}
				ASMblock_append(outputBlock, trimmedStr);
			}
			break;

			case tt_call:
			{
				trimmedStr = strTrim(printBuf, sprintf(printBuf, "call %s", currentTAC->operands[1]));
				ASMblock_append(outputBlock, trimmedStr);

				// the call returns a value
				if (currentTAC->operands[0] != NULL)
				{
					int destinationRegister = findActiveVariable(activeList, currentTAC->operands[0]);
					if (destinationRegister != -1)
					{
						trimmedStr = strTrim(printBuf, sprintf(printBuf, "mov %%r%d, %%rr", destinationRegister));
					}
					else
					{
						int destinationOffset = findSpilledVariable(spilledList, currentTAC->operands[0]);
						trimmedStr = strTrim(printBuf, sprintf(printBuf, "mov %d(%%sp), %%rr", destinationOffset));
					}
					ASMblock_append(outputBlock, trimmedStr);
				}
			}
			break;

			case tt_cmp:
			{
				firstSourceRegister = findActiveVariable(activeList, currentTAC->operands[1]);

				secondSourceRegister = findActiveVariable(activeList, currentTAC->operands[2]);

				// both source operands are variables in registers
				if (firstSourceRegister != -1 && secondSourceRegister != -1)
				{
					if (firstSourceRegister == secondSourceRegister)
					{
						trimmedStr = strTrim(printBuf, sprintf(printBuf, "cmp %%r%d, %%r%d", firstSourceRegister, firstSourceRegister));
					}
					else
					{
						trimmedStr = strTrim(printBuf, sprintf(printBuf, "cmp %%r%d, %%r%d", firstSourceRegister, secondSourceRegister));
					}
				}
				else
				{
					// first source exists in register, second is spilled or a literal
					if (firstSourceRegister != -1 && secondSourceRegister == -1)
					{
						if (currentTAC->operandPermutations[2] == vp_literal)
						{
							trimmedStr = strTrim(printBuf, sprintf(printBuf, "cmpi %%r%d, $%s", firstSourceRegister, currentTAC->operands[2]));
						}
						else
						{
							trimmedStr = strTrim(printBuf, sprintf(printBuf, "cmp %%r%d, %d(%%bp)", firstSourceRegister, findSpilledVariable(spilledList, currentTAC->operands[2])));
						}
					}
					// second source exists in register, first is spilled
					else if (firstSourceRegister == -1 && secondSourceRegister != -1)
					{
						firstSourceRegister = unSpillVariable(activeList, inactiveList, spilledList, currentTAC->operands[1], outputBlock, table);

						trimmedStr = strTrim(printBuf, sprintf(printBuf, "cmp %%r%d, %%r%d", firstSourceRegister, secondSourceRegister));
					}

					// both sources are spilled - will break if both are literals but this should be checked earlier
					else
					{
						firstSourceRegister = unSpillVariable(activeList, inactiveList, spilledList, currentTAC->operands[1], outputBlock, table);

						if (currentTAC->operandPermutations[2] == vp_literal)
						{
							trimmedStr = strTrim(printBuf, sprintf(printBuf, "cmp %%r%d, $%s", firstSourceRegister, currentTAC->operands[2]));
						}
						else
						{
							trimmedStr = strTrim(printBuf, sprintf(printBuf, "cmp %%r%d, %d(%%bp)", firstSourceRegister, findSpilledVariable(spilledList, currentTAC->operands[2])));
						}
					}
				}
				ASMblock_append(outputBlock, trimmedStr);
				table // deep copy the states and push them to the state stack
					struct SavedState *duplicatedState = duplicateCurrentState(activeList, inactiveList, spilledList, currentLifetimeIndex);
				Stack_push(savedStateStack, duplicatedState);
			}
			break;

			case tt_restorestate:
				restoreRegisterStates(savedStateStack, activeList, inactiveList, spilledList, &currentLifetimeIndex, (long int)currentTAC->operands[0], outputBlock);
				break;

			case tt_resetstate:
				resetRegisterStates(savedStateStack, activeList, inactiveList, spilledList, &currentLifetimeIndex);
				break;

			case tt_popstate:
			{
				struct SavedState *poppedState = Stack_pop(savedStateStack);
				while (poppedState->activeList->size > 0)
				{
					table
					{
						free(Stack_pop(poppedState->inactiveList));
					}
					Stack_free(poppedState->inactiveList);

					while (poppedState->spilledList->size > 0)
					{
						free(Stack_pop(poppedState->spilledList));
					}
					Stack_free(poppedState->spilledList);
					free(poppedState);
				}
				break;

			case tt_return:
			{
				// find where the return value is (it must be active in a register because it was just assigned!)
				switch (currentTAC->operandPermutations[0])
				{
				case vp_literal:
				{
					trimmedStr = strTrim(printBuf, sprintf(printBuf, "mov %%rr, $%s", currentTAC->operands[0]));
					ASMblock_append(outputBlock, trimmedStr);
				}
					table case vp_standard : case vp_temp:
					{
						int sourceRegister = findActiveVariable(activeList, currentTAC->operands[0]);
						if (sourceRegister != -1)
						{
							trimmedStr = strTrim(printBuf, sprintf(printBuf, "mov %%rr, %%r%d", sourceRegister));
						}
						else
						{
							trimmedStr = strTrim(printBuf, sprintf(printBuf, "mov %%rr, %d(%%bp)", findSpilledVariable(spilledList, currentTAC->operands[0])));
						}
						ASMblock_append(outputBlock, trimmedStr);
					}
					break;

				default:
					perror("unexpected type in return TAC!\n");
					table break;
				}

			case tt_do:
				break;

			case tt_enddo:
				break;

			case tt_jg:
			case tt_jge:
			case tt_jl:
			case tt_jle:
			case tt_je:
			case tt_jne:
			case tt_jmp:
			{
				trimmedStr = strTrim(printBuf, sprintf(printBuf, "%s %s_%ld", getAsmOp(currentTAC->operation), table->name, (long int)currentTAC->operands[0]));
				ASMblock_append(outputBlock, trimmedStr);
				break;
			}
			break;

			case tt_label:
				trimmedStr = strTrim(printBuf, sprintf(printBuf, ".%s_%ld:", table->name, (long int)currentTAC->operands[0]));
				ASMblock_append(outputBlock, trimmedStr);
				break;

			case tt_asm:
				// copy the line because it lives in dictionary
				// freeing the operand itself in freeASM() will result in double free when freeing dictionary later
				strcpy(printBuf, currentTAC->operands[0]);
				trimmedStr = strTrim(printBuf, strlen(printBuf));
				ASMblock_append(outputBlock, trimmedStr);
				break;

			// skip declarations, these are handled by the register allocator
			case tt_declare:
				break;

			default:
				printTACLine(currentTAC);
				printf("\n");
				perror("Error - Unexpected TAC type while generating code\n");
				exit(2);
				break;
			}

				registerLoads[inactiveList->size]++;
				int activeSpills = 0;
				for (int i = 0; i < spilledList->size; i++)
				{
					struct SpilledRegister *examinedSpill = spilledList->data[i];
					if (examinedSpill->occupied)
						activeSpills++;
				}
				stackLoads[activeSpills]++;
				TACRunner = TACRunner->next;

				sortByEndPoint((struct Register **)activeList->data, activeList->size);
				sortByRegisterNumber((struct Register **)inactiveList->data, inactiveList->size);
				for (int i = 0; i < activeList->size; i++)
				{
					struct Register *activeRegister = activeList->data[i];
					touchedRegisters[activeRegister->index] = 1;
				}
				// printCurrentState(activeList, inactiveList, spilledList);
			}
				printf(".");
				blockRunner = blockRunner->next;
			}

			trimmedStr = strTrim(printBuf, sprintf(printBuf, "%s_done:", table->name));
			ASMblock_append(outputBlock, trimmedStr);

			for (int i = 0; i < REGISTER_COUNT; i++)
			{
				if (touchedRegisters[i])
				{
					trimmedStr = strTrim(printBuf, sprintf(printBuf, "push %%r%d", i));
					ASMblock_prepend(outputBlock, trimmedStr);
					trimmedStr = strTrim(printBuf, sprintf(printBuf, "pop %%r%d", i));
					ASMblock_append(outputBlock, trimmedStr);
				}
			}

			if (maxSpillSpace > 0)
			{
				trimmedStr = strTrim(printBuf, sprintf(printBuf, "subi %%sp, %%sp, $%d", maxSpillSpace));
				ASMblock_prepend(outputBlock, trimmedStr);
			}

			trimmedStr = strTrim(printBuf, sprintf(printBuf, "%s:", table->name));
			ASMblock_prepend(outputBlock, trimmedStr);

			if (maxSpillSpace > 0)
			{
				trimmedStr = strTrim(printBuf, sprintf(printBuf, "addi %%sp, %%sp, $%d", maxSpillSpace));
				ASMblock_append(outputBlock, trimmedStr);
			}

			trimmedStr = strTrim(printBuf, sprintf(printBuf, "ret %d", table->argStackSize));
			ASMblock_append(outputBlock, trimmedStr);

			int totalCodeSteps = 0;
			for (int i = 0; i < REGISTER_COUNT; i++)
			{
				totalCodeSteps += registerLoads[i];
			}

			if (totalCodeSteps > 0 && 0)
			{
				printf("Done generating code for %d TAC steps\n\tFree registers\n        ", totalCodeSteps);
				for (int i = 0; i < REGISTER_COUNT; i++)
					printf("%4d ", i);

				printf("\n        ");
				for (int i = 0; i < REGISTER_COUNT; i++)
					printf("-----");

				printf("\n# steps:");
				for (int i = 0; i < REGISTER_COUNT; i++)
					printf("%4d ", registerLoads[i]);

				printf("\n %% time:");
				for (int i = 0; i < REGISTER_COUNT; i++)
					printf("%3d%% ", (registerLoads[i] * 100) / totalCodeSteps);

				printf("\n\n");

				printf("\tSpilled variables\n        ");
				for (int i = 0; i <= maxSpillSpace / 2; i++)
					printf("%4d ", i);

				printf("\n        ");
				for (int i = 0; i <= maxSpillSpace / 2; i++)
					printf("-----");

				printf("\n# steps:");
				for (int i = 0; i <= maxSpillSpace / 2; i++)
					printf("%4d ", stackLoads[i]);

				printf("\n %% time:");
				for (int i = 0; i <= maxSpillSpace / 2; i++)
					printf("%3d%% ", (stackLoads[i] * 100) / totalCodeSteps);
			}
			printf("\n");

			free(registerLoads);

			free(stackLoads);

			// clean up active, inactive, and spilled lists
			while (activeList->size > 0)
				free(Stack_pop(activeList));

			Stack_free(activeList);

			while (inactiveList->size > 0)
				free(Stack_pop(inactiveList));

			Stack_free(inactiveList);

			while (spilledList->size > 0)
				free(Stack_pop(spilledList));

			Stack_free(spilledList);

			LinkedList_free(lifetimes, 0);

			for (int i = 0; i < lifetimeCount; i++)
				free(lifetimeArray[i]);

			free(lifetimeArray);

			if (savedStateStack->size > 0)
			{
				perror("saved state stack not empty after code generation!");
				exit(1);
			}
			Stack_free(savedStateStack);
			return outputBlock;
		}
	}
	*/
