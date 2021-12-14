#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "tac.h"
/*
 * SYMBOL TABLE utilities
 *
 */
enum symTabEntryType
{
    e_variable,
    e_function
};

char *symbolNames[] = {
    "variable",
    "function"};

struct symbolTable
{
    char *name;
    struct symTabEntry **entries;
    int size;
};

struct symTabEntry
{
    char *name;
    enum symTabEntryType type;
    void *entry;
};

struct variableEntry
{
    int lsStart, lsEnd;
};

struct functionEntry
{
    struct symbolTable *table;
    struct tacLine *codeBlock;
};

struct symTabEntry *newEntry(char *name, enum symTabEntryType type)
{
    struct symTabEntry *wip = malloc(sizeof(struct symTabEntry));
    wip->name = name;
    wip->type = type;
    wip->entry = NULL;
    return wip;
}

struct variableEntry *newVariableEntry()
{
    struct variableEntry *wip = malloc(sizeof(struct variableEntry));
    wip->lsStart = 0;
    wip->lsEnd = 0;
    return wip;
}

struct functionEntry *newFunctionEntry(struct symbolTable *table)
{
    struct functionEntry *wip = malloc(sizeof(struct functionEntry));
    wip->table = table;
    return wip;
}

struct symbolTable *newSymbolTable(char *name)
{
    struct symbolTable *wip = malloc(sizeof(struct symbolTable));
    wip->size = 0;
    wip->entries = NULL;
    wip->name = name;
    return wip;
}

int symbolTableContains(struct symbolTable *table, char *name)
{
    for (int i = 0; i < table->size; i++)
        if (!strcmp(table->entries[i]->name, name))
            return 1;

    return 0;
}

struct symTabEntry *symbolTableLookup(struct symbolTable *table, char *name)
{
    for (int i = 0; i < table->size; i++)
        if (!strcmp(table->entries[i]->name, name))
            return table->entries[i];
    printf("Unable to find symbol [%s]! Something is broken in the compiler :(\n", name);
    exit(1);
}

void symTabInsert(struct symbolTable *table, char *name, void *newEntry, enum symTabEntryType type)
{
    if (symbolTableContains(table, name))
    {
        printf("Error defining symbol [%s] - name already exists!\n", name);
        exit(1);
    }
    struct symTabEntry *wip = malloc(sizeof(struct symTabEntry));
    wip->name = name;
    wip->entry = newEntry;
    wip->type = type;
    struct symTabEntry **newList = malloc(++table->size * sizeof(struct symTabEntry *));
    for (int i = 0; i < table->size - 1; i++)
        newList[i] = table->entries[i];

    newList[table->size - 1] = wip;
    free(table->entries);
    table->entries = newList;
}

void symTab_insertVariable(struct symbolTable *table, char *name)
{
    symTabInsert(table, name, newVariableEntry(name), e_variable);
}

void symTab_insertFunction(struct symbolTable *table, char *name, struct symbolTable *subTable)
{
    symTabInsert(table, name, newFunctionEntry(subTable), e_function);
}

void printSymTabRec(struct symbolTable *it, int depth)
{
    for (int i = 0; i < depth; i++)
        printf("\t");

    printf("~~~~~Symbol table for [%s]\n", it->name);
    for (int i = 0; i < it->size; i++)
    {
        for (int i = 0; i < depth; i++)
            printf("\t");

        switch (it->entries[i]->type)
        {
        case e_variable:
        {
            struct variableEntry *theVariable = it->entries[i]->entry;
            printf("> variable [%s]: (lifespan %d:%d)\n", it->entries[i]->name, theVariable->lsStart, theVariable->lsEnd);
        }
        break;

        case e_function:
        {
            struct functionEntry *theFunction = it->entries[i]->entry;
            printf("> function [%s]: %d symbols\n", it->entries[i]->name, theFunction->table->size);
            printSymTabRec(theFunction->table, depth + 1);
            printTacLine(theFunction->codeBlock);
        }
        break;
        }
    }
    for (int i = 0; i < depth; i++)
        printf("\t");
    printf("\n");
}

void printSymTab(struct symbolTable *it)
{
    printSymTabRec(it, 0);
}

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
    runner = runner->sibling;
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
                // getting multiple children is confusing but necessary
                // is there a more elegant way to write this?z
                // printf("%s = ", runner->child->child->value);
                printf("yowzer\n");
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
            struct functionEntry* theFunction = symbolTableLookup(table, runner->child->value)->entry;
            theFunction->codeBlock = linearizeFunction(runner->child->sibling);
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
    // printTacLine(theTAC);
    struct tacLine *t1 = newtacLine();
    struct tacLine *t2 = newtacLine();
    struct tacLine *t3 = newtacLine();
    t1->addresses[0] = "a";
    t2->addresses[0] = "b";
    t3->addresses[0] = "c";

    struct tacLine *head = prependTAC(NULL, t3);
    head = prependTAC(head, t2);
    head = prependTAC(head, t1);
    // printTacLine(head);

    printf("done printing\n");
}