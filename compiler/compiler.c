#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "tac.h"
#include "symtab.h"
#include "dict.h"
#include "linearizer.h"

void walkStatement(struct astNode *it, struct symbolTable *wip)
{
    //printf("walking statement\n");
    switch (it->type)
    {
    case t_var:
        char *varName;
        if (it->child->type == t_assign)
            varName = it->child->child->value;
        else
            varName = it->child->value;

        // lookup the variable being assigned, only insert if unique
        // also covers modification of argument values
        if (!symbolTableContains(wip, varName))
        {
            symTab_insertVariable(wip, varName, wip->varc++);
        }

        break;

    case t_assign:
        if (!symbolTableContains(wip, it->child->value))
        {
            printf("Error - variable [%s] assigned before declaration\n", it->child->value);
            exit(1);
        }
        break;

    case t_if:
        // having fun yet?
        struct astNode *ifRunner = it->child->sibling->child;
        while (ifRunner != NULL)
        {
            walkStatement(ifRunner, wip);
            ifRunner = ifRunner->sibling;
        }

        // no, really!
        if (it->child->sibling->sibling != NULL)
        {
            ifRunner = it->child->sibling->sibling->child->child;
            while (ifRunner != NULL)
            {
                walkStatement(ifRunner, wip);
                ifRunner = ifRunner->sibling;
            }
        }

        break;

    // function call/return can't create new symbols so ignore
    case t_call:
    case t_return:
        break;

    default:
        printf("Error walking AST for function %s - expected 'var', name, or function call, saw %s with value of [%s]\n", it->child->value, getTokenName(it->type), it->value);
        exit(1);
    }
}

void walkFunction(struct astNode *it, struct symbolTable *wip)
{
    struct astNode *functionRunner = it->child;
    char *functionName = functionRunner->value;
    struct symbolTable *subTable = newSymbolTable(functionName);
    while (functionRunner != NULL)
    {
        //printAST(functionRunner, 0);
        switch (functionRunner->type)
        {
        // looking at function name, which will have argument variables as children
        case t_name:
            struct astNode *argumentRunner = functionRunner->child;
            while (argumentRunner != NULL)
            {
                if (argumentRunner->child->type == t_assign)
                {
                    if (!symbolTableContains(subTable, argumentRunner->child->child->value))
                        symTab_insertArgument(subTable, argumentRunner->child->child->value, wip->argc++);
                }
                else
                {
                    if (!symbolTableContains(subTable, argumentRunner->child->value))
                        symTab_insertArgument(subTable, argumentRunner->child->value, subTable->argc++);
                }

                argumentRunner = argumentRunner->sibling;
            }
            break;

        // function call/return can't create new symbols so ignore
        case t_call:
        case t_return:
            break;

        case t_if:
            // having fun yet?
            struct astNode *ifRunner = functionRunner->child->sibling->child;
            while (ifRunner != NULL)
            {
                walkStatement(ifRunner, subTable);
                ifRunner = ifRunner->sibling;
            }

            // no, really!
            if (functionRunner->child->sibling->sibling != NULL)
            {
                ifRunner = functionRunner->child->sibling->sibling->child->child;
                while (ifRunner != NULL)
                {
                    walkStatement(ifRunner, subTable);
                    ifRunner = ifRunner->sibling;
                }
            }

            break;

        // otherwise we are looking at the body of the function, which is a statement list
        default:
            walkStatement(functionRunner, subTable);
        }
        functionRunner = functionRunner->sibling;
    }
    symTab_insertFunction(wip, functionName, subTable);
}

// given an AST node for a program, walk the AST and generate a symbol table for the entire thing
struct symbolTable *walkAST(struct astNode *it)
{
    struct symbolTable *wip = newSymbolTable("Program");
    wip->tl = newTempList();
    struct astNode *runner = it;
    while (runner != NULL)
    {
        printf(".");
        switch (runner->type)
        {

        case t_fun:
            walkFunction(runner, wip);
            break;

        default:
            printf("Error walking AST - expected 'v' or function declaration\n");
            exit(1);
            break;
        }
        runner = runner->sibling;
    }
    return wip;
}

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

struct ASMline
{
    char *data;
    struct ASMline *next;
};

struct ASMblock
{
    struct ASMline *head;
    struct ASMline *tail;
};

struct ASMblock *newASMblock()
{
    struct ASMblock *wip = malloc(sizeof(struct ASMblock));
    wip->head = NULL;
    wip->tail = NULL;
    return wip;
}

void ASMblock_prepend(struct ASMblock *block, char *data)
{
    struct ASMline *newLine = malloc(sizeof(struct ASMline));
    newLine->data = data;
    if (block->head != NULL)
    {
        newLine->next = block->head;
    }
    else
    {
        newLine->next = NULL;
        block->tail = newLine;
    }
    block->head = newLine;
}

void ASMblock_append(struct ASMblock *block, char *data)
{
    struct ASMline *newLine = malloc(sizeof(struct ASMline));
    newLine->data = data;
    newLine->next = NULL;
    if (block->tail != NULL)
    {
        block->tail->next = newLine;
    }
    else
    {
        block->head = newLine;
    }
    block->tail = newLine;
}

/*
 * Given a symbol table entry for a function, calculate lifetime for variables defined within
 * Also enforces some basic error checking (use before declare / use before assign)
 *
 * SymTab entries for variables containing their own lifetimes may be redundant since
 * findLifetimes() will also re-find lifetimes for these variables
 */
void checkVariableLifetimes(struct functionEntry *function)
{
    int lineIndex = 0;
    // iterate all lines of TAC in the function's codeblock
    for (struct tacLine *t = function->codeBlock; t != NULL; t = t->nextLine)
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
            break;

        default:

            // only examine RHS operands if they are variables
            struct symTabEntry *varEntry;
            if (t->operandTypes[2] == vt_var)
            {
                // attempt to get the variable's entry from the table
                varEntry = symbolTableLookup(function->table, t->operands[2]);

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
                    // otherwise, the variable is used normally in this step
                    // this means its lifespan can end no sooner than this index
                    else
                    {
                        ((struct variableEntry *)varEntry->entry)->lsEnd = lineIndex;
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
                varEntry = symbolTableLookup(function->table, t->operands[1]);
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
                    else
                    {
                        ((struct variableEntry *)varEntry->entry)->lsEnd = lineIndex;
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
                struct symTabEntry *assignedVar = symbolTableLookup(function->table, t->operands[0]);

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
                        theVar->lsStart = lineIndex;
                    }
                    theVar->lsEnd = lineIndex;
                    break;

                case e_argument:
                    // in the case we have an argument, just extend its lifespan end
                    struct variableEntry *theArg = assignedVar->entry;
                    theArg->lsEnd = lineIndex;
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

struct Lifetime *findLifetimes(struct functionEntry *function)
{
    // look at explicitly defined variables and do error checking
    checkVariableLifetimes(function);

    // make a linked list of variable lifetimes
    struct Lifetime *ltList = NULL;
    struct Lifetime *ltTail = NULL;

    // look at all lines in the TAC block for this function, keeping track of our index
    int lineIndex = 0;
    for (struct tacLine *line = function->codeBlock; line != NULL; line = line->nextLine)
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
                        if (line->operandTypes[i] == vt_var && symbolTableLookup(function->table, line->operands[i])->type == e_argument)
                        {
                            ltList->start = 0;
                        }
                        ltTail = ltList;
                    }
                    else
                    {
                        ltTail->next = newLifetime(varName, lineIndex);
                        if (line->operandTypes[i] == vt_var && symbolTableLookup(function->table, line->operands[i])->type == e_argument)
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

    return ltList;
}

/*
 *
 * Target code generation // Register Allocation
 *
 */
struct registerState
{
    char live;  // whether this var will be used in the future
    char dying; // whether this var will be used in the current instruction but no later
    char dirty;
    char touched;
    char *contains;
};

struct registerState *newRegisterState()
{
    struct registerState *wip = malloc(sizeof(struct registerState));
    wip->live = 0;
    wip->dying = 0;
    wip->dirty = 0;
    wip->touched = 0;
    wip->contains = NULL;
    return wip;
}

void printRegisterStates(struct registerState **registerStates)
{
    printf("----------\n");
    for (int i = 0; i < 12; i++)
    {
        printf("r%2d   |", i);
    }
    printf("\n");
    for (int i = 0; i < 12; i++)
    {
        // D for dead, L for live, d for Dying
        char registerLife = 'D';
        if (registerStates[i]->live)
        {
            registerLife = 'L';
        }
        if (registerStates[i]->dying)
        {
            registerLife = 'd';
        }
        printf("%c%c    |", registerLife, (registerStates[i]->dirty ? 'D' : ' '));
    }
    printf("\n");

    for (int i = 0; i < 12; i++)
    {
        if (registerStates[i]->contains == NULL)
        {
            printf("NULL |");
        }
        else
        {
            for (int j = 0; j < 6; j++)
            {
                if (registerStates[i]->contains[j] == '\0')
                {
                    while (j < 6)
                    {
                        printf(" ");
                        j++;
                    }
                    break;
                }
                else
                {
                    printf("%c", registerStates[i]->contains[j]);
                }
            }
            printf("|");
        }
    }
    printf("\n----------\n");
}

void freeRegisterStates(struct registerState **s)
{
    for (int i = 0; i < 12; i++)
        free(s[i]);

    free(s);
}

struct registerState **duplicateRegisterStates(struct registerState **theStates)
{
    struct registerState **duplicateStates = malloc(12 * sizeof(struct registerState *));
    for (int i = 0; i < 12; i++)
    {
        duplicateStates[i] = malloc(sizeof(struct registerState));
        memcpy(duplicateStates[i], theStates[i], sizeof(struct registerState));
    }
    return duplicateStates;
}

/*
 * Stack to contain register state lists
 *
 */
struct RegisterStateStack
{
    struct registerState ***stack;
    int size;
};

struct RegisterStateStack *newRegisterStateStack()
{
    struct RegisterStateStack *wip = malloc(sizeof(struct RegisterStateStack));
    wip->stack = NULL;
    wip->size = 0;
    return wip;
}

void RegisterStateStack_push(struct RegisterStateStack *s, struct registerState **stateList)
{
    printf("pushing\n");
    struct registerState ***newStack = malloc((s->size + 1) * sizeof(struct registerState **));
    for (int i = 0; i < s->size - 1; i++)
    {
        newStack[i] = s->stack[i];
    }
    newStack[s->size] = duplicateRegisterStates(stateList);
    s->size++;
    free(s->stack);
    s->stack = newStack;
    for (int i = 0; i < s->size; i++)
    {
        printRegisterStates(s->stack[i]);
    }
}

void RegisterStateStack_pop(struct RegisterStateStack *s)
{
    printf("popping\n");
    struct registerState **poppedStates = s->stack[s->size - 1];
    struct registerState ***newStack;
    freeRegisterStates(poppedStates);
    // only reallocate and copy if there is data to copy
    if (s->size > 1)
    {
        newStack = malloc((s->size - 1) * sizeof(struct registerState **));
        for (int i = 0; i < s->size - 1; i++)
        {
            newStack[i] = s->stack[i];
        }
    }
    s->size--;
    free(s->stack);
    // similarly, just null the pointer if stack is empty
    if (s->size > 1)
    {
        s->stack = newStack;
    }
    else
    {
        s->stack = NULL;
    }
    for (int i = 0; i < s->size; i++)
    {
        printRegisterStates(s->stack[i]);
    }
}

struct registerState **RegisterStateStack_peek(struct RegisterStateStack *s)
{
    return s->stack[0];
}

int findUnallocatedRegister(struct registerState **registerStates)
{
    for (int i = 0; i < 12; i++)
    {
        if (registerStates[i]->live == 0)
            return i;
    }

    printf("All registers are dirty when searching for unallocated register! Need to deal with register spilling!");
    exit(1);

    return -1;
}

void modifyRegisterContents(struct registerState **registerStates, int index, char *contents)
{
    struct registerState *destinationRegister = registerStates[index];
    destinationRegister->live = 1;
    destinationRegister->touched = 1;
    destinationRegister->dying = 0;
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

// finds and returns the positive/negative offset of a variable
// value is byte offset relative to base pointer of provided function
int findStackOffset(char *var, struct functionEntry *function)
{
    struct symTabEntry *theEntry = symbolTableLookup(function->table, var);
    int spOffset;
    switch (theEntry->type)
    {
    case e_variable:
        // may need a hardcoded offset to avoid overlapping return vector
        spOffset = (((struct variableEntry *)theEntry->entry)->index * -2);
        break;

    case e_argument:
        // + 4 because return address and old base pointer are on stack before current BP
        spOffset = (((struct argumentEntry *)theEntry->entry)->index * 2) + 4;
        break;

    default:
        printf("Saw something illegal while trying to locate variable/argument %s on the stack\n", var);
        exit(1);
    }

    return spOffset;
}


// find and return the register containing a given temp. variable (since temporaries can be guaranteed to be in a register)
int findTemp(struct registerState **registerStates, char *var)
{
    for (int i = 0; i < 12; i++)
        if (registerStates[i]->contains != NULL)
            if (!strcmp(registerStates[i]->contains, var))
                return i;

    return -1;
}

// copy one register to a different one, freeing up the source
void relocateRegister(struct registerState **registerStates, struct ASMblock *outputBlock, int source, int dest)
{
    char *outputStr = malloc(14 * sizeof(char));
    sprintf(outputStr, "movw %%r%d, %%r%d", dest, source);
    ASMblock_append(outputBlock, outputStr);
    registerStates[dest]->contains = registerStates[source]->contains;
    registerStates[dest]->live = registerStates[source]->live;
    registerStates[dest]->dying = registerStates[source]->dying;
    registerStates[dest]->dirty = registerStates[source]->dirty;
    registerStates[dest]->touched = 1;

    registerStates[source]->contains = NULL;
    registerStates[source]->live = 0;
    registerStates[source]->dying = 0;
    registerStates[source]->dirty = 0;
}

void restoreRegisterStates(struct registerState **current, struct registerState **desired, struct functionEntry *function)
{
    printf("Restore from:\n");
    printRegisterStates(current);
    printf("To:\n");
    printRegisterStates(desired);
    int paths[12];
    char flags[12] = {0};
    for (int i = 0; i < 12; i++)
    {
        if (desired[i]->live)
        {
            flags[i] = 1;
            paths[i] = findTemp(current, desired[i]->contains);
        }
    }

    for (int i = 0; i < 12; i++)
    {
        if (flags[i])
            printf("%x->%x\n", paths[i], i);
    }
}

// assign a value into a register, return the destination index
int generateAssignmentCode(struct tacLine *line, struct registerState **registerStates, struct ASMblock *outputBlock, struct functionEntry *function)
{
    int destinationIndex = findTemp(registerStates, line->operands[0]);
    if (destinationIndex == -1)
    {
        destinationIndex = findUnallocatedRegister(registerStates);
    }

    char *outputStr;
    if (line->operandTypes[1] == vt_literal)
    {

        outputStr = malloc(17 * sizeof(char));
        sprintf(outputStr, "movw %%r%d, %s", destinationIndex, line->operands[1]);
        ASMblock_append(outputBlock, outputStr);
    }
    else
    {
        int sourceIndex = findTemp(registerStates, line->operands[1]);
        if (sourceIndex == -1)
        {
            sourceIndex = findUnallocatedRegister(registerStates);
            // this should get us a dead register but might warrant some checking
            outputStr = malloc(16 * sizeof(char));
            sprintf(outputStr, "movw %%r%d, %d(%%bp)", sourceIndex, findStackOffset(line->operands[1], function));
            ASMblock_append(outputBlock, outputStr);
        }
        // source var dies this step, it can be overwritten
        if (!registerStates[sourceIndex]->live || registerStates[sourceIndex]->dying)
        {
            // don't need to do anything
            int oldIndex = findTemp(registerStates, line->operands[0]);
            if (oldIndex != -1)
            {
                registerStates[oldIndex]->contains = NULL;
                registerStates[oldIndex]->live = 0;
                registerStates[oldIndex]->dirty = 0;
                registerStates[oldIndex]->dying = 0;
            }

            destinationIndex = sourceIndex;
        }
        // if the source variable can't be overwritten (is used after this step)
        else
        {
            outputStr = malloc(15 * sizeof(char));
            sprintf(outputStr, "movw %%r%d, %%r%d", destinationIndex, sourceIndex);
            ASMblock_append(outputBlock, outputStr);
        }
    }
    modifyRegisterContents(registerStates, destinationIndex, line->operands[0]);

    return destinationIndex;
}

char canOverwrite(struct registerState **registerStates, int registerIndex)
{
    return !registerStates[registerIndex]->live || registerStates[registerIndex]->dying;
}

void generateArithmeticCode(struct tacLine *line, struct registerState **registerStates, struct ASMblock *outputBlock, struct functionEntry *function)
{
    int destinationRegister;
    char *outputStr;

    // attempt to find the variable being assigned if it's already in registers
    destinationRegister = findTemp(registerStates, line->operands[0]);
    if (destinationRegister == -1)
    {
        // if not, we need a place for it to go
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
                int existingVarIndex = findTemp(registerStates, line->operands[2]);
                if (existingVarIndex != -1)
                {
                    // see if the existing variable can be overwritten
                    if (!canOverwrite(registerStates, existingVarIndex))
                    {
                        // if not, copy the existing variable from its register to the destination
                        outputStr = malloc(18 * sizeof(char));
                        sprintf(outputStr, "movw %%r%d, %%r%d", destinationRegister, existingVarIndex);
                        ASMblock_append(outputBlock, outputStr);
                    }
                    // otherwise, just make that variable's register our destination!
                    else
                    {
                        destinationRegister = existingVarIndex;
                    }

                    // perform the operation on the destination with the literal
                    outputStr = malloc(18 * sizeof(char));
                    sprintf(outputStr, "%s %%r%d, $%s", getAsmOp(line->operation), destinationRegister, line->operands[1]);
                    ASMblock_append(outputBlock, outputStr);
                }
            }

            // if it wasn't possible to reorder the operands
            if (!didReorder)
            {

                // put the literal directly into a register
                outputStr = malloc(18 * sizeof(char));
                sprintf(outputStr, "movw %%r%d, $%s", destinationRegister, line->operands[1]);
                ASMblock_append(outputBlock, outputStr);

                outputStr = malloc(20 * sizeof(char));
                // see if the variable is already in a register
                int existingVarIndex = findTemp(registerStates, line->operands[2]);
                // if not, get the second operand from the stack
                if (existingVarIndex == -1)
                {
                    sprintf(outputStr, "%s %%r%d, %d(%%bp)", getAsmOp(line->operation), destinationRegister, findStackOffset(line->operands[2], function));
                }
                // if yes, get the second operand from its register
                else
                {
                    sprintf(outputStr, "%s %%r%d, %%r%d", getAsmOp(line->operation), destinationRegister, existingVarIndex);
                }
                ASMblock_append(outputBlock, outputStr);
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
                int existingVarIndex = findTemp(registerStates, line->operands[1]);
                if (existingVarIndex != -1)
                {
                    // see if the existing variable can be overwritten
                    if (!canOverwrite(registerStates, existingVarIndex))
                    {
                        // if not, copy the existing variable from its register to the destination
                        outputStr = malloc(18 * sizeof(char));
                        sprintf(outputStr, "movw %%r%d, %%r%d", destinationRegister, existingVarIndex);
                        ASMblock_append(outputBlock, outputStr);
                    }
                    // otherwise, just make that variable's register our destination!
                    else
                    {
                        destinationRegister = existingVarIndex;
                    }

                    // perform the operation on the destination with the literal
                    outputStr = malloc(18 * sizeof(char));
                    sprintf(outputStr, "%s %%r%d, $%s", getAsmOp(line->operation), destinationRegister, line->operands[2]);
                    ASMblock_append(outputBlock, outputStr);
                }
            }

            // if it wasn't possible to reorder the operands
            if (!didReorder)
            {
                // see if the variable is already in a register
                int existingVarIndex = findTemp(registerStates, line->operands[1]);
                // if not, get the first operand from the stack
                if (existingVarIndex == -1)
                {
                    outputStr = malloc(18 * sizeof(char));
                    sprintf(outputStr, "movw %%r%d, %d(%%bp)", destinationRegister, findStackOffset(line->operands[1], function));
                    ASMblock_append(outputBlock, outputStr);
                }
                // if yes, get the first operand from its register
                else
                {
                    // see if the first var can be overwritten
                    if (!canOverwrite(registerStates, existingVarIndex))
                    {
                        outputStr = malloc(18 * sizeof(char));
                        sprintf(outputStr, "movw %%r%d, %%r%d", destinationRegister, existingVarIndex);
                        ASMblock_append(outputBlock, outputStr);
                    }
                    else
                    {
                        // otherwise, just make that variable's register our destination!
                        destinationRegister = existingVarIndex;
                    }
                }
                outputStr = malloc(18 * sizeof(char));
                sprintf(outputStr, "%s %%r%d, $%s", getAsmOp(line->operation), destinationRegister, line->operands[2]);
                ASMblock_append(outputBlock, outputStr);
            }
        }
        // op1 var, op2 var
        else
        {
            int firstVarIndex = findTemp(registerStates, line->operands[1]);
            int secondVarIndex = findTemp(registerStates, line->operands[2]);

            // both operands live in registers
            if (firstVarIndex != -1 && secondVarIndex != -1)
            {
                char didReorder = 0;
                // see if we can reorder
                if (line->reorderable)
                {
                    // if we can overwrite the register containing the second var, do it
                    if (canOverwrite(registerStates, secondVarIndex))
                    {
                        destinationRegister = secondVarIndex;
                        outputStr = malloc(18 * sizeof(char));
                        sprintf(outputStr, "%s %%r%d, %%r%d", getAsmOp(line->operation), destinationRegister, firstVarIndex);
                        ASMblock_append(outputBlock, outputStr);
                        didReorder = 1;
                    }
                }

                // wasn't possible (or wouldn't have saved us any instructions) to reorder
                // just conduct the operation in order
                if (!didReorder)
                {
                    // see if the first var can be overwritten
                    // also verify that the variable isn't modifying itself
                    if (!canOverwrite(registerStates, firstVarIndex) && strcmp(line->operands[0], line->operands[1]))
                    {
                        // if not, copy the existing variable from its register to the destination
                        outputStr = malloc(18 * sizeof(char));
                        sprintf(outputStr, "movw %%r%d, %%r%d", destinationRegister, firstVarIndex);
                        ASMblock_append(outputBlock, outputStr);
                    }
                    else
                    {
                        // if yes, use this register as the destination!
                        destinationRegister = firstVarIndex;
                    }

                    // perform the operation by using the register containing the second operand
                    outputStr = malloc(18 * sizeof(char));
                    sprintf(outputStr, "%s %%r%d, %%r%d", getAsmOp(line->operation), destinationRegister, secondVarIndex);
                    ASMblock_append(outputBlock, outputStr);
                }
            }
            // first operand is in register, second isn't
            else if (firstVarIndex != -1 && secondVarIndex == -1)
            {
                // see if the register can be overwritten
                if (!canOverwrite(registerStates, firstVarIndex))
                {
                    // if not, copy the existing variable from its register to the destination
                    outputStr = malloc(18 * sizeof(char));
                    sprintf(outputStr, "movw %%r%d, %%r%d", destinationRegister, firstVarIndex);
                    ASMblock_append(outputBlock, outputStr);
                }
                else
                {
                    // if yes, use this register as the destination!
                    destinationRegister = firstVarIndex;
                }

                // perform the operation by grabbing the non present var's value from the stack
                outputStr = malloc(18 * sizeof(char));
                sprintf(outputStr, "%s %%r%d, %d(%%bp)", getAsmOp(line->operation), destinationRegister, findStackOffset(line->operands[2], function));
                ASMblock_append(outputBlock, outputStr);
            }
            // first operand isn't in register, second is
            else if (firstVarIndex == -1 && secondVarIndex != -1)
            {
                // reorder if possible
                // this might save a 'mov' if the operand which is in a register can be overwritten!
                if (line->reorderable)
                {
                    // see if the register can be overwritten
                    if (!canOverwrite(registerStates, secondVarIndex))
                    {
                        // if not, copy the existing variable from its register to the destination
                        outputStr = malloc(18 * sizeof(char));
                        sprintf(outputStr, "movw %%r%d, %%r%d", destinationRegister, secondVarIndex);
                        ASMblock_append(outputBlock, outputStr);
                    }
                    else
                    {
                        // if yes, use this register as the destination!
                        destinationRegister = secondVarIndex;
                    }

                    // perform the operation by grabbing the non present var's value from the stack
                    outputStr = malloc(18 * sizeof(char));
                    sprintf(outputStr, "%s %%r%d, %d(%%bp)", getAsmOp(line->operation), destinationRegister, findStackOffset(line->operands[1], function));
                    ASMblock_append(outputBlock, outputStr);
                }
                // can't reorder
                else
                {
                    // place the first operand into the destination register
                    outputStr = malloc(18 * sizeof(char));
                    sprintf(outputStr, "movw %%r%d, %d(%%bp)", destinationRegister, findStackOffset(line->operands[1], function));
                    ASMblock_append(outputBlock, outputStr);

                    // perform arithmetic using the second operand which is in a register
                    outputStr = malloc(18 * sizeof(char));
                    sprintf(outputStr, "%s %%r%d, %%r%d", getAsmOp(line->operation), destinationRegister, secondVarIndex);
                    ASMblock_append(outputBlock, outputStr);
                }
            }
            //neither operand is in registers
            else
            {
                outputStr = malloc(18 * sizeof(char));
                sprintf(outputStr, "movw %%r%d, %d(%%bp)", destinationRegister, findStackOffset(line->operands[1], function));
                ASMblock_append(outputBlock, outputStr);

                outputStr = malloc(18 * sizeof(char));
                sprintf(outputStr, "%s %%r%d, %d(%%bp)", getAsmOp(line->operation), destinationRegister, findStackOffset(line->operands[2], function));
                ASMblock_append(outputBlock, outputStr);
            }
        }
    }
    modifyRegisterContents(registerStates, destinationRegister, line->operands[0]);
}

struct ASMblock *generateCode(struct functionEntry *function, char *functionName, struct Lifetime *lifetimes)
{
    struct ASMblock *outputBlock = newASMblock();
    // generate register states for the 12 GP registers
    struct registerState **registerStates = malloc(12 * sizeof(struct registerState *));
    for (int i = 0; i < 12; i++)
        registerStates[i] = newRegisterState();

    struct RegisterStateStack *stateStack = newRegisterStateStack();

    int tacIndex = 0;
    char *outputStr;

    for (struct tacLine *line = function->codeBlock; line != NULL; line = line->nextLine)
    {
        for (int i = 0; i < 12; i++)
        {
            if (registerStates[i]->dying)
            {
                registerStates[i]->live = 0;
            }
            registerStates[i]->dying = 0;
        }

        for (struct Lifetime *lt = lifetimes; lt != NULL; lt = lt->next)
        {
            if (tacIndex > lt->end)
            {
                int varIndex = findTemp(registerStates, lt->variable);
                if (varIndex != -1)
                {
                    registerStates[varIndex]->live = 0;
                }
            }
            else if (tacIndex == lt->end)
            {
                int varIndex = findTemp(registerStates, lt->variable);
                if (varIndex != -1)
                {
                    registerStates[varIndex]->dying = 1;
                }
            }
        }

        switch (line->operation)
        {
        case tt_assign:
        {
            //printf("%s = %s %s\n", line->operands[0], line->operands[1], line->operands[2]);
            generateAssignmentCode(line, registerStates, outputBlock, function);
        }
        break;

        case tt_push:
        {
            outputStr = malloc(10 * sizeof(char));
            switch (line->operandTypes[0])
            {

            case vt_var:
                int varIndex = findTemp(registerStates, line->operands[0]);
                if (varIndex != -1)
                {
                    sprintf(outputStr, "push %%r%d", varIndex);
                }
                else
                {
                    sprintf(outputStr, "push %d(%%bp)", findStackOffset(line->operands[0], function));
                }
                break;

            case vt_temp:
                sprintf(outputStr, "push %%r%d", findTemp(registerStates, line->operands[0]));
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
            ASMblock_append(outputBlock, outputStr);
        }
        break;

        case tt_call:
        {
            // if the thing in %r0 isn't just being reassigned, we might need to relocate it
            if (strcmp(line->operands[0], registerStates[0]->contains))
            {
                // if: %r0 is live OR %r0 contains a dirty variable (not a temp)
                if (registerStates[0]->live || (registerStates[0]->contains != NULL && registerStates[0]->contains[0] != '.' && registerStates[0]->dirty))
                {
                    // right now just move it to an unallocated register
                    relocateRegister(registerStates, outputBlock, 0, findUnallocatedRegister(registerStates));
                }
            }
            // need a larger buffer because of labels
            outputStr = malloc(32 * sizeof(char));
            sprintf(outputStr, "call %s", line->operands[1]);
            ASMblock_append(outputBlock, outputStr);
            int existingVarIndex = findTemp(registerStates, line->operands[0]);
            if (existingVarIndex != -1)
            {
                registerStates[existingVarIndex]->live = 0;
                registerStates[existingVarIndex]->dirty = 0;
                registerStates[existingVarIndex]->contains = NULL;
            }
            modifyRegisterContents(registerStates, 0, line->operands[0]);
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
            ASMblock_append(outputBlock, outputStr);
            break;
        }

        case tt_return:
        {
            int retValIndex = findTemp(registerStates, ".RETVAL");
            if (retValIndex != 0)
            {
                relocateRegister(registerStates, outputBlock, retValIndex, 0);
            }
            outputStr = malloc(32 * sizeof(char));
            sprintf(outputStr, "jmp %s_done", functionName);
            ASMblock_append(outputBlock, outputStr);
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
                    int varIndex = findTemp(registerStates, line->operands[1]);
                    if (varIndex == -1)
                    {
                        // if not, grab it from the stack
                        varIndex = findUnallocatedRegister(registerStates);
                        sprintf(outputStr, "movw %%r%d, %d(%%bp)", varIndex, findStackOffset(line->operands[1], function));
                        ASMblock_append(outputBlock, outputStr);
                        outputStr = malloc(16 * sizeof(char));
                    }
                    sprintf(outputStr, "cmp %%r%d, $%s", varIndex, line->operands[2]);
                }
                // op1 var, op2 var
                else
                {
                    // try to find both variables
                    int firstVarIndex = findTemp(registerStates, line->operands[1]);
                    int secondVarIndex = findTemp(registerStates, line->operands[2]);

                    // if the first variable doesn't exist, find and place it
                    if (firstVarIndex == -1)
                    {
                        firstVarIndex = findUnallocatedRegister(registerStates);
                        sprintf(outputStr, "movw %%r%d, %d(%%bp)", firstVarIndex, findStackOffset(line->operands[1], function));
                        ASMblock_append(outputBlock, outputStr);
                        outputStr = malloc(16 * sizeof(char));
                    }
                    // if the second var doesn't exist, just do the operation from the stack
                    if (secondVarIndex == -1)
                    {
                        sprintf(outputStr, "cmp %%r%d, %d(%%bp)", firstVarIndex, findStackOffset(line->operands[2], function));
                    }
                    // otherwise, use the register containing the value
                    else
                    {
                        sprintf(outputStr, "cmp %%r%d, %%r%d", firstVarIndex, secondVarIndex);
                    }
                }
            }
            ASMblock_append(outputBlock, outputStr);
        }
        break;

        case tt_jg:
        case tt_jge:
        case tt_jl:
        case tt_jle:
        case tt_je:
        case tt_jne:
        case tt_jmp:
            outputStr = malloc(32 * sizeof(char));
            sprintf(outputStr, "%s %s_%ld", getAsmOp(line->operation), functionName, (long int)line->operands[0]);
            ASMblock_append(outputBlock, outputStr);
            break;

        case tt_pushstate:
            printf("PUSH STATE\n");
            RegisterStateStack_push(stateStack, registerStates);
            break;

        case tt_restorestate:
            printf("RESTORE STATE\n");
            restoreRegisterStates(registerStates, RegisterStateStack_peek(stateStack), function);
            break;

        case tt_resetstate:
            printf("RESET STATE\n");
            struct registerState **resetTo = RegisterStateStack_peek(stateStack);
            for (int i = 0; i < 12; i++)
            {
                memcpy(registerStates[i], resetTo[i], sizeof(struct registerState));
            }
            break;

        case tt_popstate:
            printf("POP STATE STACK\n");
            RegisterStateStack_pop(stateStack);
            break;

        default:
            //printf("%s = %s %s\n", line->operands[0], line->operands[1], line->operands[2]);
            generateArithmeticCode(line, registerStates, outputBlock, function);
            break;
        }
        //printf("%s\n", outputBlock->tail->data);
        //printRegisterStates(registerStates);
        //printf("\n\n");
        tacIndex++;
        /*struct ASMline *runner = outputBlock->head;
        while (runner != NULL)
        {
            printf("%s\n", runner->data);
            runner = runner->next;
        }
        printf("\n");*/
    }

    printRegisterStates(registerStates);
    //printf("\n");
    // clean up the registers states
    struct ASMline *entryLabel = outputBlock->head;
    outputBlock->head = outputBlock->head->next;
    /*struct ASMline *returnLine = outputBlock->tail;
    struct ASMline *runner = outputBlock->head;
    while (runner->next->next != NULL)
    {
        runner = runner->next;
    }
    outputBlock->tail = runner;*/

    outputStr = malloc(32 * sizeof(char));
    sprintf(outputStr, "%s_done:", functionName);
    ASMblock_append(outputBlock, outputStr);

    // deal with making and freeing room on the stack for local variables
    outputStr = malloc(16 * sizeof(char));
    sprintf(outputStr, "sub %%sp, $%d", function->table->varc * 2);
    ASMblock_prepend(outputBlock, outputStr);

    // reset the room made on the stack for local variables
    outputStr = malloc(16 * sizeof(char));
    sprintf(outputStr, "add %%sp, $%d", function->table->varc * 2);
    ASMblock_append(outputBlock, outputStr);


    for (int i = 11; i > 0; i--)
    {
        if (registerStates[i]->touched)
        {
            outputStr = malloc(9 * sizeof(char));
            sprintf(outputStr, "push %%r%d", i);
            ASMblock_prepend(outputBlock, outputStr);

            outputStr = malloc(8 * sizeof(char));
            sprintf(outputStr, "pop %%r%d", i);
            ASMblock_append(outputBlock, outputStr);
        }
    }
    entryLabel->next = outputBlock->head;
    outputBlock->head = entryLabel;
    //outputBlock->tail->next = returnLine;
    //outputBlock->tail = returnLine;

    outputStr = malloc(6 * sizeof(char));
    sprintf(outputStr, "ret %d", function->table->argc);
    ASMblock_append(outputBlock, outputStr);

    // need to get arg count for return statement
    freeRegisterStates(registerStates);
    free(stateStack);

    return outputBlock;
}

int main(int argc, char **argv)
{
    printf("Parsing program from %s\n", argv[1]);
    struct Dictionary *parseDict = newDictionary(10);
    struct astNode *program = parseProgram(argv[1], parseDict);

    printf("\n");

    printAST(program, 0);
    printf("Generating symbol table from AST");
    struct symbolTable *theTable = walkAST(program);
    printf("\n");
    printSymTab(theTable);
    printf("\n");

    printf("Linearizing code to TAC");
    linearizeProgram(program, theTable);
    printf("\n");

    printSymTab(theTable);
    printf("\n\n");

    for (int i = 0; i < theTable->size; i++)
    {
        if (theTable->entries[i]->type == e_function)
        {
            struct Lifetime *theseLifetimes = findLifetimes(theTable->entries[i]->entry);
            struct ASMblock *output = generateCode(theTable->entries[i]->entry, theTable->entries[i]->name, theseLifetimes);
            // run along all the lines of asm output from this funtcion, printing and freeing as we go
            struct ASMline *runner = output->head;
            while (runner != NULL)
            {
                if (runner->data[strlen(runner->data) - 1] != ':')
                    printf("\t");

                printf("%s\n", runner->data);

                struct ASMline *old = runner;
                runner = runner->next;
                free(old->data);
                free(old);
            }
            printf("\n");
            free(output);
            freeLifetime(theseLifetimes);
        }
    }
    freeDictionary(parseDict);
    freeAST(program);
    freeSymTab(theTable);

    // printTacLine(head);

    printf("done printing\n");
}