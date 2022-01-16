#include "tac.h"

char *getAsmOp(enum tacType t)
{
    switch (t)
    {
    case tt_assign:
        return "";
        
    case tt_add:
        return "add";
        
    case tt_subtract:
        return "sub";
        
    case tt_label:
        return ".";
        
    }
    return "";
}

struct tacLine *newtacLine()
{
    struct tacLine *wip = malloc(sizeof(struct tacLine));
    wip->operandTypes[0] = vt_null;
    wip->operandTypes[1] = vt_null;
    wip->operandTypes[2] = vt_null;
    // default type of a line of TAC is assignment
    wip->operation = tt_assign;
    // by default operands are NOT reorderable
    wip->reorderable = 0;
    return wip;
}

void printTacLine(struct tacLine *it)
{
    int lineIndex = 0;
    while (it != NULL)
    {
        char *operationStr;
        char fallingThrough = 0;
        switch (it->operation)
        {
        case tt_add:
            if (!fallingThrough)
                operationStr = "+";
            fallingThrough = 1;
        case tt_subtract:
            if (!fallingThrough)
                operationStr = "-";

            printf("\t%2d:%8s = %8s %2s %8s", lineIndex, it->operands[0], it->operands[1], operationStr, it->operands[2]);

            break;

        case tt_assign:
            printf("\t%2d:%8s = %8s", lineIndex, it->operands[0], it->operands[1]);
            break;
        case tt_label:
            break;
        }
        printf("%s\n", it->reorderable ? " - Reorderable" : "");
        lineIndex++;
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