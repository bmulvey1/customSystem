#include <stdlib.h>

struct ASMline
{
    char *data;
    struct ASMline *next;
    int correspondingTACindex;
};

struct ASMblock
{
    struct ASMline *head;
    struct ASMline *tail;
};

struct ASMblock *newASMblock();

void ASMblock_prepend(struct ASMblock *block, char *data, int correspondingTACindex);

void ASMblock_append(struct ASMblock *block, char *data, int correspondingTACindex);
