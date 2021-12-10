#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"

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

struct tacLine
{
    char *addresses[3];
    char *operation;
    struct tacLine *nextLine;
    struct tacLine *prevLine;
};

struct tacLine *newtacLine()
{
    struct tacLine *wip = malloc(sizeof(struct tacLine));
    return wip;
}

// stick the child at the end of the parent
void appendTAC(struct tacLine *parent, struct tacLine *child)
{
    struct tacLine *runner = parent;
    while (runner->nextLine != NULL)
        runner = runner->nextLine;

    runner->nextLine = child;
    child->prevLine = runner;
}

void printTacLine(struct tacLine *it)
{
    while (it != NULL)
    {
        printf("\t%8s = %8s %2s %8s\n", it->addresses[0], it->addresses[1], it->operation, it->addresses[2]);
        it = it->nextLine;
    }
}

struct tacLine *prependTAC(struct tacLine *parent, struct tacLine *child)
{
    struct tacLine *runner = child;
    while (runner->nextLine != NULL)
        runner = runner->nextLine;

    runner->nextLine = parent;
    return child;
}


struct tacLine *linearizeOperation(struct astNode *it, int *tempNum, int depth)
{
    printf("LINEARIZING\n");
    printAST(it, 0);
    //char *outString = malloc(1024 * sizeof(char));
    //char *outStringStart = outString;
    struct tacLine *thisOperation = newtacLine();
    struct tacLine *returnOperation = NULL;
    if (depth > 0)
    {
        thisOperation->addresses[0] = malloc(7 * sizeof(char));
        sprintf(thisOperation->addresses[0], "temp%d", *tempNum);
        if(tempNum == 0){
            printf("FUCK\n\n");
        }
        //outString += sprintf(outString, "temp%d = ", *tempNum);
        (*tempNum)++;
    }
    else
    {
        thisOperation->addresses[0] = "result";
        //outString += sprintf(outString, "result = ");
    }

    // check for subexpression on left side
    if (it->child->type == t_unOp)
    {
        // allocate enough space to store 'tempxx' - will break with >99 temps
        thisOperation->addresses[1] = malloc(7 * sizeof(char));
        sprintf(thisOperation->addresses[1], "temp%d", *tempNum);
        //outString += sprintf(outString, "temp%d ", *tempNum);
        returnOperation = prependTAC(thisOperation, linearizeOperation(it->child, tempNum, depth + 1));
    }
    else
    {
        thisOperation->addresses[1] = it->child->value;
        //outString += sprintf(outString, "%s ", it->child->value);
    }

    // operator
    //outString += sprintf(outString, "%s ", it->value);
    thisOperation->operation = it->value;

    // check for subexpression on right side
    if (it->child->sibling->type == t_unOp)
    {
        // allocate enough space to store 'tempxx' - will break with >99 temps
        thisOperation->addresses[2] = malloc(6 * sizeof(char));
        sprintf(thisOperation->addresses[2], "temp%d", *tempNum);
        //outString += sprintf(outString, "temp%d ", *tempNum);
        returnOperation = prependTAC(thisOperation, linearizeOperation(it->child->sibling, tempNum, depth + 1));
    }
    else
    {
        thisOperation->addresses[2] = it->child->sibling->value;
        //outString += sprintf(outString, it->child->sibling->value);
    }
    //printf("%s\n", outStringStart);
    //free(outString);
    if (returnOperation != NULL){
        printTacLine(returnOperation);
        printf("\tdone.\n\n");
        return returnOperation;
    }else{
        printTacLine(thisOperation);
        printf("\tdone.\n\n");

        return thisOperation;
    }
}

// this function is going to get very unpleasant
struct tacLine *linearizeExpression(struct astNode *it, int *tempNum, int depth)
{

    struct astNode *runner = it;
    //char *outString = malloc(1024 * sizeof(char));
    //char *outStringStart = outString;
    struct tacLine *thisExpression = newtacLine();
    struct tacLine *returnExpression = NULL;
    int addressIndex = 0;
    if (depth > 0)
    {
        //(*tempNum)++;
    }
    while (runner != NULL)
    {
        //printf("%s\n", getTokenName(runner->type));
        switch (runner->type)
        {
        case t_unOp:
            thisExpression->addresses[addressIndex] = malloc(6 * sizeof(char));
            sprintf(thisExpression->addresses[addressIndex++], "temp%d", *tempNum);
            thisExpression->operation = runner->value;
            if (returnExpression == NULL)
                returnExpression = prependTAC(thisExpression, linearizeOperation(runner, tempNum, 0));
            else
                returnExpression = prependTAC(returnExpression, linearizeOperation(runner, tempNum, 0));

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
    }
    //outString += sprintf(outString, "\n");
    //printf(outStringStart);
    //free(outString);
    if (returnExpression != NULL)
        return returnExpression;
    else
        return thisExpression;
}

struct tacLine *linearizeAST(struct astNode *it)
{
    struct astNode *runner = it;
    runner = runner->sibling;
    struct tacLine *asttac = NULL;
    while (runner != NULL)
    {
        switch (runner->type)
        {
        case t_var:
            if (runner->child->type == t_assign)
            {
                // getting multiple children is confusing but necessary
                // is there a more elegant way to write this?z
                //printf("%s = ", runner->child->child->value);
                int tempNum = 0;
                asttac = prependTAC(asttac, linearizeExpression(runner->child->child->sibling, &tempNum, 0));
            }
            else
                printf("initialize variable [%s]\n", runner->child->value);
            break;

        case t_fun:
            asttac = prependTAC(asttac, linearizeAST(runner->child));
            break;

        case t_assign:
            //printf("%s = ", runner->child->value);
            int tempNum = 0;
            asttac = prependTAC(asttac, linearizeExpression(runner->child->sibling, &tempNum, 0));
            break;

        default:
            printf("Something broke :(\n");
            exit(1);
        }
        runner = runner->sibling;
    }
    return asttac;
}

int main(int argc, char **argv)
{

    printf("%s\n", argv[1]);

    struct astNode *program = parseProgram(argv[1]);
    printf("DONE PARSING PROGRAM\n");
    printAST(program, 0);

    struct symbolTable *theTable = walkAST(program);
    printSymTab(theTable);
    struct tacLine *theTAC = linearizeAST(program);
    printTacLine(theTAC);
    struct tacLine *t1 = newtacLine();
    struct tacLine *t2 = newtacLine();
    struct tacLine *t3 = newtacLine();
    t1->addresses[0] = "a";
    t2->addresses[0] = "b";
    t3->addresses[0] = "c";

    struct tacLine *head = prependTAC(NULL, t3);
    head = prependTAC(head, t2);
    head = prependTAC(head, t1);
    printTacLine(head);

    printf("done printing\n");
}