#include <stdlib.h>
#include "util.h"
#include "symtab.h"
#include "asm.h"
#include "tac.h"

struct Lifetime
{
    int start, end;
    char *variable;
    enum variableTypes type;
};

struct Lifetime *newLifetime(char *variable, enum variableTypes type, int start);

void freeLifetime(struct Lifetime *it);

void updateOrInsertLifetime(struct LinkedList *ltList, char *variable, enum variableTypes type, int newEnd);

struct LinkedList *findLifetimes(struct symbolTable *table);

struct ASMblock *generateCode(struct symbolTable *table, FILE *outFile);

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

struct SavedState
{
    struct Stack *activeList;
    struct Stack *inactiveList;
    struct Stack *spilledList;
    int currentLifetimeIndex;
};
