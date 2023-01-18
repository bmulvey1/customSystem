#include "codegen.h"

int ALIGNSIZE(unsigned int size)
{
	unsigned int mask = 0b1;
	int i = 0;
	while (mask < size)
	{
		mask <<= 1;
		i++;
	}
	return i;
}

void PlaceLiteralInRegister(struct ASMblock *currentBlock, char *literalStr, char *destReg)
{
	char *thisLine = malloc(64);
	int literalValue = atoi(literalStr);
	if (literalValue < 0x100)
	{
		sprintf(thisLine, "movb %s, $%s", destReg, literalStr);
	}
	else if (literalValue < 0x10000)
	{
		sprintf(thisLine, "movh %s, $%s", destReg, literalStr);
	}
	else
	{
		int firstHalf, secondHalf;
		firstHalf = literalValue & 0xffff;
		secondHalf = literalValue >> 16;
		char halvedString[16]; // will be long enough to hold any int32/uint32 so can definitely hold half of one

		sprintf(halvedString, "%d", secondHalf);
		sprintf(thisLine, "movh %s, $%s", destReg, halvedString);

		ASM_append(currentBlock, thisLine);
		thisLine = malloc(64);
		sprintf(thisLine, "shli %s, $16", destReg);
		ASM_append(currentBlock, thisLine);
		thisLine = malloc(64);

		sprintf(halvedString, "%d", firstHalf);
		sprintf(thisLine, "movh %s, $%s", destReg, halvedString);
	}
	ASM_append(currentBlock, thisLine);
}

struct Stack *generateCode(struct SymbolTable *table, FILE *outFile)
{
	printf("generate code for [%s]\n", table->name);
	return generateCodeForScope(table->globalScope, outFile);
};

struct Stack *generateCodeForScope(struct Scope *scope, FILE *outFile)
{
	struct Stack *scopeBlocks = Stack_New();
	printf("generate code for scope [%s] (size %d)\n", scope->name, scope->entries->size);
	for (int i = 0; i < scope->entries->size; i++)
	{
		struct ScopeMember *thisMember = scope->entries->data[i];
		switch (thisMember->type)
		{
		case e_function:
			Stack_Push(scopeBlocks, generateCodeForFunction(thisMember->entry, outFile));
			break;

		case e_basicblock:
		{
			struct ASMblock *blockBlock = newASMblock();
			char touchedRegisters[REGISTER_COUNT];
			int reservedRegisters[2];
			reservedRegisters[0] = 0;
			reservedRegisters[1] = 1;
			GenerateCodeForBasicBlock(thisMember->entry, NULL, blockBlock, "global", reservedRegisters, touchedRegisters);
			Stack_Push(scopeBlocks, blockBlock);
		}
		break;

		default:
			break;
		}
	}
	return scopeBlocks;
}

/*
 * code generation for funcitons (lifetime management, etc)
 *
 */

int calculateRegisterLoading(struct LinkedList *activeLifetimes, int index)
{
	int trueLoad = 0;
	for (struct LinkedListNode *runner = activeLifetimes->head; runner != NULL; runner = runner->next)
	{
		struct Lifetime *thisLifetime = runner->data;
		if (thisLifetime->start <= index && index < thisLifetime->end)
		{
			trueLoad++;
		}
		else if (thisLifetime->end >= index)
		{
			trueLoad--;
		}
	}
	return trueLoad;
}

struct FunctionRegisterAllocationMetadata
{
	struct FunctionEntry *function; // symbol table entry for the function the register allocation data is for

	struct LinkedList *allLifetimes; // every lifetime that exists within this function

	// array allocated (of size largestTacIndex) for liveness analysis
	// index i contains a linkedList of all lifetimes active at TAC index i
	struct LinkedList **lifetimeOverlaps;

	// tracking for specialized lifetimes which may be removed from lifetimeOverlaps and need to be explicitly tracked
	struct Stack *spilledLifetimes;
	struct Stack *localPointerLifetimes;

	// largest TAC index for any basic block within the function
	int largestTacIndex;

	// flag 2 registers which should be used as scratch in case we have spilled variables (not always used)
	int reservedRegisters[2];

	// flag registers which have *ever* been used so we know what to callee-save
	char touchedRegisters[REGISTER_COUNT];
};

// populate a linkedlist array so that the list at index i contains all lifetimes active at TAC index i
// then determine which variables should be spilled
int generateLifetimeOverlaps(struct FunctionRegisterAllocationMetadata *metadata)
{
	int mostConcurrentLifetimes = 0;

	// populate the array of active lifetimes
	for (struct LinkedListNode *runner = metadata->allLifetimes->head; runner != NULL; runner = runner->next)
	{
		struct Lifetime *thisLifetime = runner->data;

		// if this lifetime must be spilled (has address-of operator used) (for future use - nothing currently uses(?)), add directly to the spilled list
		struct ScopeMember *thisVariableEntry = Scope_lookup(metadata->function->mainScope, thisLifetime->variable);

		// if we have an argument, make sure to note that it is active at index 0
		// this ensures that arguments that aren't used in code are still tracked (applies particularly to asm-only functions)
		if (thisLifetime->isArgument)
		{
			LinkedList_Append(metadata->lifetimeOverlaps[0], thisLifetime);
		}

		if (thisVariableEntry != NULL && ((struct VariableEntry *)thisVariableEntry->entry)->mustSpill)
		{
			thisLifetime->isSpilled = 1;
			Stack_Push(metadata->spilledLifetimes, thisLifetime);
		}
		// otherwise, put this lifetime into contention for a register
		else
		{
			// if we have a local pointer, make sure we track it in the localpointers stack
			// but also put it into contention for a register, we will just prefer to spill localpointers first
			if (thisVariableEntry != NULL && ((struct VariableEntry *)thisVariableEntry->entry)->localPointerTo != NULL)
			{
				thisLifetime->localPointerTo = ((struct VariableEntry *)thisVariableEntry->entry)->localPointerTo;
				thisLifetime->stackOrRegLocation = -1;
				Stack_Push(metadata->localPointerLifetimes, thisLifetime);
			}
			for (int i = thisLifetime->start; i <= thisLifetime->end; i++)
			{
				LinkedList_Append(metadata->lifetimeOverlaps[i], thisLifetime);
				if (metadata->lifetimeOverlaps[i]->size > mostConcurrentLifetimes)
				{
					mostConcurrentLifetimes = metadata->lifetimeOverlaps[i]->size;
				}
			}
		}
	}
	// printf("Function %s has at most %d concurrent lifetimes (largest TAC index %d)\n", metadata->function->name, mostConcurrentLifetimes, metadata->largestTacIndex);
	return mostConcurrentLifetimes;
}

void spillVariables(struct FunctionRegisterAllocationMetadata *metadata, int mostConcurrentLifetimes)
{
	int MAXREG = REGISTER_COUNT;
	// if we have just enough room, simply use all registers
	// if we need to spill, ensure 2 scratch registers
	if (mostConcurrentLifetimes >= REGISTER_COUNT)
	{
		MAXREG -= 2;
		metadata->reservedRegisters[0] = 0;
		metadata->reservedRegisters[1] = 1;
	}
	else
	{
		metadata->reservedRegisters[0] = mostConcurrentLifetimes;
		metadata->reservedRegisters[1] = mostConcurrentLifetimes + 1;
	}

	// look through the populated array of active lifetimes
	// if a given index has too many active lifetimes, figure out which lifetime(s) to spill
	// then allocate registers for any lifetimes without a home
	for (int i = 0; i <= metadata->largestTacIndex; i++)
	{
		while (calculateRegisterLoading(metadata->lifetimeOverlaps[i], i) > MAXREG)
		{
			int bestHeuristic = -1;
			struct Lifetime *bestLifetime = NULL;
			for (struct LinkedListNode *overlapRunner = metadata->lifetimeOverlaps[i]->head; overlapRunner != NULL; overlapRunner = overlapRunner->next)
			{
				struct Lifetime *thisLifetime = overlapRunner->data;

				// base heuristic is lifetime length (is this worth it?)
				int thisHeuristic = (thisLifetime->end - thisLifetime->start);
				// add the number of reads for this variable since they have some cost
				thisHeuristic += (thisLifetime->nreads);

				// multiply by number of writes for this variable since that is a high-cost operation
				thisHeuristic *= (thisLifetime->nwrites);

				// inflate heuristics for cases which have no actual stack space cost to spill:
				// super-prefer to "spill" arguments as they already have a stack address
				if (thisLifetime->isArgument)
				{
					thisHeuristic *= 1000;
				}
				// secondarily prefer to "spill" ointers to local objects
				// they can be generated on-the-fly from the base pointer with 1 arithmetic instruction
				else if (thisLifetime->localPointerTo != NULL)
				{
					thisHeuristic *= 100;
				}

				// printf("%s has heuristic of %f\n", thisLifetime->variable, thisHeuristic);
				if (thisHeuristic > bestHeuristic)
				{
					bestHeuristic = thisHeuristic;
					bestLifetime = thisLifetime;
				}
			}

			// this method actually deletes the spilled variable from the liveness array
			// if it becomes necessary to keep the untouched liveness array around, it will need to be copied within this function
			for (int j = bestLifetime->start; j <= bestLifetime->end; j++)
			{
				LinkedList_Delete(metadata->lifetimeOverlaps[j], compareLifetimes, bestLifetime->variable);
			}
			bestLifetime->isSpilled = 1;
			Stack_Push(metadata->spilledLifetimes, bestLifetime);
		}
	}
}

// sort the list of spilled lifetimes by size of the variable so they can be laid out cleanly on the stack
void sortSpilledLifetimes(struct FunctionRegisterAllocationMetadata *metadata)
{
	// TODO: handle arrays properly?
	//  - this shouldn't be a consideration because the only lifetimes that are considered to be spilled are localpointers?
	//  - but localpointers will still be in the spilled list if they aren't being kept in registers, because they need somewhere to be
	//     despite the fact that they really don't actually be spilled to stack, instead just generated from base pointer and offset

	// simple bubble sort
	for (int i = 0; i < metadata->spilledLifetimes->size; i++)
	{
		for (int j = 0; j < metadata->spilledLifetimes->size - i - 1; j++)
		{
			struct Lifetime *thisLifetime = metadata->spilledLifetimes->data[j];
			int thisSize = Scope_getSizeOfVariable(metadata->function->mainScope, thisLifetime->variable);
			int compSize = Scope_getSizeOfVariable(metadata->function->mainScope, ((struct Lifetime *)metadata->spilledLifetimes->data[j + 1])->variable);

			if (thisSize < compSize)
			{
				struct Lifetime *swap = metadata->spilledLifetimes->data[j];
				metadata->spilledLifetimes->data[j] = metadata->spilledLifetimes->data[j + 1];
				metadata->spilledLifetimes->data[j + 1] = swap;
			}
		}
	}
}

void assignRegisters(struct FunctionRegisterAllocationMetadata *metadata)
{
	// flag registers in use at any given TAC index so we can easily assign
	char registers[REGISTER_COUNT];
	struct Lifetime *occupiedBy[REGISTER_COUNT];

	for (int i = 0; i < REGISTER_COUNT; i++)
	{
		registers[i] = 0;
		occupiedBy[i] = NULL;
		metadata->touchedRegisters[i] = 0;
	}

	// reserve scratch registers for arithmetic
	registers[metadata->reservedRegisters[0]] = 1;
	registers[metadata->reservedRegisters[1]] = 1;

	for (int i = 0; i < metadata->largestTacIndex; i++)
	{
		// free any registers inhabited by expired lifetimes
		for (int j = 0; j < REGISTER_COUNT; j++)
		{
			if (occupiedBy[j] != NULL && occupiedBy[j]->end <= i)
			{
				registers[j] = 0;
				occupiedBy[j] = NULL;
			}
		}

		// iterate all lifetimes and assign newly-live ones to a register
		for (struct LinkedListNode *ltRunner = metadata->allLifetimes->head; ltRunner != NULL; ltRunner = ltRunner->next)
		{
			struct Lifetime *thisLifetime = ltRunner->data;
			if ((thisLifetime->start == i) &&			  // if the lifetime starts at this step
				(thisLifetime->isSpilled == 0) &&		  // lifetime expects to be in a register
				(thisLifetime->stackOrRegLocation == -1)) // lifetime does not already have a register somehow (redundancy check)
			{
				char registerFound = 0;
				// scan through all registers, looking for an unoccupied one
				for (int j = 0; j < REGISTER_COUNT; j++)
				{
					if (registers[j] == 0)
					{
						// printf("\tAssign register %d for variable %s\n", j, thisLifetime->variable);
						thisLifetime->stackOrRegLocation = j;
						registers[j] = 1;
						occupiedBy[j] = thisLifetime;
						metadata->touchedRegisters[j] = 1;
						registerFound = 1;
						break;
					}
				}
				// no unoccupied register found (redundancy check)
				if (!registerFound)
				{
					/*
					 * if we hit this, either:
					 * 1: something messed up in this function and we ended up with no register to assign this lifetime to
					 * 2: something messed up before we got to this function and too many concurrent lifetimes have been allowed to expect a register assignment
					 */

					ErrorAndExit(ERROR_INTERNAL, "Unable to find register for variable %s!\n", thisLifetime->variable);
				}
			}
		}
	}
}

struct ASMblock *generateCodeForFunction(struct FunctionEntry *function, FILE *outFile)
{
	printf("Generate code for function %s", function->name);

	struct FunctionRegisterAllocationMetadata metadata;
	metadata.function = function;
	metadata.allLifetimes = findLifetimes(function);

	// find all overlapping lifetimes, to figure out which variables can live in registers vs being spilled
	metadata.largestTacIndex = 0;
	for (struct LinkedListNode *runner = metadata.allLifetimes->head; runner != NULL; runner = runner->next)
	{
		struct Lifetime *thisLifetime = runner->data;
		if (thisLifetime->end > metadata.largestTacIndex)
		{
			metadata.largestTacIndex = thisLifetime->end;
		}
	}

	// generate an array of lists corresponding to which lifetimes are active at a given TAC step by index in the array
	metadata.lifetimeOverlaps = malloc((metadata.largestTacIndex + 1) * sizeof(struct LinkedList *));
	for (int i = 0; i <= metadata.largestTacIndex; i++)
	{
		metadata.lifetimeOverlaps[i] = LinkedList_New();
	}

	metadata.spilledLifetimes = Stack_New();
	metadata.localPointerLifetimes = Stack_New();

	metadata.reservedRegisters[0] = -1;
	metadata.reservedRegisters[1] = -1;

	int mostConcurrentLifetimes = generateLifetimeOverlaps(&metadata);

	// if there are too many concurrent lifetimes to all fit in registers, spill some of them
	if (mostConcurrentLifetimes > REGISTER_COUNT)
	{
		spillVariables(&metadata, mostConcurrentLifetimes);
	}

	sortSpilledLifetimes(&metadata);

	// find the total size of the function's stack frame containing local variables *and* spilled variables
	int stackOffset = function->localStackSize; // start with just the things guaranteed to be on the local stack
	for (int i = 0; i < metadata.spilledLifetimes->size; i++)
	{
		struct Lifetime *thisLifetime = metadata.spilledLifetimes->data[i];
		int thisSize = Scope_getSizeOfVariable(function->mainScope, thisLifetime->variable);
		struct ScopeMember *thisVariableEntry = Scope_lookup(function->mainScope, thisLifetime->variable);
		if (thisVariableEntry != NULL)
		{
			struct VariableEntry *thisVariable = thisVariableEntry->entry;
			if (thisVariableEntry->type == e_argument)
			{
				thisLifetime->stackOrRegLocation = thisVariable->stackOffset;
			}
			else
			{
				// constant offset of -2 for return address
				thisLifetime->stackOrRegLocation = (-1 * stackOffset) - 2;
				stackOffset += thisSize;
			}
		}
	}

	assignRegisters(&metadata);

	/*
	for (struct LinkedListNode *runner = metadata.allLifetimes->head; runner != NULL; runner = runner->next)
	{
		struct Lifetime *thisLifetime = runner->data;
		if (thisLifetime->isSpilled)
		{
			printf("%16s: %d(%%bp)\n", thisLifetime->variable, thisLifetime->stackOrRegLocation);
		}
		else
		{
			printf("%16s: %%r%d\n", thisLifetime->variable, thisLifetime->stackOrRegLocation);
		}
	}*/

	// actual registers have been assigned to variables
	printf(".");

	struct ASMblock *functionBlock = newASMblock();
	char *functionSetup;

	// move any applicable arguments into registers if we are expecting them not to be spilled
	for (struct LinkedListNode *ltRunner = metadata.allLifetimes->head; ltRunner != NULL; ltRunner = ltRunner->next)
	{
		struct Lifetime *thisLifetime = ltRunner->data;

		// (short-circuit away from looking up temps since they can't be arguments)
		if (thisLifetime->variable[0] != '.')
		{
			struct ScopeMember *thisEntry = Scope_lookup(function->mainScope, thisLifetime->variable);
			// we need to place this variable into its register if:
			if ((thisEntry != NULL) &&							 // it exists
				(thisEntry->type == e_argument) &&				 // it's an argument
				(!thisLifetime->isSpilled) &&					 // they're not spilled
				(thisLifetime->nreads || thisLifetime->nwrites)) // theyre are either read from or written to at all
			{
				struct VariableEntry *theArgument = thisEntry->entry;
				functionSetup = malloc(64);
				sprintf(functionSetup, "mov %%r%d, (%%bp+%d) ;place %s", thisLifetime->stackOrRegLocation, theArgument->stackOffset, thisLifetime->variable);
				metadata.touchedRegisters[thisLifetime->stackOrRegLocation] = 1;
				ASM_append(functionBlock, functionSetup);
			}
		}
	}

	// arguments placed into registers
	printf(".");

	for (struct LinkedListNode *blockRunner = function->BasicBlockList->head; blockRunner != NULL; blockRunner = blockRunner->next)
	{
		struct BasicBlock *thisBlock = blockRunner->data;
		GenerateCodeForBasicBlock(thisBlock, metadata.allLifetimes, functionBlock, function->name, metadata.reservedRegisters, metadata.touchedRegisters);
	}

	// meaningful code generated
	printf(".");

	functionSetup = malloc(64);
	sprintf(functionSetup, "%s_done:", function->name);
	ASM_append(functionBlock, functionSetup);

	for (int i = REGISTER_COUNT - 1; i >= 0; i--)
	{
		if (metadata.touchedRegisters[i])
		{
			functionSetup = malloc(64);
			sprintf(functionSetup, "pop %%r%d", i);
			ASM_append(functionBlock, functionSetup);

			functionSetup = malloc(64);
			sprintf(functionSetup, "push %%r%d", i);
			ASM_prepend(functionBlock, functionSetup);
		}
	}

	if (stackOffset > 0)
	{
		functionSetup = malloc(64);
		sprintf(functionSetup, "subi %%sp, %%sp, $%d", stackOffset);
		ASM_prepend(functionBlock, functionSetup);
	}

	functionSetup = malloc(64);
	sprintf(functionSetup, "%s:", function->name);
	ASM_prepend(functionBlock, functionSetup);

	functionSetup = malloc(64);
	sprintf(functionSetup, "#align 2048");
	ASM_prepend(functionBlock, functionSetup);

	if (stackOffset > 0)
	{
		functionSetup = malloc(64);
		sprintf(functionSetup, "addi %%sp, %%sp, $%d", stackOffset);
		ASM_append(functionBlock, functionSetup);
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
	ASM_append(functionBlock, functionSetup);

	// function setup and teardown code generated
	printf(".");

	Stack_Free(metadata.spilledLifetimes);
	Stack_Free(metadata.localPointerLifetimes);
	LinkedList_Free(metadata.allLifetimes, free);

	for (int i = 0; i <= metadata.largestTacIndex; i++)
	{
		LinkedList_Free(metadata.lifetimeOverlaps[i], NULL);
	}
	free(metadata.lifetimeOverlaps);

	printf("\n");
	return functionBlock;
}

void GenerateCodeForBasicBlock(struct BasicBlock *thisBlock, struct LinkedList *allLifetimes, struct ASMblock *asmBlock, char *functionName, int reservedRegisters[2], char *touchedRegisters)
{
	// TODO: generate localpointers as necessary
	// to registers for those that get them and on-the-fly for those that don't
	char *blockLabel = malloc(64);
	if (thisBlock->labelNum > 0)
	{
		sprintf(blockLabel, "%s_%d:", functionName, thisBlock->labelNum);
	}
	else
	{
		sprintf(blockLabel, "%s_0:", functionName);
	}
	ASM_append(asmBlock, blockLabel);

	for (struct LinkedListNode *TACRunner = thisBlock->TACList->head; TACRunner != NULL; TACRunner = TACRunner->next)
	{
		struct TACLine *thisTAC = TACRunner->data;
		char *thisLine;
		thisLine = malloc(64);
		char *printedTAC = sPrintTACLine(thisTAC);
		sprintf(thisLine, ";%s", printedTAC);
		ASM_append(asmBlock, thisLine);
		free(printedTAC);

		switch (thisTAC->operation)
		{
		case tt_asm:
			ASM_append(asmBlock, thisTAC->operands[0]);
			break;

		case tt_assign:
		{
			struct Lifetime *assignedLifetime = LinkedList_Find(allLifetimes, compareLifetimes, thisTAC->operands[0]);
			struct Lifetime *assignerLifetime = LinkedList_Find(allLifetimes, compareLifetimes, thisTAC->operands[1]);
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
						sprintf(thisLine, "mov %%r%d, (%%bp+%d*1)", assignedLifetime->stackOrRegLocation, assignerLifetime->stackOrRegLocation);
					}
					break;

				case vp_literal:
				{
					// char destReg[5];
					// sprintf(destReg, "%%r%d", assignedLifetime->stackOrRegLocation);
					// PlaceLiteralInRegister(asmBlock, thisTAC->operands[1], destReg);
					// free(thisLine);
					// thisLine = NULL;
				}
				sprintf(thisLine, "movh %%r%d, $%s", assignedLifetime->stackOrRegLocation, thisTAC->operands[1]);
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
						sprintf(thisLine, "mov (%%bp+%d), %%r%d", assignedLifetime->stackOrRegLocation, assignerLifetime->stackOrRegLocation);
					}
					else
					{
						// in this case, the value being read needs to be pulled down to a register
						// then put back up onto the stack to assign to the spilled variable
						ErrorAndExit(ERROR_INTERNAL, "assign from spilled to spilled!\n");
					}
					break;

				case vp_literal:
				{
					// if the literal is too large for one instruction, we will need to put it into a register first
					// if (atoi(thisTAC->operands[1]) > 0xffff)
					// {
						// char destReg[5];
						// this use of the first reserved register could potentially stomp other operations if we expect them to persist over more than one TAC instruction
						// expecting this is unrealistic but leaving this comment juuuuust in case
						// sprintf(destReg, "%%r%d", reservedRegisters[0]);
						// PlaceLiteralInRegister(asmBlock, thisTAC->operands[1], destReg);
						// sprintf(thisLine, "mov (%%bp+%d), %%r%d", assignedLifetime->stackOrRegLocation, reservedRegisters[0]);
					// }
					// else
					// {
						sprintf(thisLine, "mov (%%bp+%d), $%s", assignedLifetime->stackOrRegLocation, thisTAC->operands[1]);
					// }
				}
				break;
				}
			}
			if (thisLine != NULL)
			{
				ASM_append(asmBlock, thisLine);
			}
		}
		break;

		// place declared localpointer in register if necessary
		// everything else (args and regular variables) handled explicitly elsewhere
		case tt_declare:
		{
			struct Lifetime *declaredLifetime = LinkedList_Find(allLifetimes, compareLifetimes, thisTAC->operands[0]);
			if ((declaredLifetime->localPointerTo != NULL) &&
				(!declaredLifetime->isSpilled))
			{
				thisLine = malloc(64);
				sprintf(thisLine, "subi %%r%d, %%bp, $%d", declaredLifetime->stackOrRegLocation, declaredLifetime->localPointerTo->stackOffset * -1);
				ASM_append(asmBlock, thisLine);
			}
		}
		break;

		case tt_add:
		case tt_subtract:
		case tt_mul:
		case tt_div:
		{
			char immediateInstruction = 0;
			thisLine = malloc(64);

			struct Lifetime *assignedLifetime = LinkedList_Find(allLifetimes, compareLifetimes, thisTAC->operands[0]);
			struct Lifetime *operand1Lifetime = LinkedList_Find(allLifetimes, compareLifetimes, thisTAC->operands[1]);
			struct Lifetime *operand2Lifetime = LinkedList_Find(allLifetimes, compareLifetimes, thisTAC->operands[2]);
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
					placeOrFindOperandInRegister(allLifetimes, operand1Lifetime->variable, asmBlock, reservedRegisters[0], touchedRegisters);
					sprintf(op1, "%%r%d", reservedRegisters[0]);
				}
				break;

			case vp_literal:
				if (thisTAC->reorderable)
				{
					reordering = 1;
					immediateInstruction = 1;
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
					placeOrFindOperandInRegister(allLifetimes, operand2Lifetime->variable, asmBlock, reservedRegisters[1], touchedRegisters);
					sprintf(op2, "%%r%d", reservedRegisters[1]);
				}
				break;

			case vp_literal:
				sprintf(op2, "$%s", thisTAC->operands[2]);
				immediateInstruction = 1;
				break;
			}

			if (reordering)
			{
				if (immediateInstruction)
				{
					sprintf(thisLine, "%si %%r%d, %s, %s", getAsmOp(thisTAC->operation), destinationRegister, op2, op1);
				}
				else
				{
					sprintf(thisLine, "%s %%r%d, %s, %s", getAsmOp(thisTAC->operation), destinationRegister, op2, op1);
				}
			}
			else
			{
				if (immediateInstruction)
				{
					sprintf(thisLine, "%si %%r%d, %s, %s", getAsmOp(thisTAC->operation), destinationRegister, op1, op2);
				}
				else
				{
					sprintf(thisLine, "%s %%r%d, %s, %s", getAsmOp(thisTAC->operation), destinationRegister, op1, op2);
				}
			}
			ASM_append(asmBlock, thisLine);

			if (assignedLifetime->isSpilled)
			{
				thisLine = malloc(64);
				sprintf(thisLine, "mov (%%bp+%d), %%r%d;replace %s", assignedLifetime->stackOrRegLocation, destinationRegister, assignedLifetime->variable);
				ASM_append(asmBlock, thisLine);
			}
		}
		break;

		case tt_reference:
			ErrorAndExit(ERROR_INTERNAL, "Code generation not implemented for this operation (%s) yet!\n", getAsmOp(thisTAC->operation));
			break;

		case tt_memw_1:
		{
			thisLine = malloc(64);
			int dstIndexReg = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[0], asmBlock, reservedRegisters[0], touchedRegisters);
			int sourceReg = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[1], asmBlock, reservedRegisters[1], touchedRegisters);
			sprintf(thisLine, "mov (%%r%d), %%r%d", dstIndexReg, sourceReg);
			ASM_append(asmBlock, thisLine);
		}
		break;

		case tt_memw_2:
		{
			thisLine = malloc(64);
			int baseReg = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[0], asmBlock, reservedRegisters[0], touchedRegisters);
			int sourceReg = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[2], asmBlock, reservedRegisters[1], touchedRegisters);
			sprintf(thisLine, "mov (%%r%d+%d), %%r%d", baseReg, (int)(long int)thisTAC->operands[1], sourceReg);
			ASM_append(asmBlock, thisLine);
		}
		// ErrorAndExit(ERROR_INTERNAL, "Code generation not implemented for this operation (%s) yet!\n", getAsmOp(thisTAC->operation));
		break;

		case tt_memw_3:
		{
			thisLine = malloc(64);
			int baseReg = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[0], asmBlock, reservedRegisters[0], touchedRegisters);
			int offsetReg = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[1], asmBlock, reservedRegisters[1], touchedRegisters);
			int sourceReg = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[3], asmBlock, 16, touchedRegisters);
			sprintf(thisLine, "mov (%%r%d+%%r%d,%d), %%r%d", baseReg, offsetReg, ALIGNSIZE((int)(long int)thisTAC->operands[2]), sourceReg);
			ASM_append(asmBlock, thisLine);
		}
		break;

		case tt_dereference: // these are redundant... probably makes sense to remove one?
		case tt_memr_1:
		{
			thisLine = malloc(64);
			struct Lifetime *destinationLifetime = LinkedList_Find(allLifetimes, compareLifetimes, thisTAC->operands[0]);
			if (destinationLifetime->isSpilled)
			{
				ErrorAndExit(ERROR_INTERNAL, "Code generation for tt_dereference/tt_memr_w with spilled destination not supported!\n");
			}
			else
			{
				int destReg = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[0], asmBlock, reservedRegisters[0], touchedRegisters);
				int sourceReg = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[1], asmBlock, reservedRegisters[1], touchedRegisters);
				sprintf(thisLine, "mov %%r%d, (%%r%d)", destReg, sourceReg);
			}
			ASM_append(asmBlock, thisLine);
		}
		break;
			break;

		case tt_memr_2:
		{
			thisLine = malloc(64);
			struct Lifetime *destinationLifetime = LinkedList_Find(allLifetimes, compareLifetimes, thisTAC->operands[0]);
			if (destinationLifetime->isSpilled)
			{
				ErrorAndExit(ERROR_INTERNAL, "Code generation for tt_memr_2 with spilled destination not supported!\n");
			}
			else
			{
				int destReg = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[0], asmBlock, reservedRegisters[0], touchedRegisters);
				int sourceReg = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[1], asmBlock, reservedRegisters[1], touchedRegisters);
				sprintf(thisLine, "mov %%r%d, (%%r%d+%d)", destReg, sourceReg, (int)(long int)thisTAC->operands[2]);
			}
			ASM_append(asmBlock, thisLine);
		}
		break;

		case tt_memr_3:
		{
			thisLine = malloc(64);
			struct Lifetime *destinationLifetime = LinkedList_Find(allLifetimes, compareLifetimes, thisTAC->operands[0]);
			if (destinationLifetime->isSpilled)
			{
				ErrorAndExit(ERROR_INTERNAL, "Code generation for tt_memr_3 with spilled destination not supported!\n");
			}
			else
			{
				int destReg = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[0], asmBlock, reservedRegisters[0], touchedRegisters);
				int baseReg = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[1], asmBlock, reservedRegisters[1], touchedRegisters);
				int offsetReg = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[2], asmBlock, 16, touchedRegisters);
				sprintf(thisLine, "mov %%r%d, (%%r%d+%%r%d,%d)", destReg, baseReg, offsetReg, ALIGNSIZE((int)(long int)thisTAC->operands[3]));
			}
			ASM_append(asmBlock, thisLine);
		}
		break;

		case tt_cmp:
		{
			char immediateInstruction = 0;
			thisLine = malloc(64);

			struct Lifetime *operand1Lifetime = LinkedList_Find(allLifetimes, compareLifetimes, thisTAC->operands[1]);
			struct Lifetime *operand2Lifetime = LinkedList_Find(allLifetimes, compareLifetimes, thisTAC->operands[2]);
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
					placeOrFindOperandInRegister(allLifetimes, operand1Lifetime->variable, asmBlock, reservedRegisters[0], touchedRegisters);
					sprintf(op1, "%%r%d", reservedRegisters[0]);
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
					placeOrFindOperandInRegister(allLifetimes, operand2Lifetime->variable, asmBlock, reservedRegisters[1], touchedRegisters);
					sprintf(op2, "%%r%d", reservedRegisters[1]);
				}
				break;

			case vp_literal:
				// TODO:
				// range check immediate values
				// if they don't fit fully, load into register via 2 instructions

				immediateInstruction = 1;
				sprintf(op2, "$%s", thisTAC->operands[2]);
				break;
			}

			if (immediateInstruction)
			{
				sprintf(thisLine, "%si %s, %s", getAsmOp(thisTAC->operation), op1, op2);
			}
			else
			{
				sprintf(thisLine, "%s %s, %s", getAsmOp(thisTAC->operation), op1, op2);
			}
			ASM_append(asmBlock, thisLine);
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
			ASM_append(asmBlock, thisLine);
		}
		break;

		case tt_jmp:
		{
			thisLine = malloc(64);
			sprintf(thisLine, "jmp %s_%d", functionName, (int)(long int)thisTAC->operands[0]);
			ASM_append(asmBlock, thisLine);
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
				int registerIndex = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[0], asmBlock, reservedRegisters[0], touchedRegisters);
				sprintf(thisLine, "push %%r%d", registerIndex);
			}
			break;

			case vp_literal:
				sprintf(thisLine, "pushi $%s", thisTAC->operands[0]);
				break;
			}
			ASM_append(asmBlock, thisLine);
		}
		break;

		case tt_call:
		{
			thisLine = malloc(64);
			sprintf(thisLine, "call %s", thisTAC->operands[1]);
			ASM_append(asmBlock, thisLine);
			thisLine = malloc(64);

			// the call returns a value
			if (thisTAC->operands[0] != NULL)
			{
				struct Lifetime *returnedLifetime = LinkedList_Find(allLifetimes, compareLifetimes, thisTAC->operands[0]);
				if (!returnedLifetime->isSpilled)
				{
					sprintf(thisLine, "mov %%r%d, %%rr", returnedLifetime->stackOrRegLocation);
				}
				else
				{
					sprintf(thisLine, "mov (%%bp+%d), %%rr", returnedLifetime->stackOrRegLocation);
				}
				ASM_append(asmBlock, thisLine);
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
				// PlaceLiteralInRegister(asmBlock, thisTAC->operands[0], "%%rr");
				// free(thisLine);
				sprintf(thisLine, "movh %%rr, $%s", thisTAC->operands[0]);
				ASM_append(asmBlock, thisLine);
			}
			break;

			case vp_standard:
			case vp_temp:
			{
				int destReg = placeOrFindOperandInRegister(allLifetimes, thisTAC->operands[0], asmBlock, reservedRegisters[0], touchedRegisters);
				sprintf(thisLine, "mov %%rr, %%r%d", destReg);
				ASM_append(asmBlock, thisLine);
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
