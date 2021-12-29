#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "tac.h"
#include "symtab.h"

// walk the AST and generate a symbol table
struct symbolTable *walkAST(struct astNode *it)
{
    printf("Walking ast...\n");
    struct symbolTable *wip = newSymbolTable("Program");
    struct astNode *runner = it;
    while (runner != NULL)
    {
        switch (runner->type)
        {
        case t_var:
            printf("walking variable\n");
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
                    printf("examining function arguments\n");
                    while (argumentRunner != NULL)
                    {
                        if (argumentRunner->child->type == t_assign)
                            symTab_insertVariable(subTable, argumentRunner->child->child->value);
                        else
                            symTab_insertVariable(subTable, argumentRunner->child->value);

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
~~~~~Symbol table for [Program]
> variable [a]: (lifespan 0:0)
> variable [b]: (lifespan 0:0)
> function [qwerty]: 6 symbols
    ~~~~~Symbol table for [qwerty]
    > variable [x]: (lifespan 0:0)
    > variable [y]: (lifespan 0:0)
    > variable [z]: (lifespan 0:0)
    > variable [c]: (lifespan 0:0)
    > variable [d]: (lifespan 0:0)
    > variable [f]: (lifespan 0:0)

     0:   temp3 =        b  -        1
     1:   temp2 =        2  +    temp3
     2:   temp1 =        b  -        a
     3:       c =    temp1  +    temp2
     4:   temp6 =        d  +        z
     5:   temp5 =        c  +        a
     6:       f =    temp5  -    temp6
> function [notFun]: 3 symbols
    ~~~~~Symbol table for [notFun]
    > variable [argument1]: (lifespan 0:0)
    > variable [argument2]: (lifespan 0:0)
    > variable [compute]: (lifespan 0:0)

     0:   temp1 = argument2  -        1
     1: compute = argument1  +    temp1

done printing







*/
/*
 * These functions walk the AST and convert it to three-address code
 */
struct tacLine *linearizeExpression(struct astNode *it, int *tempNum, int depth)
{
    struct tacLine *thisExpression = newtacLine();
    struct tacLine *returnExpression = NULL;

    /*
     * sort of hacky solution to dealing with parsing only the right hand side of the AST when depth = 0
     */
    if (depth == 0 && it->child->sibling->type == t_unOp)
    {
        return linearizeExpression(it->child->sibling, tempNum, depth + 1);
    }

    // increment count of temp variables, the parse of this expression will be written to a temp
    thisExpression->addresses[0] = malloc(4 * sizeof(char));
    sprintf(thisExpression->addresses[0], "t%d", *tempNum);

    (*tempNum)++;

    // check left child
    if (it->child->type == t_unOp)
    {
        // found another operator, so will need to use more temporary variables (recurse deeper)
        //  allocate enough space to store 'txx' - will break with >99 temps
        thisExpression->addresses[1] = malloc(4 * sizeof(char));
        sprintf(thisExpression->addresses[1], "t%d", *tempNum);
        returnExpression = prependTAC(thisExpression, linearizeExpression(it->child, tempNum, depth + 1));
    }
    else
    {
        // otherwise we just have an existing variable, write it to the corresponding address in the TAC line
        thisExpression->addresses[1] = it->child->value;
    }
    thisExpression->operation = it->value;

    // check right child, same as left but with correct address index
    if (it->child->sibling->type == t_unOp)
    {
        thisExpression->addresses[2] = malloc(4 * sizeof(char));
        sprintf(thisExpression->addresses[2], "t%d", *tempNum);

        // when recursing, we need to prepend this line so things happen in the correct order
        // figure out what to prepend to, then set the return expression TACline to the head of the block
        if (returnExpression != NULL)
            returnExpression = prependTAC(returnExpression, linearizeExpression(it->child->sibling, tempNum, depth + 1));
        else
            returnExpression = prependTAC(thisExpression, linearizeExpression(it->child->sibling, tempNum, depth + 1));
    }
    else
    {
        thisExpression->addresses[2] = it->child->sibling->value;
    }

    // if we have prepended, make sure to return the first line of TAC
    // otherwise there is only one line of TAC to return, so return that
    if (returnExpression != NULL)
        return returnExpression;
    else
        return thisExpression;
}

struct tacLine *linearizeFunction(struct astNode *it)
{
    struct astNode *runner = it;
    struct tacLine *asttac = NULL;
    int tempNum = 0;
    while (runner != NULL)
    {
        struct tacLine *thisBlock;
        switch (runner->type)
        {
        case t_var:
            if (runner->child->type == t_assign)
            {
                // if there is an assignment we need to linearize the right side
                thisBlock = linearizeExpression(runner->child, &tempNum, 0);
                findLastTAC(thisBlock)->addresses[0] = runner->child->child->value;
                asttac = appendTAC(asttac, thisBlock);
            }
            else
                printf("initialize variable [%s]\n", runner->child->value);

            break;

        case t_assign:
            thisBlock = linearizeExpression(runner, &tempNum, 0);
            findLastTAC(thisBlock)->addresses[0] = runner->child->value;
            asttac = appendTAC(asttac, thisBlock);
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
    runner = runner->sibling;
    while (runner != NULL)
    {
        switch (runner->type)
        {
        case t_fun:
        {
            struct functionEntry *theFunction = symbolTableLookup(table, runner->child->value)->entry;
            struct tacLine *generatedTAC = linearizeFunction(runner->child->sibling);
            printf("\n");
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

void findLifetimes(struct functionEntry *function)
{
    struct Lifetime *ltList = NULL;
    struct Lifetime *ltTail = NULL;
    int lineIndex = 0;
    for (struct tacLine *line = function->codeBlock; line != NULL; line = line->nextLine)
    {
        for (int i = 0; i < 3; i++)
        {
            char *varName = line->addresses[i];

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

    printTacLine(function->codeBlock);
    for (struct Lifetime *runner = ltList; runner != NULL; runner = runner->next)
    {
        printf("%s: %d - %d\n", runner->variable, runner->start, runner->end);
    }
}

int main(int argc, char **argv)
{
    printf("%s\n", argv[1]);

    struct astNode *program = parseProgram(argv[1]);
    printf("DONE PARSING PROGRAM\n");
    // printAST(program, 0);

    struct symbolTable *theTable = walkAST(program);
    linearizeProgram(program, theTable);
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