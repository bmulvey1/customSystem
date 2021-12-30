#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "tac.h"
#include "symtab.h"

// walk the AST and generate a symbol table
struct symbolTable *walkAST(struct astNode *it)
{
    struct symbolTable *wip = newSymbolTable("Program");
    struct astNode *runner = it;
    while (runner != NULL)
    {
        switch (runner->type)
        {
        case t_var:
            if (runner->child->type == t_assign)
                symTab_insertVariable(wip, runner->child->child->value);
            else
                symTab_insertVariable(wip, runner->child->value);
            break;
        case t_fun:
        {
            struct astNode *functionNode = runner->child;
            struct symbolTable *subTable = newSymbolTable(functionNode->value);
            symTab_insertFunction(wip, functionNode->value, subTable);
            while (functionNode != NULL)
            {
                switch (functionNode->type)
                {
                case t_var:
                    if (functionNode->child->type == t_assign)
                        symTab_insertVariable(subTable, functionNode->child->child->value);
                    else
                        symTab_insertVariable(subTable, functionNode->child->value);

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
                            symTabInsert(subTable, argumentRunner->child->child->value, NULL, e_argument);
                        else
                            symTabInsert(subTable, argumentRunner->child->value, NULL, e_argument);

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

// given a node with an assignment, return the TAC block necessary to generate the correct value
struct tacLine *linearizeAssignment(struct astNode *it, int *tempNum)
{
    struct tacLine *assignment;
    if (it->child->sibling->child == NULL)
    {
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
    else
    {
        assignment = linearizeExpression(it->child->sibling, tempNum, 0);
        struct tacLine *lastLine = findLastTAC(assignment);
        lastLine->operands[0] = it->child->value;
        lastLine->operandTypes[0] = vt_var;
    }
    return assignment;
}

struct tacLine *linearizeFunction(struct astNode *it)
{
    struct astNode *runner = it;
    struct tacLine *asttac = NULL;
    int tempNum = 0;
    while (runner != NULL)
    {
        switch (runner->type)
        {
        case t_var:
            if (runner->child->type == t_assign)
            {
                asttac = appendTAC(asttac, linearizeAssignment(runner->child, &tempNum));
            }
            //else
            //printf("initialize variable [%s]\n", runner->child->value);

            break;

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

void linearizeProgram(struct astNode *it, struct symbolTable *table)
{
    struct astNode *runner = it;
    while (runner != NULL)
    {
        switch (runner->type)
        {
        case t_fun:
        {
            struct functionEntry *theFunction = symbolTableLookup(table, runner->child->value)->entry;
            struct tacLine *generatedTAC = linearizeFunction(runner->child->sibling);
            theFunction->codeBlock = generatedTAC;
            break;
        }
        break;
        default:
            break;
        }
        runner = runner->sibling;
    }
}

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

void checkVariableLifetimes(struct functionEntry *function)
{
    int tacIndex = 0;
    // iterate all lines of TAC in the function's codeblock
    for (struct tacLine *t = function->codeBlock; t != NULL; t = t->nextLine)
    {
        // only examine RHS operands if they are variables
        struct symTabEntry *varEntry;
        if (t->operandTypes[2] == vt_var)
        {
            varEntry = symbolTableLookup(function->table, t->operands[2]);
            
            if (varEntry == NULL)
            {
                printf("Error: use of undeclared variable %s\n", t->operands[2]);
                exit(1);
            }
            // ignore function arguments, their lifetimes will always be valid
            else if (varEntry->type != e_argument && !((struct variableEntry *)varEntry->entry)->isAssigned)
            {
                printf("Error: use of variable [%s] before assignment\n", varEntry->name);
                exit(1);
            }
            else if (varEntry->type != e_argument)
            {
                printf("Extended lifespan of variable %s to %d\n", varEntry->name, tacIndex);
                ((struct variableEntry *)varEntry->entry)->lsEnd = tacIndex;
            }
        }

        varEntry = symbolTableLookup(function->table, t->operands[1]);
        if (t->operandTypes[1] == vt_var)
        {
            if (varEntry == NULL)
            {
                printf("Error: use of undeclared variable %s\n", t->operands[1]);
                exit(1);
            }
            // ignore function arguments, their lifetimes will always be valid
            else if (varEntry->type != e_argument && !((struct variableEntry *)varEntry->entry)->isAssigned)
            {
                printf("Error: use of variable [%s] before assignment\n", varEntry->name);
                exit(1);
            }
            else if (varEntry->type != e_argument)
            {
                ((struct variableEntry *)varEntry->entry)->lsEnd = tacIndex;
            }
        }

        if (t->operandTypes[0] == vt_var)
        {
            struct symTabEntry *assignedVar = symbolTableLookup(function->table, t->operands[0]);
            if (assignedVar == NULL)
            {
                printf("Error: assignment to uninitialized variable %s\n", t->operands[0]);
                exit(1);
            }
            struct variableEntry *theVar = assignedVar->entry;
            if (!theVar->isAssigned)
            {
                theVar->isAssigned = 1;
                theVar->lsStart = tacIndex;
            }

            theVar->lsEnd = tacIndex;
        }

        tacIndex++;
    }

    printf("exitgint\n");

    /*// iterate all entries in the symbol table
        for (int e = 0; e < function->table->size; e++)
        {
            struct symTabEntry *theEntry = function->table->entries[e];
            // if looking at a variable entry
            if (theEntry->type == e_variable)
            {
                printf("looking at variable %s\n", theEntry->name);
                // examine the two operands that are read from to make sure they have been assigned
                if (!t->literals[0] && !strcmp(theEntry->name, t->operands[2]))
                {
                    varFound = 1;
                    if (!((struct variableEntry *)theEntry->entry)->isAssigned)
                    {
                        printf("Error: use of variable [%s] before assignment\n", theEntry->name);
                        exit(1);
                    }
                    else
                    {
                        printf("Extended lifespan of variable %s to %d\n", theEntry->name, tacIndex);
                        ((struct variableEntry *)theEntry->entry)->lsEnd = tacIndex;
                    }
                }
                if (!t->literals[1] && !strcmp(theEntry->name, t->operands[1]))
                {
                    varFound = 1;
                    if (!((struct variableEntry *)theEntry->entry)->isAssigned)
                    {
                        printf("Error: use of variable [%s] before assignment\n", theEntry->name);
                        exit(1);
                    }
                    else
                    {
                        printf("Extended lifespan of variable %s to %d\n", theEntry->name, tacIndex);

                        ((struct variableEntry *)theEntry->entry)->lsEnd = tacIndex;
                    }
                }
                if (!strcmp(theEntry->name, t->operands[0]))
                {
                    printf("Saw assignment to variable %s at %d\n", theEntry->name, tacIndex);
                    ((struct variableEntry *)theEntry->entry)->lsStart = tacIndex;
                    ((struct variableEntry *)theEntry->entry)->lsEnd = tacIndex;
                    ((struct variableEntry *)theEntry->entry)->isAssigned = 1;
                }
            }
        }*/
}

void findLifetimes(struct functionEntry *function)
{
    printf("EVALUATING VARIABLE LIFETIMES FOR %s\n", function->table->name);
    checkVariableLifetimes(function);
    printf("now calculating temporary variable lifetimes\n");
    struct Lifetime *ltList = NULL;
    struct Lifetime *ltTail = NULL;
    int lineIndex = 0;
    for (struct tacLine *line = function->codeBlock; line != NULL; line = line->nextLine)
    {
        for (int i = 0; i < 3; i++)
        {
            char *varName = line->operands[i];
            if (line->operands[i] == NULL)
                continue;

            if (i > 0)
                if (line->operandTypes[i] == vt_literal)
                    continue;

            int found = 0;
            for (struct Lifetime *runner = ltList; runner != NULL; runner = runner->next)
            {
                if (!strcmp(varName, runner->variable))
                {
                    runner->end = lineIndex;
                    found = 1;
                }
            }
            if (!found)
            {
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
    }
    //printTacLine(function->codeBlock);
    //for (struct Lifetime *runner = ltList; runner != NULL; runner = runner->next)
    //{
    // printf("%s: %d - %d\n", runner->variable, runner->start, runner->end);
    //}
}

int main(int argc, char **argv)
{
    printf("%s\n", argv[1]);

    struct astNode *program = parseProgram(argv[1]);
    printf("DONE PARSING PROGRAM\n");
    // printAST(program, 0);

    struct symbolTable *theTable = walkAST(program);
    linearizeProgram(program, theTable);
    printf("DONE LINEARIZING TO TAC\n");

    for (int i = 0; i < theTable->size; i++)
    {
        if (theTable->entries[i]->type == e_function)
        {
            findLifetimes(theTable->entries[i]->entry);
        }
    }

    printSymTab(theTable);

    // printTacLine(head);

    printf("done printing\n");
}