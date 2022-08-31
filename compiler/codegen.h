#include "regalloc.h"

struct ASMblock *generateCode(struct SymbolTable *table, FILE *outFile);

struct ASMblock *generateCodeForScope(struct Scope *scope, FILE *outFile);

struct ASMblock *generateCodeForFunction(struct FunctionEntry *function, FILE *outFile);
