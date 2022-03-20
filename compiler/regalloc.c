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

void sortByRegisterNumber(struct Register **list, int size)
{
    for (int i = 0; i < size; i++)
    {
        for (int j = 0; j < size - i - 1; j++)
        {
            if (list[j]->index > list[j + 1]->index)
            {
                struct Register *temp = list[j];
                list[j] = list[j + 1];
                list[j + 1] = temp;
            }
        }
    }
}

void expireOldIntervals(struct Stack *activeList, struct Stack *inactiveList, int TACIndex)
{
    struct Stack *intermediateStack = newStack();
    while (activeList->size > 0)
    {
        struct Register *poppedRegister = StackPop(activeList);
        if (poppedRegister->lifetime->end < TACIndex)
        {
            printf("Expire %s\n", poppedRegister->lifetime->variable);
            StackPush(inactiveList, poppedRegister);
        }
        else
        {
            StackPush(intermediateStack, poppedRegister);
        }
    }

    while (intermediateStack->size > 0)
        StackPush(activeList, StackPop(intermediateStack));

    freeStack(intermediateStack);
}

void spillRegister(struct Stack *activeList, struct Stack *inactiveList, struct LinkedList *spilledList, int *maxSpillSpace)
{
    struct Stack *intermediate = newStack();
    while (activeList->size > 0)
        StackPush(intermediate, StackPop(activeList));

    struct Register *victim = StackPop(intermediate);
    struct SpilledRegister *thisSpill = malloc(sizeof(struct SpilledRegister));
    thisSpill->lifetime = victim->lifetime;
    thisSpill->lastUsed = victim->lastUsed;
    thisSpill->stackOffset = (spilledList->size * -2) - 2;
    printf("spill variable %s to stack (offset of %d)\n", victim->lifetime->variable, thisSpill->stackOffset);
    LinkedList_insert(spilledList, thisSpill);

    // StackPush(spilledList, thisSpill);
    if (spilledList->size * 2 > *maxSpillSpace)
        *maxSpillSpace = spilledList->size * 2;

    StackPush(inactiveList, victim);

    while (intermediate->size > 0)
        StackPush(activeList, StackPop(intermediate));

    freeStack(intermediate);
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

    struct Lifetime **lifetimeIntervals = malloc(lifetimes->size * sizeof(struct Lifetime *));
    int lifetimeCount = 0;
    for (struct LinkedListNode *runner = lifetimes->head; runner != NULL; runner = runner->next)
    {
        struct Lifetime *this = runner->data;
        printf("%s: %x-%x\n", this->variable, this->start, this->end);
        lifetimeIntervals[lifetimeCount++] = this;
    }
    printf("\n");

    struct Stack *inactiveList = newStack();
    struct Stack *activeList = newStack();
    struct LinkedList *spilledList = LinkedList_new();
    int maxSpillSpace = 0;
    // struct Stack *stackList = newStack();
    for (int i = 0; i < 2; i++)
    {
        struct Register *wip = malloc(sizeof(struct Register));
        wip->lifetime = NULL;
        wip->index = 1 - i;
        StackPush(inactiveList, wip);
    }
    TACIndex = 0;
    int currentLifetimeIndex = 0;
    for (struct TACLine *runner = table->codeBlock; runner != NULL; runner = runner->nextLine)
    {
        printf("TAC INDEX %d\n", TACIndex);
        printTACLine(runner);
        printf("\n");
        expireOldIntervals(activeList, inactiveList, TACIndex);
        printf("Active lifetimes:\n");
        for (int i = 0; i < activeList->size; i++)
        {
            struct Register *thisReg = activeList->data[i];
            // printf("\t%%r%d --- %s: %d-%d (last used %d ago)\n", thisReg->index, thisReg->lifetime->variable, thisReg->lifetime->start, thisReg->lifetime->end, thisReg->lastUsed);
            printf("\t%%r%d: %s (last used %d ago)\n", thisReg->index, thisReg->lifetime->variable, thisReg->lastUsed);
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
        printf("Free registers:");
        for (int i = 0; i < inactiveList->size; i++)
        {
            printf("%%r%d, ", ((struct Register *)inactiveList->data[inactiveList->size - i - 1])->index);
        }
        printf("\n\nSpilled on stack:\n");

        for (struct LinkedListNode *runner = spilledList->head; runner != NULL; runner = runner->next)
        {
            struct SpilledRegister *thisSpill = runner->data;
            printf("%s (offset %d)\n", thisSpill->lifetime->variable, thisSpill->stackOffset);
        }
        printf("\n\n");

        while (currentLifetimeIndex < lifetimeCount && lifetimeIntervals[currentLifetimeIndex]->start <= TACIndex)
        {
            struct Lifetime *this = lifetimeIntervals[currentLifetimeIndex];
            printf("INTRODUCING:::\n\n");
            printf("\t%s: %x-%x\n", this->variable, this->start, this->end);
            if (inactiveList->size == 0)
            {
                // printf("need to spill\n");
                spillRegister(activeList, inactiveList, spilledList, &maxSpillSpace);
            }
            // printf("Next available register is %d\n", ((struct Register *)StackPeek(inactiveList))->index);
            struct Register *assignedRegister = StackPop(inactiveList);
            printf("assign register %d for %s\n", assignedRegister->index, this->variable);
            assignedRegister->lifetime = this;

            if (symbolTableLookup(table, this->variable)->type == e_argument)
            {
                printf("need to place argument into register %d\n", assignedRegister->index);
            }

            StackPush(activeList, assignedRegister);
            if (inactiveList->size > 0)
                sortByEndPoint((struct Register **)activeList->data, activeList->size);

            sortByRegisterNumber((struct Register **)inactiveList->data, inactiveList->size);
            // activeList[activeRegisters++]->lifetime = this;
            //  sortByStartPoint(activeList,)

            currentLifetimeIndex++;
        }
        printf("\n");
        switch (runner->operation)
        {
        case tt_add:
        case tt_subtract:
        case tt_assign:
            int assignedRegister = 123;
            for (int i = 0; i < activeList->size; i++)
            {
                if (!strcmp(((struct Register *)activeList->data[i])->lifetime->variable, runner->operands[0]))
                {
                    assignedRegister = i;
                    break;
                }
            }
            printf("destination %%r%d\n", assignedRegister);
            break;

        default:
        }
        printf("\n");

        TACIndex++;
    }

    printf("Active lifetimes:\n");
    for (int i = 0; i < activeList->size; i++)
    {
        struct Register *thisReg = activeList->data[i];
        // printf("\t%%r%d --- %s: %d-%d (last used %d ago)\n", thisReg->index, thisReg->lifetime->variable, thisReg->lifetime->start, thisReg->lifetime->end, thisReg->lastUsed);
        printf("\t%%r%d: %s (last used %d ago)\n", thisReg->index, thisReg->lifetime->variable, thisReg->lastUsed);
        thisReg->lastUsed++;
    }
    printf("Free registers:");
    for (int i = 0; i < inactiveList->size; i++)
    {
        printf("%%r%d, ", ((struct Register *)inactiveList->data[inactiveList->size - i - 1])->index);
    }
    printf("\n\nSpilled on stack:\n");

    for (struct LinkedListNode *runner = spilledList->head; runner != NULL; runner = runner->next)
    {
        struct SpilledRegister *thisSpill = runner->data;
        printf("%s (offset %d)\n", thisSpill->lifetime->variable, thisSpill->stackOffset);
    }
    printf("\n\n");

    while (activeList->size > 0)
        free(StackPop(activeList));

    freeStack(activeList);

    while (inactiveList->size > 0)
        free(StackPop(inactiveList));

    freeStack(inactiveList);

    LinkedList_free(lifetimes, 0);

    for (int i = 0; i < lifetimeCount; i++)
        free(lifetimeIntervals[i]);

    free(lifetimeIntervals);
}
