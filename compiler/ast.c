#include <stdio.h>
#include <string.h>

#include "ast.h"

struct astNode *newastNode(enum token t, char *value)
{
    struct astNode *retNode = malloc(sizeof(struct astNode));
    retNode->child = NULL;
    retNode->sibling = NULL;
    retNode->type = t;
    retNode->value = value;
    return retNode;
}

void astNode_insertSibling(struct astNode *it, struct astNode *newSibling)
{
    struct astNode *runner = it;
    while (runner->sibling != NULL)
        runner = runner->sibling;

    runner->sibling = newSibling;
}

void astNode_insertChild(struct astNode *it, struct astNode *newChild)
{
    if (it->child == NULL)
        it->child = newChild;
    else
        astNode_insertSibling(it->child, newChild);
}

void printAST(struct astNode *it, int depth)
{
    if (it->sibling != NULL)
        printAST(it->sibling, depth);

    for (int i = 0; i < depth; i++)
        printf("\t");

    printf("%s\n", it->value);
    if (it->child != NULL)
        printAST(it->child, depth + 1);
}

void freeAST(struct astNode *it)
{
    struct astNode *runner = it;
    while (runner != NULL)
    {
        if (runner->child != NULL)
        {
            freeAST(runner->child);
        }
        struct astNode *old = runner;
        runner = runner->sibling;
        free(old);
    }
}