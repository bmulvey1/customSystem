#include "regalloc.h"

void PlaceLiteralInRegister(struct LinkedList *currentBlock, char *literalStr, char *destReg);

struct Stack *generateCode(struct SymbolTable *table, FILE *outFile);

struct Stack *generateCodeForScope(struct Scope *scope, FILE *outFile);

struct LinkedList *generateCodeForFunction(struct FunctionEntry *function, FILE *outFile);

void GenerateCodeForBasicBlock(struct BasicBlock *thisBlock, struct LinkedList *allLifetimes, struct LinkedList *asmBlock, char *functionName, int reservedRegisters[2], char *touchedRegisters);
