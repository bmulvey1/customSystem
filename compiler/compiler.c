#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "tac.h"
#include "symtab.h"
#include "util.h"
#include "linearizer.h"
#include "state.h"
#include "asm.h"

/*
 * setup code for linked list containing variable lifetime info
 * may belong in its own file, depends on how beginning code generation works out
 */
struct Lifetime
{
    int start, end;
    char *variable;
    struct Lifetime *next;
};

struct Lifetime *newLifetime(char *variable, int start)
{
    struct Lifetime *wip = malloc(sizeof(struct Lifetime));
    wip->variable = variable;
    wip->start = start;
    wip->end = start;
    wip->next = NULL;
    return wip;
}

void freeLifetime(struct Lifetime *it)
{
    while (it != NULL)
    {
        struct Lifetime *old = it;
        it = it->next;
        free(old);
    }
}

/*
 * Given a symbol table entry for a function, calculate lifetime for variables defined within
 * Also enforces some basic error checking (use before declare / use before assign)
 *
 * SymTab entries for variables containing their own lifetimes may be redundant since
 * findLifetimes() will also re-find lifetimes for these variables
 */

void checkVariableLifetimes(struct symbolTable *table)
{
    int lineIndex = 0;
    // iterate all lines of TAC in the function's codeblock
    for (struct TACLine *t = table->codeBlock; t != NULL; t = t->nextLine)
    {
        switch (t->operation)
        {
            // jmp's have no bearing on variable lifetimes
        case tt_jg:
        case tt_jge:
        case tt_jl:
        case tt_jle:
        case tt_je:
        case tt_jne:
        case tt_jmp:

        default:

            // only examine RHS operands if they are variables
            struct symTabEntry *varEntry;
            if (t->operandTypes[2] == vt_var)
            {
                // attempt to get the variable's entry from the table
                varEntry = symbolTableLookup(table, t->operands[2]);

                // if we get nothing back, we have a use before declare
                if (varEntry == NULL)
                {
                    printf("Error: use of undeclared variable %s\n", t->operands[2]);
                    exit(1);
                }

                switch (varEntry->type)
                {
                case e_argument:
                    // arguments can be used anywhere, so ignore
                    break;

                case e_variable:
                    // if a variable on the RHS hasn't been assigned yet, we have a use before asssign
                    if (!((struct variableEntry *)varEntry->entry)->isAssigned)
                    {
                        printf("Error: use of variable [%s] before assignment\n", varEntry->name);
                        exit(1);
                    }
                    break;

                // won't hit this at the moment, only captures e_function
                default:

                    break;
                }
            }

            // perform the same checks as for operand index 2 on index 1

            if (t->operandTypes[1] == vt_var)
            {
                varEntry = symbolTableLookup(table, t->operands[1]);
                if (varEntry == NULL)
                {
                    printf("Error: use of undeclared variable %s\n", t->operands[1]);
                    exit(1);
                }
                switch (varEntry->type)
                {
                case e_argument:
                    // arguments can be used anywhere, so ignore
                    break;

                case e_variable:
                    if (!((struct variableEntry *)varEntry->entry)->isAssigned)
                    {
                        printf("Error: use of variable [%s] before assignment\n", varEntry->name);
                        exit(1);
                    }
                    break;

                default:

                    break;
                }
            }

            // look at operand index 0 (what is being assigned to)
            // we only care about it if it's an explicitly defined variable (not a temp)
            if (t->operandTypes[0] == vt_var)
            {
                // do a lookup in the symbol table for a variable with this name
                struct symTabEntry *assignedVar = symbolTableLookup(table, t->operands[0]);

                // catch assign before initialize
                if (assignedVar == NULL)
                {
                    printf("Error: assignment to uninitialized variable %s\n", t->operands[0]);
                    exit(1);
                }

                switch (assignedVar->type)
                {
                case e_variable:
                    // grab the variable entry itself
                    struct variableEntry *theVar = assignedVar->entry;
                    // if it hasn't already been assigned
                    if (!theVar->isAssigned)
                    {
                        // set the assigned flag and record the start of its lifetime
                        theVar->isAssigned = 1;
                    }
                    break;

                case e_argument:
                    // just ignore arguments
                    break;

                default:
                    printf("Error - assignment to something that's not an argument or variable\n");
                    exit(1);
                }

                // regardless of whether or not this variable's lifetime started at this line
                // its lifetime can end no sooner than this index
            }
        }

        lineIndex++;
    }
}

struct Lifetime *findLifetimes(struct symbolTable *table)
{
    // look at explicitly defined variables and do error checking
    checkVariableLifetimes(table);

    // make a linked list of variable lifetimes
    struct Lifetime *ltList = NULL;
    struct Lifetime *ltTail = NULL;

    struct Stack *doBlockLifetimes = newStack();

    // look at all lines in the TAC block for this function, keeping track of our index
    int lineIndex = 0;
    for (struct TACLine *line = table->codeBlock; line != NULL; line = line->nextLine)
    {
        switch (line->operation)
        {
        case tt_label:
        case tt_jg:
        case tt_jge:
        case tt_jl:
        case tt_jle:
        case tt_je:
        case tt_jne:
        case tt_jmp:
            break;

        default:

            // iterate all operands in the line
            for (int i = 0; i < 3; i++)
            {
                char *varName = line->operands[i];

                // ignore if null
                if (line->operands[i] == NULL)
                    continue;

                // if operand is a literal, ignore
                // (only possible for operands index 1 and 2 which are on the RHS of the assignment)
                if (line->operandTypes[i] == vt_literal)
                    continue;

                // if we made it here, we must be looking at a variable of some sort
                // search through the list of existing lifetimes
                int found = 0;
                for (struct Lifetime *runner = ltList; runner != NULL; runner = runner->next)
                {
                    // if we find a lifetime for this variable
                    if (!strcmp(varName, runner->variable))
                    {
                        // since the variable exists at this lineIndex, update its end
                        // since it can end no sooner than this line
                        runner->end = lineIndex;
                        found = 1;
                        break;
                    }
                }

                // if we scanned all existing lifetimes without finding one for this variable
                if (!found)
                {
                    // insert a lifetime for this variable (starting and ending at this step)
                    if (ltList == NULL)
                    {
                        ltList = newLifetime(varName, lineIndex);
                        if (line->operandTypes[i] == vt_var && symbolTableLookup(table, line->operands[i])->type == e_argument)
                        {
                            ltList->start = 0;
                        }
                        ltTail = ltList;
                    }
                    else
                    {
                        ltTail->next = newLifetime(varName, lineIndex);
                        if (line->operandTypes[i] == vt_var && symbolTableLookup(table, line->operands[i])->type == e_argument)
                        {
                            ltTail->next->start = 0;
                        }
                        ltTail = ltTail->next;
                    }
                }
            }
        }
        lineIndex++;
    }

    if (doBlockLifetimes->size > 0)
    {
        printf("Error - done finding lifetimes for function %s but still in 'do' block!\n", table->name);
    }
    else
    {
        freeStack(doBlockLifetimes);
    }

    return ltList;
}

int findUnallocatedRegister(struct registerState **registerStates)
{
    for (int i = 0; i < 12; i++)
        if (registerStates[i]->live == 0)
            return i;

    printf("All registers are dirty when searching for unallocated register! Need to deal with register spilling!");
    exit(1);

    return -1;
}

void modifyRegisterContents(struct registerState **registerStates, int index, char *contents)
{
    struct registerState *destinationRegister = registerStates[index];
    destinationRegister->live = 1;
    destinationRegister->touched = 1;
    if (destinationRegister->contains != NULL && !strcmp(destinationRegister->contains, contents))
    {
        destinationRegister->dirty = 1;
    }
    else
    {
        destinationRegister->contains = contents;
        destinationRegister->dirty = 0;
    }
}

void restoreRegisterStates(struct registerState **current, struct registerState **desired, struct ASMblock *outputBlock, struct symbolTable *table, int TACindex)
{
    char *outputStr;

    // int scratchIndex = findUnallocatedRegister(current);
    for (int i = 0; i < 12; i++)
    {
        // printf("Restoring %d (should contain %s)\n", i, desired[i]->contains);
        //  desired state at this index contains something - need to setup the current state to match this
        if (desired[i]->contains != NULL && desired[i]->live)
        {
            // this register currently contains something
            if (current[i]->contains != NULL)
            {
                // only need to do something if the register doesn't contain what we're looking for
                if (strcmp(current[i]->contains, desired[i]->contains) != 0)
                {
                    // iff the register contains a variable that's not a temp
                    if (current[i]->contains[0] != '.')
                    {
                        // if there's something there that needs to be saved, save it
                        if (current[i]->live || current[i]->dirty)
                        {
                            outputStr = malloc(24 * sizeof(char));
                            sprintf(outputStr, "mov %%r%d, %d(%%bp)", i, findInStack(desired[i]->contains, table));
                            ASMblock_append(outputBlock, outputStr, TACindex);
                        }
                    }
                    int existingVarIndex = findInRegisters(current, desired[i]->contains);
                    outputStr = malloc(16 * sizeof(char));
                    if (existingVarIndex == -1)
                    {
                        sprintf(outputStr, "mov %%r%d, %d(%%bp)", i, findInStack(desired[i]->contains, table));
                    }
                    else
                    {
                        sprintf(outputStr, "mov %%r%d, %%r%d", i, existingVarIndex);
                        current[existingVarIndex]->contains = NULL;
                        current[existingVarIndex]->live = 0;
                        current[existingVarIndex]->dirty = 0;
                    }
                    ASMblock_append(outputBlock, outputStr, TACindex);
                }
            }
            // the register contains nothing, we can just find the variable that needs to be there and place it
            else
            {
                int existingVarIndex = findInRegisters(current, desired[i]->contains);
                outputStr = malloc(16 * sizeof(char));
                if (existingVarIndex == -1)
                {
                    sprintf(outputStr, "mov %%r%d, %d(%%bp)", i, findInStack(desired[i]->contains, table));
                }
                else
                {
                    sprintf(outputStr, "mov %%r%d, %%r%d", i, existingVarIndex);
                    current[existingVarIndex]->contains = NULL;
                    current[existingVarIndex]->live = 0;
                    current[existingVarIndex]->dirty = 0;
                }
                ASMblock_append(outputBlock, outputStr, TACindex);
            }
        }
        // we expect this register to contain nothing
        else
        {
            // iff the register contains a variable that's not a temp
            if (current[i]->contains != NULL && current[i]->contains[0] != '.')
            {
                // if there's something there that needs to be saved, save it
                if (current[i]->live || current[i]->dirty)
                {
                    outputStr = malloc(20 * sizeof(char));
                    sprintf(outputStr, "mov %d(%%bp), %%r%d", findInStack(current[i]->contains, table), i);
                    ASMblock_append(outputBlock, outputStr, TACindex);
                }
            }
        }
        current[i]->contains = desired[i]->contains;
        current[i]->live = desired[i]->live;
        current[i]->dirty = desired[i]->dirty;
    }
}

// copy one register to a different one, freeing up the source
void relocateRegister(struct registerState **registerStates, struct ASMblock *outputBlock, int source, int dest, int TACindex)
{
    char *outputStr = malloc(14 * sizeof(char));
    sprintf(outputStr, "mov %%r%d, %%r%d", dest, source);
    ASMblock_append(outputBlock, outputStr, TACindex);
    registerStates[dest]->contains = registerStates[source]->contains;
    registerStates[dest]->live = registerStates[source]->live;
    registerStates[dest]->dirty = registerStates[source]->dirty;
    registerStates[dest]->touched = 1;

    registerStates[source]->contains = NULL;
    registerStates[source]->live = 0;
    registerStates[source]->dirty = 0;
}

// assign a value into a register, return the destination index
int generateAssignmentCode(struct TACLine *line, struct registerState **registerStates, struct ASMblock *outputBlock, struct symbolTable *table, int TACindex)
{
    int destinationIndex = findInRegisters(registerStates, line->operands[0]);
    if (destinationIndex == -1)
        destinationIndex = findUnallocatedRegister(registerStates);

    char *outputStr;
    if (line->operandTypes[1] == vt_literal)
    {
        outputStr = malloc(17 * sizeof(char));
        sprintf(outputStr, "mov %%r%d, $%s", destinationIndex, line->operands[1]);
        ASMblock_append(outputBlock, outputStr, TACindex);
    }
    else
    {
        int sourceIndex = findInRegisters(registerStates, line->operands[1]);
        // if the variable whose value we are assigning to exists
        if (sourceIndex != -1)
        {
            // if the variable can be overwritten
            if (!registerStates[sourceIndex]->live)
            {
                // if the variable being assigned to exists in a register
                // (was originally found up top by the original destinationregister assignment)
                if (findInRegisters(registerStates, line->operands[0]) != -1)
                {
                    registerStates[destinationIndex]->contains = NULL;
                    registerStates[destinationIndex]->live = 0;
                    registerStates[destinationIndex]->dirty = 0;
                }
                destinationIndex = sourceIndex;
            }
            // variable still alive - can't be overwritten!
            else
            {
                outputStr = malloc(16 * sizeof(char));
                sprintf(outputStr, "mov %%r%d, %%r%d", destinationIndex, sourceIndex);
                ASMblock_append(outputBlock, outputStr, TACindex);
            }
        }
        // the other var we are assigning the variable to doesn't exist in a register
        else
        {
            outputStr = malloc(16 * sizeof(char));
            sprintf(outputStr, "mov %%r%d, %d(%%bp)", destinationIndex, findInStack(line->operands[1], table));
            ASMblock_append(outputBlock, outputStr, TACindex);
        }
    }
    registerStates[destinationIndex]->contains = line->operands[0];
    registerStates[destinationIndex]->live = 1;
    registerStates[destinationIndex]->touched = 1;
    // check if we just assigned to a ssa temp variable or a real variable
    // if it was a real variable, track that it's now dirty
    if (line->operands[0][0] != '.')
    {
        registerStates[destinationIndex]->dirty = 1;
    }
    return destinationIndex;
}

void generateArithmeticCode(struct TACLine *line, struct registerState **registerStates, struct ASMblock *outputBlock, struct symbolTable *table, int TACindex)
{
    int destinationRegister;
    char *outputStr;

    // attempt to find the variable being assigned if it's already in registers
    // track whether the result value already exists in a register
    // this will inform where and how we attempt to place variables later
    char destinationExists = 1;
    destinationRegister = findInRegisters(registerStates, line->operands[0]);
    if (destinationRegister == -1)
    {
        // if not, we need a place for it to go
        destinationExists = 0;
        destinationRegister = findUnallocatedRegister(registerStates);
    }

    if (line->operandTypes[1] == vt_literal)
    {
        // both operands are literals
        if (line->operandTypes[2] == vt_literal)
        {
            printf("arithmetic between 2 literals - could precompute this!\n");
            exit(1);
        }
        // op1 literal, op2 var
        else
        {
            char didReorder = 0;

            // try to reorder the operands if possible
            if (line->reorderable)
            {
                // if the second operand already exists in a register
                int existingVarIndex = findInRegisters(registerStates, line->operands[2]);
                if (existingVarIndex != -1)
                {
                    // if NOT overwriting one of the operands would consume an additional register
                    if (!destinationExists)
                    {
                        // if the existing variable can't be overwritten
                        // OR destination var already exists in a register
                        if (registerStates[existingVarIndex]->live || destinationExists)
                        {
                            // if not, copy the existing variable from its register to the destination
                            outputStr = malloc(18 * sizeof(char));
                            sprintf(outputStr, "mov %%r%d, %%r%d", destinationRegister, existingVarIndex);
                            ASMblock_append(outputBlock, outputStr, TACindex);
                        }
                        // otherwise, just make that variable's register our destination!
                        else
                        {
                            destinationRegister = existingVarIndex;
                        }
                    }

                    // perform the operation on the destination with the literal
                    outputStr = malloc(18 * sizeof(char));
                    sprintf(outputStr, "%s %%r%d, $%s", getAsmOp(line->operation), destinationRegister, line->operands[1]);
                    ASMblock_append(outputBlock, outputStr, TACindex);
                    didReorder = 1;
                }
            }

            // if it wasn't possible to reorder the operands
            if (!didReorder)
            {
                char forceRelocate = 0;

                // if the operation looks something like 'i = 1 - i' we risk stomping i's value if we write directly to the destination
                int oldAssigneeLocation = destinationRegister;
                if (!strcmp(line->operands[0], line->operands[2]) && destinationExists)
                {
                    forceRelocate = 1;
                    destinationRegister = findUnallocatedRegister(registerStates);
                }

                // put the literal directly into a register
                outputStr = malloc(18 * sizeof(char));
                sprintf(outputStr, "mov %%r%d, $%s", destinationRegister, line->operands[1]);
                ASMblock_append(outputBlock, outputStr, TACindex);

                outputStr = malloc(20 * sizeof(char));
                // see if the variable is already in a register
                int existingVarIndex = findInRegisters(registerStates, line->operands[2]);

                // if not, get the second operand from the stack
                if (existingVarIndex == -1)
                {
                    sprintf(outputStr, "%s %%r%d, %d(%%bp)", getAsmOp(line->operation), destinationRegister, findInStack(line->operands[2], table));
                }
                // if yes, get the second operand from its register
                else
                {
                    sprintf(outputStr, "%s %%r%d, %%r%d", getAsmOp(line->operation), destinationRegister, existingVarIndex);
                }

                if (forceRelocate)
                {
                    registerStates[oldAssigneeLocation]->live = 0;
                    registerStates[oldAssigneeLocation]->contains = NULL;
                }

                ASMblock_append(outputBlock, outputStr, TACindex);
            }
        }
    }
    // op1 var
    else
    {
        // op1 var, op2 literal
        if (line->operandTypes[2] == vt_literal)
        {

            char didReorder = 0;

            // try to reorder the operands if possible
            if (line->reorderable)
            {
                // if the first operand already exists in a register
                int existingVarIndex = findInRegisters(registerStates, line->operands[1]);
                if (existingVarIndex != -1)
                {
                    // if (the existing variable can't be overwritten OR destination var already exists in a register)
                    // AND the variable being assigned is different from the first operand
                    if ((registerStates[existingVarIndex]->live || destinationExists) && strcmp(line->operands[0], line->operands[1]))
                    {
                        // if not, copy the existing variable from its register to the destination
                        outputStr = malloc(18 * sizeof(char));
                        sprintf(outputStr, "mov %%r%d, %%r%d", destinationRegister, existingVarIndex);
                        ASMblock_append(outputBlock, outputStr, TACindex);
                    }
                    // otherwise, just make that variable's register our destination!
                    else
                    {
                        destinationRegister = existingVarIndex;
                    }

                    // perform the operation on the destination with the literal
                    outputStr = malloc(18 * sizeof(char));
                    sprintf(outputStr, "%s %%r%d, $%s", getAsmOp(line->operation), destinationRegister, line->operands[2]);
                    ASMblock_append(outputBlock, outputStr, TACindex);
                    didReorder = 1;
                }
            }

            // if it wasn't possible to reorder the operands
            if (!didReorder)
            {
                // see if the variable is already in a register
                int existingVarIndex = findInRegisters(registerStates, line->operands[1]);
                // if not, get the first operand from the stack
                if (existingVarIndex == -1)
                {
                    outputStr = malloc(18 * sizeof(char));
                    sprintf(outputStr, "mov %%r%d, %d(%%bp)", destinationRegister, findInStack(line->operands[1], table));
                    ASMblock_append(outputBlock, outputStr, TACindex);
                }
                // if yes, get the first operand from its register
                else
                {
                    // see if the register containing the first operand can't be overwritten
                    // AND the variable being assigned is different from the first operand
                    if (registerStates[existingVarIndex]->live && strcmp(line->operands[0], line->operands[1]))
                    {
                        outputStr = malloc(18 * sizeof(char));
                        sprintf(outputStr, "mov %%r%d, %%r%d", destinationRegister, existingVarIndex);
                        ASMblock_append(outputBlock, outputStr, TACindex);
                    }
                    else
                    {
                        // otherwise, just make that variable's register our destination!
                        destinationRegister = existingVarIndex;
                    }
                }
                outputStr = malloc(18 * sizeof(char));
                sprintf(outputStr, "%s %%r%d, $%s", getAsmOp(line->operation), destinationRegister, line->operands[2]);
                ASMblock_append(outputBlock, outputStr, TACindex);
            }
        }
        // op1 var, op2 var
        else
        {
            int firstVarIndex = findInRegisters(registerStates, line->operands[1]);
            int secondVarIndex = findInRegisters(registerStates, line->operands[2]);

            // both operands live in registers
            if (firstVarIndex != -1 && secondVarIndex != -1)
            {
                char didReorder = 0;
                // see if we can reorder
                if (line->reorderable)
                {
                    // if we can't overwrite the register containing the second var,
                    // AND the destination variable doesn't exist in a register already
                    if (registerStates[secondVarIndex]->live && !destinationExists)
                    {
                        destinationRegister = secondVarIndex;
                        outputStr = malloc(18 * sizeof(char));
                        sprintf(outputStr, "%s %%r%d, %%r%d", getAsmOp(line->operation), destinationRegister, firstVarIndex);
                        ASMblock_append(outputBlock, outputStr, TACindex);
                        didReorder = 1;
                    }
                }

                // wasn't possible (or wouldn't have saved us any instructions) to reorder
                // just conduct the operation in order
                if (!didReorder)
                {
                    char forceRelocate = 0;

                    // if the operation looks something like 'i = j - i' we risk stomping i's value if we write directly to the destination
                    int oldAssigneeLocation = destinationRegister;
                    if (!strcmp(line->operands[0], line->operands[2]) && destinationExists)
                    {
                        forceRelocate = 1;
                        destinationRegister = findUnallocatedRegister(registerStates);
                    }

                    // if we can't overwrite the register containing the first var,
                    // OR the destination variable exists in a register already
                    if (registerStates[firstVarIndex]->live || destinationExists)
                    {
                        // if not, copy the existing variable from its register to the destination
                        outputStr = malloc(18 * sizeof(char));
                        sprintf(outputStr, "mov %%r%d, %%r%d", destinationRegister, firstVarIndex);
                        ASMblock_append(outputBlock, outputStr, TACindex);
                    }
                    else
                    {
                        // if yes, use this register as the destination!
                        destinationRegister = firstVarIndex;
                    }

                    // perform the operation by using the register containing the second operand
                    outputStr = malloc(18 * sizeof(char));
                    sprintf(outputStr, "%s %%r%d, %%r%d", getAsmOp(line->operation), destinationRegister, secondVarIndex);
                    ASMblock_append(outputBlock, outputStr, TACindex);

                    if (forceRelocate)
                    {
                        registerStates[oldAssigneeLocation]->live = 0;
                        registerStates[oldAssigneeLocation]->contains = NULL;
                    }
                }
            }
            // first operand is in register, second isn't
            else if (firstVarIndex != -1 && secondVarIndex == -1)
            {
                // if we can't overwrite the register containing the first var,
                // OR the destination variable exists in a register already
                if (registerStates[firstVarIndex]->live || destinationExists)
                {
                    // if not, copy the existing variable from its register to the destination
                    outputStr = malloc(18 * sizeof(char));
                    sprintf(outputStr, "mov %%r%d, %%r%d", destinationRegister, firstVarIndex);
                    ASMblock_append(outputBlock, outputStr, TACindex);
                }
                else
                {
                    // if yes, use this register as the destination!
                    destinationRegister = firstVarIndex;
                }

                // perform the operation by grabbing the non present var's value from the stack
                outputStr = malloc(18 * sizeof(char));
                sprintf(outputStr, "%s %%r%d, %d(%%bp)", getAsmOp(line->operation), destinationRegister, findInStack(line->operands[2], table));
                ASMblock_append(outputBlock, outputStr, TACindex);
            }
            // first operand isn't in register, second is
            else if (firstVarIndex == -1 && secondVarIndex != -1)
            {
                // reorder if possible
                // this might save a 'mov' if the operand which is in a register can be overwritten!
                if (line->reorderable)
                {
                    // see if the register can be overwritten
                    if (registerStates[secondVarIndex]->live)
                    {
                        // if not, copy the existing variable from its register to the destination
                        outputStr = malloc(18 * sizeof(char));
                        sprintf(outputStr, "mov %%r%d, %%r%d", destinationRegister, secondVarIndex);
                        ASMblock_append(outputBlock, outputStr, TACindex);
                    }
                    else
                    {
                        // if yes, use this register as the destination!
                        destinationRegister = secondVarIndex;
                    }

                    // perform the operation by grabbing the non present var's value from the stack
                    outputStr = malloc(18 * sizeof(char));
                    sprintf(outputStr, "%s %%r%d, %d(%%bp)", getAsmOp(line->operation), destinationRegister, findInStack(line->operands[1], table));
                    ASMblock_append(outputBlock, outputStr, TACindex);
                }
                // can't reorder
                else
                {
                    // place the first operand into the destination register
                    outputStr = malloc(18 * sizeof(char));
                    sprintf(outputStr, "mov %%r%d, %d(%%bp)", destinationRegister, findInStack(line->operands[1], table));
                    ASMblock_append(outputBlock, outputStr, TACindex);

                    // perform arithmetic using the second operand which is in a register
                    outputStr = malloc(18 * sizeof(char));
                    sprintf(outputStr, "%s %%r%d, %%r%d", getAsmOp(line->operation), destinationRegister, secondVarIndex);
                    ASMblock_append(outputBlock, outputStr, TACindex);
                }
            }
            // neither operand is in registers
            else
            {
                outputStr = malloc(18 * sizeof(char));
                sprintf(outputStr, "mov %%r%d, %d(%%bp)", destinationRegister, findInStack(line->operands[1], table));
                ASMblock_append(outputBlock, outputStr, TACindex);

                outputStr = malloc(18 * sizeof(char));
                sprintf(outputStr, "%s %%r%d, %d(%%bp)", getAsmOp(line->operation), destinationRegister, findInStack(line->operands[2], table));
                ASMblock_append(outputBlock, outputStr, TACindex);
            }
        }
    }
    registerStates[destinationRegister]->contains = line->operands[0];
    registerStates[destinationRegister]->live = 1;
    registerStates[destinationRegister]->touched = 1;

    // check if we just assigned to a ssa temp variable or a real variable
    // if it was a real variable, track that it's now dirty
    if (line->operands[0][0] != '.')
        registerStates[destinationRegister]->dirty = 1;
}

void generateMemoryCode(struct TACLine *line, struct registerState **registerStates, struct ASMblock *outputBlock, struct symbolTable *table, int TACindex)
{
    char *outputStr;
    int sourceIndex;
    int destinationIndex;
    switch (line->operation)
    {
    case tt_memr_1:
        outputStr = malloc(32 * sizeof(char));
        destinationIndex = findInRegisters(registerStates, line->operands[0]);
        sourceIndex = findInRegisters(registerStates, line->operands[1]);
        if (destinationIndex == -1)
        {
            destinationIndex = findUnallocatedRegister(registerStates);
            registerStates[destinationIndex]->dirty = 1;
        }

        if (sourceIndex == -1)
        {
            printf("Couldn't find %s\n", line->operands[1]);
            destinationIndex = findUnallocatedRegister(registerStates);
            sprintf(outputStr, "mov %%r%d, %d(%%sp)", destinationIndex, findInStack(line->operands[1], table));
            ASMblock_append(outputBlock, outputStr, TACindex);
            outputStr = malloc(32 * sizeof(char));
        }

        sprintf(outputStr, "mov %%r%d, (%%r%d)", destinationIndex, sourceIndex);
        registerStates[destinationIndex]->contains = line->operands[0];
        registerStates[destinationIndex]->live = 1;
        registerStates[destinationIndex]->touched = 1;

        ASMblock_append(outputBlock, outputStr, TACindex);
        break;

    case tt_memr_2:
        printf("TAC for unsupported addressing mode! Can't generate code (yet)\n");
        exit(1);
        break;

    case tt_memr_3:
        printf("TAC for unsupported addressing mode! Can't generate code (yet)\n");
        exit(1);
        break;

    case tt_memw_1:
        outputStr = malloc(32 * sizeof(char));
        destinationIndex = findInRegisters(registerStates, line->operands[0]);
        sourceIndex = findInRegisters(registerStates, line->operands[1]);
        if (destinationIndex == -1)
        {
            destinationIndex = findUnallocatedRegister(registerStates);
            sprintf(outputStr, "mov %%r%d, %d(%%sp)", destinationIndex, findInStack(line->operands[0], table));
            ASMblock_append(outputBlock, outputStr, TACindex);
            outputStr = malloc(32 * sizeof(char));
        }

        if (sourceIndex == -1)
        {
            destinationIndex = findUnallocatedRegister(registerStates);
            sprintf(outputStr, "mov %%r%d, %d(%%sp)", destinationIndex, findInStack(line->operands[1], table));
            ASMblock_append(outputBlock, outputStr, TACindex);
            outputStr = malloc(32 * sizeof(char));
        }

        sprintf(outputStr, "mov (%%r%d), %%r%d", destinationIndex, sourceIndex);
        registerStates[destinationIndex]->contains = line->operands[0];
        registerStates[destinationIndex]->live = 1;
        registerStates[destinationIndex]->touched = 1;

        ASMblock_append(outputBlock, outputStr, TACindex);
        break;

    case tt_memw_2:
        printf("TAC for unsupported addressing mode! Can't generate code (yet)\n");
        exit(1);
        break;

    case tt_memw_3:
        printf("TAC for unsupported addressing mode! Can't generate code (yet)\n");
        exit(1);
        break;

    default:
        printf("Illegal TAC type passe to generateMemoryCode()\n");
        exit(1);
    }
}

struct ASMblock *generateCode(struct symbolTable *table, char *functionName, struct Lifetime *lifetimes)
{
    struct ASMblock *outputBlock = newASMblock();
    // generate register states for the 12 GP registers
    struct registerState **registerStates = malloc(12 * sizeof(struct registerState *));
    for (int i = 0; i < 12; i++)
        registerStates[i] = newRegisterState();

    struct Stack *stateStack = newStack();

    int TACindex = 0;
    char *outputStr;

    for (struct TACLine *line = table->codeBlock; line != NULL; line = line->nextLine)
    {
        for (struct Lifetime *lt = lifetimes; lt != NULL; lt = lt->next)
        {
            if (TACindex > lt->end)
            {
                int varIndex = findInRegisters(registerStates, lt->variable);
                if (varIndex != -1)
                    registerStates[varIndex]->live = 0;
            }
        }

        switch (line->operation)
        {
        case tt_assign:
        {
            generateAssignmentCode(line, registerStates, outputBlock, table, TACindex);
        }
        break;

        case tt_push:
        {
            outputStr = malloc(16 * sizeof(char));
            switch (line->operandTypes[0])
            {

            case vt_var:
                int varIndex = findInRegisters(registerStates, line->operands[0]);
                if (varIndex != -1)
                {
                    sprintf(outputStr, "push %%r%d", varIndex);
                }
                else
                {
                    sprintf(outputStr, "push %d(%%bp)", findInStack(line->operands[0], table));
                }
                break;

            case vt_temp:
                sprintf(outputStr, "push %%r%d", findInRegisters(registerStates, line->operands[0]));
                break;

            case vt_literal:
                sprintf(outputStr, "push %s", line->operands[0]);
                break;

            case vt_returnval:
                break;

            case vt_null:
                printf("Error - NULL type assigned to operand of 'push'\n");
                exit(1);
            }
            ASMblock_append(outputBlock, outputStr, TACindex);
        }
        break;

        case tt_call:
        {
            // nonexistent/unused return value
            if (line->operands[0] == NULL)
            {
                if (registerStates[0]->live)
                    relocateRegister(registerStates, outputBlock, 0, findUnallocatedRegister(registerStates), TACindex);

                outputStr = malloc(32 * sizeof(char));
                sprintf(outputStr, "call %s", line->operands[1]);
                ASMblock_append(outputBlock, outputStr, TACindex);
            }
            // we are using the return value
            else
            {
                // if the thing in %r0 isn't just being reassigned, we might need to relocate it
                if (strcmp(line->operands[0], registerStates[0]->contains))
                {
                    // if: %r0 is live OR %r0 contains a dirty variable (not a temp)
                    if (registerStates[0]->live || (registerStates[0]->contains != NULL && registerStates[0]->contains[0] != '.' && registerStates[0]->dirty))
                    {
                        // right now just move it to an unallocated register
                        relocateRegister(registerStates, outputBlock, 0, findUnallocatedRegister(registerStates), TACindex);
                    }
                }

                // need a larger buffer because of labels
                outputStr = malloc(32 * sizeof(char));
                sprintf(outputStr, "call %s", line->operands[1]);
                ASMblock_append(outputBlock, outputStr, TACindex);

                // find whether the variable the return value goes into exists in a register
                int existingVarIndex = findInRegisters(registerStates, line->operands[0]);
                // if it does, just nullify that register since the value will be replaced entirely
                if (existingVarIndex != -1)
                {
                    registerStates[existingVarIndex]->live = 0;
                    registerStates[existingVarIndex]->dirty = 0;
                    registerStates[existingVarIndex]->contains = NULL;
                }
                registerStates[0]->contains = line->operands[0];
                registerStates[0]->live = 1;
                registerStates[0]->touched = 1;
                // check if we just assigned to a ssa temp variable or a real variable
                // if it was a real variable, track that it's now dirty
                if (line->operands[0][0] != '.')
                    registerStates[0]->dirty = 1;
            }
        }
        break;

        case tt_label:
        {
            outputStr = malloc(32 * sizeof(char));
            if (line->operands[0] > 0)
            {
                sprintf(outputStr, "%s_%ld:", functionName, (long int)line->operands[0]);
            }
            else
            {
                sprintf(outputStr, "%s:", functionName);
            }
            ASMblock_append(outputBlock, outputStr, TACindex);
            break;
        }

        case tt_return:
        {
            int retValIndex = findInRegisters(registerStates, ".RETVAL");
            if (retValIndex != 0)
                relocateRegister(registerStates, outputBlock, retValIndex, 0, TACindex);

            outputStr = malloc(32 * sizeof(char));
            sprintf(outputStr, "jmp %s_done", functionName);
            ASMblock_append(outputBlock, outputStr, TACindex);
        }
        break;

        case tt_cmp:
        {
            outputStr = malloc(16 * sizeof(char));
            if (line->operandTypes[1] == vt_literal)
            {
                if (line->operandTypes[2] == vt_literal)
                {
                    printf("Error - comparison between 2 literals! This should be impossible with previous error checking!\n");
                    exit(1);
                }
                // problem - this operand ordering doesn't exist in the ISA
                printf("Error - compare with bad instruction order - need to implement a system to flip this!\n");
                exit(1);
            }
            else
            {
                // op1 var, op2 literal
                if (line->operandTypes[2] == vt_literal)
                {
                    // see if the var exists in a register
                    int varIndex = findInRegisters(registerStates, line->operands[1]);
                    if (varIndex == -1)
                    {
                        // if not, grab it from the stack
                        varIndex = findUnallocatedRegister(registerStates);
                        sprintf(outputStr, "mov %%r%d, %d(%%bp)", varIndex, findInStack(line->operands[1], table));
                        ASMblock_append(outputBlock, outputStr, TACindex);
                        outputStr = malloc(16 * sizeof(char));
                    }
                    sprintf(outputStr, "cmp %%r%d, $%s", varIndex, line->operands[2]);
                }
                // op1 var, op2 var
                else
                {
                    // try to find both variables
                    int firstVarIndex = findInRegisters(registerStates, line->operands[1]);
                    int secondVarIndex = findInRegisters(registerStates, line->operands[2]);

                    // if the first variable doesn't exist, find and place it
                    if (firstVarIndex == -1)
                    {
                        firstVarIndex = findUnallocatedRegister(registerStates);
                        sprintf(outputStr, "mov %%r%d, %d(%%bp)", firstVarIndex, findInStack(line->operands[1], table));
                        ASMblock_append(outputBlock, outputStr, TACindex);
                        outputStr = malloc(16 * sizeof(char));
                    }
                    // if the second var doesn't exist, just do the operation from the stack
                    if (secondVarIndex == -1)
                    {
                        sprintf(outputStr, "cmp %%r%d, %d(%%bp)", firstVarIndex, findInStack(line->operands[2], table));
                    }
                    // otherwise, use the register containing the value
                    else
                    {
                        sprintf(outputStr, "cmp %%r%d, %%r%d", firstVarIndex, secondVarIndex);
                    }
                }
            }
            ASMblock_append(outputBlock, outputStr, TACindex);
        }
        break;

        case tt_jg:
        case tt_jge:
        case tt_jl:
        case tt_jle:
        case tt_je:
        case tt_jne:
        case tt_jmp:
        {
            outputStr = malloc(32 * sizeof(char));
            sprintf(outputStr, "%s %s_%ld", getAsmOp(line->operation), functionName, (long int)line->operands[0]);
            ASMblock_append(outputBlock, outputStr, TACindex);
        }
        break;

        case tt_memw_1:
        case tt_memw_2:
        case tt_memw_3:
        case tt_memr_1:
        case tt_memr_2:
        case tt_memr_3:
            generateMemoryCode(line, registerStates, outputBlock, table, TACindex);
            break;

        case tt_pushstate:
        {
            StackPush(stateStack, duplicateRegisterStates(registerStates));
        }
        break;

        case tt_restorestate:
        {
            restoreRegisterStates(registerStates, StackPeek(stateStack), outputBlock, table, TACindex);
        }
        break;

        case tt_resetstate:
        {
            struct registerState **resetTo = StackPeek(stateStack);
            for (int i = 0; i < 12; i++)
                memcpy(registerStates[i], resetTo[i], sizeof(struct registerState));
        }
        break;

        case tt_popstate:
        {
            freeRegisterStates(StackPop(stateStack));
        }
        break;

        case tt_asm:
        {
            char *outputAsmString = malloc((strlen(line->operands[0]) + 8) * sizeof(char));
            int outputAsmIndex = 0;
            for (int i = 0; i < strlen(line->operands[0]); i++)
            {
                // check to see if we need to substitute any variables in
                if (line->operands[0][i] == '(')
                {
                    // establish an intermediate buffer to contain the name of the variable
                    char intermediateBuffer[BUF_SIZE];
                    int intermediateBufLen = 0;
                    // scan through the asm line, copy the string to the intermediate buffer
                    while (line->operands[0][++i] != ')')
                    {
                        // break if we see the end of the asm line with no close paren
                        if (line->operands[0][i] == '\0')
                        {
                            printf("Error - unmatched open paren in asm line\n[%s]\n", line->operands[0]);
                            exit(1);
                        }
                        intermediateBuffer[intermediateBufLen++] = line->operands[0][i];
                    }
                    intermediateBuffer[intermediateBufLen] = '\0';

                    // allocate another intermediate string to store where the variable will actually come from
                    char locationString[16];
                    int registerLocation = findInRegisters(registerStates, intermediateBuffer);
                    if (registerLocation != -1)
                        sprintf(locationString, "%%r%d", registerLocation);
                    else
                        sprintf(locationString, "%d(%%sp)", findInStack(intermediateBuffer, table));

                    // finally, copy the location of the variable to the final output string
                    for (int j = 0; j < strlen(locationString); j++)
                        outputAsmString[outputAsmIndex++] = locationString[j];
                }
                // no variable find/substitution, so just copy the chars directly
                else
                {
                    outputAsmString[outputAsmIndex++] = line->operands[0][i];
                }
            }
            outputAsmString[outputAsmIndex] = '\0';
            ASMblock_append(outputBlock, outputAsmString, TACindex);
        }
        break;

        default:
            // printf("%s = %s %s\n", line->operands[0], line->operands[1], line->operands[2]);
            generateArithmeticCode(line, registerStates, outputBlock, table, TACindex);
            break;
        }
        TACindex++;
        // printRegisterStatesHorizontal(registerStates);
        // printf("\n\n");
    }

    //  clean up the registers states
    struct ASMline *entryLabel = outputBlock->head;
    outputBlock->head = outputBlock->head->next;

    outputStr = malloc(32 * sizeof(char));
    sprintf(outputStr, "%s_done:", functionName);
    ASMblock_append(outputBlock, outputStr, 999);

    for (int i = 11; i > 0; i--)
    {
        if (registerStates[i]->touched)
        {
            outputStr = malloc(9 * sizeof(char));
            sprintf(outputStr, "push %%r%d", i);
            ASMblock_prepend(outputBlock, outputStr, 0);

            outputStr = malloc(8 * sizeof(char));
            sprintf(outputStr, "pop %%r%d", i);
            ASMblock_append(outputBlock, outputStr, 999);
        }
    }

    if (table->argStackSize > 0)
    {
        outputStr = malloc(16 * sizeof(char));
        sprintf(outputStr, "sub %%sp, $%d", table->argStackSize);
        ASMblock_prepend(outputBlock, outputStr, 0);
    }

    entryLabel->next = outputBlock->head;
    outputBlock->head = entryLabel;

    // deal with making and freeing room on the stack for local variables
    if (table->argStackSize > 0)
    {
        // reset the room made on the stack for local variables
        outputStr = malloc(16 * sizeof(char));
        sprintf(outputStr, "add %%sp, $%d", table->argStackSize);
        ASMblock_append(outputBlock, outputStr, 999);
    }
    outputStr = malloc(6 * sizeof(char));
    sprintf(outputStr, "ret %d", table->argStackSize);
    ASMblock_append(outputBlock, outputStr, 999);

    // need to get arg count for return statement
    freeRegisterStates(registerStates);
    freeStack(stateStack);

    return outputBlock;
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("Error - please specify an input and output file!\n");
        exit(1);
    }
    else if (argc < 3)
    {
        printf("Error - please specify an output file!\n");
        exit(1);
    }
    printf("Parsing program from %s\n", argv[1]);

    printf("Generating output to %s\n", argv[2]);
    struct Dictionary *parseDict = newDictionary(10);
    struct astNode *program = parseProgram(argv[1], parseDict);

    printf("\n");

    printAST(program, 0);
    printf("Generating symbol table from AST");
    struct symbolTable *theTable = walkAST(program);
    printf("\n");

    printf("Linearizing code to TAC");
    linearizeProgram(program, theTable);
    printf("\n");

    printSymTab(theTable, 1);
    printf("\n\n");

    FILE *outFile = fopen(argv[2], "wb");
    struct Lifetime *theseLifetimes = findLifetimes(theTable);
    struct ASMblock *output = generateCode(theTable, theTable->name, theseLifetimes);
    ASMblock_output(output, outFile);
    ASMblock_free(output);
    freeLifetime(theseLifetimes);
    for (int i = 0; i < theTable->size; i++)
    {
        if (theTable->entries[i]->type == e_function)
        {
            struct functionEntry *thisEntry = theTable->entries[i]->entry;
            theseLifetimes = findLifetimes(thisEntry->table);

            // for (struct Lifetime *ltRunner = theseLifetimes; ltRunner != NULL; ltRunner = ltRunner->next)
            // {
                // printf("Var [%8s]: %2x - %2x\n", ltRunner->variable, ltRunner->start, ltRunner->end);
            // }

            output = generateCode(thisEntry->table, theTable->entries[i]->name, theseLifetimes);
            // run along all the lines of asm output from this funtcion, printing and freeing as we go
            ASMblock_output(output, outFile);
            ASMblock_free(output);
            freeLifetime(theseLifetimes);
        }
    }
    fclose(outFile);
    freeDictionary(parseDict);
    freeAST(program);
    freeSymTab(theTable);

    printf("done printing\n");
}
