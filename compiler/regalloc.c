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

/*
void unSpillVariable(struct Stack *activeList, struct Stack *inactiveList, struct LinkedList *spilledList, char *varName)
{
    if (inactiveList->size == 0)
    {
        spillRegister(activeList, inactiveList, spilledList);
    }
    struct SpilledRegister *toUnspill = LinkedList_find(spilledList, &compareLifetimes, varName);
    LinkedList_delete(spilledList, &compareLifetimes, varName);

    assignRegister(activeList, inactiveList, toUnspill->lifetime);
}
*/
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

    // pull the saved states off the stack, re-duplicate and re-place them on the stack
    struct Stack *savedSpilledList = Stack_pop(savedStateStack);
    struct Stack *savedInactiveList = Stack_pop(savedStateStack);
    struct Stack *savedActiveList = Stack_pop(savedStateStack);
    Stack_push(savedStateStack, Stack_duplicate(savedSpilledList));
    Stack_push(savedStateStack, Stack_duplicate(savedInactiveList));
    Stack_push(savedStateStack, Stack_duplicate(savedActiveList));

    // variables which have literally been pushed to the stack to free their registers
    struct Stack *relocationStack = Stack_new();

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

}

void resetRegisterStates(struct Stack *savedStateStack, struct Stack *activeList, struct Stack *inactiveList, struct Stack *spilledList)
{
    printf("reset register states\n\n");
}

void findLifetimes(struct symbolTable *table)
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
    for (int i = 0; i < 2; i++)
    {
        struct Register *wip = malloc(sizeof(struct Register));
        wip->lifetime = NULL;
        wip->index = 1 - i;
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
            printf("%s, ", ((struct Register *)activeList->data[i])->lifetime->variable);
        }
        printf("\n");

        printf("Spilled variables after this step:");
        for (int i = 0; i < spilledList->size; i++)
        {
            struct SpilledRegister *sr = spilledList->data[i];
            if (sr->occupied)
            {
                printf("%s, ", sr->lifetime->variable);
            }
            else
            {
                printf("[ ],");
            }
        }
        printf("\n\n");

        switch (runner->operation)
        {
        case tt_add:
        case tt_subtract:
        case tt_assign:
            // int assignedRegister = 123;
            for (int i = 0; i < activeList->size; i++)
            {
                if (!strcmp(((struct Register *)activeList->data[i])->lifetime->variable, runner->operands[0]))
                {
                    // assignedRegister = i;
                    break;
                }
            }
            // printf("destination %%r%d\n", assignedRegister);
            break;

        case tt_pushstate:
            printf("PUSH STATE\n");
            Stack_push(savedStateStack, Stack_duplicate(activeList));
            Stack_push(savedStateStack, Stack_duplicate(inactiveList));
            Stack_push(savedStateStack, Stack_duplicate(spilledList));
            break;

        case tt_restorestate:
            restoreRegisterStates(savedStateStack, activeList, inactiveList, spilledList);
            break;

        case tt_resetstate:
            resetRegisterStates(savedStateStack, activeList, inactiveList, spilledList);
            break;

        default:
        }
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
