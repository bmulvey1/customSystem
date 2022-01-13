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
struct tacLine *linearizeExpression(struct astNode *it, int *tempNum, int depth)
{
    struct tacLine *thisExpression = newtacLine();
    struct tacLine *returnExpression = NULL;

    // increment count of temp variables, the parse of this expression will be written to a temp
    thisExpression->operands[0] = malloc(5 * sizeof(char));
    thisExpression->operandTypes[0] = vt_temp;
    sprintf(thisExpression->operands[0], ".t%d", *tempNum);

    (*tempNum)++;

    // check left child
    if (it->child->type == t_unOp)
    {
        // found another operator, so will need to use more temporary variables (recurse deeper)
        //  allocate enough space to store 'txx' - will break with >99 temps
        thisExpression->operands[1] = malloc(5 * sizeof(char));
        thisExpression->operandTypes[1] = vt_temp;
        sprintf(thisExpression->operands[1], ".t%d", *tempNum);
        returnExpression = prependTAC(thisExpression, linearizeExpression(it->child, tempNum, depth + 1));
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
    thisExpression->operation = it->value;
    printf("%s, %ld\n", thisExpression->operation, strlen(thisExpression->operation));

    // check right child, same as left but with correct address index
    if (it->child->sibling->type == t_unOp)
    {
        thisExpression->operands[2] = malloc(5 * sizeof(char));
        sprintf(thisExpression->operands[2], ".t%d", *tempNum);
        thisExpression->operandTypes[2] = vt_temp;

        // when recursing, we need to prepend this line so things happen in the correct order
        // figure out what to prepend to, then set the return expression TACline to the head of the block
        if (returnExpression != NULL)
            returnExpression = prependTAC(returnExpression, linearizeExpression(it->child->sibling, tempNum, depth + 1));
        else
            returnExpression = prependTAC(thisExpression, linearizeExpression(it->child->sibling, tempNum, depth + 1));
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
struct tacLine *linearizeAssignment(struct astNode *it, int *tempNum)
{
    struct tacLine *assignment;

    // if this assignment is simply setting one thing to another
    if (it->child->sibling->child == NULL)
    {
        // pull out the relevant values and generate a single line of TAC to return
        assignment = newtacLine();
        assignment->operands[0] = it->child->value;
        assignment->operands[1] = it->child->sibling->value;
        if (it->child->sibling->type == t_literal)
        {
            assignment->operandTypes[0] = vt_var;
            assignment->operandTypes[1] = vt_literal;
            assignment->operandTypes[2] = vt_null;
        }
    }
    // otherwise there is some sort of expression, which will need to be broken down into multiple lines of TAC
    else
    {
        assignment = linearizeExpression(it->child->sibling, tempNum, 0);
        struct tacLine *lastLine = findLastTAC(assignment);

        // set the final line's operand 0 to the variable actually being assigned
        lastLine->operands[0] = it->child->value;
        lastLine->operandTypes[0] = vt_var;
    }

    return assignment;
}

// given the AST for a function, generate TAC and return a pointer to the head of the generated block
struct tacLine *linearizeFunction(struct astNode *it)
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
                asttac = appendTAC(asttac, linearizeAssignment(runner->child, &tempNum));

            break;

        // if we see an assignment, generate the code and stick it on to the end of the block
        case t_assign:
            asttac = appendTAC(asttac, linearizeAssignment(runner, &tempNum));
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
            struct tacLine *generatedTAC = linearizeFunction(runner->child->sibling);
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
    return wip;
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
    char live;
    char dirty;
    char *contains;
};

struct registerState *newRegisterState()
{
    struct registerState *wip = malloc(sizeof(struct registerState));
    wip->live = 0;
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
    registerStates[index]->live = 1;
    if (registerStates[index]->contains != NULL && !strcmp(registerStates[index]->contains, contents))
    {
        registerStates[index]->dirty = 1;
    }
    else
    {
        registerStates[index]->contains = contents;
        registerStates[index]->dirty = 0;
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
        if (registerStates[registerIndex]->live)
        {
            int sourceIndex = registerIndex;
            registerIndex = findUnallocatedRegister(registerStates);
            printf("movw r%d, r%d\n", registerIndex, sourceIndex);
            modifyRegisterContents(registerStates, registerIndex, var);
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

int findModifiableTemp(struct registerState **registerStates, char *var)
{
    int registerIndex = -1;
    for (int i = 0; i < 12; i++)
        if (registerStates[i]->contains != NULL)
            if (!strcmp(registerStates[i]->contains, var))
                registerIndex = i;

    if (registerIndex == -1)
    {
        printf("Error finding temp %s\n", var);
        exit(1);
    }
    if (registerStates[registerIndex]->live)
    {
        int sourceIndex = registerIndex;
        registerIndex = findUnallocatedRegister(registerStates);
        printf("movw r%d, r%d\n", registerIndex, sourceIndex);
        modifyRegisterContents(registerStates, registerIndex, var);
    }

    return registerIndex;
}

void printRegisterStates(struct registerState **registerStates)
{
    printf("----------\n");
    for (int i = 0; i < 12; i++)
    {
        if (registerStates[i]->contains != NULL)
            printf("r%2d    |", i);
    }
    printf("\n");
    for (int i = 0; i < 12; i++)
    {
        if (registerStates[i]->contains != NULL)
            printf("%4s %c%c|", registerStates[i]->contains, (registerStates[i]->live ? 'L' : 'D'), (registerStates[i]->dirty ? 'D' : ' '));
    }
    printf("\n----------\n");
}

/*
 * NOTES/TODO
 * CRITICAL BUG:
 * for direct modification of variables (d = d + 1)
 *  - 'findOrPlaceModifiableVar()' will duplicate the variable we're trying to manipulate
 *  - need to implement check for whether TAC operand and destination are identical!
 * 
 * 
 * This code generator makes NO consideration of when it may be possible to re-order operands
 * - Implementing this in some way will probably reduce registers with more complex operations
 * - will definitely generate higher-quality code with less loads/stores
 * 
 * 
 */

void emitInstruction(struct tacLine *line, struct registerState **registerStates, int destinationRegister, struct functionEntry *function)
{
    //printf("emit instruction %s\n", line->operation);
    if (line->operation == NULL)
    {
        return;
    }
    char *operation;
    switch (line->operation[0])
    {
    case '+':
        switch (line->operation[1])
        {
        case '\0':
            operation = "add";
            break;
        }
        break;
    case '-':
        switch (line->operation[1])
        {
        case '\0':
            operation = "sub";
            break;
        }
        break;
    }

    switch (line->operandTypes[2])
    {
    case vt_var:
        printf("%s r%d, r%d\n", operation, destinationRegister, findOrPlaceVar(registerStates, line->operands[2], function));
        break;

    case vt_temp:
        printf("%s r%d, r%d\n", operation, destinationRegister, findTemp(registerStates, line->operands[2]));
        break;

    case vt_literal:
        printf("%s r%d, %s\n", operation, destinationRegister, line->operands[2]);
        break;

    // will never hit this case (yet) becuase function returns upon seeing null as the operation
    // will need to implement for single-operand operations (++, --, *, & etc...)
    case vt_null:
        //printf("%s r%d\n", operation, destinationRegister);
        break;
    }
}

void generateCode(struct functionEntry *function, struct Lifetime *lifetimes)
{
    printf("Generating target code for function %s\n", function->table->name);

    int lineIndex = 0;
    for (struct tacLine *line = function->codeBlock; line != NULL; line = line->nextLine)
    {
        printf("\t%2d:%8s = %8s %2s %8s [", lineIndex, line->operands[0], line->operands[1], line->operation, line->operands[2]);
        for (struct Lifetime *runner = lifetimes; runner != NULL; runner = runner->next)
        {
            if (lineIndex >= runner->start && lineIndex <= runner->end)
            {
                printf("%s ", runner->variable);
            }
        }
        printf("]\n");
        lineIndex++;
    }

    struct registerState *registerStates[12];
    for (int i = 0; i < 12; i++)
        registerStates[i] = newRegisterState();

    printf("doin the allocation\n");
    int tacIndex = 0;
    for (struct tacLine *line = function->codeBlock; line != NULL; line = line->nextLine)
    {
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
        }

        int destinationRegister;
        switch (line->operandTypes[1])
        {

        case vt_literal:
            destinationRegister = findUnallocatedRegister(registerStates);
            printf("movw r%d, %s\n", destinationRegister, line->operands[1]);
            emitInstruction(line, registerStates, destinationRegister, function);
            modifyRegisterContents(registerStates, destinationRegister, line->operands[0]);
            break;

        case vt_var:
            destinationRegister = findOrPlaceModifiableVar(registerStates, line->operands[1], function);
            emitInstruction(line, registerStates, destinationRegister, function);
            modifyRegisterContents(registerStates, destinationRegister, line->operands[0]);

            //printf("%s r%d, r%d\n", line->operation, firstOperandIndex, findOrPlaceVar(registerStates, line->operands[2], function));
            break;

        case vt_temp:
            destinationRegister = findModifiableTemp(registerStates, line->operands[1]);
            emitInstruction(line, registerStates, destinationRegister, function);
            modifyRegisterContents(registerStates, destinationRegister, line->operands[0]);

            //printf("%s r%d, r%d\n", line->operation, firstOperandIndex, findOrPlaceVar(registerStates, line->operands[2], function));
            break;

        case vt_null:
            printf("Error: null detected as operand 1 of TAC line!\n");
            exit(1);
            break;
        };

        tacIndex++;
    }
    printRegisterStates(registerStates);
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
        }
    }

    // printTacLine(head);

    printf("done printing\n");
}
