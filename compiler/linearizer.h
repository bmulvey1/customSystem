#include "tac.h"
#include "symtab.h"

struct tacLine *linearizeFunctionCall(struct astNode *it, int *tempNum, struct tempList *tl);

struct tacLine *linearizeExpression(struct astNode *it, int *tempNum, struct tempList *tl);

struct tacLine *linearizeAssignment(struct astNode *it, int *tempNum, struct tempList *tl);

struct tacLine *linearizeFunction(struct astNode *it, struct tempList *tl);

void linearizeProgram(struct astNode *it, struct symbolTable *table);
