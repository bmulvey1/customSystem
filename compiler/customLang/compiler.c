#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"

enum symbolType
{
    variable,
    function
};

char *symbolNames[] = {
    "variable",
    "function"};

struct symTabEntry
{
    char *name;
    enum symbolType type;
};

struct symbolTable
{
    char *name;
    struct symTabEntry **entries;
    int size;
};

struct symbolTable *newSymbolTable()
{
    struct symbolTable *wip = malloc(sizeof(struct symbolTable));
    wip->size = 0;
    wip->entries = NULL;
    wip->name = NULL;
    return wip;
}

void symbolTableInsert(struct symbolTable *table, char *newName)
{
    struct symTabEntry *wip = malloc(sizeof(struct symTabEntry));
    // NO deep copy of name info - if the AST this string comes from goes away, so does this name!
    wip->name = newName;
    struct symTabEntry **newList = malloc(++table->size * sizeof(struct symTabEntry *));
    for (int i = 0; i < table->size - 1; i++)
    {
        newList[i] = table->entries[i];
    }
    newList[table->size - 1] = wip;
    free(table->entries);
    table->entries = newList;
}

int symbolTableContains(struct symbolTable *table, char *name)
{
    for (int i = 0; i < table->size; i++)
        if (!strcmp(table->entries[i]->name, name))
            return 1;

    return 0;
}

void printSymTab(struct symbolTable *it)
{
    printf("~~~~~\nSymbol table for [%s]\n", it->name);
    for (int i = 0; i < it->size; i++)
    {
        printf("%s\n", it->entries[i]->name);
    }
    printf("~~~~~\n");
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
                symbolTableInsert(wip, runner->child->child->value);
            else
                symbolTableInsert(wip, runner->child->value);
            break;
        case t_fun:
        {
            struct astNode *functionNode = runner->child;
            struct symbolTable *subTable = newSymbolTable();
            while (functionNode != NULL)
            {
                switch (functionNode->type)
                {
                case t_var:
                    if (functionNode->child->type == t_assign)
                        symbolTableInsert(subTable, functionNode->child->child->value);
                    else
                        symbolTableInsert(subTable, functionNode->child->value);
                    break;
                    break;
                case t_assign:
                    if (!symbolTableContains(subTable, functionNode->child->value))
                    {
                        printf("Error - variable [%s] assigned before declaration\n", functionNode->child->value);
                        exit(1);
                    }
                    break;
                case t_name:
                    subTable->name = functionNode->value;
                    break;
                default:
                    printf("Error walking AST - expected 'v' or name, saw %s with value of [%s]\n", getTokenName(functionNode->type), functionNode->value);
                    exit(1);
                }
                functionNode = functionNode->sibling;
            }
            printSymTab(subTable);
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

int main(int argc, char **argv)
{
    printf("%s\n", argv[1]);

    struct astNode *program = parseProgram(argv[1]);
    printf("DONE PARSING PROGRAM\n");
    printAST(program, 0);

    struct symbolTable *theTable = walkAST(program);
    printSymTab(theTable);
    printf("done printing\n");
}