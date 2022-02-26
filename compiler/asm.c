#include "asm.h"

struct ASMblock *newASMblock()
{
    struct ASMblock *wip = malloc(sizeof(struct ASMblock));
    wip->head = NULL;
    wip->tail = NULL;
    return wip;
}

void ASMblock_prepend(struct ASMblock *block, char *data, int correspondingTACindex)
{
    struct ASMline *newLine = malloc(sizeof(struct ASMline));
    newLine->data = data;
    newLine->correspondingTACindex = correspondingTACindex;

    if (block->head != NULL)
    {
        newLine->next = block->head;
    }
    else
    {
        newLine->next = NULL;
        block->tail = newLine;
    }
    block->head = newLine;
}

void ASMblock_append(struct ASMblock *block, char *data, int correspondingTACindex)
{
    struct ASMline *newLine = malloc(sizeof(struct ASMline));
    newLine->data = data;
    newLine->next = NULL;
    newLine->correspondingTACindex = correspondingTACindex;

    if (block->tail != NULL)
    {
        block->tail->next = newLine;
    }
    else
    {
        block->head = newLine;
    }
    block->tail = newLine;
}
