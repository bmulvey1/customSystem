#include "regalloc.h"

#define REGISTER_COUNT 3

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

void restoreRegisterStates(struct Stack *savedStateStack, struct Stack *activeList, struct Stack *inactiveList, struct Stack *spilledList, struct ASMblock *outputBlock, int currentTACIndex)
{
    printf("restore register states\n");
    printf("%d live registers, %d free registers, %d vars on stack\n\n", activeList->size, inactiveList->size, spilledList->size);

    // pull the saved states off the stack
    struct Stack *savedSpilledList = Stack_pop(savedStateStack);
    struct Stack *savedInactiveList = Stack_pop(savedStateStack);
    struct Stack *savedActiveList = Stack_pop(savedStateStack);

    // deep copy the state snapshots and put them back on the state stack
    struct Stack *duplicatedStack = Stack_new();
    for (int i = 0; i < savedActiveList->size; i++)
        Stack_push(duplicatedStack, duplicateRegister(savedActiveList->data[i]));

    Stack_push(savedStateStack, duplicatedStack);

    duplicatedStack = Stack_new();
    for (int i = 0; i < savedInactiveList->size; i++)
        Stack_push(duplicatedStack, duplicateRegister(savedInactiveList->data[i]));

    Stack_push(savedStateStack, duplicatedStack);

    duplicatedStack = Stack_new();
    for (int i = 0; i < savedSpilledList->size; i++)
        Stack_push(duplicatedStack, duplicateSpilledRegister(savedSpilledList->data[i]));

    Stack_push(savedStateStack, duplicatedStack);

    // expire anything that is no longer living at the TAC index of the restore
    expireOldIntervals(savedActiveList, savedInactiveList, savedSpilledList, currentTACIndex);

    struct Stack *relocationStack = Stack_new();

    // variables which have literally been pushed to the stack to free their registers
    // struct Stack *relocationStack = Stack_new();

    struct Register **currentState = malloc(12 * sizeof(struct Register *));
    struct Register **desiredState = malloc(12 * sizeof(struct Register *));
    sortByRegisterNumber((struct Register **)activeList->data, activeList->size);
    sortByRegisterNumber((struct Register **)inactiveList->data, inactiveList->size);

    int p = 0;
    while (activeList->size > 0 || inactiveList->size > 0)
    {
        if (activeList->size > 0)
        {
            if (((struct Register *)Stack_peek(activeList))->index == p)
            {
                currentState[p++] = Stack_pop(activeList);
            }
            else
            {
                currentState[p] = Stack_pop(inactiveList);
                currentState[p]->lifetime = NULL;
                p++;
            }
        }
        else
        {
            currentState[p] = Stack_pop(inactiveList);
            currentState[p]->lifetime = NULL;
            p++;
        }
    }

    sortByRegisterNumber((struct Register **)savedActiveList->data, savedActiveList->size);
    sortByRegisterNumber((struct Register **)savedInactiveList->data, savedInactiveList->size);
    p = 0;
    while (savedActiveList->size > 0 || savedInactiveList->size > 0)
    {
        if (savedActiveList->size > 0)
        {
            if (((struct Register *)Stack_peek(savedActiveList))->index == p)
            {
                desiredState[p++] = Stack_pop(savedActiveList);
            }
            else
            {
                desiredState[p] = Stack_pop(savedInactiveList);
                desiredState[p]->lifetime = NULL;
                p++;
            }
        }
        else
        {
            desiredState[p] = Stack_pop(savedInactiveList);
            desiredState[p]->lifetime = NULL;
            p++;
        }
    }

    // for()

    printf("Current state:");
    for (int i = 0; i < REGISTER_COUNT; i++)
    {
        printf("%%r%d: ", i);
        if (currentState[i]->lifetime == NULL)
        {
            printf("[ ], ");
        }
        else
        {
            printf("[%s], ", currentState[i]->lifetime->variable);
        }
    }
    printf("\nDesired state:");
    for (int i = 0; i < REGISTER_COUNT; i++)
    {
        printf("%%r%d: ", i);
        if (desiredState[i]->lifetime == NULL)
        {
            printf("[ ], ");
        }
        else
        {
            printf("[%s], ", desiredState[i]->lifetime->variable);
        }
    }
    printf("\n");

    char *outputLine;
    // examine all registers
    for (int i = 0; i < REGISTER_COUNT; i++)
    {
        // if the current and desired lifetimes are different
        // (can directly compare pointers because lifetimes are only instantiated once, never copied!)
        if (currentState[i]->lifetime != desiredState[i]->lifetime)
        {
            // contents of this register exist, but are wrong - store on stack for the mean time
            if (currentState[i]->lifetime != NULL)
            {
                outputLine = malloc(16);
                sprintf(outputLine, "push %%r%d", i);
                ASMblock_append(outputBlock, outputLine);
                Stack_push(relocationStack, currentState[i]->lifetime);
                currentState[i]->lifetime = NULL;
            }
        }
    }

    while (relocationStack->size > 0)
    {
        struct Lifetime *toPop = Stack_pop(relocationStack);
        for (int i = 0; i < REGISTER_COUNT; i++)
        {
            if (desiredState[i]->lifetime == toPop)
            {
                outputLine = malloc(16);
                sprintf(outputLine, "pop %%r%d", i);
                ASMblock_append(outputBlock, outputLine);
                currentState[i]->lifetime = toPop;
            }
        }

        // printf("RELOCATE PROPERLY\n");
        // exit(2);
    }

    for (int i = 0; i < REGISTER_COUNT; i++)
        if (currentState[i]->lifetime != NULL)
            Stack_push(activeList, currentState[i]);
        else
            Stack_push(inactiveList, currentState[i]);

    Stack_free(savedActiveList);
    Stack_free(savedInactiveList);
    Stack_free(savedSpilledList);
    free(currentState);
    for (int i = 0; i < REGISTER_COUNT; i++)
    {
        free(desiredState[i]);
    }
    free(desiredState);

    Stack_free(relocationStack);
}

void resetRegisterStates(struct Stack *savedStateStack, struct Stack *activeList, struct Stack *inactiveList, struct Stack *spilledList)
{
    // pull the saved states off the stack
    struct Stack *savedSpilledList = Stack_pop(savedStateStack);
    struct Stack *savedInactiveList = Stack_pop(savedStateStack);
    struct Stack *savedActiveList = Stack_pop(savedStateStack);

    // deep copy the state snapshots and put them back on the state stack
    struct Stack *duplicatedStack = Stack_new();
    for (int i = 0; i < savedActiveList->size; i++)
        Stack_push(duplicatedStack, duplicateRegister(savedActiveList->data[i]));

    Stack_push(savedStateStack, duplicatedStack);

    duplicatedStack = Stack_new();
    for (int i = 0; i < savedInactiveList->size; i++)
        Stack_push(duplicatedStack, duplicateRegister(savedInactiveList->data[i]));

    Stack_push(savedStateStack, duplicatedStack);

    duplicatedStack = Stack_new();
    for (int i = 0; i < savedSpilledList->size; i++)
        Stack_push(duplicatedStack, duplicateSpilledRegister(savedSpilledList->data[i]));

    Stack_push(savedStateStack, duplicatedStack);

    printf("RESET REGISTER STATES FROM:\n");
    printCurrentState(activeList, inactiveList, spilledList);

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
    // int TACIndex = 0;
    // int currentLifetimeIndex = 0;
    char *outputLine;
    struct Stack *savedStateStack = Stack_new();
    /*
    for (struct TACLine *runner = table->codeBlock; runner != NULL; runner = runner->nextLine)
    {

        expireOldIntervals(activeList, inactiveList, spilledList, TACIndex);

        // increment last used values of all active registers or reset if variable used in this step
        for (int i = 0; i < activeList->size; i++)
        {
            struct Register *thisReg = activeList->data[i];
            thisReg->lastUsed++;
            for (int i = 0; i < 3; i++)
            {
                switch (runner->operandTypes[i])
                {
                case vt_var:
                case vt_temp:
                    if (!strcmp(thisReg->lifetime->variable, runner->operands[i]))
                        thisReg->lastUsed = 0;

                    break;

                default:
                    break;
                }
            }
        }

        char *finalOutputLine;
        char *printedTAC;

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
                printf("need to place argument [%s] in newly assigned register %d\n", this->variable, destinationIndex);
            }
        }
        int destinationRegister;
        int firstSourceRegister;
        int secondSourceRegister;
        switch (runner->operation)
        {
        case tt_assign:
        {
            outputLine = malloc(20);
            finalOutputLine = malloc(64);
            destinationRegister = findOrPlaceAssignedVariable(activeList, inactiveList, spilledList, runner->operands[0], outputBlock, table);
            if (runner->operandTypes[1] == vt_literal)
            {
                sprintf(outputLine, "mov %%r%d, $%s", destinationRegister, runner->operands[1]);
            }
            else
            {
                firstSourceRegister = findActiveVariable(activeList, runner->operands[1]);
                if (firstSourceRegister != -1)
                {
                    sprintf(outputLine, "mov %%r%d, %%r%d", destinationRegister, firstSourceRegister);
                }
                else
                {
                    sprintf(outputLine, "mov %%r%d, %d(%%bp)", destinationRegister, findSpilledVariable(spilledList, runner->operands[1]));
                }
            }
            // ASMblock_append(outputBlock, outputLine);
            printedTAC = sPrintTACLine(runner);
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
            destinationRegister = findOrPlaceAssignedVariable(activeList, inactiveList, spilledList, runner->operands[0], outputBlock, table);

            if (strcmp(runner->operands[0], runner->operands[1]))
                firstSourceRegister = findActiveVariable(activeList, runner->operands[1]);
            else
                firstSourceRegister = destinationRegister;

            if (strcmp(runner->operands[0], runner->operands[2]))
                secondSourceRegister = findActiveVariable(activeList, runner->operands[2]);
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
                        sprintf(outputLine, "%s %%r%d, %%r%d", getAsmOp(runner->operation), destinationRegister, destinationRegister);
                    }
                    else
                    {
                        sprintf(outputLine, "mov %%r%d, %%r%d", destinationRegister, firstSourceRegister);
                        ASMblock_append(outputBlock, outputLine);
                        outputLine = malloc(20);
                        sprintf(outputLine, "%s %%r%d, %%r%d", getAsmOp(runner->operation), secondSourceRegister, secondSourceRegister);
                    }
                }
                else
                {
                    sprintf(outputLine, "mov %%r%d, %%r%d", destinationRegister, firstSourceRegister);
                    ASMblock_append(outputBlock, outputLine);
                    outputLine = malloc(20);
                    sprintf(outputLine, "%s %%r%d, %%r%d", getAsmOp(runner->operation), destinationRegister, secondSourceRegister);
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
                    if (runner->operandTypes[2] == vt_literal)
                    {
                        sprintf(outputLine, "%s %%r%d, $%s", getAsmOp(runner->operation), destinationRegister, runner->operands[2]);
                    }
                    else
                    {
                        sprintf(outputLine, "%s %%r%d, %d(%%bp)", getAsmOp(runner->operation), destinationRegister, findSpilledVariable(spilledList, runner->operands[2]));
                    }
                }
                // second source exists in register, first is spilled
                else if (firstSourceRegister == -1 && secondSourceRegister != -1)
                {
                    if (runner->operandTypes[1] == vt_literal)
                    {
                        sprintf(outputLine, "mov %%r%d, $%s", destinationRegister, runner->operands[1]);
                        ASMblock_append(outputBlock, outputLine);
                    }
                    else
                    {
                        sprintf(outputLine, "mov %%r%d, %d(%%bp)", destinationRegister, findSpilledVariable(spilledList, runner->operands[1]));
                        ASMblock_append(outputBlock, outputLine);
                    }
                    outputLine = malloc(20);
                    sprintf(outputLine, "%s %%r%d, %%r%d", getAsmOp(runner->operation), destinationRegister, secondSourceRegister);
                }
                // both sources are spilled - will break if both are literals but this should be checked earlier
                else
                {
                    sprintf(outputLine, "mov %%r%d, %d(%%bp)", destinationRegister, findSpilledVariable(spilledList, runner->operands[1]));
                    ASMblock_append(outputBlock, outputLine);
                    outputLine = malloc(20);
                    sprintf(outputLine, "%s %%r%d, %d(%%bp)", getAsmOp(runner->operation), destinationRegister, findSpilledVariable(spilledList, runner->operands[2]));
                }
            }
            printedTAC = sPrintTACLine(runner);
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
            firstSourceRegister = findActiveVariable(activeList, runner->operands[1]);

            secondSourceRegister = findActiveVariable(activeList, runner->operands[2]);

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
                    if (runner->operandTypes[2] == vt_literal)
                    {
                        sprintf(outputLine, "cmp %%r%d, $%s", firstSourceRegister, runner->operands[2]);
                    }
                    else
                    {
                        sprintf(outputLine, "cmp %%r%d, %d(%%bp)", firstSourceRegister, findSpilledVariable(spilledList, runner->operands[2]));
                    }
                }
                // second source exists in register, first is spilled
                else if (firstSourceRegister == -1 && secondSourceRegister != -1)
                {
                    firstSourceRegister = unSpillVariable(activeList, inactiveList, spilledList, runner->operands[0], outputBlock, table);

                    sprintf(outputLine, "cmp %%r%d, %%r%d", firstSourceRegister, secondSourceRegister);
                }

                // both sources are spilled - will break if both are literals but this should be checked earlier
                else
                {
                    firstSourceRegister = unSpillVariable(activeList, inactiveList, spilledList, runner->operands[0], outputBlock, table);
                    sprintf(outputLine, "cmp %%r%d, %d(%%bp)", firstSourceRegister, findSpilledVariable(spilledList, runner->operands[2]));
                }
            }
            printedTAC = sPrintTACLine(runner);
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
            struct Stack *duplicatedStack = Stack_new();
            for (int i = 0; i < activeList->size; i++)
                Stack_push(duplicatedStack, duplicateRegister(activeList->data[i]));

            Stack_push(savedStateStack, duplicatedStack);

            duplicatedStack = Stack_new();
            for (int i = 0; i < inactiveList->size; i++)
                Stack_push(duplicatedStack, duplicateRegister(inactiveList->data[i]));

            Stack_push(savedStateStack, duplicatedStack);

            duplicatedStack = Stack_new();
            for (int i = 0; i < spilledList->size; i++)
                Stack_push(duplicatedStack, duplicateSpilledRegister(spilledList->data[i]));

            Stack_push(savedStateStack, duplicatedStack);
        }
        break;

        case tt_restorestate:
            restoreRegisterStates(savedStateStack, activeList, inactiveList, spilledList, outputBlock, TACIndex);
            break;

        case tt_resetstate:
            resetRegisterStates(savedStateStack, activeList, inactiveList, spilledList);
            break;

        case tt_popstate:
        {
            for (int i = 0; i < 3; i++)
            {
                struct Stack *poppedStack = Stack_pop(savedStateStack);
                while (poppedStack->size > 0)
                    free(Stack_pop(poppedStack));

                Stack_free(poppedStack);
            }
        }
        break;

        case tt_return:
        {
            // find where the return value is (it must be active in a register because it was just assigned!)
            int retValIndex = findActiveVariable(activeList, runner->operands[0]);
            if (retValIndex == -1)
            {
                printf("RETURN VALUE %s NOT FOUND ACTIVE IN REGISTER!", runner->operands[0]);
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
            sprintf(outputLine, "%s %s_%ld", getAsmOp(runner->operation), table->name, (long int)runner->operands[0]);
            ASMblock_append(outputBlock, outputLine);
            break;
        }
        break;

        case tt_label:
            outputLine = malloc(64);
            sprintf(outputLine, ".%s_%ld:", table->name, (long int)runner->operands[0]);
            ASMblock_append(outputBlock, outputLine);
            break;

        case tt_asm:
            ASMblock_append(outputBlock, runner->operands[0]);
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

        TACIndex++;
    }
    */

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
