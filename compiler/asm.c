#include "asm.h"

struct ASMblock *newASMblock()
{
    struct ASMblock *wip = malloc(sizeof(struct ASMblock));
    wip->head = NULL;
    wip->tail = NULL;
    return wip;
}

void ASMblock_free(struct ASMblock *it)
{
    struct ASMline *runner = it->head;
    while (runner != NULL)
    {
        struct ASMline *old = runner;
        runner = runner->next;
        free(old->data);
        free(old);
    }
    free(it);
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

void ASMblock_output(struct ASMblock *block, FILE *outFile)
{
    for (struct ASMline *runner = block->head; runner != NULL; runner = runner->next)
    {
        if (runner->data[strlen(runner->data) - 1] != ':')
        {
            fprintf(outFile, "\t");
        }
        fprintf(outFile, "%s\n", runner->data);
    }
}