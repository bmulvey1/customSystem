#include "regalloc.h"

#define REGISTER_COUNT 2

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
void updateOrInsertLifetime(struct LinkedList *ltList, char *variable, enum variableTypes type, int newEnd)
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

void printCurrentState(struct Stack *activeList, struct Stack *inactiveList, struct Stack *spilledList)
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

struct SpilledRegister *duplicateSpilledRegister(struct Register *r)
{
    struct SpilledRegister *wip = malloc(sizeof(struct SpilledRegister));
    memcpy(wip, r, sizeof(struct SpilledRegister));
    return wip;
}

void expireOldIntervals(struct Stack *activeList, struct Stack *inactiveList, struct Stack *spilledList, int TACIndex)
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
        printf("pop stack\n");
        free(Stack_pop(spilledList));
    }
}

void spillRegister(struct Stack *activeList, struct Stack *inactiveList, struct Stack *spilledList, struct ASMblock *outputBlock, struct symbolTable *table)
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

int assignRegister(struct Stack *activeList, struct Stack *inactiveList, struct Lifetime *assignedLifetime)
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
int unSpillVariable(struct Stack *activeList, struct Stack *inactiveList, struct Stack *spilledList, char *varName, struct ASMblock *outputBlock, struct symbolTable *table)
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
struct SavedState *duplicateCurrentState(struct Stack *activeList, struct Stack *inactiveList, struct Stack *spilledList, int currentLifetimeIndex)
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

void restoreRegisterStates(struct Stack *savedStateStack, struct Stack *activeList, struct Stack *inactiveList, struct Stack *spilledList, int *currentLifetimeIndex, int TACIndex, struct ASMblock *outputBlock)
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

    struct Register *scratchRegister;
    if (inactiveList->size > 0)
    {
        scratchRegister = Stack_pop(inactiveList);
    }
    else
    {
        scratchRegister = Stack_pop(correctPlaceVariables);
        outputLine = malloc(64);
        sprintf(outputLine, "push %%r%d;temp store %s", scratchRegister->index, scratchRegister->lifetime->variable);
        ASMblock_append(outputBlock, outputLine);
        Stack_push(relocationStack, scratchRegister->lifetime);
        scratchRegister->lifetime = NULL;
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
    Stack_free(savedActiveList);
    Stack_free(savedInactiveList);
    Stack_free(savedSpilledList);
    free(editableState);
    Stack_free(relocationStack);
    printf("done restoring register states\n");
}

void resetRegisterStates(struct Stack *savedStateStack, struct Stack *activeList, struct Stack *inactiveList, struct Stack *spilledList, int *currentLifetimeIndex)
{
    struct SavedState *resetTo = Stack_peek(savedStateStack);
    printf("RESET REGISTER STATES FROM:\n");
    printCurrentState(activeList, inactiveList, spilledList);
    struct SavedState *editableState = duplicateCurrentState(resetTo->activeList, resetTo->activeList, resetTo->spilledList, resetTo->currentLifetimeIndex);
    struct Stack *savedActiveList = editableState->activeList;
    struct Stack *savedInactiveList = editableState->inactiveList;
    struct Stack *savedSpilledList = editableState->spilledList;

    *currentLifetimeIndex = editableState->currentLifetimeIndex;

    while (activeList->size > 0)
        free(Stack_pop(activeList));

    for (int i = 0; i < savedActiveList->size; i++)
        Stack_push(activeList, savedActiveList->data[i]);

    while (inactiveList->size > 0)
        free(Stack_pop(inactiveList));

    for (int i = 0; i < savedInactiveList->size; i++)
        Stack_push(inactiveList, savedInactiveList->data[i]);

    while (spilledList->size > 0)
        free(Stack_pop(spilledList));

    for (int i = 0; i < savedSpilledList->size; i++)
        Stack_push(spilledList, savedSpilledList->data[i]);

    printf("TO:\n");
    printCurrentState(activeList, inactiveList, spilledList);

    printf("\n");

    Stack_free(savedActiveList);
    Stack_free(savedInactiveList);
    Stack_free(savedSpilledList);
}

// find a variable which is to be assigned if in an active register
// if the variable is spilled, unspill it
int findOrPlaceAssignedVariable(struct Stack *activeList, struct Stack *inactiveList, struct Stack *spilledList, char *varName, struct ASMblock *outputBlock, struct symbolTable *table)
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
    printLifetimesGraph(lifetimes);

    int *registerLoads = malloc((REGISTER_COUNT + 1) * sizeof(int)); // count of TAC steps with different numbers of registers free
    for (int i = 0; i < REGISTER_COUNT; i++)
        registerLoads[i] = 0;

    int *stackLoads = malloc(lifetimeCount * sizeof(int)); // count of TAC steps with different numbers of variables spilled
    for (int i = 0; i < lifetimeCount; i++)
        stackLoads[i] = 0;

    struct Stack *inactiveList = Stack_new(); // registers not currently in use
    struct Stack *activeList = Stack_new();   // registers containing variables
    struct Stack *spilledList = Stack_new();  // list of live variables which have been spilled to stack
    int maxSpillSpace = 0;

    // create all register instances, add to inactive list
    for (int i = 0; i < REGISTER_COUNT; i++)
    {
        struct Register *wip = malloc(sizeof(struct Register));
        wip->lifetime = NULL;
        wip->index = REGISTER_COUNT - 1 - i;
        Stack_push(inactiveList, wip);
    }

    // iterate each TAC line
    int TACIndex = 0;
    int currentLifetimeIndex = 0;
    char *outputLine;
    struct Stack *savedStateStack = Stack_new();

    struct LinkedListNode *blockRunner = table->BasicBlockList->head;
    while (blockRunner != NULL)
    {
        struct BasicBlock *thisBlock = blockRunner->data;
        printf("GENERATE CODE FOR BASIC BLOCK %d\n", thisBlock->labelNum);
        outputLine = malloc(64);
        sprintf(outputLine, "%s_%d:", table->name, thisBlock->labelNum);
        ASMblock_append(outputBlock, outputLine);
        struct LinkedListNode *TACRunner = thisBlock->TACList->head;
        while (TACRunner != NULL)
        {
            struct TACLine *currentTAC = TACRunner->data;
            printTACLine(currentTAC);
            printf("\n");
            TACIndex = currentTAC->index;
            expireOldIntervals(activeList, inactiveList, spilledList, TACIndex);

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

            char *finalOutputLine;
            char *printedTAC;

            printf("canintroduce\n");
            // for all previously unseen lifetimes starting before or on this TAC index
            while (currentLifetimeIndex < lifetimeCount && lifetimeArray[currentLifetimeIndex]->start <= TACIndex)
            {
                // grab the lifetime
                struct Lifetime *this = lifetimeArray[currentLifetimeIndex++];

                // spill a register if one isn't available
                if (inactiveList->size == 0)
                {
                    // printf("need to spill\n");
                    spillRegister(activeList, inactiveList, spilledList, outputBlock, table);

                    // Stack_push(spilledList, thisSpill);
                    if (spilledList->size * 2 > maxSpillSpace)
                        maxSpillSpace = spilledList->size * 2;
                }

                // assign this variable to the next free register
                int destinationIndex = assignRegister(activeList, inactiveList, this);

                // place the value into the register if this is an argument (value starts on the stack)
                if (this->variable[0] != '.' && symbolTableLookup(table, this->variable)->type == e_argument)
                {
                    printf("PLACE ARGUMENT %s AT REGISTER %d\n", this->variable, destinationIndex);
                    outputLine = malloc(20);
                    struct variableEntry *theArgument = symbolTableLookup(table, this->variable)->entry;
                    sprintf(outputLine, "mov %%r%d, %d(%%bp)", destinationIndex, theArgument->stackOffset);
                    finalOutputLine = malloc(128);
                    sprintf(finalOutputLine, "%s ;place argument %s", outputLine, this->variable);
                    free(outputLine);
                    ASMblock_append(outputBlock, finalOutputLine);
                }
            }

            int destinationRegister;
            int firstSourceRegister;
            int secondSourceRegister;
            switch (currentTAC->operation)
            {
            case tt_assign:
            {
                outputLine = malloc(20);
                finalOutputLine = malloc(64);
                destinationRegister = findOrPlaceAssignedVariable(activeList, inactiveList, spilledList, currentTAC->operands[0], outputBlock, table);
                if (currentTAC->operandTypes[1] == vt_literal)
                {
                    sprintf(outputLine, "mov %%r%d, $%s", destinationRegister, currentTAC->operands[1]);
                }
                else
                {
                    firstSourceRegister = findActiveVariable(activeList, currentTAC->operands[1]);
                    if (firstSourceRegister != -1)
                    {
                        sprintf(outputLine, "mov %%r%d, %%r%d", destinationRegister, firstSourceRegister);
                    }
                    else
                    {
                        sprintf(outputLine, "mov %%r%d, %d(%%bp)", destinationRegister, findSpilledVariable(spilledList, currentTAC->operands[1]));
                    }
                }
                // ASMblock_append(outputBlock, outputLine);
                printedTAC = sPrintTACLine(currentTAC);
                sprintf(finalOutputLine, "%s;%s", outputLine, printedTAC);
                free(outputLine);
                free(printedTAC);
                ASMblock_append(outputBlock, finalOutputLine);
            }
            break;

            case tt_add:
            case tt_subtract:
            {
                outputLine = malloc(20);
                finalOutputLine = malloc(64);
                destinationRegister = findOrPlaceAssignedVariable(activeList, inactiveList, spilledList, currentTAC->operands[0], outputBlock, table);

                if (strcmp(currentTAC->operands[0], currentTAC->operands[1]))
                    firstSourceRegister = findActiveVariable(activeList, currentTAC->operands[1]);
                else
                    firstSourceRegister = destinationRegister;

                if (strcmp(currentTAC->operands[0], currentTAC->operands[2]))
                    secondSourceRegister = findActiveVariable(activeList, currentTAC->operands[2]);
                else
                    secondSourceRegister = destinationRegister;

                // printf("OPERAND INDICES ARE %d %d %d\n", destinationRegister, firstSourceRegister, secondSourceRegister);

                // both source operands are variables in registers
                if (firstSourceRegister != -1 && secondSourceRegister != -1)
                {
                    if (firstSourceRegister == secondSourceRegister)
                    {
                        if (firstSourceRegister == destinationRegister)
                        {
                            sprintf(outputLine, "%s %%r%d, %%r%d", getAsmOp(currentTAC->operation), destinationRegister, destinationRegister);
                        }
                        else
                        {
                            sprintf(outputLine, "mov %%r%d, %%r%d", destinationRegister, firstSourceRegister);
                            ASMblock_append(outputBlock, outputLine);
                            outputLine = malloc(20);
                            sprintf(outputLine, "%s %%r%d, %%r%d", getAsmOp(currentTAC->operation), secondSourceRegister, secondSourceRegister);
                        }
                    }
                    else
                    {
                        sprintf(outputLine, "mov %%r%d, %%r%d", destinationRegister, firstSourceRegister);
                        ASMblock_append(outputBlock, outputLine);
                        outputLine = malloc(20);
                        sprintf(outputLine, "%s %%r%d, %%r%d", getAsmOp(currentTAC->operation), destinationRegister, secondSourceRegister);
                    }
                }
                else
                {
                    // first source exists in register, second is spilled or a literal
                    if (firstSourceRegister != -1 && secondSourceRegister == -1)
                    {
                        sprintf(outputLine, "mov %%r%d, %%r%d", destinationRegister, firstSourceRegister);
                        ASMblock_append(outputBlock, outputLine);
                        outputLine = malloc(20);
                        if (currentTAC->operandTypes[2] == vt_literal)
                        {
                            sprintf(outputLine, "%s %%r%d, $%s", getAsmOp(currentTAC->operation), destinationRegister, currentTAC->operands[2]);
                        }
                        else
                        {
                            sprintf(outputLine, "%s %%r%d, %d(%%bp)", getAsmOp(currentTAC->operation), destinationRegister, findSpilledVariable(spilledList, currentTAC->operands[2]));
                        }
                    }
                    // second source exists in register, first is spilled
                    else if (firstSourceRegister == -1 && secondSourceRegister != -1)
                    {
                        if (currentTAC->operandTypes[1] == vt_literal)
                        {
                            sprintf(outputLine, "mov %%r%d, $%s", destinationRegister, currentTAC->operands[1]);
                            ASMblock_append(outputBlock, outputLine);
                        }
                        else
                        {
                            sprintf(outputLine, "mov %%r%d, %d(%%bp)", destinationRegister, findSpilledVariable(spilledList, currentTAC->operands[1]));
                            ASMblock_append(outputBlock, outputLine);
                        }
                        outputLine = malloc(20);
                        sprintf(outputLine, "%s %%r%d, %%r%d", getAsmOp(currentTAC->operation), destinationRegister, secondSourceRegister);
                    }
                    // both sources are spilled - will break if both are literals but this should be checked earlier
                    else
                    {
                        sprintf(outputLine, "mov %%r%d, %d(%%bp)", destinationRegister, findSpilledVariable(spilledList, currentTAC->operands[1]));
                        ASMblock_append(outputBlock, outputLine);
                        outputLine = malloc(20);
                        sprintf(outputLine, "%s %%r%d, %d(%%bp)", getAsmOp(currentTAC->operation), destinationRegister, findSpilledVariable(spilledList, currentTAC->operands[2]));
                    }
                }
                printedTAC = sPrintTACLine(currentTAC);
                sprintf(finalOutputLine, "%s;%s", outputLine, printedTAC);
                free(outputLine);
                free(printedTAC);
                ASMblock_append(outputBlock, finalOutputLine);
            }
            break;

            case tt_cmp:
            {
                outputLine = malloc(20);
                finalOutputLine = malloc(64);
                firstSourceRegister = findActiveVariable(activeList, currentTAC->operands[1]);

                secondSourceRegister = findActiveVariable(activeList, currentTAC->operands[2]);

                // printf("OPERAND INDICES ARE %d %d %d\n", destinationRegister, firstSourceRegister, secondSourceRegister);

                // both source operands are variables in registers
                if (firstSourceRegister != -1 && secondSourceRegister != -1)
                {
                    if (firstSourceRegister == secondSourceRegister)
                    {
                        sprintf(outputLine, "cmp %%r%d, %%r%d", firstSourceRegister, firstSourceRegister);
                    }
                    else
                    {
                        sprintf(outputLine, "cmp %%r%d, %%r%d", firstSourceRegister, secondSourceRegister);
                    }
                }
                else
                {
                    // first source exists in register, second is spilled or a literal
                    if (firstSourceRegister != -1 && secondSourceRegister == -1)
                    {
                        if (currentTAC->operandTypes[2] == vt_literal)
                        {
                            sprintf(outputLine, "cmp %%r%d, $%s", firstSourceRegister, currentTAC->operands[2]);
                        }
                        else
                        {
                            sprintf(outputLine, "cmp %%r%d, %d(%%bp)", firstSourceRegister, findSpilledVariable(spilledList, currentTAC->operands[2]));
                        }
                    }
                    // second source exists in register, first is spilled
                    else if (firstSourceRegister == -1 && secondSourceRegister != -1)
                    {
                        firstSourceRegister = unSpillVariable(activeList, inactiveList, spilledList, currentTAC->operands[1], outputBlock, table);

                        sprintf(outputLine, "cmp %%r%d, %%r%d", firstSourceRegister, secondSourceRegister);
                    }

                    // both sources are spilled - will break if both are literals but this should be checked earlier
                    else
                    {
                        firstSourceRegister = unSpillVariable(activeList, inactiveList, spilledList, currentTAC->operands[1], outputBlock, table);

                        if (currentTAC->operandTypes[2] == vt_literal)
                        {
                            sprintf(outputLine, "cmp %%r%d, %s", firstSourceRegister, currentTAC->operands[0]);
                        }
                        else
                        {
                            sprintf(outputLine, "cmp %%r%d, %d(%%bp)", firstSourceRegister, findSpilledVariable(spilledList, currentTAC->operands[2]));
                        }
                    }
                }
                printedTAC = sPrintTACLine(currentTAC);
                sprintf(finalOutputLine, "%s;%s", outputLine, printedTAC);
                free(outputLine);
                free(printedTAC);
                ASMblock_append(outputBlock, finalOutputLine);
            }
            break;

            case tt_pushstate:
            {
                printf("PUSH STATE\n");
                // deep copy the states and push them to the state stack
                Stack_push(savedStateStack, duplicateCurrentState(activeList, inactiveList, spilledList, currentLifetimeIndex));
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
            }
            break;

            case tt_return:
            {
                // find where the return value is (it must be active in a register because it was just assigned!)
                int retValIndex = findActiveVariable(activeList, currentTAC->operands[0]);
                if (retValIndex == -1)
                {
                    printf("RETURN VALUE %s NOT FOUND ACTIVE IN REGISTER!", currentTAC->operands[0]);
                    exit(1);
                }

                // if it's not in register 0, move it there
                if (retValIndex != 0)
                {
                    outputLine = malloc(64);
                    sprintf(outputLine, "mov %%r0, %%r%d", retValIndex);
                    ASMblock_append(outputBlock, outputLine);
                }
                break;
            }

            case tt_jg:
            case tt_jge:
            case tt_jl:
            case tt_jle:
            case tt_je:
            case tt_jne:
            case tt_jmp:
            {
                outputLine = malloc(64);
                sprintf(outputLine, "%s %s_%ld", getAsmOp(currentTAC->operation), table->name, (long int)currentTAC->operands[0]);
                ASMblock_append(outputBlock, outputLine);
                break;
            }
            break;

            case tt_label:
                outputLine = malloc(64);
                sprintf(outputLine, ".%s_%ld:", table->name, (long int)currentTAC->operands[0]);
                ASMblock_append(outputBlock, outputLine);
                break;

            case tt_asm:
                ASMblock_append(outputBlock, currentTAC->operands[0]);
                break;

            default:
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
            // printCurrentState(activeList, inactiveList, spilledList);
            printf("\n\n");
            TACRunner = TACRunner->next;
        }
        printf("done generating code for basic block %d\n", thisBlock->labelNum);
        printCurrentState(activeList, inactiveList, spilledList);
        printf("\n\n");
        blockRunner = blockRunner->next;
    }

    if (maxSpillSpace > 0)
    {
        outputLine = malloc(20);
        sprintf(outputLine, "sub %%sp, $%d", maxSpillSpace);
        ASMblock_prepend(outputBlock, outputLine);
    }

    outputLine = malloc(64);
    sprintf(outputLine, ".%s:", table->name);
    ASMblock_prepend(outputBlock, outputLine);

    if (maxSpillSpace > 0)
    {
        outputLine = malloc(20);
        sprintf(outputLine, "add %%sp, $%d", maxSpillSpace);
        ASMblock_append(outputBlock, outputLine);
    }

    outputLine = malloc(20);
    sprintf(outputLine, "ret %d", table->argStackSize);
    ASMblock_append(outputBlock, outputLine);

    int totalCodeSteps = 0;
    for (int i = 0; i < REGISTER_COUNT; i++)
    {
        totalCodeSteps += registerLoads[i];
    }

    if (totalCodeSteps > 0)
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

        printf("\n\n");
    }

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

    Stack_free(savedStateStack);

    return outputBlock;
}
