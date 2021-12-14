#include "tac.h"

struct tacLine *newtacLine()
{
    struct tacLine *wip = malloc(sizeof(struct tacLine));
    return wip;
}

void printTacLine(struct tacLine *it)
{
    while (it != NULL)
    {
        printf("\t%8s = %8s %2s %8s\n", it->addresses[0], it->addresses[1], it->operation, it->addresses[2]);
        it = it->nextLine;
    }
}

// stick the child at the end of the parent
struct tacLine* appendTAC(struct tacLine *parent, struct tacLine *child)
{
    if(parent == NULL)
        return child;

    struct tacLine *runner = parent;
    while (runner->nextLine != NULL)
        runner = runner->nextLine;

    runner->nextLine = child;
    child->prevLine = runner;

    return parent;
}

struct tacLine *prependTAC(struct tacLine *parent, struct tacLine *child)
{
    struct tacLine *runner = child;
    while (runner->nextLine != NULL)
        runner = runner->nextLine;

    runner->nextLine = parent;
    return child;
}

struct tacLine *findLastTAC(struct tacLine *head)
{
    while (head->nextLine != NULL)
        head = head->nextLine;

    return head;
}