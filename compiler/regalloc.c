#include "regalloc.h"

struct Lifetime *newLifetime(char *variable, enum variableTypes type, int start)
{
	struct Lifetime *wip = malloc(sizeof(struct Lifetime));
	wip->variable = variable;
	wip->start = start;
	wip->end = start;
	wip->stackOrRegLocation = -1;
	wip->type = type;
	wip->nwrites = 0;
	wip->nreads = 0;
	wip->isSpilled = 0;
	wip->localPointerTo = NULL;
	return wip;
}

char compareLifetimes(struct Lifetime *a, char *variable)
{
	return strcmp(a->variable, variable);
}

// search through the list of existing lifetimes
// update the lifetime if it exists, insert if it doesn't
// returns pointer to the lifetime corresponding to the passed variable name
struct Lifetime *updateOrInsertLifetime(struct LinkedList *ltList,
										char *variable,
										enum variableTypes type,
										int newEnd)
{
	struct Lifetime *thisLt = LinkedList_find(ltList, &compareLifetimes, variable);

	if (thisLt != NULL)
	{
		// this should never fire with well-formed TAC
		// may be helpful when adding/troubleshooting new TAC generation
		if (thisLt->type != type)
		{
			ErrorAndExit(ERROR_CODE, "Error - type mismatch between identically named variables [%s] expected %d, saw %d!\n", variable, thisLt->type, type);
		}
		if (newEnd > thisLt->end)
			thisLt->end = newEnd;
	}
	else
	{
		thisLt = newLifetime(variable, type, newEnd);
		LinkedList_append(ltList, thisLt);
	}

	return thisLt;
}

// wrapper function for updateOrInsertLifetime
//  increments write count for the given variable
void recordVariableWrite(struct LinkedList *ltList,
						 char *variable,
						 enum variableTypes type,
						 int newEnd)
{
	struct Lifetime *updatedLifetime = updateOrInsertLifetime(ltList, variable, type, newEnd);
	updatedLifetime->nwrites = updatedLifetime->nwrites + 1;
}

// wrapper function for updateOrInsertLifetime
//  increments read count for the given variable
void recordVariableRead(struct LinkedList *ltList,
						char *variable,
						enum variableTypes type,
						int newEnd)
{
	struct Lifetime *updatedLifetime = updateOrInsertLifetime(ltList, variable, type, newEnd);
	updatedLifetime->nwrites = updatedLifetime->nreads + 1;
}

// places an operand by name into the specified register, or returns the register containing if it's already in a register
// does *NOT* guarantee that returned register indices are modifiable in the case where the variable is found in a register
int placeOrFindOperandInRegister(struct LinkedList *lifetimes, char *variable, struct ASMblock *currentBlock, int registerIndex, char *touchedRegisters)
{
	struct Lifetime *relevantLifetime = LinkedList_find(lifetimes, compareLifetimes, variable);
	if (relevantLifetime == NULL)
	{
		ErrorAndExit(ERROR_INTERNAL, "Unable to find lifetime for variable %s!\n", variable);
	}

	// if not a local pointer, the value for this variable *must* exist either in a register or spilled on the stack
	if (relevantLifetime->localPointerTo == NULL)
	{
		if (relevantLifetime->isSpilled)
		{
			char *copyLine = malloc(32);
			sprintf(copyLine, "mov %%r%d, %d(%%bp)", registerIndex, relevantLifetime->stackOrRegLocation);
			ASMblock_append(currentBlock, copyLine);
			touchedRegisters[registerIndex] = 1;
			return registerIndex;
		}
		else
		{
			return relevantLifetime->stackOrRegLocation;
		}
	}
	else
	{
		// if this local pointer doesn't live in a register, we will need to construct it on demant
		if (relevantLifetime->stackOrRegLocation == -1)
		{
			char *constructLocalPointerLine = malloc(64);
			int basepointerOffset = relevantLifetime->localPointerTo->stackOffset;
			if (basepointerOffset > 0)
			{
				sprintf(constructLocalPointerLine, "add %%r%d, %%bp, $%d", registerIndex, basepointerOffset);
			}
			else if (basepointerOffset < 0)
			{
				sprintf(constructLocalPointerLine, "sub %%r%d, %%bp, $%d", registerIndex, -1 * basepointerOffset);
			}
			else
			{
				sprintf(constructLocalPointerLine, "mov %%r%d, %%bp", registerIndex);
			}
			ASMblock_append(currentBlock, constructLocalPointerLine);
			touchedRegisters[registerIndex] = 1;
			return registerIndex;
		}
		// if it does get a register, all we need to do is return it
		else
		{
			return relevantLifetime->stackOrRegLocation;
		}
	}
}

struct LinkedList *findLifetimes(struct FunctionEntry *function)
{
	struct LinkedList *lifetimes = LinkedList_new();
	for (int i = 0; i < function->mainScope->entries->size; i++)
	{
		struct ScopeMember *thisMember = function->mainScope->entries->data[i];
		if (thisMember->type == e_argument)
		{
			// struct variableEntry *theArgument = table->entries[i]->entry;

			updateOrInsertLifetime(lifetimes, thisMember->name, ((struct VariableEntry *)thisMember->entry)->type, 0);
		}
	}

	struct LinkedListNode *blockRunner = function->BasicBlockList->head;
	struct Stack *doDepth = Stack_new();
	while (blockRunner != NULL)
	{
		struct BasicBlock *thisBlock = blockRunner->data;
		struct LinkedListNode *TACRunner = thisBlock->TACList->head;
		while (TACRunner != NULL)
		{
			struct TACLine *thisLine = TACRunner->data;
			int TACIndex = thisLine->index;

			switch (thisLine->operation)
			{
			case tt_do:
				Stack_push(doDepth, (void *)(long int)thisLine->index);
				break;

			case tt_enddo:
			{
				int extendTo = thisLine->index;
				int extendFrom = (int)(long int)Stack_pop(doDepth);
				for (struct LinkedListNode *lifetimeRunner = lifetimes->head; lifetimeRunner != NULL; lifetimeRunner = lifetimeRunner->next)
				{
					struct Lifetime *examinedLifetime = lifetimeRunner->data;
					if (examinedLifetime->end >= extendFrom && examinedLifetime->end < extendTo)
					{
						if (examinedLifetime->variable[0] != '.')
						{
							examinedLifetime->end = extendTo + 1;
						}
					}
				}
			}
			break;

			case tt_asm:
				break;

			case tt_declare:
				updateOrInsertLifetime(lifetimes, thisLine->operands[0], thisLine->operandTypes[0], TACIndex);
				break;

			case tt_call:
				if (thisLine->operandTypes[0] != vt_null)
				{
					recordVariableWrite(lifetimes, thisLine->operands[0], thisLine->operandTypes[0], TACIndex);
				}
				break;

			case tt_assign:
			{
				recordVariableWrite(lifetimes, thisLine->operands[0], thisLine->operandTypes[0], TACIndex);
				if (thisLine->operandPermutations[1] != vp_literal)
				{
					recordVariableRead(lifetimes, thisLine->operands[1], thisLine->operandTypes[1], TACIndex);
				}
			}
			break;

			// single operand in slot 0
			case tt_push:
			case tt_return:
			{
				if (thisLine->operandPermutations[0] != vp_literal)
				{
					switch (thisLine->operandTypes[0])
					{
					case vt_var:
						recordVariableRead(lifetimes, thisLine->operands[0], thisLine->operandTypes[0], TACIndex);
						break;

					default:
					}
				}
			}
			break;

			case tt_add:
			case tt_subtract:
			case tt_mul:
			case tt_div:
			case tt_cmp:
			case tt_memr_1:
			case tt_memr_2:
			case tt_memr_3:
			case tt_memw_1:
			case tt_memw_2:
			case tt_memw_3:
			{
				if (thisLine->operandTypes[0] != vt_null)
				{
					recordVariableWrite(lifetimes, thisLine->operands[0], thisLine->operandTypes[0], TACIndex);
				}

				for (int i = 1; i < 4; i++)
				{
					// lifetimes for every permutation except literal
					if (thisLine->operandPermutations[i] != vp_literal)
					{
						// and any type except null
						switch (thisLine->operandTypes[i])
						{
						case vt_var:
							recordVariableRead(lifetimes, thisLine->operands[i], thisLine->operandTypes[i], TACIndex);
							break;
						default:
						}
					}
				}
			}
			break;

			default:
				break;
			}
			TACRunner = TACRunner->next;
		}
		blockRunner = blockRunner->next;
	}

	Stack_free(doDepth);

	return lifetimes;
}
