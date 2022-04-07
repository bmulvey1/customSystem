#include <stdlib.h>
#include "util.h"
#include "symtab.h"
#include "asm.h"
#include "tac.h"

#define REGISTER_COUNT 5

//4: 2091119
//5: 1976539

struct Lifetime
{
    int start, end;
    char *variable;
    enum variableTypes type;
};

struct Lifetime *newLifetime(char *variable, enum variableTypes type, int start);

char compareLifetimes(struct Lifetime *a, char *variable);

void updateOrInsertLifetime(struct LinkedList *ltList,
                            char *variable,
                            enum variableTypes type,
                            int newEnd);

void printCurrentState(struct Stack *activeList,
                       struct Stack *inactiveList,
                       struct Stack *spilledList);

/*
 * Register, SpilledRegister, and SavedState functions
 *
 */

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


struct Register *duplicateRegister(struct Register *r);

void sortByStartPoint(struct Register **list, int size);

void sortByEndPoint(struct Register **list, int size);

void sortByRegisterNumber(struct Register **list, int size);

void sortByStackOffset(struct SpilledRegister **list, int size);

struct Register *findAndRemoveLiveVariable(struct Stack *activeList, char *name);

struct SpilledRegister *findAndRemoveSpilledVariable(struct Stack *spilledList, char *name);

struct Register *findAndRemoveInactiveRegisterByIndex(struct Stack *inactiveList, int index);

struct SpilledRegister *duplicateSpilledRegister(struct Register *r);

void expireOldIntervals(struct Stack *activeList,
                        struct Stack *inactiveList,
                        struct Stack *spilledList,
                        int TACIndex);

void spillRegister(struct Stack *activeList,
                   struct Stack *inactiveList,
                   struct Stack *spilledList,
                   struct ASMblock *outputBlock,
                   struct symbolTable *table);

int assignRegister(struct Stack *activeList,
                   struct Stack *inactiveList,
                   struct Lifetime *assignedLifetime);

int unSpillVariable(struct Stack *activeList,
                    struct Stack *inactiveList,
                    struct Stack *spilledList,
                    char *varName,
                    struct ASMblock *outputBlock,
                    struct symbolTable *table);

int findActiveVariable(struct Stack *activeList, char *varName);

int findSpilledVariable(struct Stack *spilledLilst, char *varName);

void printLifetimesGraph(struct LinkedList *lifetimeList);

struct SavedState *duplicateCurrentState(struct Stack *activeList,
                                         struct Stack *inactiveList,
                                         struct Stack *spilledList,
                                         int currentLifetimeIndex);

void restoreRegisterStates(struct Stack *savedStateStack,
                           struct Stack *activeList,
                           struct Stack *inactiveList,
                           struct Stack *spilledList,
                           int *currentLifetimeIndex,
                           int TACIndex,
                           struct ASMblock *outputBlock);

void resetRegisterStates(struct Stack *savedStateStack,
                         struct Stack *activeList,
                         struct Stack *inactiveList,
                         struct Stack *spilledList,
                         int *currentLifetimeIndex);

int findOrPlaceAssignedVariable(struct Stack *activeList,
                                struct Stack *inactiveList,
                                struct Stack *spilledList,
                                char *varName,
                                struct ASMblock *outputBlock,
                                struct symbolTable *table);

struct LinkedList *findLifetimes(struct symbolTable *table);
