#include "symtab.h"

char *symbolNames[] = {
	"variable",
	"function"};

char *getTempString(struct tempList *tempList, int tempNum)
{
	if (tempList->size <= tempNum)
	{
		int newSize = tempList->size + 10;
		char **newTemps = malloc(newSize * sizeof(char *));
		for (int i = 0; i < tempList->size; i++)
		{
			newTemps[i] = tempList->temps[i];
		}
		for (int i = tempList->size; i < newSize; i++)
		{
			newTemps[i] = malloc(6 * sizeof(char));
			sprintf(newTemps[i], ".t%d", i);
		}
		free(tempList->temps);
		tempList->temps = newTemps;
		tempList->size = newSize;
	}
	return tempList->temps[tempNum];
}

struct tempList *newTempList()
{
	struct tempList *wip = malloc(sizeof(struct tempList));
	wip->size = 10;
	wip->temps = malloc(10 * sizeof(char *));
	for (int i = 0; i < 10; i++)
	{
		wip->temps[i] = malloc(6 * sizeof(char));
		sprintf(wip->temps[i], ".t%d", i);
	}

	return wip;
}

void freeTempList(struct tempList *it)
{
	for (int i = 0; i < it->size; i++)
	{
		free(it->temps[i]);
	}
	free(it->temps);
	free(it);
}

/*
struct symTabEntry *newEntry(char *name, enum symTabEntryType type)
{
	struct symTabEntry *wip = malloc(sizeof(struct symTabEntry));
	wip->name = name;
	wip->type = type;
	wip->entry = NULL;
	return wip;
}

struct VariableEntry *newVariableEntry(int indirectionLevel, enum variableTypes type)
{
	struct VariableEntry *wip = malloc(sizeof(struct VariableEntry));
	wip->stackOffset = 0;
	wip->indirectionLevel = indirectionLevel;
	wip->type = type;
	wip->assignedAt = -1;
	wip->declaredAt = -1;
	wip->isAssigned = 0;
	return wip;
}

struct FunctionEntry *FunctionEntry_new(struct Scope *parentScope)
{
	struct FunctionEntry *wip = malloc(sizeof(struct FunctionEntry));
	wip->mainScope = Scope_new(parentScope);
	wip->mainScope->parentFunction = wip;
	wip->localStackSize = 0;
	wip->argStackSize = 0;
	wip->BasicBlockList = LinkedList_new();
	return wip;
}
*/

struct SymbolTable *SymbolTable_new(char *name)
{
	struct SymbolTable *wip = malloc(sizeof(struct SymbolTable));
	wip->name = name;
	wip->globalScope = Scope_new(NULL, "Global");
	// generate a single templist at the top level symTab
	// leave all others as NULL to avoid duplication
	wip->tl = NULL;
	return wip;
}

/*
int SymbolTableContains(struct SymbolTable *table, char *name)
{
	for (int i = 0; i < table->size; i++)
		if (!strcmp(table->entries[i]->name, name))
			return 1;

	return 0;
}

// return a symbol table entry by name, or NULL if not found
// automatically looks at most local scope, then enclosing scopes until found or no scopes left
struct symTabEntry *SymbolTableLookup(struct SymbolTable *table, char *name)
{
	while (table != NULL)
	{
		for (int i = 0; i < table->size; i++)
			if (!strcmp(table->entries[i]->name, name))
				return table->entries[i];

		table = table->parentScope;
	}

	return NULL;
}

// return the VariableEntry for a given name, or throw a use-before-declare error if nonexistent
struct VariableEntry *SymbolTable_lookupVar(struct SymbolTable *table, char *name)
{
	struct symTabEntry *e = SymbolTableLookup(table, name);

	if (e != NULL)
		return e->entry;
	else
	{
		ErrorAndExit(ERROR_CODE, "Error - Use of variable [%s] before declaration\n", name);
	}
}

int SymbolTable_getSizeOfVariable(struct SymbolTable *table, enum variableTypes type)
{
	switch (type)
	{
	case vt_var:
		return 2;

	default:
		ErrorAndExit(ERROR_CODE, "Error - attempt to get size of unknown type!");
	}
}

// unwind the scope of the current table to find and return the name of the actual function the scopes are within
char *SymbolTable_getNameOfParentFunction(struct SymbolTable *table)
{
	printf("getnameofparent\n");
	char *functionName = table->name;
	// track all the scope numbers so we can name this scope appropriately
	// also find out the base function name
	while (table->parentScope != NULL)
	{
		printf("table [%s] (%p) (parent is %p)\n", table->name, table, table->parentScope);
		for (int i = 0; i < 0xffffff; i++)
		{
		}
		if (table->parentScope->parentScope != NULL)
		{
			functionName = table->parentScope->name;
		}

		table = table->parentScope;
	}
	return functionName;
}

struct FunctionEntry *SymbolTable_lookupFun(struct SymbolTable *table, char *name)
{
	struct symTabEntry *e = SymbolTableLookup(table, name);

	if (e != NULL)
		return e->entry;
	else
	{
		ErrorAndExit(ERROR_CODE, "Error - Use of function [%s] before declaration\n", name);
	}
}

struct FunctionEntry *SymbolTable_lookupScope(struct SymbolTable *table, char *name)
{
	struct symTabEntry *e = NULL;
	while (table != NULL)
	{
		e = SymbolTableLookup(table, name);
		if (e == NULL)
		{
			table = table->parentScope;
		}
		else
		{
			printf("scope [%s] has pointer (%p)\n", name, e->entry);
			return e->entry;
		}
	}
	ErrorAndExit(ERROR_CODE, "Error - Use of scope [%s] before declaration\n", name);
}
*/
struct Scope *Scope_new(struct Scope *parentScope, char *name)
{
	struct Scope *wip = malloc(sizeof(struct Scope));
	wip->entries = Stack_new();
	// need to set this manually to handle when new functions are declared
	// TODO: supports nested functions? ;)
	wip->parentFunction = NULL; 
	wip->parentScope = parentScope;
	wip->subScopeCount = 0;
	wip->name = name;
	return wip;
}

char Scope_contains(struct Scope *scope, char *name)
{
	for(int i = 0; i < scope->entries->size; i++)
	{
		if(!strcmp(name, ((struct ScopeMember *)scope->entries->data[i])->name))
		{
			return 1;
		}
	}
	return 0;
}

// if a member with the given name exists in this scope or any of its parents,
// return it
struct ScopeMember *Scope_lookup(struct Scope *scope, char *name)
{
	while(scope != NULL)
	{
		for(int i = 0; i < scope->entries->size; i++)
		{
			struct ScopeMember *examinedEntry = scope->entries->data[i];
			if(!strcmp(examinedEntry->name, name))
			{
				return examinedEntry;
			}
		}
		scope = scope->parentScope;
	}
	return NULL;
}

int Scope_getSizeOfVariable(struct Scope *scope, char *name)
{
	struct ScopeMember *examinedEntry = Scope_lookup(scope, name);
	struct VariableEntry *theVariable;
	switch(examinedEntry->type)
	{
		case e_argument:
		case e_variable:
			theVariable = examinedEntry->entry;
			break;

		default:
			ErrorAndExit(ERROR_INTERNAL, "Expected variable entry in scope, got entry type %d instead!\n", examinedEntry->type);
			break;
	}

	switch(theVariable->type)
	{
		case vt_var:
			return 2;

		default:
			ErrorAndExit(ERROR_INTERNAL, "Unexpected variable type %d!\n", theVariable->type);
			break;
	}
}

void Scope_insert(struct Scope *scope, char *name, void *newEntry, enum ScopeMemberType type)
{
	if (Scope_contains(scope, name))
	{
		ErrorAndExit(ERROR_CODE, "Error defining symbol [%s] - name already exists!\n", name);
	}
	struct ScopeMember *wip = malloc(sizeof(struct ScopeMember));
	wip->name = name;
	wip->entry = newEntry;
	wip->type = type;
	Stack_push(scope->entries, wip);
}

void Scope_createVariable(struct Scope *scope, char *name, enum variableTypes type, int indirectionLevel)
{
	struct VariableEntry *newVariable = malloc(sizeof(struct VariableEntry));
	newVariable->type = type;
	newVariable->indirectionLevel = indirectionLevel;
	// we'll see how well this works with structs but should hold together for now...
	newVariable->stackOffset = 0;
	newVariable->assignedAt = -1;
	newVariable->declaredAt = -1;
	newVariable->isAssigned = 0;
	Scope_insert(scope, name, newVariable, e_variable);
	newVariable->stackOffset = scope->parentFunction->localStackSize;
	scope->parentFunction->localStackSize += Scope_getSizeOfVariable(scope, name);
	// return newVariable;
}

void FunctionEntry_createArgument(struct FunctionEntry *func, char *name, enum variableTypes type, int indirectionLevel)
{
	struct VariableEntry *newArgument = malloc(sizeof(struct VariableEntry));
	newArgument->type = type;
	newArgument->indirectionLevel = indirectionLevel;
	newArgument->assignedAt = -1;
	newArgument->declaredAt = -1;
	newArgument->isAssigned = 0;


	Scope_insert(func->mainScope, name, newArgument, e_argument);
	int argSize = Scope_getSizeOfVariable(func->mainScope, name);
	func->argStackSize += argSize;
	newArgument->stackOffset = -1 * (func->argStackSize + argSize);

	// return newArgument;
}

struct FunctionEntry *Scope_createFunction(struct Scope *scope, char *name)
{
	struct FunctionEntry *newFunction = malloc(sizeof(struct FunctionEntry));
	newFunction->argStackSize = 0;
	newFunction->localStackSize = 0;
	newFunction->mainScope = Scope_new(scope, name);
	newFunction->mainScope->parentFunction = newFunction;
	newFunction->returnType = vt_var; // hardcoded... for now ;)
	newFunction->name = name;
	Scope_insert(scope, name, newFunction, e_function);
	return newFunction;
}

struct Scope *Scope_createSubScope(struct Scope *parentScope)
{
	if(parentScope->subScopeCount == 0xff)
	{
		ErrorAndExit(ERROR_INTERNAL, "Too many subscopes of scope %s\n", parentScope->name);
	}
	char *newScopeName = malloc(3 + strlen(parentScope->name) + 1);
	sprintf(newScopeName, "%s_%02x", parentScope->name, parentScope->subScopeCount);
	parentScope->subScopeCount++;
	
	struct Scope *newScope = Scope_new(parentScope, newScopeName);
	newScope->parentFunction = parentScope->parentFunction;
	
	Scope_insert(parentScope, newScopeName, newScope, e_scope);
	return newScope;
}
/*
// currently just does basically the same thing as insertFunction
// since scopes are similar to functions in terms of data structures
// this could change later, so leaving this for easy support
struct Scope *symTab_insertScope(struct SymbolTable *table, char *name)
{
	struct Scope *newScope = Scope_new(name);
	symTabInsert(table, name, newScope, e_scope);
	return newScope;
}

void printSymTabRec(struct SymbolTable *it, int depth, char printTAC)
{
	for (int i = 0; i < depth; i++)
		printf("\t");

	printf("pointer (%p), parent (%p)\n", it, it->parentScope);

	for (int i = 0; i < depth; i++)
		printf("\t");
	printf("~~~~~Symbol table for [%s] (stack footprint of %d:%d)\n", it->name, it->argStackSize, it->localStackSize);
	for (int i = 0; i < it->size; i++)
	{
		for (int i = 0; i < depth; i++)
			printf("\t");

		switch (it->entries[i]->type)
		{
		case e_argument:
		{
			struct VariableEntry *theArgument = it->entries[i]->entry;
			printf("> argument variable ");
			for (int i = 0; i < theArgument->indirectionLevel; i++)
			{
				printf("*");
			}
			printf("[%s] (type %d)\n", it->entries[i]->name, theArgument->type);
		}
		break;

		case e_variable:
		{
			struct VariableEntry *theVariable = it->entries[i]->entry;
			printf("> variable");

			for (int i = 0; i < theVariable->indirectionLevel; i++)
			{
				printf("*");
			}
			printf(" [%s] (type %d)\n", it->entries[i]->name, theVariable->type);
		}
		break;

		case e_function:
		{
			struct FunctionEntry *theFunction = it->entries[i]->entry;
			printf("> function [%s]: %d symbols\n", it->entries[i]->name, theFunction->table->size);
			printSymTabRec(theFunction->table, depth + 1, printTAC);
			// if (printTAC)
			// printTACBlock(theFunction->table->codeBlock, depth);
			printf("\n");
		}
		break;

		case e_scope:
		{
			struct ScopeMember *theScope = it->entries[i]->entry;
			printf("> scope [%s]: %d symbols\n", it->entries[i]->name, theScope->table->size);
			printSymTabRec(theScope->table, depth + 1, printTAC);
			printf("\n");
		}
		break;
		}
	}
	// if (printTAC)
	// printBasicBlockList(it->BasicBlockList, depth * 2);

	printf("\n");
}

void printSymTab(struct SymbolTable *it, char printTAC)
{
	printSymTabRec(it, 0, printTAC);
}



void freeSymTab(struct SymbolTable *it)
{
	for (int i = 0; i < it->size; i++)
	{
		struct symTabEntry *currentEntry = it->entries[i];
		switch (currentEntry->type)
		{
		case e_argument:
		case e_variable:
			free((struct VariableEntry *)currentEntry->entry);
			break;

		case e_function:
			freeSymTab(((struct FunctionEntry *)currentEntry->entry)->table);
			free((struct FunctionEntry *)currentEntry->entry);
			break;

		case e_scope:
			freeSymTab(((struct ScopeMember *)currentEntry->entry)->table);
			free(currentEntry->name);
			break;
		}
		// free(it->name);
		free(currentEntry);
	}

	LinkedList_free(it->BasicBlockList, &BasicBlock_free);

	if (it->tl != NULL)
	{
		freeTempList(it->tl);
	}

	free(it->entries);
	free(it);
}
*/

void Scope_print(struct Scope *it, int depth)
{
	for(int i = 0; i < it->entries->size; i++)
	{
		struct ScopeMember *thisMember = it->entries->data[i];

		for(int j = 0; j < depth; j++)
		{
			printf("\t");
		}

		switch(thisMember->type)
		{
			case e_argument:
			{
				struct VariableEntry *theArgument = thisMember->entry;
				printf("> Argument %s (stack offset %d)\n", thisMember->name, theArgument->stackOffset);
			}
			break;

			case e_variable:
			{
				struct VariableEntry *theVariable = thisMember->entry;
				printf("> Variable %s (stack offset %d)\n", thisMember->name, theVariable->stackOffset);
			}
			break;

			case e_function:
			{
				struct FunctionEntry *theFunction = thisMember->entry;
				printf("> Function %s (returns %d)\n", thisMember->name, theFunction->returnType);
				Scope_print(theFunction->mainScope, depth + 1);
			}
			break;

			case e_scope:
			{
				struct Scope *theScope = thisMember->entry;
				printf("> Subscope %s\n", thisMember->name);
				Scope_print(theScope, depth + 1);
			}
			break;
		}
	}
}


void SymbolTable_print(struct SymbolTable *it, char printTAC)
{
	printf("Symbol Table %s\n", it->name);
	Scope_print(it->globalScope, 0);
}

void SymbolTable_free(struct SymbolTable *it)
{

}

/*
 * AST walk and symbol table generation functions
 */
void walkStatement(struct ASTNode *it, struct Scope *wip)
{
	struct ASTNode *runner;
	switch (it->type)
	{
	case t_scope:
		walkScope(it, Scope_createSubScope(wip), 0);
		break;

	case t_var:
		char *varName;
		int indirectionLevel = 0;
		runner = it;
		char scraping = 1;
		while (scraping)
		{
			runner = runner->child;
			switch (runner->type)
			{
			case t_dereference:
				indirectionLevel++;
				break;

			default:
				scraping = 0;
				break;
			}
		}

		if (runner->type == t_assign)
			varName = runner->child->value;
		else
			varName = runner->value;

		// lookup the variable being assigned, only insert if unique
		// also covers modification of argument values
		if (!Scope_contains(wip, varName))
			Scope_createVariable(wip, varName, vt_var, indirectionLevel);
		else
		{
			ErrorAndExit(ERROR_CODE, "Error - redeclaration of symbol [%s]\n", varName);
		}

		break;

	case t_assign:
		/*
		ignore assignments as lifetime checks can be done more easily on TAC
		runner = it->child;
		while(runner->type == t_dereference){
			runner = runner->child;
		}
		if (!SymbolTableContains(wip, runner->value))
		{
			printf("Error - variable [%s] assigned before declaration\n", runner->child->value);
			exit(1);
		}
		*/
		break;

	case t_if:
		// having fun yet?
		struct ASTNode *ifRunner = it->child->sibling->child;
		while (ifRunner != NULL)
		{
			walkStatement(ifRunner, wip);
			ifRunner = ifRunner->sibling;
		}

		// no, really!
		if (it->child->sibling->sibling != NULL)
		{
			ifRunner = it->child->sibling->sibling->child->child;
			while (ifRunner != NULL)
			{
				walkStatement(ifRunner, wip);
				ifRunner = ifRunner->sibling;
			}
		}

		break;

	case t_while:
		struct ASTNode *whileRunner = it->child->sibling->child;
		while (whileRunner != NULL)
		{
			walkStatement(whileRunner, wip);
			whileRunner = whileRunner->sibling;
		}
		break;

	// function call/return and asm blocks can't create new symbols so ignore
	case t_call:
	case t_return:
	case t_asm:
		break;

	default:
		// TODO: should this really be an internal error?
		ErrorAndExit(ERROR_INTERNAL, "Error walking AST for function %s - expected 'var', name, or function call, saw %s with value of [%s]\n", wip->parentFunction->name, getTokenName(it->type), it->value);
	}
}

void walkScope(struct ASTNode *it, struct Scope *wipScope, char isMainScope)
{
	struct ASTNode *scopeRunner = it->child;
	while (scopeRunner != NULL)
	{
		switch (scopeRunner->type)
		{
			// nested scopes!
		case t_scope:
			walkScope(scopeRunner, Scope_createSubScope(wipScope), 0);
			break;

			// function call/return can't create new symbols so ignore
		case t_call:
		case t_return:
			break;

		case t_if:
			// having fun yet?
			struct ASTNode *ifRunner = scopeRunner->child->sibling->child;
			while (ifRunner != NULL)
			{
				walkStatement(ifRunner, wipScope);
				ifRunner = ifRunner->sibling;
			}

			// no, really! (if an else block exists, walk that too)
			if (scopeRunner->child->sibling->sibling != NULL)
			{
				ifRunner = scopeRunner->child->sibling->sibling->child->child;
				while (ifRunner != NULL)
				{
					walkStatement(ifRunner, wipScope);
					ifRunner = ifRunner->sibling;
				}
			}

			break;

		case t_while:
			struct ASTNode *whileRunner = scopeRunner->child->sibling->child;
			while (whileRunner != NULL)
			{
				walkStatement(whileRunner, wipScope);
				whileRunner = whileRunner->sibling;
			}
			break;

		// otherwise we are looking at the body of the function, which is a statement list
		default:
			walkStatement(scopeRunner, wipScope);
			break;
		}
		scopeRunner = scopeRunner->sibling;
	}
}

void walkFunction(struct ASTNode *it, struct Scope *parentScope)
{
	struct ASTNode *functionRunner = it->child;
	struct FunctionEntry *func = Scope_createFunction(parentScope, functionRunner->value);
	func->mainScope->parentScope = parentScope;
	while (functionRunner != NULL)
	{
		switch (functionRunner->type)
		{
		// looking at function name, which will have argument variables as children
		case t_name:
		{
			struct ASTNode *argumentRunner = functionRunner->child;
			while (argumentRunner != NULL)
			{
				enum variableTypes theType;
				switch (argumentRunner->type)
				{
				case t_var:
					theType = vt_var;
					break;

				default:
					// TODO: should this really be an internal error?
					ErrorAndExit(ERROR_INTERNAL, "Unexpected argument type while walking function!\n");
					break;
				}

				int indirectionLevel = 0;
				struct ASTNode *runner = argumentRunner->child;
				while (runner->child != NULL)
				{
					switch (runner->type)
					{
					case t_dereference:
						indirectionLevel++;
						break;

					case t_reference:
						indirectionLevel--;
						break;

					default:
						ErrorAndExit(ERROR_INTERNAL, "Unexpected token as child of argument definition!");
					}
					runner = runner->child;
				}

				FunctionEntry_createArgument(func, runner->value, theType, indirectionLevel);

				argumentRunner = argumentRunner->sibling;
			}
		}
		break;

		case t_scope:
		{
			walkScope(functionRunner, func->mainScope, 1);
		}
		break;

		default:
			ErrorAndExit(ERROR_INTERNAL, "Malformed AST within function - expected function name and main scope only!\nMalformed node was of type %s with value [%s]\n", getTokenName(functionRunner->type), functionRunner->value);

			/*
			// function call/return can't create new symbols so ignore
			case t_call:
			case t_return:
				break;

			case t_if:
				// having fun yet?
				struct ASTNode *ifRunner = functionRunner->child->sibling->child;
				while (ifRunner != NULL)
				{
					walkStatement(ifRunner, subTable);
					ifRunner = ifRunner->sibling;
				}

				// no, really! (if an else block exists, walk that too)
				if (functionRunner->child->sibling->sibling != NULL)
				{
					ifRunner = functionRunner->child->sibling->sibling->child->child;
					while (ifRunner != NULL)
					{
						walkStatement(ifRunner, subTable);
						ifRunner = ifRunner->sibling;
					}
				}

				break;

			case t_while:
				struct ASTNode *whileRunner = functionRunner->child->sibling->child;
				while (whileRunner != NULL)
				{
					walkStatement(whileRunner, subTable);
					whileRunner = whileRunner->sibling;
				}
				break;

			// otherwise we are looking at the body of the function, which is a statement list
			default:
				walkStatement(functionRunner, subTable);
			*/
		}
		functionRunner = functionRunner->sibling;
	}
}

// given an AST node for a program, walk the AST and generate a symbol table for the entire thing
struct SymbolTable *walkAST(struct ASTNode *it)
{
	struct SymbolTable *programTable = SymbolTable_new("Program");
	programTable->tl = newTempList();
	struct Scope *globalScope = programTable->globalScope;
	struct ASTNode *runner = it;
	while (runner != NULL)
	{
		printf(".");
		switch (runner->type)
		{
		// global variable declarations/definitions are allowed
		// use walkStatement to handle this
		case t_var:
			walkStatement(runner, globalScope);
			struct ASTNode *scraper = runner->child;
			while (scraper->type != t_name)
			{
				scraper = scraper->child;
			}
			break;

		case t_fun:
			walkFunction(runner, globalScope);
			break;

		// ignore asm blocks
		case t_asm:
			break;

		default:
			ErrorAndExit(ERROR_INTERNAL, "Error walking AST - expected 'v' or function declaration\nInstead, got %s with type %d\n", runner->value, runner->type);
			break;
		}
		runner = runner->sibling;
	}
	return programTable;
}
