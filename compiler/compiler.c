#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "tac.h"
#include "symtab.h"

// given an AST node for a program, walk the AST and generate a symbol table for the entire thing
struct symbolTable *walkAST(struct astNode *it)
{
    struct symbolTable *wip = newSymbolTable("Program");
    wip->tl = newTempList();
    int globalVariableCount = 0;
    struct astNode *runner = it;
    while (runner != NULL)
    {
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
            struct symbolTable *subTable = newSymbolTable(functionNode->value);
            symTab_insertFunction(wip, functionNode->value, subTable);
            while (functionNode != NULL)
            {
                switch (functionNode->type)
                {
                case t_var:
                    if (functionNode->child->type == t_assign)
                        symTab_insertVariable(subTable, functionNode->child->child->value, functionVariableCount++);
                    else
                        symTab_insertVariable(subTable, functionNode->child->value, functionVariableCount++);

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
                            symTab_insertArgument(subTable, argumentRunner->child->child->value, functionArgumentCount++);
                        else
                            symTab_insertArgument(subTable, argumentRunner->child->value, functionArgumentCount++);

                        argumentRunner = argumentRunner->sibling;
                    }
                    break;

                default:
                    printf("Error walking AST - expected 'v' or name, saw %s with value of [%s]\n", getTokenName(functionNode->type), functionNode->value);
                    exit(1);
                }
                functionNode = functionNode->sibling;
            }
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
 * These functions walk the AST and convert it to three-address code
 */

// given an AST node of an expression, figure out how to break it down into multiple lines of three address code
struct tacLine *linearizeExpression(struct astNode *it, int *tempNum, struct tempList *tl)
{
    struct tacLine *thisExpression = newtacLine();
    struct tacLine *returnExpression = NULL;

    thisExpression->operands[0] = getTempString(tl, *tempNum);
    // increment count of temp variables, the parse of this expression will be written to a temp
    (*tempNum)++;

    // check left child
    if (it->child->type == t_unOp)
    {
        thisExpression->operandTypes[1] = vt_temp;
        thisExpression->operands[1] = getTempString(tl, *tempNum);
        returnExpression = prependTAC(thisExpression, linearizeExpression(it->child, tempNum, tl));
    }
    else
    {
        // otherwise we just have an existing variable, write it to the corresponding address in the TAC line
        thisExpression->operands[1] = it->child->value;
        switch (it->child->type)
        {
        case t_name:
            thisExpression->operandTypes[1] = vt_var;
            break;

        case t_literal:
            thisExpression->operandTypes[1] = vt_literal;
            break;

        default:
            printf("Unexpected type seen while linearizing expression!\n Expected variable or literal\n");
            exit(1);
        }
    }
    switch (it->value[0])
    {
    case '+':
        thisExpression->reorderable = 1;
        thisExpression->operation = tt_add;
        break;

    case '-':
        thisExpression->operation = tt_subtract;
        break;
    }
    // thisExpression->operation = it->value;

    // check right child, same as left but with correct address index
    if (it->child->sibling->type == t_unOp)
    {
        thisExpression->operands[2] = getTempString(tl, *tempNum);
        thisExpression->operandTypes[2] = vt_temp;

        // when recursing, we need to prepend this line so things happen in the correct order
        // figure out what to prepend to, then set the return expression TACline to the head of the block
        if (returnExpression != NULL)
            returnExpression = prependTAC(returnExpression, linearizeExpression(it->child->sibling, tempNum, tl));
        else
            returnExpression = prependTAC(thisExpression, linearizeExpression(it->child->sibling, tempNum, tl));
    }
    else
    {
        thisExpression->operands[2] = it->child->sibling->value;
        switch (it->child->sibling->type)
        {
        case t_name:
            thisExpression->operandTypes[2] = vt_var;
            break;

        case t_literal:
            thisExpression->operandTypes[2] = vt_literal;
            break;

        default:
            printf("Unexpected type seen while linearizing expression!\n Expected variable or literal\n");
            exit(1);
        }
    }

    // if we have prepended, make sure to return the first line of TAC
    // otherwise there is only one line of TAC to return, so return that
    if (returnExpression == NULL)
        returnExpression = thisExpression;

    return returnExpression;
}

// given an AST node of an assignment, return the TAC block necessary to generate the correct value
struct tacLine *linearizeAssignment(struct astNode *it, int *tempNum, struct tempList *tl)
{
    struct tacLine *assignment;

    // if this assignment is simply setting one thing to another
    if (it->child->sibling->child == NULL)
    {
        // pull out the relevant values and generate a single line of TAC to return
        assignment = newtacLine();
        assignment->operands[0] = it->child->value;
        assignment->operands[1] = it->child->sibling->value;
        assignment->operandTypes[0] = vt_var;
        assignment->operandTypes[2] = vt_null;

        switch (it->child->sibling->type)
        {
        case t_literal:
            assignment->operandTypes[1] = vt_literal;
            break;

        case t_name:
            assignment->operandTypes[1] = vt_var;
            break;

        default:
            printf("Error parsing simple assignment - unexpected type on RHS\n");
            exit(1);
        }
    }
    // otherwise there is some sort of expression, which will need to be broken down into multiple lines of TAC
    else
    {
        assignment = linearizeExpression(it->child->sibling, tempNum, tl);
        struct tacLine *lastLine = findLastTAC(assignment);

        // set the final line's operand 0 to the variable actually being assigned
        lastLine->operands[0] = it->child->value;
        lastLine->operandTypes[0] = vt_var;
    }

    return assignment;
}

// given the AST for a function, generate TAC and return a pointer to the head of the generated block
struct tacLine *linearizeFunction(struct astNode *it, struct tempList *tl)
{
    // scrape along the function
    struct astNode *runner = it;
    struct tacLine *asttac = NULL;
    int tempNum = 0; // track the number of temporary variables used
    while (runner != NULL)
    {
        switch (runner->type)
        {

        // if we see a variable being declared and then assigned
        // generate the code and stick it on to the end of the block
        case t_var:
            if (runner->child->type == t_assign)
                asttac = appendTAC(asttac, linearizeAssignment(runner->child, &tempNum, tl));

            break;

        // if we see an assignment, generate the code and stick it on to the end of the block
        case t_assign:
            asttac = appendTAC(asttac, linearizeAssignment(runner, &tempNum, tl));
            break;

        default:
            printf("Something broke :(\n");
            exit(1);
        }
        runner = runner->sibling;
    }
    return asttac;
}

// given an AST and a populated symbol table, generate three address code for the function entries
void linearizeProgram(struct astNode *it, struct symbolTable *table)
{
    // scrape along the top level of the AST
    struct astNode *runner = it;
    while (runner != NULL)
    {
        switch (runner->type)
        {
        // if we encounter a function, lookup its symbol table entry
        // then generate the TAC for it and add a reference to the start of the generated code to the function entry
        case t_fun:
        {
            struct functionEntry *theFunction = symbolTableLookup(table, runner->child->value)->entry;
            struct tacLine *generatedTAC = linearizeFunction(runner->child->sibling, table->tl);
            theFunction->codeBlock = generatedTAC;
            break;
        }
        break;

        // ignore everything else
        default:
            break;
        }
        runner = runner->sibling;
    }
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
            // ignore function arguments
            case e_argument:
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
        varEntry = symbolTableLookup(function->table, t->operands[1]);
        if (t->operandTypes[1] == vt_var)
        {
            if (varEntry == NULL)
            {
                printf("Error: use of undeclared variable %s\n", t->operands[1]);
                exit(1);
            }
            switch (varEntry->type)
            {
            case e_argument:
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

            // grab the variable entry itself
            struct variableEntry *theVar = assignedVar->entry;
            // if it hasn't already been assigned
            if (!theVar->isAssigned)
            {
                // set the assigned flag and record the start of its lifetime
                theVar->isAssigned = 1;
                theVar->lsStart = lineIndex;
            }

            // regardless of whether or not this variable's lifetime started at this line
            // its lifetime can end no sooner than this index
            theVar->lsEnd = lineIndex;
        }

        lineIndex++;
    }
}

struct Lifetime *findLifetimes(struct functionEntry *function)
{
    printf("EVALUATING VARIABLE LIFETIMES FOR %s\n", function->table->name);

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
                    ltTail = ltList;
                }
                else
                {
                    ltTail->next = newLifetime(varName, lineIndex);
                    ltTail = ltTail->next;
                }
            }
        }
        lineIndex++;
    }

    /*// print TAC lines with active variables at that time next to them
    lineIndex = 0;
    for (struct tacLine *line = function->codeBlock; line != NULL; line = line->nextLine)
    {
        printf("\t%2d:%8s = %8s %2s %8s [", lineIndex, line->operands[0], line->operands[1], line->operation, line->operands[2]);
        for (struct Lifetime *runner = ltList; runner != NULL; runner = runner->next)
        {
            if (lineIndex >= runner->start && lineIndex <= runner->end)
            {
                printf("%s ", runner->variable);
            }
        }
        printf("]\n");
        lineIndex++;
    }*/
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
    char *contains;
};

struct registerState *newRegisterState()
{
    struct registerState *wip = malloc(sizeof(struct registerState));
    wip->live = 0;
    wip->dying = 0;
    wip->dirty = 0;
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
int findOrPlaceVar(struct registerState **registerStates, char *var, struct functionEntry *function)
{
    for (int i = 0; i < 12; i++)
        if (registerStates[i]->contains != NULL)
            if (!strcmp(registerStates[i]->contains, var))
                return i;

    int freshRegisterIndex = findUnallocatedRegister(registerStates);

    printf("movw r%d, %d(rbp)\n", freshRegisterIndex, findStackOffset(var, function));
    modifyRegisterContents(registerStates, freshRegisterIndex, var);
    return freshRegisterIndex;
}

/*
 * Find what register a variable lives in
 * if it isn't currently in a register, retrieve it from the stack
 */
int findOrPlaceModifiableVar(struct registerState **registerStates, char *var, struct functionEntry *function)
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
            printf("movw r%d, r%d\n", registerIndex, sourceIndex);
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
        printf("movw r%d, %d(rbp)\n", registerIndex, findStackOffset(var, function));
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
int generateAssignmentCode(struct tacLine *line, struct registerState **registerStates, struct functionEntry *function)
{
    int destinationIndex;
    if (line->operandTypes[1] == vt_literal)
    {
        destinationIndex = findTemp(registerStates, line->operands[0]);
        if (destinationIndex == -1)
        {
            destinationIndex = findUnallocatedRegister(registerStates);
        }
        printf("movw r%d, %s\n", destinationIndex, line->operands[1]);
    }
    else
    {
        int sourceIndex = findOrPlaceVar(registerStates, line->operands[1], function);
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
            printf("movw r%d, r%d\n", destinationIndex, sourceIndex);
        }
    }
    modifyRegisterContents(registerStates, destinationIndex, line->operands[0]);

    return destinationIndex;
}

void generateArithmeticCode(struct tacLine *line, struct registerState **registerStates, struct functionEntry *function)
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
                        printf("%s r%d, %s\n", getAsmOp(line->operation), destinationRegister, line->operands[1]);
                        didReorder = 1;
                    }
                }
            }

            if (!didReorder)
            {
                // move the first operand into a register
                destinationRegister = findUnallocatedRegister(registerStates);
                int sourceIndex = findOrPlaceVar(registerStates, line->operands[2], function);
                printf("movw r%d, %s\n", destinationRegister, line->operands[1]);
                printf("%s r%d, r%d\n", getAsmOp(line->operation), destinationRegister, sourceIndex);

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
                        printf("%s r%d, %s\n", getAsmOp(line->operation), destinationRegister, line->operands[2]);
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
                    }
                    printf("movw r%d, r%d\n", destinationRegister, findOrPlaceVar(registerStates, line->operands[1], function));
                }
                else
                {
                    // move the first operand into a register
                    if (!strcmp(line->operands[0], line->operands[1]))
                    {
                        // if the variable is modifying itself, we can indiscriminately get a register for it
                        destinationRegister = findOrPlaceVar(registerStates, line->operands[0], function);
                    }
                    else
                    {
                        // otherwise we need to guarantee the register is modifiable
                        destinationRegister = findOrPlaceModifiableVar(registerStates, line->operands[0], function);
                    }
                }

                printf("%s r%d, %s\n", getAsmOp(line->operation), destinationRegister, line->operands[2]);
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
                    destinationRegister = findOrPlaceVar(registerStates, line->operands[0], function);
                }
                // "c = d + d" case"
                else
                {
                    // no reason we can't just use this function to cheese the operand into the correct place
                    destinationRegister = generateAssignmentCode(line, registerStates, function);
                    // after that, it can self-modify just like the "d = d + d" case
                }
                printf("%s r%d, r%d\n", getAsmOp(line->operation), destinationRegister, destinationRegister);
            }
            else
            {
                // will always find the temp we need, or find/place a var/arg if necessary
                int firstOperandIndex = findOrPlaceVar(registerStates, line->operands[1], function);

                char didReorder = 0;
                // only bother trying to reorder if possible and the first operand exists and can't be overwritten at this step
                char canOverwrite = registerStates[firstOperandIndex]->live && !registerStates[firstOperandIndex]->dying;
                if (line->reorderable && ((firstOperandIndex != -1 && canOverwrite) || !strcmp(line->operands[0], line->operands[2])))
                {
                    destinationRegister = findOrPlaceModifiableVar(registerStates, line->operands[2], function);
                    printf("%s r%d, r%d\n", getAsmOp(line->operation), destinationRegister, firstOperandIndex);
                    didReorder = 1;
                }

                if (!didReorder)
                {
                    int secondOperandIndex = findOrPlaceVar(registerStates, line->operands[2], function);

                    // seems there are no side effects of just always doing the "findOrPlaceModifiable()"
                    /*if (firstOperandIndex != -1)
                    {
                        // if this register isn't dying or dead (can't be overwritten at this step)
                        if (!(registerStates[firstOperandIndex]->dying || !registerStates[firstOperandIndex]->live))
                        {
                            destinationRegister = findUnallocatedRegister(registerStates);
                            printf("movw r%d, r%d\n", destinationRegister, firstOperandIndex);
                        }
                        else
                        {
                            destinationRegister = firstOperandIndex;
                        }
                        printf("%s r%d, r%d\n", getAsmOp(line->operation), destinationRegister, secondOperandIndex);
                    }
                    else
                    {*/
                    destinationRegister = findOrPlaceModifiableVar(registerStates, line->operands[1], function);
                    printf("%s r%d, r%d\n", getAsmOp(line->operation), destinationRegister, secondOperandIndex);
                    //}
                }
            }
        }
    }
    modifyRegisterContents(registerStates, destinationRegister, line->operands[0]);
}

void generateCode(struct functionEntry *function, struct Lifetime *lifetimes)
{
    printf("Generating target code for function %s\n", function->table->name);

    // generate register states for the 12 GP registers
    struct registerState *registerStates[12];
    for (int i = 0; i < 12; i++)
        registerStates[i] = newRegisterState();

    printf("doin the allocation\n");
    int tacIndex = 0;
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
            generateAssignmentCode(line, registerStates, function);
            break;

        case tt_label:
            printf("emit label\n");
            break;

        default:
            // printf("%s = %s %s\n", line->operands[0], line->operands[1], line->operands[2]);
            generateArithmeticCode(line, registerStates, function);
            break;
        }
        // printRegisterStates(registerStates);
        // printf("\n\n");
        tacIndex++;
    }
    printRegisterStates(registerStates);
    
    // clean up the registers states
    for (int i = 0; i < 12; i++)
        free(registerStates[i]);

}

int main(int argc, char **argv)
{
    printf("%s\n", argv[1]);

    struct astNode *program = parseProgram(argv[1]);
    printf("DONE PARSING PROGRAM\n");

    struct symbolTable *theTable = walkAST(program);
    linearizeProgram(program, theTable);
    printf("DONE LINEARIZING TO TAC\n");
    printSymTab(theTable);
    for (int i = 0; i < theTable->size; i++)
    {
        if (theTable->entries[i]->type == e_function)
        {
            struct Lifetime *theseLifetimes = findLifetimes(theTable->entries[i]->entry);
            generateCode(theTable->entries[i]->entry, theseLifetimes);
            freeLifetime(theseLifetimes);
        }
    }
    freeAST(program);
    freeSymTab(theTable);

    // printTacLine(head);

    printf("done printing\n");
}
