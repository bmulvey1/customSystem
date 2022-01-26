#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "tac.h"
#include "symtab.h"
#include "dict.h"
#include "linearizer.h"

// given an AST node for a program, walk the AST and generate a symbol table for the entire thing
struct symbolTable *walkAST(struct astNode *it)
{
    struct symbolTable *wip = newSymbolTable("Program");
    wip->tl = newTempList();
    int globalVariableCount = 0;
    struct astNode *runner = it;
    while (runner != NULL)
    {
        printf(".");
        switch (runner->type)
        {

        case t_var:
            if (runner->child->type == t_assign)
                symTab_insertVariable(wip, runner->child->child->value, globalVariableCount++);
            else
                symTab_insertVariable(wip, runner->child->value, globalVariableCount++);
            break;

        case t_fun:
        {
            int functionArgumentCount = 0;
            int functionVariableCount = 0;
            struct astNode *functionNode = runner->child;
            char *functionName = functionNode->value;
            struct symbolTable *subTable = newSymbolTable(functionNode->value);
            while (functionNode != NULL)
            {
                switch (functionNode->type)
                {
                case t_var:
                    char *varName;
                    if (functionNode->child->type == t_assign)
                        varName = functionNode->child->child->value;
                    else
                        varName = functionNode->child->value;

                    // lookup the variable being assigned, only insert if unique
                    // also covers modification of argument values
                    if (!symbolTableContains(subTable, varName))
                    {
                        symTab_insertVariable(subTable, varName, functionVariableCount++);
                    }

                    break;

                case t_assign:
                    if (!symbolTableContains(subTable, functionNode->child->value))
                    {
                        printf("Error - variable [%s] assigned before declaration\n", functionNode->child->value);
                        exit(1);
                    }
                    break;

                // looking at function name, which will have argument variables as children
                case t_name:
                    struct astNode *argumentRunner = functionNode->child;
                    while (argumentRunner != NULL)
                    {
                        if (argumentRunner->child->type == t_assign)
                        {
                            symTab_insertArgument(subTable, argumentRunner->child->child->value, functionArgumentCount++);
                        }
                        else
                        {
                            symTab_insertArgument(subTable, argumentRunner->child->value, functionArgumentCount++);
                        }

                        argumentRunner = argumentRunner->sibling;
                    }
                    break;

                // function calls and return statements have no implication on symtab entries (for now?)
                case t_call:
                case t_return:
                    break;

                default:
                    printf("Error walking AST - expected 'var', name, or function call, saw %s with value of [%s]\n", getTokenName(functionNode->type), functionNode->value);
                    exit(1);
                }
                functionNode = functionNode->sibling;
            }
            symTab_insertFunction(wip, functionName, subTable, functionArgumentCount);
        }
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

// TODO: search parent scopes for global vars
// will require more stack stuff and backwards pointers in function entries pointint to their parent table
int findOrPlaceVar(struct registerState **registerStates, struct ASMblock *outputBlock, char *var, struct functionEntry *function)
{
    for (int i = 0; i < 12; i++)
        if (registerStates[i]->contains != NULL)
            if (!strcmp(registerStates[i]->contains, var))
                return i;

    int freshRegisterIndex = findUnallocatedRegister(registerStates);

    char *outputStr = malloc(18 * sizeof(char));
    sprintf(outputStr, "movw %%r%d, %d(%%bp)", freshRegisterIndex, findStackOffset(var, function));
    ASMblock_append(outputBlock, outputStr);
    modifyRegisterContents(registerStates, freshRegisterIndex, var);
    return freshRegisterIndex;
}

/*
 * Find what register a variable lives in
 * if it isn't currently in a register, retrieve it from the stack
 */
int findOrPlaceModifiableVar(struct registerState **registerStates, struct ASMblock *outputBlock, char *var, struct functionEntry *function)
{
    int registerIndex = -1;
    for (int i = 0; i < 12; i++)
        if (registerStates[i]->contains != NULL)
            if (!strcmp(registerStates[i]->contains, var))
            {
                registerIndex = i;
                break;
            }

    if (registerIndex != -1)
    {
        // if the register is live we need to copy the value elsewhere to get a modifiable register containing the value
        // note - if the register is dying it shouldn't need to be copied (d = d + d -> `add r0, r0` is allowable if the var is dying)
        if (registerStates[registerIndex]->live && !registerStates[registerIndex]->dying)
        {
            int sourceIndex = registerIndex;
            registerIndex = findUnallocatedRegister(registerStates);
            registerStates[registerIndex]->touched = 1;

            char *outputStr = malloc(15 * sizeof(char));
            sprintf(outputStr, "movw %%r%d, %%r%d", registerIndex, sourceIndex);
            ASMblock_append(outputBlock, outputStr);
            /*
            modifyRegisterContents(registerStates, registerIndex, var);
            // if a register is being duplicated elsewhere, its value will be dirty if the register still contains that variable at the end
            registerStates[registerIndex]->dirty = 1;
            */
        }
    }
    else // if the register couldn't be located, the variable needs to be retrieved from the stack
    {
        registerIndex = findUnallocatedRegister(registerStates);
        char *outputStr = malloc(18 * sizeof(char));
        sprintf(outputStr, "movw %%r%d, %d(%%bp)", registerIndex, findStackOffset(var, function));
        ASMblock_append(outputBlock, outputStr);
        modifyRegisterContents(registerStates, registerIndex, var);
    }
    return registerIndex;
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
}

void printRegisterStates(struct registerState **registerStates)
{
    printf("----------\n");
    for (int i = 0; i < 12; i++)
    {
        if (registerStates[i]->contains != NULL)
            printf("r%2d      |", i);
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
        if (registerStates[i]->contains != NULL)
            printf("[%4s] %c%c|", registerStates[i]->contains, registerLife, (registerStates[i]->dirty ? 'D' : ' '));
    }
    printf("\n----------\n");
}

// assign a value into a register, return the destination index
int generateAssignmentCode(struct tacLine *line, struct registerState **registerStates, struct ASMblock *outputBlock, struct functionEntry *function)
{
    int destinationIndex;
    if (line->operandTypes[1] == vt_literal)
    {
        destinationIndex = findTemp(registerStates, line->operands[0]);
        if (destinationIndex == -1)
        {
            destinationIndex = findUnallocatedRegister(registerStates);
        }
        char *outputStr = malloc(17 * sizeof(char));
        sprintf(outputStr, "movw %%r%d, %s", destinationIndex, line->operands[1]);
        ASMblock_append(outputBlock, outputStr);
    }
    else
    {
        int sourceIndex = findOrPlaceVar(registerStates, outputBlock, line->operands[1], function);
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
            // use findTemp here find the register ONLY if the variable exists in a register already
            destinationIndex = findTemp(registerStates, line->operands[0]);
            // if the variable isn't in a register already we need to find one to put it in
            if (destinationIndex == -1)
            {
                destinationIndex = findUnallocatedRegister(registerStates);
            }
            char *outputStr = malloc(15 * sizeof(char));
            sprintf(outputStr, "movw %%r%d, %%r%d", destinationIndex, sourceIndex);
            ASMblock_append(outputBlock, outputStr);
        }
    }
    modifyRegisterContents(registerStates, destinationIndex, line->operands[0]);

    return destinationIndex;
}

void generateArithmeticCode(struct tacLine *line, struct registerState **registerStates, struct ASMblock *outputBlock, struct functionEntry *function)
{
    int destinationRegister;
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
            if (line->reorderable)
            {
                destinationRegister = findTemp(registerStates, line->operands[2]);
                // only proceed with checks if the second operand exists in a register
                if (destinationRegister != -1)
                {
                    // if the register can be overwritten at this step (or the var is the same as the destination)
                    char canOverwriteDest = !registerStates[destinationRegister]->live || registerStates[destinationRegister]->dying;
                    if (canOverwriteDest || !strcmp(line->operands[0], line->operands[2]))
                    {
                        // if a register already contains the variable, purge it to avoid duplication
                        int oldIndex = findTemp(registerStates, line->operands[0]);
                        if (oldIndex != -1)
                        {
                            registerStates[oldIndex]->contains = NULL;
                            registerStates[oldIndex]->live = 0;
                            registerStates[oldIndex]->dirty = 0;
                        }
                        // just perform the operation on the existing value
                        char *outputStr = malloc(15 * sizeof(char));
                        sprintf(outputStr, "%s %%r%d, $%s", getAsmOp(line->operation), destinationRegister, line->operands[1]);
                        ASMblock_append(outputBlock, outputStr);
                        didReorder = 1;
                    }
                }
            }

            if (!didReorder)
            {
                // move the first operand into a register
                destinationRegister = findUnallocatedRegister(registerStates);
                int sourceIndex = findOrPlaceVar(registerStates, outputBlock, line->operands[2], function);
                char *outputStr = malloc(17 * sizeof(char));
                sprintf(outputStr, "movw %%r%d, $%s", destinationRegister, line->operands[1]);
                ASMblock_append(outputBlock, outputStr);
                outputStr = malloc(12 * sizeof(char));
                sprintf(outputStr, "%s %%r%d, %%r%d", getAsmOp(line->operation), destinationRegister, sourceIndex);
                ASMblock_append(outputBlock, outputStr);

                // if the variable has been copied, invalidate the old one
                if (!strcmp(line->operands[0], line->operands[2]))
                {
                    registerStates[sourceIndex]->contains = NULL;
                    registerStates[sourceIndex]->live = 0;
                    registerStates[sourceIndex]->dying = 0;
                    registerStates[sourceIndex]->dirty = 0;
                }
            }
        }
    }
    else
    {
        // op1 var, op2 literal
        if (line->operandTypes[2] == vt_literal)
        {
            char didReorder = 0;
            if (line->reorderable)
            {
                destinationRegister = findTemp(registerStates, line->operands[1]);
                // only proceed with checks if the first operand exists in a register
                if (destinationRegister != -1)
                {
                    // if the register can be overwritten at this step (or the var is the same as the destination)
                    char canOverwriteDest = !registerStates[destinationRegister]->live || registerStates[destinationRegister]->dying;
                    if (canOverwriteDest || !strcmp(line->operands[0], line->operands[1]))
                    {
                        // if a register already contains the variable, purge it to avoid duplication
                        int oldIndex = findTemp(registerStates, line->operands[0]);
                        if (oldIndex != -1)
                        {
                            registerStates[oldIndex]->contains = NULL;
                            registerStates[oldIndex]->live = 0;
                            registerStates[oldIndex]->dirty = 0;
                        }
                        // just perform the operation on the existing value
                        char *outputStr = malloc(17 * sizeof(char));
                        sprintf(outputStr, "%s %%r%d, $%s", getAsmOp(line->operation), destinationRegister, line->operands[2]);
                        ASMblock_append(outputBlock, outputStr);
                        didReorder = 1;
                    }
                }
            }

            if (!didReorder)
            {
                if (line->operandTypes[0] == vt_temp)
                {
                    destinationRegister = findTemp(registerStates, line->operands[0]);
                    if (destinationRegister == -1)
                    {
                        destinationRegister = findUnallocatedRegister(registerStates);
                        modifyRegisterContents(registerStates, destinationRegister, line->operands[0]);
                    }
                    char *outputStr = malloc(15 * sizeof(char));
                    sprintf(outputStr, "movw %%r%d, %%r%d", destinationRegister, findOrPlaceVar(registerStates, outputBlock, line->operands[1], function));
                    ASMblock_append(outputBlock, outputStr);
                }
                else
                {
                    // move the first operand into a register
                    if (!strcmp(line->operands[0], line->operands[1]))
                    {
                        // if the variable is modifying itself, we can indiscriminately get a register for it
                        destinationRegister = findOrPlaceVar(registerStates, outputBlock, line->operands[0], function);
                    }
                    else
                    {
                        // otherwise we need to guarantee the register is modifiable
                        destinationRegister = findOrPlaceModifiableVar(registerStates, outputBlock, line->operands[1], function);
                    }
                }
                char *outputStr = malloc(17 * sizeof(char));
                sprintf(outputStr, "%s %%r%d, $%s", getAsmOp(line->operation), destinationRegister, line->operands[2]);
                ASMblock_append(outputBlock, outputStr);
            }
        }
        // op1 var, op2 var
        else
        {
            // check if we have something that looks like "c = d + d" or "d = d + d"
            if (!strcmp(line->operands[1], line->operands[2]))
            {
                // "d = d + d" case
                if (!strcmp(line->operands[0], line->operands[2]))
                {
                    destinationRegister = findOrPlaceVar(registerStates, outputBlock, line->operands[0], function);
                }
                // "c = d + d" case"
                else
                {
                    // no reason we can't just use this function to cheese the operand into the correct place
                    destinationRegister = generateAssignmentCode(line, registerStates, outputBlock, function);
                    // after that, it can self-modify just like the "d = d + d" case
                }
                char *outputStr = malloc(18 * sizeof(char));
                sprintf(outputStr, "%s %%r%d, %%r%d", getAsmOp(line->operation), destinationRegister, destinationRegister);
                ASMblock_append(outputBlock, outputStr);
            }
            else
            {
                // will always find the temp we need, or find/place a var/arg if necessary
                int firstOperandIndex = findOrPlaceVar(registerStates, outputBlock, line->operands[1], function);

                char didReorder = 0;
                // only bother trying to reorder if possible and the first operand exists and can't be overwritten at this step
                char canOverwrite = registerStates[firstOperandIndex]->live && !registerStates[firstOperandIndex]->dying;
                if (line->reorderable && ((firstOperandIndex != -1 && canOverwrite) || !strcmp(line->operands[0], line->operands[2])))
                {

                    destinationRegister = findOrPlaceModifiableVar(registerStates, outputBlock, line->operands[2], function);
                    char *outputStr = malloc(17 * sizeof(char));
                    sprintf(outputStr, "%s %%r%d, %%r%d", getAsmOp(line->operation), destinationRegister, firstOperandIndex);
                    ASMblock_append(outputBlock, outputStr);
                    didReorder = 1;
                }

                if (!didReorder)
                {
                    int secondOperandIndex = findOrPlaceVar(registerStates, outputBlock, line->operands[2], function);

                    destinationRegister = findOrPlaceModifiableVar(registerStates, outputBlock, line->operands[1], function);
                    char *outputStr = malloc(18 * sizeof(char));
                    sprintf(outputStr, "%s %%r%d, %%r%d", getAsmOp(line->operation), destinationRegister, secondOperandIndex);
                    ASMblock_append(outputBlock, outputStr);
                }
            }
        }
    }
    modifyRegisterContents(registerStates, destinationRegister, line->operands[0]);
}

struct ASMblock *generateCode(struct functionEntry *function, char *functionName, struct Lifetime *lifetimes)
{
    struct ASMblock *outputBlock = newASMblock();
    // generate register states for the 12 GP registers
    struct registerState *registerStates[12];
    for (int i = 0; i < 12; i++)
        registerStates[i] = newRegisterState();

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

        // printRegisterStates(registerStates);
        switch (line->operation)
        {
        case tt_assign:
            //printf("%s = %s %s\n", line->operands[0], line->operands[1], line->operands[2]);
            generateAssignmentCode(line, registerStates, outputBlock, function);
            break;

        case tt_push:
            outputStr = malloc(10 * sizeof(char));
            switch (line->operandTypes[0])
            {

            case vt_var:
                sprintf(outputStr, "push %%r%d", findOrPlaceVar(registerStates, outputBlock, line->operands[0], function));
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
            break;

        case tt_call:
            // if: %r0 is live OR %r0 contains a dirty variable (not a temp)
            if (registerStates[0]->live || (registerStates[0]->contains != NULL && registerStates[0]->contains[0] != '.' && registerStates[0]->dirty))
            {
                // right now just move it to an unallocated register
                relocateRegister(registerStates, outputBlock, 0, findUnallocatedRegister(registerStates));
            }
            // need a larger buffer because of labels
            outputStr = malloc(32 * sizeof(char));
            sprintf(outputStr, "call %s", line->operands[1]);
            ASMblock_append(outputBlock, outputStr);
            int existingVarIndex = findTemp(registerStates, line->operands[0]);
            if (existingVarIndex != -1)
            {
                registerStates[existingVarIndex]->live = 0;
                registerStates[existingVarIndex]->contains = NULL;
            }
            modifyRegisterContents(registerStates, 0, line->operands[0]);

            break;

        case tt_label:
            outputStr = malloc(32 * sizeof(char));
            sprintf(outputStr, "%s:", line->operands[0]);
            ASMblock_append(outputBlock, outputStr);
            break;

        case tt_return:
        {
            int retValIndex = findTemp(registerStates, ".RETVAL");
            if (retValIndex != 0)
            {
                relocateRegister(registerStates, outputBlock, retValIndex, 0);
            }
            outputStr = malloc(10 * sizeof(char));
            sprintf(outputStr, "ret %d", function->argc);
            ASMblock_append(outputBlock, outputStr);
        }
        break;

        default:
            //printf("%s = %s %s\n", line->operands[0], line->operands[1], line->operands[2]);
            generateArithmeticCode(line, registerStates, outputBlock, function);
            break;
        }
        //printRegisterStates(registerStates);
        //printf("\n\n");
        tacIndex++;
    }
    //printRegisterStates(registerStates);
    //printf("\n");
    // clean up the registers states
    struct ASMline *entryLabel = outputBlock->head;
    outputBlock->head = outputBlock->head->next;
    struct ASMline *returnLine = outputBlock->tail;
    struct ASMline *runner = outputBlock->head;
    while (runner->next->next != NULL)
    {
        runner = runner->next;
    }
    outputBlock->tail = runner;

    outputStr = malloc(32 * sizeof(char));
    sprintf(outputStr, "%s_done:", functionName);
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
    outputBlock->tail->next = returnLine;
    outputBlock->tail = returnLine;

    // need to get arg count for return statement
    for (int i = 0; i < 12; i++)
        free(registerStates[i]);

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
                if(runner->data[strlen(runner->data) - 1] != ':')
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
