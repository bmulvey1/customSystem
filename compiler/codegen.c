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
    printf("\n");
    printLifetimesGraph(lifetimes);
    printf("Generate code for function %s", table->name);

    int *registerLoads = malloc((REGISTER_COUNT + 1) * sizeof(int)); // count of TAC steps with different numbers of registers free
    for (int i = 0; i < REGISTER_COUNT; i++)
        registerLoads[i] = 0;

    int *stackLoads = malloc((lifetimeCount + 1) * sizeof(int)); // count of TAC steps with different numbers of variables spilled
    for (int i = 0; i <= lifetimeCount; i++)
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
        wip->index = (REGISTER_COUNT - 1) - i;
        Stack_push(inactiveList, wip);
    }

    // iterate each TAC line
    int TACIndex = 0;
    int currentLifetimeIndex = 0;
    char *outputLine;
    char *finalOutputLine;
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
        outputLine = malloc(64);
        sprintf(outputLine, "%s_%d:", table->name, thisBlock->labelNum);
        ASMblock_append(outputBlock, outputLine);

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
            outputLine = malloc(32);
            sprintf(outputLine, ";introduce var %s to %%r%d", this->variable, destinationIndex);
            ASMblock_append(outputBlock, outputLine);
            // place the value into the register if this is an argument (value starts on the stack)
            if (this->variable[0] != '.' && symbolTableLookup(table, this->variable)->type == e_argument)
            {
                outputLine = malloc(20);
                struct variableEntry *theArgument = symbolTableLookup(table, this->variable)->entry;
                sprintf(outputLine, "mov %%r%d, %d(%%bp)", destinationIndex, theArgument->stackOffset);
                finalOutputLine = malloc(128);
                sprintf(finalOutputLine, "%s ;place argument %s", outputLine, this->variable);
                free(outputLine);
                ASMblock_append(outputBlock, finalOutputLine);
            }
        }

        // iterate all TAC lines in the current Basic Block
        struct LinkedListNode *TACRunner = thisBlock->TACList->head;
        while (TACRunner != NULL)
        {
            struct TACLine *currentTAC = TACRunner->data;
            TACIndex = currentTAC->index;
            expireOldIntervals(activeList, inactiveList, spilledList, TACIndex);
            // printTACLine(currentTAC);
            // printf("\n");

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

            char *printedTAC;

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
                        outputLine = malloc(32);
                        sprintf(outputLine, "\t;introduce var %s to %%r%d", this->variable, destinationIndex);
                        ASMblock_append(outputBlock, outputLine);
                        // place the value into the register if this is an argument (value starts on the stack)
                        if (this->variable[0] != '.' && symbolTableLookup(table, this->variable)->type == e_argument)
                        {
                            outputLine = malloc(20);
                            struct variableEntry *theArgument = symbolTableLookup(table, this->variable)->entry;
                            sprintf(outputLine, "mov %%r%d, %d(%%bp)", destinationIndex, theArgument->stackOffset);
                            finalOutputLine = malloc(128);
                            sprintf(finalOutputLine, "%s ;place argument %s", outputLine, this->variable);
                            free(outputLine);
                            ASMblock_append(outputBlock, finalOutputLine);
                        }
                    }
                }

            default:
                break;
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
            case tt_mul:
            case tt_div:
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
                        if (currentTAC->operandTypes[2] == vt_literal)
                        {
                            sprintf(outputLine, "%s %%r%d, $%s", getAsmOp(currentTAC->operation), destinationRegister, currentTAC->operands[2]);
                        }
                        else
                        {
                            sprintf(outputLine, "%s %%r%d, %d(%%bp)", getAsmOp(currentTAC->operation), destinationRegister, findSpilledVariable(spilledList, currentTAC->operands[2]));
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

            case tt_memr_1:
            {
                finalOutputLine = malloc(48);
                int destinationIndex = findOrPlaceAssignedVariable(activeList, inactiveList, spilledList, currentTAC->operands[0], outputBlock, table);
                int sourceIndex = findOrPlaceAssignedVariable(activeList, inactiveList, spilledList, currentTAC->operands[1], outputBlock, table);

                outputLine = malloc(16);
                sprintf(outputLine, "mov %%r%d, (%%r%d)", destinationIndex, sourceIndex);

                printedTAC = sPrintTACLine(currentTAC);
                sprintf(finalOutputLine, "%s;%s", outputLine, printedTAC);
                free(outputLine);
                free(printedTAC);

                ASMblock_append(outputBlock, finalOutputLine);
            }
            break;

            case tt_memw_1:
            {
                finalOutputLine = malloc(48);
                int destinationIndex = findOrPlaceAssignedVariable(activeList, inactiveList, spilledList, currentTAC->operands[0], outputBlock, table);
                int sourceIndex = findOrPlaceAssignedVariable(activeList, inactiveList, spilledList, currentTAC->operands[1], outputBlock, table);

                outputLine = malloc(16);
                sprintf(outputLine, "mov (%%r%d), %%r%d", destinationIndex, sourceIndex);

                printedTAC = sPrintTACLine(currentTAC);
                sprintf(finalOutputLine, "%s;%s", outputLine, printedTAC);
                free(outputLine);
                free(printedTAC);

                ASMblock_append(outputBlock, finalOutputLine);
            }
            break;

            case tt_push:
            {
                int sourceRegister = findActiveVariable(activeList, currentTAC->operands[0]);
                if (sourceRegister == -1)
                {
                    sourceRegister = unSpillVariable(activeList, inactiveList, spilledList, currentTAC->operands[0], outputBlock, table);
                }
                outputLine = malloc(16);
                sprintf(outputLine, "push %%r%d", sourceRegister);
                ASMblock_append(outputBlock, outputLine);
            }
            break;

            case tt_call:
            {
                outputLine = malloc(32);
                sprintf(outputLine, "call %s", currentTAC->operands[1]);
                ASMblock_append(outputBlock, outputLine);

                // the call returns a value
                if (currentTAC->operands[0] != NULL)
                {
                    outputLine = malloc(32);
                    int destinationRegister = findActiveVariable(activeList, currentTAC->operands[0]);
                    if (destinationRegister != -1)
                    {
                        sprintf(outputLine, "mov %%r%d, %%rr", destinationRegister);
                    }
                    else
                    {
                        int destinationOffset = findSpilledVariable(spilledList, currentTAC->operands[0]);
                        sprintf(outputLine, "mov %d(%%sp), %%rr", destinationOffset);
                    }
                    finalOutputLine = malloc(64);
                    printedTAC = sPrintTACLine(currentTAC);
                    sprintf(finalOutputLine, "%s;%s", outputLine, printedTAC);
                    free(outputLine);
                    free(printedTAC);
                    ASMblock_append(outputBlock, finalOutputLine);
                }
            }
            break;

            case tt_cmp:
            {
                outputLine = malloc(20);
                finalOutputLine = malloc(64);
                firstSourceRegister = findActiveVariable(activeList, currentTAC->operands[1]);

                secondSourceRegister = findActiveVariable(activeList, currentTAC->operands[2]);

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
                            sprintf(outputLine, "cmp %%r%d, $%s", firstSourceRegister, currentTAC->operands[2]);
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
                    outputLine = malloc(24);
                    sprintf(outputLine, "mov %%rr, $%s", currentTAC->operands[0]);
                }
                break;

                case vt_var:
                case vt_temp:
                {
                    int sourceRegister = findActiveVariable(activeList, currentTAC->operands[0]);
                    if (sourceRegister != -1)
                    {
                        outputLine = malloc(16);
                        sprintf(outputLine, "mov %%rr, %%r%d", sourceRegister);
                    }
                    else
                    {
                        outputLine = malloc(24);
                        sprintf(outputLine, "mov %%rr, %d(%%bp)", findSpilledVariable(spilledList, currentTAC->operands[0]));
                    }
                }
                break;

                default:
                    perror("unexpected type in return TAC!\n");
                    exit(1);
                }
                ASMblock_append(outputBlock, outputLine);
                outputLine = malloc(32);
                sprintf(outputLine, "jmp %s_done", table->name);
                ASMblock_append(outputBlock, outputLine);

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
                // copy the line because it lives in dictionary
                // freeing the operand itself in freeASM() will result in double free when freeing dictionary later
                outputLine = malloc(strlen(currentTAC->operands[0]) + 1);
                strcpy(outputLine, currentTAC->operands[0]);
                ASMblock_append(outputBlock, outputLine);
                break;

            default:
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
    
    outputLine = malloc(32);
    sprintf(outputLine, "%s_done:", table->name);
    ASMblock_append(outputBlock, outputLine);

    for (int i = 0; i < REGISTER_COUNT; i++)
    {
        if (touchedRegisters[i])
        {
            outputLine = malloc(16);
            sprintf(outputLine, "push %%r%d", i);
            ASMblock_prepend(outputBlock, outputLine);
            outputLine = malloc(16);
            sprintf(outputLine, "pop %%r%d", i);
            ASMblock_append(outputBlock, outputLine);
        }
    }

    if (maxSpillSpace > 0)
    {
        outputLine = malloc(20);
        sprintf(outputLine, "sub %%sp, $%d", maxSpillSpace);
        ASMblock_prepend(outputBlock, outputLine);
    }

    outputLine = malloc(64);
    sprintf(outputLine, "%s:", table->name);
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

    if (savedStateStack->size > 0)
    {
        perror("saved state stack not empty after code generation!");
        exit(1);
    }
    Stack_free(savedStateStack);
    return outputBlock;
}
