#include "regalloc.h"

struct Lifetime *newLifetime(char *variable, int start)
{
    struct Lifetime *wip = malloc(sizeof(struct Lifetime));
    wip->variable = variable;
    wip->start = start;
    wip->end = start;
    return wip;
}

char compareLifetimes(struct Lifetime *a, char *variable)
{
    return strcmp(a->variable, variable);
}

// search through the list of existing lifetimes
// update the lifetime if it exists, insert if it doesn't
void updateOrInsertLifetime(struct LinkedList *ltList, char *variable, int newEnd)
{
    struct Lifetime *thisLt = LinkedList_find(ltList, &compareLifetimes, variable);
    if (thisLt != NULL)
    {
        thisLt->end = newEnd;
    }
    else
    {
        thisLt = newLifetime(variable, newEnd);
        LinkedList_insert(ltList, thisLt);
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
            printf("Expire %s\n", poppedRegister->lifetime->variable);
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
        Stack_pop(spilledList);
    }
}

void spillRegister(struct Stack *activeList, struct Stack *inactiveList, struct Stack *spilledList)
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

    printf("Spill [%s]", victim->lifetime->variable);
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
            printf(" to existing slot\n");
            break;
        }
    }

    // no unoccupied slot was found, push a new spill to the stack
    if (needNewSlot)
    {
        struct SpilledRegister *thisSpill = malloc(sizeof(struct SpilledRegister));
        thisSpill->lifetime = victim->lifetime;
        thisSpill->lastUsed = victim->lastUsed;
        thisSpill->stackOffset = (spilledList->size * -2) - 2;
        thisSpill->occupied = 1;
        Stack_push(spilledList, thisSpill);
        printf(" to new slot\n");
    }

    // return the newly-spilled register to the inactive list
    Stack_push(inactiveList, victim);
}

int assignRegister(struct Stack *activeList, struct Stack *inactiveList, struct Lifetime *assignedLifetime)
{
    struct Register *assignedRegister = Stack_pop(inactiveList);
    // printf("assign register %d for [%s]\n", assignedRegister->index, assignedLifetime->variable);
    printf("assign register for [%s]\n", assignedLifetime->variable);
    assignedRegister->lifetime = assignedLifetime;
    assignedRegister->lastUsed = 0;

    Stack_push(activeList, assignedRegister);
    if (inactiveList->size > 0)
        sortByEndPoint((struct Register **)activeList->data, activeList->size);

    sortByRegisterNumber((struct Register **)inactiveList->data, inactiveList->size);

    return assignedRegister->index;
}

// unspill variable from stack to register, returning the register index the variable now lives in
int unSpillVariable(struct Stack *activeList, struct Stack *inactiveList, struct Stack *spilledList, char *varName)
{
    if (inactiveList->size == 0)
    {
        spillRegister(activeList, inactiveList, spilledList);
    }
    for (int i = 0; i < spilledList->size; i++)
    {
        struct SpilledRegister *examinedSpill = spilledList->data[i];
        if (!strcmp(examinedSpill->lifetime->variable, varName))
        {
            int destinationRegister = assignRegister(activeList, inactiveList, examinedSpill->lifetime);

            printf("unspill variable %s from stack offset %d to %%r%d\n\n", varName, examinedSpill->stackOffset, destinationRegister);
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

void restoreRegisterStates(struct Stack *savedStateStack, struct Stack *activeList, struct Stack *inactiveList, struct Stack *spilledList)
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
    for (int i = 0; i < 4; i++)
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
    for (int i = 0; i < 4; i++)
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

    // struct Register **desiredState = malloc(12 * sizeof(struct Register *));

    /*
    struct Stack *intermediateStack = Stack_new(); // intermediate stack to aid in removal of elements buried within stacks

    // examine every currently active variable
    while (activeList->size > 0)
    {
        struct Register *currentAllocation = Stack_pop(activeList);
        // scan through the saved active list (desired state)
        for (int i = 0; i < savedActiveList->size; i++)
        {
            struct Register *desiredRegister = activeList->data[i];
            if (desiredRegister->index == currentAllocation->index)
            {
                // if variable is in the correct place, keep this allocation, push to intermediate stack
                if (!strcmp(desiredRegister->lifetime->variable, currentAllocation->lifetime->variable))
                {
                    printf("[%s] (%%r%d) Is in the right place!\n", desiredRegister->lifetime->variable, desiredRegister->index);
                    Stack_push(intermediateStack, currentAllocation);
                    break;
                }
                // if this variable isn't in the right place, push the register's contents to the stack to preserve the value
                // add the newly-freed register to inactive list
                else
                {
                    printf("[%s] is in the wrong place - need to push %%r%d\n", desiredRegister->lifetime->variable, desiredRegister->index);
                    Stack_push(relocationStack, currentAllocation->lifetime);
                    Stack_push(inactiveList, currentAllocation);
                }
                break;
            }
        }
    }

    while (intermediateStack->size > 0)
        Stack_push(activeList, Stack_pop(intermediateStack));


    printf("Live registers only contain correct values:\n");
    printf("%d live registers, %d free registers, %d vars on stack\n\n", activeList->size, inactiveList->size, spilledList->size);

    sortByRegisterNumber((struct Register **)savedActiveList->data, savedActiveList->size);
    while (savedActiveList->size > 0)
    {
        struct Register *desiredRegister = Stack_pop(savedActiveList);
        printf("need %%r%d to contain: %s\n", desiredRegister->index, desiredRegister->lifetime->variable);
    }
    */
    exit(2);
}

void resetRegisterStates(struct Stack *savedStateStack, struct Stack *activeList, struct Stack *inactiveList, struct Stack *spilledList)
{
    printf("reset register states\n\n");
}

// find a variable which is to be assigned if in an active register
// if the variable is spilled, unspill it
int findOrPlaceAssignedVariable(struct Stack *activeList, struct Stack *inactiveList, struct Stack *spilledList, char *varName)
{
    for (int i = 0; i < activeList->size; i++)
        if (!strcmp(((struct Register *)activeList->data[i])->lifetime->variable, varName))
            return ((struct Register *)activeList->data[i])->index;

    return unSpillVariable(activeList, inactiveList, spilledList, varName);
}

// find which register a variable lives in, returning -1 if not currently active in a register
int findActiveVariable(struct Stack *activeList, char *varName)
{
    for (int i = 0; i < activeList->size; i++)
        if (!strcmp(((struct Register *)activeList->data[i])->lifetime->variable, varName))
            return ((struct Register *)activeList->data[i])->index;

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

void findLifetimes(struct symbolTable *table, FILE *outFile)
{
    struct LinkedList *lifetimes = LinkedList_new();
    int TACIndex = 0;
    for (int i = 0; i < table->size; i++)
    {
        if (table->entries[i]->type == e_argument)
        {
            // struct variableEntry *theArgument = table->entries[i]->entry;
            updateOrInsertLifetime(lifetimes, table->entries[i]->name, 0);
        }
    }
    for (struct TACLine *runner = table->codeBlock; runner != NULL; runner = runner->nextLine)
    {
        for (int i = 0; i < 3; i++)
        {
            switch (runner->operandTypes[i])
            {
            case vt_var:
            case vt_temp:
                updateOrInsertLifetime(lifetimes, runner->operands[i], TACIndex);
                break;
            default:
            }
        }

        TACIndex++;
    }

    // convert the linked list of lifetimes to an array for easy indexing
    struct Lifetime **lifetimeIntervals = malloc(lifetimes->size * sizeof(struct Lifetime *));
    int lifetimeCount = 0;
    for (struct LinkedListNode *runner = lifetimes->head; runner != NULL; runner = runner->next)
        lifetimeIntervals[lifetimeCount++] = runner->data;

    // print out the variable lifetimes as a horizontal graph
    printLifetimesGraph(lifetimes);

    struct Stack *inactiveList = Stack_new(); // registers not currently in use
    struct Stack *activeList = Stack_new();   // registers containing variables
    struct Stack *spilledList = Stack_new();  // list of live variables which have been spilled to stack
    int maxSpillSpace = 0;

    // create all register instances, add to inactive list
    for (int i = 0; i < 4; i++)
    {
        struct Register *wip = malloc(sizeof(struct Register));
        wip->lifetime = NULL;
        wip->index = 3 - i;
        Stack_push(inactiveList, wip);
    }

    // iterate each TAC line
    TACIndex = 0;
    int currentLifetimeIndex = 0;
    struct Stack *savedStateStack = Stack_new();
    for (struct TACLine *runner = table->codeBlock; runner != NULL; runner = runner->nextLine)
    {
        // printf("TAC INDEX %d\n", TACIndex);
        printTACLine(runner);
        printf("\n");

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

        // for all previously unseen lifetimes starting before or on this TAC index
        while (currentLifetimeIndex < lifetimeCount && lifetimeIntervals[currentLifetimeIndex]->start <= TACIndex)
        {
            // grab the lifetime
            struct Lifetime *this = lifetimeIntervals[currentLifetimeIndex++];
            // printf("INTRODUCING:\n");
            // printf("\t%s: %x-%x\n", this->variable, this->start, this->end);

            // spill a register if one isn't available
            if (inactiveList->size == 0)
            {
                // printf("need to spill\n");
                spillRegister(activeList, inactiveList, spilledList);

                // Stack_push(spilledList, thisSpill);
                if (spilledList->size * 2 > maxSpillSpace)
                    maxSpillSpace = spilledList->size * 2;
            }

            // assign this variable to the next free register
            int destinationIndex = assignRegister(activeList, inactiveList, this);

            // place the value into the register if this is an argument (value starts on the stack)
            if (this->variable[0] != '.' && symbolTableLookup(table, this->variable)->type == e_argument)
            {
                printf("need to place argument [%s] in newly assigned register %d\n", this->variable, destinationIndex);
            }
        }
        int destinationRegister;
        int firstSourceRegister;
        int secondSourceRegister;
        switch (runner->operation)
        {
        case tt_assign:
            destinationRegister = findOrPlaceAssignedVariable(activeList, inactiveList, spilledList, runner->operands[0]);
            if (runner->operandTypes[1] == vt_literal)
            {
                fprintf(outFile, "mov %%r%d, %s", destinationRegister, runner->operands[1]);
            }
            else
            {
                firstSourceRegister = findActiveVariable(activeList, runner->operands[1]);
                if (firstSourceRegister != -1)
                {
                    fprintf(outFile, "mov %%r%d, %%r%d", destinationRegister, firstSourceRegister);
                }
                else
                {
                    fprintf(outFile, "mov %%r%d, %d(%%rbp)", destinationRegister, findSpilledVariable(spilledList, runner->operands[1]));
                }
            }
            fprintf(outFile, ";%s\n", runner->operands[0]);

            break;

        case tt_add:
        case tt_subtract:
            destinationRegister = findOrPlaceAssignedVariable(activeList, inactiveList, spilledList, runner->operands[0]);

            if (strcmp(runner->operands[0], runner->operands[1]))
                firstSourceRegister = findActiveVariable(activeList, runner->operands[1]);
            else
                firstSourceRegister = destinationRegister;

            if (strcmp(runner->operands[0], runner->operands[2]))
                secondSourceRegister = findActiveVariable(activeList, runner->operands[2]);
            else
                secondSourceRegister = destinationRegister;

            printf("OPERAND INDICES ARE %d %d %d\n", destinationRegister, firstSourceRegister, secondSourceRegister);

            // both source operands are variables in registers
            if (firstSourceRegister != -1 && secondSourceRegister != -1)
            {
                if (firstSourceRegister == secondSourceRegister)
                {
                    if (firstSourceRegister == destinationRegister)
                        fprintf(outFile, "%s %%r%d, %%r%d", getAsmOp(runner->operation), destinationRegister, destinationRegister);
                    else
                    {
                        fprintf(outFile, "mov %%r%d, %%r%d\n", destinationRegister, firstSourceRegister);
                        fprintf(outFile, "%s %%r%d, %%r%d", getAsmOp(runner->operation), secondSourceRegister, secondSourceRegister);
                    }
                }
                else
                {
                    fprintf(outFile, "mov %%r%d, %%r%d\n", destinationRegister, firstSourceRegister);
                    fprintf(outFile, "%s %%r%d, %%r%d", getAsmOp(runner->operation), destinationRegister, secondSourceRegister);
                }
            }
            else
            {
                // first source exists in register, second is spilled or a literal
                if (firstSourceRegister != -1 && secondSourceRegister == -1)
                {
                    fprintf(outFile, "mov %%r%d, %%r%d\n", destinationRegister, firstSourceRegister);
                    if (runner->operandTypes[2] == vt_literal)
                    {
                        fprintf(outFile, "%s %%r%d, %s", getAsmOp(runner->operation), destinationRegister, runner->operands[2]);
                    }
                    else
                    {
                        fprintf(outFile, "%s %%r%d, %d(%%rbp)", getAsmOp(runner->operation), destinationRegister, findSpilledVariable(spilledList, runner->operands[2]));
                    }
                }
                // second source exists in register, first is spilled
                else if (firstSourceRegister == -1 && secondSourceRegister != -1)
                {
                    if (runner->operandTypes[1] == vt_literal)
                    {
                        fprintf(outFile, "mov %%r%d, %s\n", destinationRegister, runner->operands[1]);
                    }
                    else
                    {
                        fprintf(outFile, "mov %%r%d, %d(%%rbp)\n", destinationRegister, findSpilledVariable(spilledList, runner->operands[1]));
                    }

                    fprintf(outFile, "%s %%r%d, %%r%d", getAsmOp(runner->operation), destinationRegister, secondSourceRegister);
                }
                // both sources are spilled - will break if both are literals but this should be checked earlier
                else
                {
                    fprintf(outFile, "mov %%r%d, %d(%%rbp)\n", destinationRegister, findSpilledVariable(spilledList, runner->operands[1]));
                    fprintf(outFile, "%s %%r%d, %d(%%rbp)", getAsmOp(runner->operation), destinationRegister, findSpilledVariable(spilledList, runner->operands[2]));
                }
            }
            fprintf(outFile, ";%s\n", runner->operands[0]);
            fflush(outFile);
            // printf("destination %%r%d\n", assignedRegister);
            break;

        case tt_pushstate:
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

            break;

        case tt_restorestate:
            restoreRegisterStates(savedStateStack, activeList, inactiveList, spilledList);
            break;

        case tt_resetstate:
            resetRegisterStates(savedStateStack, activeList, inactiveList, spilledList);
            break;

        default:
        }

        printf("\nFree registers after this step:");
        for (int i = 0; i < inactiveList->size; i++)
        {
            printf("%%r%d, ", ((struct Register *)inactiveList->data[i])->index);
        }
        printf("\n");

        /*
        printf("Allocated registers after this step:");
        for (int i = 0; i < activeList->size; i++)
        {
            printf("%%r%d, ", ((struct Register *)activeList->data[i])->index);
        }
        printf("\n");
        */
        printf("Variables in registers after this step:");
        for (int i = 0; i < activeList->size; i++)
        {
            printf("[%s], ", ((struct Register *)activeList->data[i])->lifetime->variable);
        }
        printf("\n");

        printf("Spilled variables after this step:");
        for (int i = 0; i < spilledList->size; i++)
        {
            struct SpilledRegister *sr = spilledList->data[i];
            if (sr->occupied)
            {
                printf("[%s], ", sr->lifetime->variable);
            }
            else
            {
                printf("[ ],");
            }
        }
        printf("\n\n");
        printf("done\n\n");

        TACIndex++;
    }

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
        free(lifetimeIntervals[i]);

    free(lifetimeIntervals);
}
