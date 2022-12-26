#include "regalloc.h"

struct Stack *generateCode(struct SymbolTable *table, FILE *outFile);

struct Stack *generateCodeForScope(struct Scope *scope, FILE *outFile);

struct ASMblock *generateCodeForFunction(struct FunctionEntry *function, FILE *outFile);

void GenerateCodeForBasicBlock(struct BasicBlock *thisBlock, struct LinkedList *allLifetimes, struct ASMblock *asmBlock, char *functionName, int reservedRegisters[2], char *touchedRegisters);
