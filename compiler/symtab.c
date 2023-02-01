#include "symtab.h"

extern struct Dictionary *parseDict;

char *symbolNames[] = {
	"variable",
	"function"};

// create a variable denoted to be an argument within the given function entry
void FunctionEntry_createArgument(struct FunctionEntry *func, char *name, enum variableTypes type, int indirectionLevel, int arraySize)
{
	struct VariableEntry *newArgument = malloc(sizeof(struct VariableEntry));
	newArgument->type = type;
	newArgument->indirectionLevel = indirectionLevel;
	newArgument->assignedAt = -1;
	newArgument->declaredAt = -1;
	newArgument->isAssigned = 0;
	newArgument->mustSpill = 0;
	newArgument->name = name;

	int argSize = Scope_getSizeOfVariable(func->mainScope, name);
	newArgument->stackOffset = func->argStackSize + 8;
	func->argStackSize += argSize;

	if (arraySize > 1)
	{
		struct StackObjectEntry *objForArg = malloc(sizeof(struct StackObjectEntry));
		newArgument->localPointerTo = objForArg;
		objForArg->localPointer = newArgument;

		objForArg->size = argSize;
		objForArg->arraySize = arraySize;
		char *modName = malloc(strlen(name) + 5);
		sprintf(modName, "%s.obj", name);
		Scope_insert(func->mainScope, Dictionary_LookupOrInsert(parseDict, modName), objForArg, e_stackobj);
		free(modName);
	}
	else
	{
		newArgument->localPointerTo = NULL;
	}

	Scope_insert(func->mainScope, name, newArgument, e_argument);

	// return newArgument;
}

struct SymbolTable *SymbolTable_new(char *name)
{
	struct SymbolTable *wip = malloc(sizeof(struct SymbolTable));
	wip->name = name;
	wip->globalScope = Scope_new(NULL, "Global");
	struct BasicBlock *globalBlock = BasicBlock_new(0);
	// char *globalBlockName = malloc(12);
	// sprintf(globalBlockName, "globalblock");
	// manually insert a basic block for global code so we can give it the custom name of "globalblock"
	Scope_insert(wip->globalScope, "globalblock", globalBlock, e_basicblock);

	return wip;
}

void SymbolTable_print(struct SymbolTable *it, char printTAC)
{
	printf("Symbol Table %s\n", it->name);
	Scope_print(it->globalScope, 0, printTAC);
}

void SymbolTable_free(struct SymbolTable *it)
{
	Scope_free(it->globalScope, 0);
	free(it);
}

/*
 * Scope functions
 *
 */
struct Scope *Scope_new(struct Scope *parentScope, char *name)
{
	struct Scope *wip = malloc(sizeof(struct Scope));
	wip->entries = Stack_New();

	// need to set this manually to handle when new functions are declared
	// TODO: supports nested functions? ;)
	wip->parentFunction = NULL;
	wip->parentScope = parentScope;
	wip->name = name;
	wip->subScopeCount = 0;
	return wip;
}

void Scope_free(struct Scope *scope, int depth)
{
	for (int i = 0; i < scope->entries->size; i++)
	{
		struct ScopeMember *examinedEntry = scope->entries->data[i];
		switch (examinedEntry->type)
		{
		case e_scope:
			Scope_free(examinedEntry->entry, depth + 1);
			break;

		case e_function:
		{
			struct FunctionEntry *theFunction = examinedEntry->entry;
			LinkedList_Free(theFunction->BasicBlockList, NULL);
			Scope_free(theFunction->mainScope, depth);
			free(theFunction);
		}
		break;

		case e_variable:
		case e_argument:
		{
			struct VariableEntry *theVariable = examinedEntry->entry;
			free(theVariable);
		}
		break;

		case e_basicblock:
			BasicBlock_free(examinedEntry->entry);
			break;

		case e_stackobj:
			free(examinedEntry->entry);
			break;
		}

		free(examinedEntry);
	}
	Stack_Free(scope->entries);
	free(scope);
}

// insert a member with a given name and pointer to entry, along with info about the entry type
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
	Stack_Push(scope->entries, wip);
}

// create a variable within the given scope
void Scope_createVariable(struct Scope *scope, char *name, enum variableTypes type, int indirectionLevel, int arraySize)
{
	struct VariableEntry *newVariable = malloc(sizeof(struct VariableEntry));
	newVariable->type = type;
	newVariable->indirectionLevel = indirectionLevel;
	newVariable->stackOffset = 0;
	newVariable->assignedAt = -1;
	newVariable->declaredAt = -1;
	newVariable->isAssigned = 0;
	newVariable->mustSpill = 0;
	newVariable->name = name;

	int varSize = Scope_getSizeOfVariable(scope, name);

	if (arraySize > 1)
	{
		struct StackObjectEntry *objForvar = malloc(sizeof(struct StackObjectEntry));
		newVariable->localPointerTo = objForvar;
		objForvar->localPointer = newVariable;

		objForvar->size = varSize;
		objForvar->arraySize = arraySize;
		// since indexing pushes address forward, set the stack offset of the variable to be index 0 (the - size * arraySize) term does this
		objForvar->stackOffset = (scope->parentFunction->localStackSize * -1) - (objForvar->size * objForvar->arraySize);
		scope->parentFunction->localStackSize += varSize * arraySize;
		char *modName = malloc(strlen(name) + 5);
		sprintf(modName, "%s.obj", name);
		Scope_insert(scope, Dictionary_LookupOrInsert(parseDict, modName), objForvar, e_stackobj);
		free(modName);
	}
	else
	{
		newVariable->localPointerTo = NULL;
	}
	Scope_insert(scope, name, newVariable, e_variable);
	// scope->parentFunction->localStackSize += ( * arraySize);
	// return newVariable;
}

// create a new function accessible within the given scope
struct FunctionEntry *Scope_createFunction(struct Scope *scope, char *name)
{
	struct FunctionEntry *newFunction = malloc(sizeof(struct FunctionEntry));
	newFunction->argStackSize = 0;
	newFunction->localStackSize = 0;
	newFunction->mainScope = Scope_new(scope, "");
	newFunction->BasicBlockList = LinkedList_New();
	newFunction->mainScope->parentFunction = newFunction;
	newFunction->returnType = vt_var; // hardcoded... for now ;)
	newFunction->name = name;
	Scope_insert(scope, name, newFunction, e_function);
	return newFunction;
}

// create and return a child scope of the scope provided as an argument
struct Scope *Scope_createSubScope(struct Scope *parentScope)
{
	if (parentScope->subScopeCount == 0xff)
	{
		ErrorAndExit(ERROR_INTERNAL, "Too many subscopes of scope %s\n", parentScope->name);
	}
	char *helpStr = malloc(3 + strlen(parentScope->name) + 1);
	sprintf(helpStr, ".%02x", parentScope->subScopeCount);
	char *newScopeName = Dictionary_LookupOrInsert(parseDict, helpStr);
	free(helpStr);
	parentScope->subScopeCount++;

	struct Scope *newScope = Scope_new(parentScope, newScopeName);
	newScope->parentFunction = parentScope->parentFunction;

	Scope_insert(parentScope, newScopeName, newScope, e_scope);
	return newScope;
}

// Scope lookup functions

char Scope_contains(struct Scope *scope, char *name)
{
	for (int i = 0; i < scope->entries->size; i++)
	{
		if (!strcmp(name, ((struct ScopeMember *)scope->entries->data[i])->name))
		{
			return 1;
		}
	}
	return 0;
}

// if a member with the given name exists in this scope or any of its parents, return it
// also looks up entries from deeper scopes, but only as their mangled names specify
struct ScopeMember *Scope_lookup(struct Scope *scope, char *name)
{
	while (scope != NULL)
	{
		for (int i = 0; i < scope->entries->size; i++)
		{
			struct ScopeMember *examinedEntry = scope->entries->data[i];
			if (!strcmp(examinedEntry->name, name))
			{
				return examinedEntry;
			}
		}

		scope = scope->parentScope;
	}
	return NULL;
}

struct VariableEntry *Scope_lookupVar(struct Scope *scope, char *name)
{
	struct ScopeMember *lookedUp = Scope_lookup(scope, name);
	if (lookedUp == NULL)
	{
		ErrorAndExit(ERROR_CODE, "Use of undeclared variable [%s]\n", name);
	}

	switch (lookedUp->type)
	{
	case e_argument:
	case e_variable:
		return lookedUp->entry;

	default:
		ErrorAndExit(ERROR_CODE, "Use of undeclared variable [%s]\n", name);
	}
}

struct FunctionEntry *Scope_lookupFun(struct Scope *scope, char *name)
{
	struct ScopeMember *lookedUp = Scope_lookup(scope, name);
	if (lookedUp == NULL)
	{
		ErrorAndExit(ERROR_CODE, "Use of undeclared function [%s]\n", name);
	}
	switch (lookedUp->type)
	{
	case e_function:
		return lookedUp->entry;

	default:
		ErrorAndExit(ERROR_CODE, "Use of undeclared function [%s]\n", name);
	}
}

struct Scope *Scope_lookupSubScope(struct Scope *scope, char *name)
{
	struct ScopeMember *lookedUp = Scope_lookup(scope, name);
	if (lookedUp == NULL)
	{
		ErrorAndExit(ERROR_INTERNAL, "Use of undeclared scope [%s]\n", name);
	}

	switch (lookedUp->type)
	{
	case e_scope:
		return lookedUp->entry;

	default:
		ErrorAndExit(ERROR_INTERNAL, "Use of undeclared scope [%s]\n", name);
	}
}

struct Scope *Scope_lookupSubScopeByNumber(struct Scope *scope, unsigned char subScopeNumber)
{
	char subScopeName[4];
	sprintf(subScopeName, ".%02x", subScopeNumber);
	struct Scope *lookedUp = Scope_lookupSubScope(scope, subScopeName);
	return lookedUp;
}

int Scope_getSizeOfVariable(struct Scope *scope, char *name)
{
	return 4;
	/*
	switch (theVariable->type)
	{
	case vt_var:
		return 2;

	default:
		ErrorAndExit(ERROR_INTERNAL, "Unexpected variable type %d!\n", theVariable->type);
		break;
	}
	*/
}

void Scope_print(struct Scope *it, int depth, char printTAC)
{
	for (int i = 0; i < it->entries->size; i++)
	{
		struct ScopeMember *thisMember = it->entries->data[i];

		if (thisMember->type != e_basicblock || printTAC)
		{
			for (int j = 0; j < depth; j++)
			{
				printf("\t");
			}
		}

		switch (thisMember->type)
		{
		case e_argument:
		{
			struct VariableEntry *theArgument = thisMember->entry;
			printf("> Argument %s", thisMember->name);
			if (theArgument->localPointerTo != NULL)
			{
				printf("[%d]", theArgument->localPointerTo->arraySize);
			}
			printf(" (stack offset %d)\n", theArgument->stackOffset);
		}
		break;

		case e_variable:
		{
			struct VariableEntry *theVariable = thisMember->entry;
			printf("> Variable %s", thisMember->name);
			if (theVariable->localPointerTo != NULL)
			{
				printf("[%d]", theVariable->localPointerTo->arraySize);
			}
			printf("\n");
			// printf(" (stack offset %d)\n", theVariable->stackOffset);
		}
		break;

		case e_function:
		{
			struct FunctionEntry *theFunction = thisMember->entry;
			printf("> Function %s (returns %d)\n", thisMember->name, theFunction->returnType);
			if (printTAC)
			{
				for (struct LinkedListNode *b = theFunction->BasicBlockList->head; b != NULL; b = b->next)
				{
					printBasicBlock(b->data, depth + 1);
				}
			}
			Scope_print(theFunction->mainScope, depth + 1, printTAC);
			printf("\n");
		}
		break;

		case e_scope:
		{
			struct Scope *theScope = thisMember->entry;
			printf("> Subscope %s\n", thisMember->name);
			Scope_print(theScope, depth + 1, printTAC);
		}
		break;

		case e_basicblock:
		{
			if (printTAC)
			{
				struct BasicBlock *thisBlock = thisMember->entry;
				printf("> Basic Block %d\n", thisBlock->labelNum);
				printBasicBlock(thisBlock, depth + 1);
			}
		}
		break;

		case e_stackobj:
		{
			struct StackObjectEntry *thisObj = thisMember->entry;
			printf("> Stack object (%d bytes * %d) for %s (stack offset of %d)\n", thisObj->size, thisObj->arraySize, thisObj->localPointer->name, thisObj->stackOffset);
		}
		break;
		}
	}
}

void Scope_addBasicBlock(struct Scope *scope, struct BasicBlock *b)
{
	char *blockName = malloc(10);
	sprintf(blockName, "Block%d", b->labelNum);
	Scope_insert(scope, Dictionary_LookupOrInsert(parseDict, blockName), b, e_basicblock);
	free(blockName);
}

void Function_addBasicBlock(struct FunctionEntry *function, struct BasicBlock *b)
{
	LinkedList_Append(function->BasicBlockList, b);
}

/*
 * AST walk and symbol table generation functions
 */
void walkStatement(struct AST *it, struct Scope *wip)
{
	struct AST *runner;
	switch (it->type)
	{
	case t_scope:
		walkScope(it, Scope_createSubScope(wip), 0);
		break;

	case t_var:
	{
		char *varName;
		int arraySize;
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
		{
			varName = runner->child->value;
		}
		else
		{
			if (runner->type == t_array)
			{
				varName = runner->child->value;
				arraySize = atoi(runner->child->sibling->value);
			}
			else
			{
				varName = runner->value;
				arraySize = 1;
			}
		}

		// lookup the variable being assigned, only insert if unique
		// also covers modification of argument values
		if (!Scope_contains(wip, varName))
		{
			Scope_createVariable(wip, varName, vt_var, indirectionLevel, arraySize);
		}
		else
		{
			ErrorAndExit(ERROR_CODE, "Error - redeclaration of symbol [%s]\n", varName);
		}
	}
	break;

		// ignore assignments as lifetime checks can be done more easily on TAC
	case t_assign:
		break;

	case t_if:
	{
		// having fun yet?
		struct AST *ifRunner = it->child->sibling->child;
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
	}
	break;

	case t_while:
	{
		struct AST *whileRunner = it->child->sibling->child;
		while (whileRunner != NULL)
		{
			walkStatement(whileRunner, wip);
			whileRunner = whileRunner->sibling;
		}
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

void walkScope(struct AST *it, struct Scope *wipScope, char isMainScope)
{
	struct AST *scopeRunner = it->child;
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
		{
			// having fun yet?
			struct AST *ifRunner = scopeRunner->child->sibling->child;
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
		}
		break;

		case t_while:
		{
			struct AST *whileRunner = scopeRunner->child->sibling->child;
			while (whileRunner != NULL)
			{
				walkStatement(whileRunner, wipScope);
				whileRunner = whileRunner->sibling;
			}
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

void walkFunction(struct AST *it, struct Scope *parentScope)
{
	struct AST *functionRunner = it->child;
	struct FunctionEntry *func = Scope_createFunction(parentScope, functionRunner->value);
	func->mainScope->parentScope = parentScope;
	while (functionRunner != NULL)
	{
		switch (functionRunner->type)
		{
		// looking at function name, which will have argument variables as children
		case t_name:
		{
			struct AST *argumentRunner = functionRunner->child;
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

				char *argName;
				int arraySize;
				int indirectionLevel = 0;
				struct AST *runner = argumentRunner;
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
				{
					argName = runner->child->value;
				}
				else
				{
					if (runner->type == t_array)
					{
						argName = runner->child->value;
						arraySize = atoi(runner->child->sibling->value);
					}
					else
					{
						argName = runner->value;
						arraySize = 1;
					}
				}

				// lookup the variable being assigned, only insert if unique
				// also covers modification of argument values
				if (!Scope_contains(func->mainScope, argName))
				{
					FunctionEntry_createArgument(func, argName, theType, indirectionLevel, arraySize);
				}
				else
				{
					ErrorAndExit(ERROR_CODE, "Error - redeclaration of symbol [%s]\n", argName);
				}

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
struct SymbolTable *walkAST(struct AST *it)
{
	struct SymbolTable *programTable = SymbolTable_new("Program");
	struct AST *runner = it;
	while (runner != NULL)
	{
		printf(".");
		switch (runner->type)
		{
		// global variable declarations/definitions are allowed
		// use walkStatement to handle this
		case t_var:
		{
			walkStatement(runner, programTable->globalScope);
			struct AST *scraper = runner->child;
			while (scraper->type != t_name)
			{
				scraper = scraper->child;
			}
		}
		break;

		case t_fun:
			walkFunction(runner, programTable->globalScope);
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
