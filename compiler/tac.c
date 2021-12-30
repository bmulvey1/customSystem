#include "tac.h"

struct tacLine *newtacLine()
{
    struct tacLine *wip = malloc(sizeof(struct tacLine));
    wip->operandTypes[0] = vt_null;
    wip->operandTypes[1] = vt_null;
    wip->operandTypes[2] = vt_null;
    return wip;
}

void printTacLine(struct tacLine *it)
{
    int lineIndex = 0;
    while (it != NULL)
    {
        printf("\t%2d:%8s = %8s %2s %8s\n", lineIndex++, it->operands[0], it->operands[1], it->operation, it->operands[2]);
        //printf("[%d %d %d]\n", it->operandTypes[0], it->operandTypes[1], it->operandTypes[2]);
        it = it->nextLine;
    }
}

// stick the child at the end of the parent
struct tacLine *appendTAC(struct tacLine *before, struct tacLine *after)
{
    if (before == NULL)
        return after;

    struct tacLine *runner = before;
    while (runner->nextLine != NULL)
        runner = runner->nextLine;

    runner->nextLine = after;
    after->prevLine = runner;

    return before;
}

struct tacLine *prependTAC(struct tacLine *after, struct tacLine *before)
{
    struct tacLine *runner = before;
    while (runner->nextLine != NULL)
        runner = runner->nextLine;

    runner->nextLine = after;
    return before;
}

struct tacLine *findLastTAC(struct tacLine *head)
{
    while (head->nextLine != NULL)
        head = head->nextLine;

    return head;
}