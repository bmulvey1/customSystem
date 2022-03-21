#include <stdlib.h>
#include "util.h"
#include "symtab.h"

struct Lifetime
{
    int start, end;
    char *variable;
};

struct Lifetime *newLifetime(char *variable, int start);

void freeLifetime(struct Lifetime *it);

void updateOrInsertLifetime(struct LinkedList *ltList, char *variable, int newEnd);

void findLifetimes(struct symbolTable *table);

struct Register
{
    struct Lifetime *lifetime;
    int index;
    int lastUsed;
};

struct SpilledRegister
{
    struct Lifetime *lifetime;
    int lastUsed;
    int stackOffset;
    char occupied;
};
