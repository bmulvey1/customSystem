#include "regalloc.h"

struct Lifetime *newLifetime(char *variable, enum variableTypes type, int start)
{
    struct Lifetime *wip = malloc(sizeof(struct Lifetime));
    wip->variable = variable;
    wip->start = start;
    wip->end = start;
    wip->type = type;
    return wip;
}

char compareLifetimes(struct Lifetime *a, char *variable)
{
    return strcmp(a->variable, variable);
}

// search through the list of existing lifetimes
// update the lifetime if it exists, insert if it doesn't
void updateOrInsertLifetime(struct LinkedList *ltList,
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
            printf("Error - type mismatch between identically named variables [%s] expected %d, saw %d!\n", variable, thisLt->type, type);
            exit(1);
        }

        thisLt->end = newEnd;
    }
    else
    {
        thisLt = newLifetime(variable, type, newEnd);
        LinkedList_append(ltList, thisLt);
    }
}

void printCurrentState(struct Stack *activeList,
                       struct Stack *inactiveList,
                       struct Stack *spilledList)
{

    printf("\nFree registers:");
    for (int i = 0; i < inactiveList->size; i++)
        printf("%%r%d, ", ((struct Register *)inactiveList->data[i])->index);

    printf("\n");

    printf("Allocated registers:");
    for (int i = 0; i < activeList->size; i++)
    {
        printf("%%r%d: [%s], ", ((struct Register *)activeList->data[i])->index, ((struct Register *)activeList->data[i])->lifetime->variable);
    }
    printf("\n");

    printf("Spilled variables:");
    for (int i = 0; i < spilledList->size; i++)
    {
        struct SpilledRegister *sr = spilledList->data[i];
        if (sr->occupied)
            printf("[%s], ", sr->lifetime->variable);
        else
            printf("[ ],");
    }
    printf("\n\n");
}

struct Register *duplicateRegister(struct Register *r)
{
    struct Register *wip = malloc(sizeof(struct Register));
    memcpy(wip, r, sizeof(struct Register));
    return wip;
}

// sort a list of registers by the start points of the lifetimes contained within
void sortByStartPoint(struct Register **list, int size)
{
    for (int i = 0; i < size; i++)
    {
        for (int j = 0; j < size - i - 1; j++)
        {
            if (list[j]->lifetime->start > list[j + 1]->lifetime->start)
            {
                struct Register *temp = list[j];
                list[j] = list[j + 1];
                list[j + 1] = temp;
            }
        }
    }
}

// sort a list of registers by the end points of the lifetimes contained within
void sortByEndPoint(struct Register **list, int size)
{
    for (int i = 0; i < size; i++)
    {
        for (int j = 0; j < size - i - 1; j++)
        {
            if (list[j]->lifetime->end > list[j + 1]->lifetime->end)
            {
                struct Register *temp = list[j];
                list[j] = list[j + 1];
                list[j + 1] = temp;
            }
        }
    }
}

// sort so the lowest register indices are on top of the stack
void sortByRegisterNumber(struct Register **list, int size)
{
    for (int i = 0; i < size; i++)
    {
        for (int j = 0; j < size - i - 1; j++)
        {
            if (list[j]->index < list[j + 1]->index)
            {
                struct Register *temp = list[j];
                list[j] = list[j + 1];
                list[j + 1] = temp;
            }
        }
    }
}

// search by name for a variable in the active list - return the register if found or NULL if not
struct Register *findAndRemoveLiveVariable(struct Stack *activeList, char *name)
{
    struct Stack *intermediate = Stack_new();
    struct Register *found = NULL;
    while (activeList->size > 0)
    {
        struct Register *examined = Stack_pop(activeList);
        if (!strcmp(examined->lifetime->variable, name))
        {
            found = examined;
            break;
        }
        else
        {
            Stack_push(intermediate, examined);
        }
    }

    while (intermediate->size > 0)
    {
        Stack_push(activeList, Stack_pop(intermediate));
    }
    Stack_free(intermediate);
    return found;
}

// search by name for a variable in the spilled list - return the register if found or NULL if not
struct SpilledRegister *findAndRemoveSpilledVariable(struct Stack *spilledList, char *name)
{
    struct Stack *intermediate = Stack_new();
    struct SpilledRegister *found = NULL;
    while (spilledList->size > 0)
    {
        struct SpilledRegister *examined = Stack_pop(spilledList);
        if (!strcmp(examined->lifetime->variable, name))
        {
            found = examined;
            break;
        }
        else
        {
            Stack_push(intermediate, examined);
        }
    }

    while (intermediate->size > 0)
    {
        Stack_push(spilledList, Stack_pop(intermediate));
    }
    Stack_free(intermediate);
    return found;
}

struct SpilledRegister *duplicateSpilledRegister(struct Register *r)
{
    struct SpilledRegister *wip = malloc(sizeof(struct SpilledRegister));
    memcpy(wip, r, sizeof(struct SpilledRegister));
    return wip;
}

void expireOldIntervals(struct Stack *activeList,
                        struct Stack *inactiveList,
                        struct Stack *spilledList,
                        int TACIndex)
{

    struct Stack *intermediateStack = Stack_new();
    while (activeList->size > 0)
    {
        // pop all active retisters
        struct Register *poppedRegister = Stack_pop(activeList);
        if (poppedRegister->lifetime->end < TACIndex)
        {
            // if the variable this register contains expires, add register to inactive list
            // printf("Expire %s\n", poppedRegister->lifetime->variable);
            Stack_push(inactiveList, poppedRegister);
        }
        else
        {
            // otherwise put on intermediate stack
            Stack_push(intermediateStack, poppedRegister);
        }
    }

    // return all surviving lifetimes to the active list
    while (intermediateStack->size > 0)
        Stack_push(activeList, Stack_pop(intermediateStack));

    Stack_free(intermediateStack);

    // iterate over the list of spilled registers
    for (int i = 0; i < spilledList->size; i++)
    {
        struct SpilledRegister *checkedSpill = spilledList->data[i];
        // if the spill slot is occupied and the variable in that slot is expired
        if (checkedSpill->occupied && checkedSpill->lifetime->end < TACIndex)
        {
            printf("Expire %s (spilled on stack)\n", checkedSpill->lifetime->variable);
            // free the slot
            checkedSpill->occupied = 0;
        }
    }
    // if possible (slots on top are unoccupied/expired), reduce the size of the spill stack
    while (spilledList->size > 0 && ((struct SpilledRegister *)Stack_peek(spilledList))->occupied == 0)
    {
        free(Stack_pop(spilledList));
    }
}

void spillRegister(struct Stack *activeList,
                   struct Stack *inactiveList,
                   struct Stack *spilledList,
                   struct ASMblock *outputBlock,
                   struct symbolTable *table)
{
    // invert the order of the active list by pushing onto an intermediate stack
    struct Stack *intermediate = Stack_new();
    while (activeList->size > 0)
        Stack_push(intermediate, Stack_pop(activeList));

    // examine inverted active list (variables with latest expiration will appear first)
    struct Register *victim = Stack_pop(intermediate);
    // skip variables which will be used in the current TAC
    while (victim->lastUsed == 0 && intermediate->size > 0)
    {
        Stack_push(activeList, victim);
        victim = Stack_pop(intermediate);
    }

    // return the rest of the intermediate stack back to the active list
    while (intermediate->size > 0)
        Stack_push(activeList, Stack_pop(intermediate));

    Stack_free(intermediate);

    char *outputLine = malloc(20);
    // printf("spill variable %s to stack (offset of %d)\n", victim->lifetime->variable, thisSpill->stackOffset);

    // examine the existing slots on the stack
    char needNewSlot = 1;
    for (int i = 0; i < spilledList->size; i++)
    {
        struct SpilledRegister *checkedSlot = spilledList->data[i];
        // if there is an unoccupied slot, use it
        if (checkedSlot->occupied == 0)
        {
            needNewSlot = 0;
            checkedSlot->lifetime = victim->lifetime;
            checkedSlot->lastUsed = victim->lastUsed;
            checkedSlot->occupied = 1;
            sprintf(outputLine, "mov %d(%%bp), %%r%d", checkedSlot->stackOffset, victim->index);
            break;
        }
    }

    // no unoccupied slot was found, push a new spill to the stack
    if (needNewSlot)
    {
        struct SpilledRegister *newSpill = malloc(sizeof(struct SpilledRegister));
        newSpill->lifetime = victim->lifetime;
        newSpill->lastUsed = victim->lastUsed;
        newSpill->stackOffset = (spilledList->size * -2) - 2;
        newSpill->occupied = 1;
        Stack_push(spilledList, newSpill);
        sprintf(outputLine, "mov %d(%%bp), %%r%d", newSpill->stackOffset, victim->index);
    }

    char *finalOutputLine = malloc(40);
    sprintf(finalOutputLine, "%s;spill %12s", outputLine, victim->lifetime->variable);
    free(outputLine);
    ASMblock_append(outputBlock, finalOutputLine);

    // return the newly-spilled register to the inactive list
    Stack_push(inactiveList, victim);
}

// given that there is at least one register free in the inactivelist, assign the given lifetime to it and place it in the active list
int assignRegister(struct Stack *activeList,
                   struct Stack *inactiveList,
                   struct Lifetime *assignedLifetime)
{
    struct Register *assignedRegister = Stack_pop(inactiveList);
    // printf("assign register for [%s]\n", assignedLifetime->variable);
    assignedRegister->lifetime = assignedLifetime;
    assignedRegister->lastUsed = 0;

    Stack_push(activeList, assignedRegister);
    if (inactiveList->size > 0)
        sortByEndPoint((struct Register **)activeList->data, activeList->size);

    sortByRegisterNumber((struct Register **)inactiveList->data, inactiveList->size);

    return assignedRegister->index;
}

// unspill variable from stack to register, returning the register index the variable now lives in
int unSpillVariable(struct Stack *activeList,
                    struct Stack *inactiveList,
                    struct Stack *spilledList,
                    char *varName,
                    struct ASMblock *outputBlock,
                    struct symbolTable *table)
{
    if (inactiveList->size == 0)
    {
        spillRegister(activeList, inactiveList, spilledList, outputBlock, table);
    }
    for (int i = 0; i < spilledList->size; i++)
    {
        struct SpilledRegister *examinedSpill = spilledList->data[i];
        if (!strcmp(examinedSpill->lifetime->variable, varName))
        {
            int destinationRegister = assignRegister(activeList, inactiveList, examinedSpill->lifetime);
            char *outputStr = malloc(20);
            sprintf(outputStr, "mov %%r%d, %d(%%bp)", destinationRegister, examinedSpill->stackOffset);
            ASMblock_append(outputBlock, outputStr);
            // printf("unspill variable %s from stack offset %d to %%r%d\n\n", varName, examinedSpill->stackOffset, destinationRegister);
            examinedSpill->occupied = 0;
            return i;
        }
    }

    printf("Error - unable to unspill variable [%s] - not on stack!\n", varName);
    exit(1);
}

// find which register a variable lives in, returning -1 if not currently active in a register
int findActiveVariable(struct Stack *activeList, char *varName)
{
    for (int i = 0; i < activeList->size; i++)
    {
        if (!strcmp(((struct Register *)activeList->data[i])->lifetime->variable, varName))
            return ((struct Register *)activeList->data[i])->index;
    }

    return -1;
}

// return the stack offset of a variable which has been spilled to stack
int findSpilledVariable(struct Stack *spilledLilst, char *varName)
{
    for (int i = 0; i < spilledLilst->size; i++)
    {
        struct SpilledRegister *examinedSpill = spilledLilst->data[i];
        if (examinedSpill->occupied && !strcmp(examinedSpill->lifetime->variable, varName))
        {
            return examinedSpill->stackOffset;
        }
    }

    printf("Error - unable to find inactive variable [%s] - not on stack!\n", varName);
    exit(1);
}

void printLifetimesGraph(struct LinkedList *lifetimeList)
{
    for (struct LinkedListNode *runner = lifetimeList->head; runner != NULL; runner = runner->next)
    {
        struct Lifetime *thisLifetime = runner->data;
        printf("%8s:", thisLifetime->variable);
        int i = 0;
        for (; i < thisLifetime->start; i++)
        {
            printf(" ");
        }
        printf("#");
        i++;
        for (; i <= thisLifetime->end; i++)
        {
            printf("-");
        }
        printf("\n");
    }
}

/*
 * state duplication/reading
 *
 *
 */
struct SavedState *duplicateCurrentState(struct Stack *activeList,
                                         struct Stack *inactiveList,
                                         struct Stack *spilledList,
                                         int currentLifetimeIndex)
{
    struct SavedState *wip = malloc(sizeof(struct SavedState));
    struct Stack *duplicatedStack = Stack_new();
    for (int i = 0; i < activeList->size; i++)
        Stack_push(duplicatedStack, duplicateRegister(activeList->data[i]));
    wip->activeList = duplicatedStack;

    duplicatedStack = Stack_new();
    for (int i = 0; i < inactiveList->size; i++)
        Stack_push(duplicatedStack, duplicateRegister(inactiveList->data[i]));
    wip->inactiveList = duplicatedStack;

    duplicatedStack = Stack_new();
    for (int i = 0; i < spilledList->size; i++)
        Stack_push(duplicatedStack, duplicateSpilledRegister(spilledList->data[i]));
    wip->spilledList = duplicatedStack;

    wip->currentLifetimeIndex = currentLifetimeIndex;
    return wip;
}

void restoreRegisterStates(struct Stack *savedStateStack,
                           struct Stack *activeList,
                           struct Stack *inactiveList,
                           struct Stack *spilledList,
                           int *currentLifetimeIndex,
                           int TACIndex,
                           struct ASMblock *outputBlock)
{
    printf("restore register states at TAC index %d\n", TACIndex);
    printf("%d live registers, %d free registers, %d vars on stack\n\n", activeList->size, inactiveList->size, spilledList->size);

    // pull the saved states off the stack
    struct SavedState *restoreTo = Stack_peek(savedStateStack);
    struct SavedState *editableState = duplicateCurrentState(restoreTo->activeList, restoreTo->inactiveList, restoreTo->spilledList, restoreTo->currentLifetimeIndex);
    struct Stack *savedActiveList = editableState->activeList;
    struct Stack *savedInactiveList = editableState->inactiveList;
    struct Stack *savedSpilledList = editableState->spilledList;

    *currentLifetimeIndex = editableState->currentLifetimeIndex;

    // expire anything that is no longer living at the TAC index of the restore
    expireOldIntervals(activeList, inactiveList, spilledList, TACIndex);
    expireOldIntervals(savedActiveList, savedInactiveList, savedSpilledList, TACIndex);

    // variables which have literally been pushed to the stack to free their registers
    // struct Stack *relocationStack = Stack_new();

    sortByRegisterNumber((struct Register **)activeList->data, activeList->size);
    sortByRegisterNumber((struct Register **)inactiveList->data, inactiveList->size);

    printf("Current state:\n");
    printCurrentState(activeList, inactiveList, spilledList);

    printf("Desired state:\n");
    printCurrentState(savedActiveList, savedInactiveList, savedSpilledList);

    sortByRegisterNumber((struct Register **)savedActiveList->data, savedActiveList->size);
    sortByRegisterNumber((struct Register **)savedInactiveList->data, savedInactiveList->size);

    char *outputLine;
    struct Stack *correctPlaceVariables = Stack_new();
    struct Stack *relocationStack = Stack_new();
    // examine all registers
    while (activeList->size > 0)
    {
        struct Register *examinedRegister = Stack_pop(activeList);
        char needRelocate = 1;
        for (int i = 0; i < savedActiveList->size; i++)
        {
            struct Register *desiredRegister = savedActiveList->data[i];
            if (desiredRegister->index == examinedRegister->index && desiredRegister->lifetime == examinedRegister->lifetime)
            {
                needRelocate = 0;
                break;
            }
        }
        if (needRelocate)
        {
            outputLine = malloc(64);
            sprintf(outputLine, "push %%r%d;temp store %s", examinedRegister->index, examinedRegister->lifetime->variable);
            ASMblock_append(outputBlock, outputLine);
            Stack_push(relocationStack, examinedRegister->lifetime);
            examinedRegister->lifetime = NULL;
            Stack_push(inactiveList, examinedRegister);
        }
        else
        {
            Stack_push(correctPlaceVariables, examinedRegister);
        }
    }

    while (correctPlaceVariables->size > 0)
    {
        Stack_push(activeList, Stack_pop(correctPlaceVariables));
    }
    Stack_free(correctPlaceVariables);

    struct Register *scratchRegister;
    char scratchRegisterLive = 0;
    if (inactiveList->size > 0)
    {
        scratchRegister = Stack_pop(inactiveList);
    }
    else
    {
        scratchRegisterLive = 1;
        scratchRegister = Stack_pop(correctPlaceVariables);
        outputLine = malloc(16);
        sprintf(outputLine, "mov %%rr, %%r%d", scratchRegister->index);
        ASMblock_append(outputBlock, outputLine);
    }

    while (spilledList->size > 0)
    {
        struct SpilledRegister *examinedSpill = Stack_pop(spilledList);
        if (examinedSpill->occupied)
        {
            outputLine = malloc(64);
            sprintf(outputLine, "mov %%r%d, %d(%%bp);unspill %s", scratchRegister->index, examinedSpill->stackOffset, examinedSpill->lifetime->variable);
            ASMblock_append(outputBlock, outputLine);
            outputLine = malloc(64);
            sprintf(outputLine, "push %%r%d;temp store %s", scratchRegister->index, examinedSpill->lifetime->variable);
            ASMblock_append(outputBlock, outputLine);
            Stack_push(relocationStack, examinedSpill->lifetime);
        }
        free(examinedSpill);
    }

    while (relocationStack->size > 0)
    {
        struct Lifetime *relocatedLifetime = Stack_pop(relocationStack);
        int destinationRegister = -1;
        for (int i = 0; i < savedActiveList->size; i++)
        {
            struct Register *desiredDestination = savedActiveList->data[i];
            if (desiredDestination->lifetime == relocatedLifetime)
            {
                destinationRegister = i;
                break;
            }
        }

        if (destinationRegister != -1)
        {
            struct Stack *inactiveSearchStack = Stack_new();
            while (inactiveList->size > 0)
            {
                struct Register *examinedRegister = Stack_pop(inactiveList);
                if (examinedRegister->index == destinationRegister)
                {
                    outputLine = malloc(64);
                    sprintf(outputLine, "pop %%r%d;place %s", examinedRegister->index, relocatedLifetime->variable);
                    ASMblock_append(outputBlock, outputLine);
                    examinedRegister->lifetime = relocatedLifetime;
                    Stack_push(activeList, examinedRegister);
                    break;
                }
                else
                {
                    Stack_push(inactiveSearchStack, examinedRegister);
                }
            }

            while (inactiveSearchStack->size > 0)
            {
                Stack_push(inactiveList, Stack_pop(inactiveSearchStack));
            }

            Stack_free(inactiveSearchStack);
        }
        else
        {

            int stackOffset = findSpilledVariable(savedSpilledList, relocatedLifetime->variable);
            if (stackOffset != -1)
            {
                outputLine = malloc(64);
                sprintf(outputLine, "pop %%r%d;retrieve %s", scratchRegister->index, relocatedLifetime->variable);
                ASMblock_append(outputBlock, outputLine);
                outputLine = malloc(64);
                sprintf(outputLine, "mov %d(%%bp) %%r%d;place %s", stackOffset, scratchRegister->index, relocatedLifetime->variable);
                ASMblock_append(outputBlock, outputLine);

                // printf("pop and put on stack offset %d\n", stackOffset);
            }
            else
            {
                printf("yowch - couldn't find destination register or stack offset for variable %s\n", relocatedLifetime->variable);
                exit(1);
            }
        }
    }

    if (scratchRegisterLive)
    {
        outputLine = malloc(16);
        sprintf(outputLine, "mov %%r%d, %%rr", scratchRegister->index);
        ASMblock_append(outputBlock, outputLine);
        Stack_push(activeList, scratchRegister);
    }else{
        Stack_push(inactiveList, scratchRegister);

    }

    printf("done restoring register states\n");

    printCurrentState(activeList, inactiveList, spilledList);

    while (savedActiveList->size > 0)
        free(Stack_pop(savedActiveList));

    Stack_free(savedActiveList);

    while (savedInactiveList->size > 0)
        free(Stack_pop(savedInactiveList));

    Stack_free(savedInactiveList);

    while (savedSpilledList->size > 0)
        free(Stack_pop(savedSpilledList));

    Stack_free(savedSpilledList);
    free(editableState);
    Stack_free(relocationStack);
    printf("\n");
}

void resetRegisterStates(struct Stack *savedStateStack,
                         struct Stack *activeList,
                         struct Stack *inactiveList,
                         struct Stack *spilledList,
                         int *currentLifetimeIndex)
{
    
    struct SavedState *resetTo = Stack_peek(savedStateStack);
    printf("\nRESET REGISTER STATES FROM:\n");
    printCurrentState(activeList, inactiveList, spilledList);
    printf("TO:\n");
   printCurrentState(resetTo->activeList, resetTo->inactiveList, resetTo->spilledList);
    
    
    // *currentLifetimeIndex = editableState->currentLifetimeIndex;

    while (activeList->size > 0)
        free(Stack_pop(activeList));

    for (int i = 0; i < resetTo->activeList->size; i++)
        Stack_push(activeList, duplicateRegister(resetTo->activeList->data[i]));
    
    while (inactiveList->size > 0)
        free(Stack_pop(inactiveList));

    for (int i = 0; i < resetTo->inactiveList->size; i++)
        Stack_push(inactiveList, duplicateRegister(resetTo->inactiveList->data[i]));

    while (spilledList->size > 0)
        free(Stack_pop(spilledList));

    for (int i = 0; i < resetTo->spilledList->size; i++)
        Stack_push(inactiveList, duplicateSpilledRegister(resetTo->spilledList->data[i]));

    printf("DONE:\n");
    printCurrentState(activeList, inactiveList, spilledList);

    printf("\n\n\n~\n~\n~\n~\n");

}

// find a variable which is to be assigned if in an active register
// if the variable is spilled, unspill it
int findOrPlaceAssignedVariable(struct Stack *activeList,
                                struct Stack *inactiveList,
                                struct Stack *spilledList,
                                char *varName,
                                struct ASMblock *outputBlock,
                                struct symbolTable *table)
{
    for (int i = 0; i < activeList->size; i++)
        if (!strcmp(((struct Register *)activeList->data[i])->lifetime->variable, varName))
            return ((struct Register *)activeList->data[i])->index;

    return unSpillVariable(activeList, inactiveList, spilledList, varName, outputBlock, table);
}

struct LinkedList *findLifetimes(struct symbolTable *table)
{
    struct LinkedList *lifetimes = LinkedList_new();

    for (int i = 0; i < table->size; i++)
    {
        if (table->entries[i]->type == e_argument)
        {
            // struct variableEntry *theArgument = table->entries[i]->entry;
            // printf("%s is %d\n", table->entries[i]->name, ((struct variableEntry *)table->entries[i]->entry)->type);

            updateOrInsertLifetime(lifetimes, table->entries[i]->name, ((struct variableEntry *)table->entries[i]->entry)->type, 0);
        }
    }

    struct LinkedListNode *blockRunner = table->BasicBlockList->head;
    while (blockRunner != NULL)
    {
        struct BasicBlock *thisBlock = blockRunner->data;
        struct LinkedListNode *TACRunner = thisBlock->TACList->head;
        while (TACRunner != NULL)
        {
            struct TACLine *thisLine = TACRunner->data;
            int TACIndex = thisLine->index;

            for (int i = 0; i < 3; i++)
            {
                switch (thisLine->operandTypes[i])
                {
                case vt_var:
                case vt_temp:
                    // printf("%s is %d\n", runner->operands[i], runner->operandTypes[i]);
                    updateOrInsertLifetime(lifetimes, thisLine->operands[i], thisLine->operandTypes[i], TACIndex);
                    break;
                default:
                }
            }
            TACRunner = TACRunner->next;
        }
        blockRunner = blockRunner->next;
    }
    return lifetimes;
}
