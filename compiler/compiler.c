#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "tac.h"
#include "symtab.h"
/*
 * SYMBOL TABLE utilities
 *
 */
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

// this function is going to get very unpleasant
struct tacLine *linearizeExpression(struct astNode *it, int *tempNum, int depth)
{
    // printf("LINEARIZING\n");
    // printAST(it, 0);
    // char *outString = malloc(1024 * sizeof(char));
    // char *outStringStart = outString;
    struct tacLine *thisExpression = newtacLine();
    struct tacLine *returnExpression = NULL;
    // int addressIndex = 0;
    if (depth == 0 && it->child->sibling->type == t_unOp)
    {
        return linearizeExpression(it->child->sibling, tempNum, depth + 1);
    }
    thisExpression->addresses[0] = malloc(7 * sizeof(char));
    sprintf(thisExpression->addresses[0], "temp%d", *tempNum);
    if (tempNum == 0)
    {
        printf("FUCK\n\n");
        exit(1);
    }
    // outString += sprintf(outString, "temp%d = ", *tempNum);
    (*tempNum)++;
    // printf("checking l child\n");
    if (it->child == NULL)
        printf("OOPS\n");
    if (it->child->type == t_unOp)
    {
        // printf("L: see unop, recursing\n");
        //  allocate enough space to store 'tempxx' - will break with >99 temps
        thisExpression->addresses[1] = malloc(7 * sizeof(char));
        sprintf(thisExpression->addresses[1], "temp%d", *tempNum);
        // outString += sprintf(outString, "temp%d ", *tempNum);
        returnExpression = prependTAC(thisExpression, linearizeExpression(it->child, tempNum, depth + 1));
    }
    else
    {
        // printf("L: see value\n");
        thisExpression->addresses[1] = it->child->value;
        // outString += sprintf(outString, "%s ", it->child->value);
    }
    thisExpression->operation = it->value;
    // printf("checking r child\n");
    if (it->child->sibling->type == t_unOp)
    {
        // printf("R: see unop, recursing\n"); 0:
        //  allocate enough space to store 'tempxx' - will break with >99 temps
        thisExpression->addresses[2] = malloc(7 * sizeof(char));
        sprintf(thisExpression->addresses[2], "temp%d", *tempNum);
        // outString += sprintf(outString, "temp%d ", *tempNum);
        if (returnExpression != NULL)
            returnExpression = prependTAC(returnExpression, linearizeExpression(it->child->sibling, tempNum, depth + 1));
        else
            returnExpression = prependTAC(thisExpression, linearizeExpression(it->child->sibling, tempNum, depth + 1));
    }
    else
    {
        // printf("R: see value\n");
        thisExpression->addresses[2] = it->child->sibling->value;
        // outString += sprintf(outString, "%s ", it->child->value);
    }

    /*while (runner != NULL)
    {
        //printf("%s\n", getTokenName(runner->type));
        switch (runner->type)
        {
        case t_unOp:
            printf("see unop\n");
            thisExpression->addresses[addressIndex] = malloc(6 * sizeof(char));
            sprintf(thisExpression->addresses[addressIndex++], "temp%d", *tempNum);
            thisExpression->operation = runner->value;
            if (returnExpression == NULL)
                returnExpression = prependTAC(thisExpression, linearizeExpression(runner, tempNum, 0));
            else
                returnExpression = prependTAC(returnExpression, linearizeExpression(runner, tempNum, 0));

            break;
        case t_name:
        case t_literal:
            thisExpression->addresses[addressIndex++] = runner->value;
            //outString += sprintf(outString, "%s ", runner->value);
            break;

        default:
            printf("something broke :( %s\n", getTokenName(runner->type));
            //exit(1);
        }
        printf("skip to sibling\n");
        runner = runner->sibling;
    }*/

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
            printf("Linearizing function %s\n", runner->child->value);
            struct functionEntry* theFunction = symbolTableLookup(table, runner->child->value)->entry;
            printAST(runner, 0);
            struct tacLine* generatedTAC = linearizeFunction(runner->child->sibling);
            printf("linearized to:\n");
            printTacLine(generatedTAC);
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

int main(int argc, char **argv)
{
    printf("%s\n", argv[1]);

    struct astNode *program = parseProgram(argv[1]);
    printf("DONE PARSING PROGRAM\n");
    printAST(program, 0);

    struct symbolTable *theTable = walkAST(program);
    linearizeProgram(program, theTable);
    printSymTab(theTable);

    // printTacLine(head);

    printf("done printing\n");
}