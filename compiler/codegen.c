#include "codegen.h"

struct ASMblock *generateCode(struct symbolTable *table, FILE *outFile)
{
	struct ASMblock *outputBlock = newASMblock();
	struct LinkedList *lifetimes = findLifetimes(table);
	// convert the linked list of lifetimes to an array for easy indexing
	struct Lifetime **lifetimeArray = malloc(lifetimes->size * sizeof(struct Lifetime *));
	int lifetimeCount = 0;
	for (struct LinkedListNode *runner = lifetimes->head; runner != NULL; runner = runner->next)
		lifetimeArray[lifetimeCount++] = runner->data;

	// print out the variable lifetimes as a horizontal graph
	// printf("\n");
	// printLifetimesGraph(lifetimes);
	printf("Generate code for function %s\n", table->name);
	printLifetimesGraph(lifetimes);
	printf("\n");

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
		wip->index = (REGISTER_COUNT) - i;
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

	struct LinkedListNode *blockRunner = table->BasicBlockList->head;
	while (blockRunner != NULL)
	{
		struct BasicBlock *thisBlock = blockRunner->data;
		trimmedStr = strTrim(printBuf, sprintf(printBuf, "%s_%d:", table->name, thisBlock->labelNum));
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
			if (this->variable[0] != '.' && symbolTableLookup(table, this->variable)->type == e_argument)
			{
				struct variableEntry *theArgument = symbolTableLookup(table, this->variable)->entry;
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
			printTACLine(currentTAC);
			printf("\n");

			// increment last used values of all active registers or reset if variable used in this step
			for (int i = 0; i < activeList->size; i++)
			{
				struct Register *thisReg = activeList->data[i];
				thisReg->lastUsed++;
				for (int i = 0; i < 3; i++)
				{
					switch (currentTAC->operandTypes[i])
					{
					case vt_var:
					case vt_temp:
						if (!strcmp(thisReg->lifetime->variable, currentTAC->operands[i]))
							thisReg->lastUsed = 0;

						break;

					default:
						break;
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
						if (this->variable[0] != '.' && symbolTableLookup(table, this->variable)->type == e_argument)
						{
							struct variableEntry *theArgument = symbolTableLookup(table, this->variable)->entry;
							trimmedStr = strTrim(printBuf, sprintf(printBuf, "mov %%r%d, %d(%%bp)", destinationIndex, theArgument->stackOffset));
							// trimmedStr = strTrim(printBuf, sprintf(printBuf, "%s ;place argument %s", outputLine, this->variable);
							ASMblock_append(outputBlock, trimmedStr);
						}
					}
				}

			default:
				break;
			}

			/*
			 * double check spill space because some functions in regalloc may spill variables on their own
			 * TODO: consider just making this the only check, any reason it wouldn't work?
			 * 
			*/
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
				if (currentTAC->operandTypes[1] == vt_literal)
				{
					trimmedStr = strTrim(printBuf, sprintf(printBuf, "mov %%r%d, $%s", destinationRegister, currentTAC->operands[1]));
				}
				else
				{
					firstSourceRegister = findActiveVariable(activeList, currentTAC->operands[1]);
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

				int source1Register = findOrPlaceOperand(activeList, inactiveList, spilledList, currentTAC->operands[1], outputBlock, table);
				int source2Register = findOrPlaceOperand(activeList, inactiveList, spilledList, currentTAC->operands[2], outputBlock, table);
				destinationRegister = findOrPlaceAssignedVariable(activeList, inactiveList, spilledList, currentTAC->operands[0], outputBlock, table);
				
				trimmedStr = strTrim(printBuf, sprintf(printBuf, "%s %%r%d, %%r%d, %%r%d", getAsmOp(currentTAC->operation), destinationRegister, source1Register, source2Register));

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

				printTACLine(currentTAC);
				printf("\n");
				int dest = findOrPlaceAssignedVariable(activeList, inactiveList, spilledList, currentTAC->operands[0], outputBlock, table);
				int baseIndex = findOrPlaceAssignedVariable(activeList, inactiveList, spilledList, currentTAC->operands[1], outputBlock, table);
				int offsetIndex = findOrPlaceAssignedVariable(activeList, inactiveList, spilledList, currentTAC->operands[2], outputBlock, table);
				int scale = (int)(long int)currentTAC->operands[3];
				trimmedStr = strTrim(printBuf, sprintf(printBuf, "mov %%r%d, %%r%d(%%r%d, %d)", dest, offsetIndex, baseIndex, scale));
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
				switch (currentTAC->operandTypes[0])
				{
				case vt_literal:
					trimmedStr = strTrim(printBuf, sprintf(printBuf, "push $%s", currentTAC->operands[0]));
					break;

				case vt_var:
				case vt_temp:
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
						if (currentTAC->operandTypes[2] == vt_literal)
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

						if (currentTAC->operandTypes[2] == vt_literal)
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
			}
			break;

			case tt_pushstate:
			{
				// deep copy the states and push them to the state stack
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
					free(Stack_pop(poppedState->activeList));
				}
				Stack_free(poppedState->activeList);

				while (poppedState->inactiveList->size > 0)
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
				switch (currentTAC->operandTypes[0])
				{
				case vt_literal:
				{
					trimmedStr = strTrim(printBuf, sprintf(printBuf, "mov %%rr, $%s", currentTAC->operands[0]));
				}
				break;

				case vt_var:
				case vt_temp:
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
				}
				break;

				default:
					perror("unexpected type in return TAC!\n");
					exit(2);
				}
				ASMblock_append(outputBlock, trimmedStr);

				break;
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
