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
	struct LinkedList *BasicBlockList;
	char *name; // duplicate pointer from ScopeMember for ease of use
};

struct VariableEntry
{
	int stackOffset;
	enum variableTypes type;
	int indirectionLevel;
	int assignedAt;
	int declaredAt;
	char isAssigned;
};


struct SymbolTable
{
	char *name;
	struct Scope *globalScope;
	// global basic blocks for defining/declaring globals
	struct LinkedList *BasicBlockList;
};


struct variableEntry *VariableEntry_new(int indirectionLevel, enum variableTypes type);

struct functionEntry *FunctionEntry_new(struct SymbolTable *table);

struct SymbolTable *SymbolTable_new(char *name);

// int SymbolTable_contains(struct SymbolTable *table, char *name);

// struct symTabEntry *SymbolTable_lookup(struct SymbolTable *table, char *name);

// struct VariableEntry *SymbolTable_lookupVar(struct SymbolTable *table, char *name);

// struct FunctionEntry *SymbolTable_lookupFun(struct SymbolTable *table, char *name);

// struct Scope *SymbolTable_lookupScope(struct SymbolTable *table, char *name);

struct Scope *Scope_new(struct Scope *parentScope, char *name);

void Scope_print(struct Scope *it, int depth, char printTAC);

// look only within the given scope, don't traverse upwards
char Scope_contains(struct Scope *scope, char *name);

struct ScopeMember *Scope_lookup(struct Scope *scope, char *name);

struct VariableEntry *Scope_lookupVar(struct Scope *scope, char *name);

struct FunctionEntry *Scope_lookupFun(struct Scope *scope, char *name);

struct Scope *Scope_lookupSubScope(struct Scope *scope, char *name);

int Scope_getSizeOfVariable(struct Scope *scope, char *name);

void Scope_insert(struct Scope *scope, char *name, void *newEntry, enum ScopeMemberType type);

void Scope_createVariable(struct Scope *scope, char *name, enum variableTypes type, int indirectionLevel);

void FunctionEntry_createArgument(struct FunctionEntry *func, char *name, enum variableTypes type, int indirectionLevel);

struct FunctionEntry *Scope_createFunction(struct Scope *scope, char *name);

struct Scope *Scope_createSubScope(struct Scope *scope);

void SymbolTable_print(struct SymbolTable *it, char printTAC);

void SymbolTable_free(struct SymbolTable *it);

// AST walk functions
void walkStatement(struct ASTNode *it, struct Scope *wip);

void walkScope(struct ASTNode *it, struct Scope *wip, char isMainScope);

void walkFunction(struct ASTNode *it, struct Scope *parentScope);

struct SymbolTable *walkAST(struct ASTNode *it);