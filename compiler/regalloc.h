#include <stdlib.h>
#include "util.h"
#include "symtab.h"
#include "asm.h"
#include "tac.h"
/*
	4: 3941
	5: 3615
	6: 3364
	7:
	8:
	9:

*/
#define REGISTER_COUNT 6

struct Lifetime
{
	int start, end, nwrites, nreads;
	char *variable;
	int stackOrRegLocation;
	enum variableTypes type;
	char isSpilled;
};

struct Lifetime *newLifetime(char *variable, enum variableTypes type, int start);

char compareLifetimes(struct Lifetime *a, char *variable);

// update the lifetime start/end indices
// returns pointer to the lifetime corresponding to the passed variable name
struct Lifetime *updateOrInsertLifetime(struct LinkedList *ltList,
										char *variable,
										enum variableTypes type,
										int newEnd);

// wrapper function for updateOrInsertLifetime
//  increments write count for the given variable
void recordVariableWrite(struct LinkedList *ltList,
						 char *variable,
						 enum variableTypes type,
						 int newEnd);

// wrapper function for updateOrInsertLifetime
//  increments read count for the given variable
void recordVariableRead(struct LinkedList *ltList,
						char *variable,
						enum variableTypes type,
						int newEnd);

// places a variable in a register, with no guarantee that it is modifiable
int placeOperandInRegister(struct LinkedList *lifetimes, char *variable, struct ASMblock *currentBlock, int registerIndex);

struct LinkedList *findLifetimes(struct FunctionEntry *function);
