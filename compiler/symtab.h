#include <stdio.h>
#include <string.h>

#include "util.h"
#include "ast.h"
#include "tac.h"
#include "parser.h"
#include "tac.h"

#pragma once

enum ScopeMemberType
{
	e_variable,
	e_function,
	e_argument,
	e_scope,
	e_basicblock,
	e_stackobj,
};


struct ScopeMember
{
	char *name;
	enum ScopeMemberType type;
	void *entry;
};

struct FunctionEntry;

struct Scope
{
	struct Scope *parentScope;
	struct FunctionEntry *parentFunction;
	struct Stack *entries;
	char subScopeCount;
	char *name; // duplicate pointer from ScopeMember for ease of use
};

struct FunctionEntry
{
	int localStackSize;
	int argStackSize;
	enum variableTypes returnType;
	// struct SymbolTable *table;
	struct Scope *mainScope;
	char *name; // duplicate pointer from ScopeMember for ease of use
	struct LinkedList *BasicBlockList;
};

struct VariableEntry
{
	int stackOffset;
	struct StackObjectEntry *localPointerTo;
	char *name; // duplicate pointer from ScopeMember for ease of use
	enum variableTypes type;
	int indirectionLevel;
	int assignedAt;
	int declaredAt;
	char isAssigned;
	// if this variable has the address-of operator used on it
	// we need to denote that it *must* live on the stack so it isn't lost
	// and can have an address
	char mustSpill;
};

struct StackObjectEntry
{
	int size;
	int arraySize;
	int stackOffset;
	struct VariableEntry *localPointer;
};

struct SymbolTable
{
	char *name;
	struct Scope *globalScope;
};


void FunctionEntry_createArgument(struct FunctionEntry *func, char *name, enum variableTypes type, int indirectionLevel, int arraySize);

// symbol table functions
struct SymbolTable *SymbolTable_new(char *name);

// scope functions
struct Scope *Scope_new(struct Scope *parentScope, char *name);

void Scope_free(struct Scope *scope, int depth);

void Scope_print(struct Scope *it, int depth, char printTAC);

void Scope_insert(struct Scope *scope, char *name, void *newEntry, enum ScopeMemberType type);

void Scope_createVariable(struct Scope *scope, char *name, enum variableTypes type, int indirectionLevel, int arraySize);

struct FunctionEntry *Scope_createFunction(struct Scope *scope, char *name);

struct Scope *Scope_createSubScope(struct Scope *scope);

// scope lookup functions
char Scope_contains(struct Scope *scope, char *name);

struct ScopeMember *Scope_lookup(struct Scope *scope, char *name);

struct VariableEntry *Scope_lookupVar(struct Scope *scope, char *name);

struct FunctionEntry *Scope_lookupFun(struct Scope *scope, char *name);

struct Scope *Scope_lookupSubScope(struct Scope *scope, char *name);

struct Scope *Scope_lookupSubScopeByNumber(struct Scope *scope, unsigned char subScopeNumber);

int Scope_getSizeOfVariable(struct Scope *scope, char *name);

// scope linearization functions

// adds an entry in the given scope denoting that the block is from that scope
void Scope_addBasicBlock(struct Scope *scope, struct BasicBlock *b);

// add the basic block to the linkedlist for the parent function
void Function_addBasicBlock(struct FunctionEntry *function, struct BasicBlock *b);

void SymbolTable_print(struct SymbolTable *it, char printTAC);

void SymbolTable_free(struct SymbolTable *it);

// AST walk functions
void walkStatement(struct ASTNode *it, struct Scope *wip);

void walkScope(struct ASTNode *it, struct Scope *wip, char isMainScope);

void walkFunction(struct ASTNode *it, struct Scope *parentScope);

struct SymbolTable *walkAST(struct ASTNode *it);
