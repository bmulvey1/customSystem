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

struct symTabEntry *newEntry(char *name, enum symTabEntryType type)
{
	struct symTabEntry *wip = malloc(sizeof(struct symTabEntry));
	wip->name = name;
	wip->type = type;
	wip->entry = NULL;
	return wip;
}

struct variableEntry *newVariableEntry(int indirectionLevel, enum variableTypes type)
{
	struct variableEntry *wip = malloc(sizeof(struct variableEntry));
	wip->global = 0;
	wip->stackOffset = 0;
	wip->indirectionLevel = indirectionLevel;
	wip->type = type;
	wip->assignedAt = -1;
	wip->declaredAt = -1;
	wip->isAssigned = 0;
	return wip;
}

struct functionEntry *newFunctionEntry(struct symbolTable *table)
{
	struct functionEntry *wip = malloc(sizeof(struct functionEntry));
	wip->table = table;
	return wip;
}

struct symbolTable *newSymbolTable(char *name)
{
	struct symbolTable *wip = malloc(sizeof(struct symbolTable));
	wip->size = 0;
	wip->parentScope = NULL;
	wip->entries = NULL;
	wip->BasicBlockList = NULL;
	wip->name = name;
	wip->localStackSize = 0;
	wip->argStackSize = 0;
	// generate a single templist at the top level symTab
	// leave all others as NULL to avoid duplication
	wip->tl = NULL;
	return wip;
}

int symbolTableContains(struct symbolTable *table, char *name)
{
	for (int i = 0; i < table->size; i++)
		if (!strcmp(table->entries[i]->name, name))
			return 1;

	return 0;
}

// return a symbol table entry by name, or NULL if not found
// automatically looks at most local scope, then enclosing scopes until found or no scopes left
struct symTabEntry *symbolTableLookup(struct symbolTable *table, char *name)
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

// return the variableEntry for a given name, or throw a use-before-declare error if nonexistent
struct variableEntry *symbolTableLookup_var(struct symbolTable *table, char *name)
{
	struct symTabEntry *e = symbolTableLookup(table, name);

	if (e != NULL)
		return e->entry;
	else
		{
			ErrorAndExit(ERROR_CODE, "Error - Use of variable [%s] before declaration\n", name);
		}
}

int symbolTable_getSizeOfVariable(struct symbolTable *table, enum variableTypes type)
{
	switch (type)
	{
	case vt_var:
		return 2;

	default:
		ErrorAndExit(ERROR_CODE, "Error - attempt to get size of unknown type!");
	}
}

struct functionEntry *symbolTableLookup_fun(struct symbolTable *table, char *name){
	struct symTabEntry *e = symbolTableLookup(table, name);

	if(e != NULL)
		return e->entry;
	else{
		ErrorAndExit(ERROR_CODE, "Error - Use of function [%s] before declaration\n", name);
	}
}

void symTabInsert(struct symbolTable *table, char *name, void *newEntry, enum symTabEntryType type)
{
	if (symbolTableContains(table, name))
	{
		ErrorAndExit(ERROR_CODE, "Error defining symbol [%s] - name already exists!\n", name);
	}
	struct symTabEntry *wip = malloc(sizeof(struct symTabEntry));
	wip->name = name;
	wip->entry = newEntry;
	wip->type = type;

	struct symTabEntry **newList = malloc((table->size + 1) * sizeof(struct symTabEntry *));
	for (int i = 0; i < table->size; i++)
	{
		newList[i] = table->entries[i];
	}

	newList[table->size] = wip;
	free(table->entries);
	table->entries = newList;
	table->size++;
}

void symTab_insertVariable(struct symbolTable *table, char *name, enum variableTypes type, int indirectionLevel)
{
	struct variableEntry *newVariable = newVariableEntry(indirectionLevel, type);
	symTabInsert(table, name, newVariable, e_variable);
}

void symTab_insertArgument(struct symbolTable *table, char *name, enum variableTypes type, int indirectionLevel)
{
	struct variableEntry *newArgument = newVariableEntry(indirectionLevel, type);
	newArgument->isAssigned = 1;
	newArgument->assignedAt = 0;
	newArgument->declaredAt = 0;
	switch (type)
	{
	case vt_var:
		table->argStackSize += 2;
		newArgument->stackOffset = (table->argStackSize + 2);
		break;

	default:
		break;
	}
	symTabInsert(table, name, newArgument, e_argument);
}

void symTab_insertFunction(struct symbolTable *table, char *name, struct symbolTable *subTable)
{
	struct functionEntry *newFunction = newFunctionEntry(subTable);
	newFunction->returnType = vt_var; // hardcoded... for now ;)
	symTabInsert(table, name, newFunction, e_function);
}

void printSymTabRec(struct symbolTable *it, int depth, char printTAC)
{
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
			struct variableEntry *theArgument = it->entries[i]->entry;
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
			struct variableEntry *theVariable = it->entries[i]->entry;
			if (theVariable->global)
			{
				printf("> global variable");
			}
			else
			{
				printf("> variable");
			}

			for (int i = 0; i < theVariable->indirectionLevel; i++)
			{
				printf("*");
			}
			printf(" [%s] (type %d)\n", it->entries[i]->name, theVariable->type);
		}
		break;

		case e_function:
		{
			struct functionEntry *theFunction = it->entries[i]->entry;
			printf("> function [%s]: %d symbols\n", it->entries[i]->name, theFunction->table->size);
			printSymTabRec(theFunction->table, depth + 1, printTAC);
			// if (printTAC)
			// printTACBlock(theFunction->table->codeBlock, depth);
			printf("\n");
		}
		break;
		}
	}
	// if (printTAC)
	// printBasicBlockList(it->BasicBlockList, depth * 2);

	printf("\n");
}

void printSymTab(struct symbolTable *it, char printTAC)
{
	printSymTabRec(it, 0, printTAC);
}

void freeSymTab(struct symbolTable *it)
{
	for (int i = 0; i < it->size; i++)
	{
		struct symTabEntry *currentEntry = it->entries[i];
		switch (currentEntry->type)
		{
		case e_argument:
		case e_variable:
			free((struct variableEntry *)currentEntry->entry);
			break;

		case e_function:
			freeSymTab(((struct functionEntry *)currentEntry->entry)->table);
			free((struct functionEntry *)currentEntry->entry);
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

/*
 * AST walk and symbol table generation functions
 */
void walkStatement(struct ASTNode *it, struct symbolTable *wip)
{
	struct ASTNode *runner;
	switch (it->type)
	{
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
		if (!symbolTableContains(wip, varName))
			symTab_insertVariable(wip, varName, vt_var, indirectionLevel);
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
		if (!symbolTableContains(wip, runner->value))
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
		ErrorAndExit(ERROR_INTERNAL, "Error walking AST for function %s - expected 'var', name, or function call, saw %s with value of [%s]\n", it->child->value, getTokenName(it->type), it->value);
	}
}

void walkFunction(struct ASTNode *it, struct symbolTable *wip)
{
	struct ASTNode *functionRunner = it->child;
	char *functionName = functionRunner->value;
	struct symbolTable *subTable = newSymbolTable(functionName);
	subTable->parentScope = wip;
	while (functionRunner != NULL)
	{
		switch (functionRunner->type)
		{
		// looking at function name, which will have argument variables as children
		case t_name:
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

				symTab_insertArgument(subTable, runner->value, theType, indirectionLevel);

				argumentRunner = argumentRunner->sibling;
			}
			break;

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
		}
		functionRunner = functionRunner->sibling;
	}
	symTab_insertFunction(wip, functionName, subTable);
}

// given an AST node for a program, walk the AST and generate a symbol table for the entire thing
struct symbolTable *walkAST(struct ASTNode *it)
{
	struct symbolTable *wip = newSymbolTable("Program");
	wip->tl = newTempList();
	struct ASTNode *runner = it;
	while (runner != NULL)
	{
		printf(".");
		switch (runner->type)
		{

		// global variable declarations/definitions are allowed
		// use walkStatement to handle this
		case t_var:
			walkStatement(runner, wip);
			struct ASTNode *scraper = runner->child;
			while (scraper->type != t_name)
			{
				scraper = scraper->child;
			}

			struct variableEntry *theGlobal = symbolTableLookup(wip, scraper->value)->entry;
			if (theGlobal->global == 0)
				theGlobal->global = 1;

			break;

		case t_fun:
			walkFunction(runner, wip);
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
	return wip;
}
