#include "regalloc.h"

struct Lifetime *newLifetime(char *variable, enum variableTypes type, int start)
{
	struct Lifetime *wip = malloc(sizeof(struct Lifetime));
	wip->variable = variable;
	wip->start = start;
	wip->end = start;
	wip->stackOrRegLocation = 0;
	wip->type = type;
	wip->nwrites = 0;
	wip->nreads = 0;
	wip->isSpilled = 0;
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
	updateOrInsertLifetime(ltList, variable, type, newEnd)->nwrites++;
}

// wrapper function for updateOrInsertLifetime
//  increments read count for the given variable
void recordVariableRead(struct LinkedList *ltList,
						char *variable,
						enum variableTypes type,
						int newEnd)
{
	updateOrInsertLifetime(ltList, variable, type, newEnd)->nreads++;
}

int placeOperandInRegister(struct LinkedList *lifetimes, char *variable, struct ASMblock *currentBlock)
{
	struct Lifetime *relevantLifetime = LinkedList_find(lifetimes, compareLifetimes, variable);
	if(relevantLifetime == NULL)
	{
		ErrorAndExit(ERROR_INTERNAL, "Unable to find lifetime for variable %s!\n", variable);
	}
	if(relevantLifetime->isSpilled)
	{
		char *copyLine = malloc(32);
		sprintf(copyLine, "mov %%r0, %d(%%bp)", relevantLifetime->stackOrRegLocation);
		ASMblock_append(currentBlock, copyLine);
		printf("copy variable %s from stack to scratch register\n", variable);
		return 0;
	}
	else
	{
		return relevantLifetime->stackOrRegLocation;
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
							if (examinedLifetime->end < extendTo)
								examinedLifetime->end = extendTo + 1;
							// if (examinedLifetime->start > extendFrom)
							// examinedLifetime->start = extendFrom;
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
					updateOrInsertLifetime(lifetimes, thisLine->operands[0], thisLine->operandTypes[0], TACIndex);
				}
				break;

			case tt_assign:
				updateOrInsertLifetime(lifetimes, thisLine->operands[0], thisLine->operandTypes[0], TACIndex);
				if (thisLine->operandPermutations[1] != vp_literal)
				{
					updateOrInsertLifetime(lifetimes, thisLine->operands[1], thisLine->operandTypes[1], TACIndex);
				}
				break;

			// single operand in slot 0
			case tt_push:
			case tt_return:
				if (thisLine->operandPermutations[0] != vp_literal)
				{
					switch (thisLine->operandTypes[0])
					{
					case vt_var:
						updateOrInsertLifetime(lifetimes, thisLine->operands[0], thisLine->operandTypes[0], TACIndex);
						break;

					default:
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
				for (int i = 0; i < 4; i++)
				{
					// lifetimes for every permutation except literal
					if (thisLine->operandPermutations[i] != vp_literal)
					{
						// and any type except null
						switch (thisLine->operandTypes[i])
						{
						case vt_var:
							updateOrInsertLifetime(lifetimes, thisLine->operands[i], thisLine->operandTypes[i], TACIndex);
							break;
						default:
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

