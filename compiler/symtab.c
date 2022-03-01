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

struct variableEntry *newVariableEntry()
{
    struct variableEntry *wip = malloc(sizeof(struct variableEntry));
    wip->isAssigned = 0;
    wip->ensureWriteBack = 0;
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
    wip->entries = NULL;
    wip->codeBlock = NULL;
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
struct symTabEntry *symbolTableLookup(struct symbolTable *table, char *name)
{
    for (int i = 0; i < table->size; i++)
        if (!strcmp(table->entries[i]->name, name))
            return table->entries[i];

    return NULL;
}

void symTabInsert(struct symbolTable *table, char *name, void *newEntry, enum symTabEntryType type)
{
    if (symbolTableContains(table, name))
    {
        printf("Error defining symbol [%s] - name already exists!\n", name);
        exit(1);
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

void symTab_insertVariable(struct symbolTable *table, char *name, enum variableTypes type)
{
    struct variableEntry *newVariable = newVariableEntry();
    newVariable->stackOffset = (table->localStackSize * -1) - 2;
    switch (type)
    {
    case vt_var:
        table->localStackSize += 2;
        break;

    default:
        break;
    }
    symTabInsert(table, name, newVariable, e_variable);
}

void symTab_insertArgument(struct symbolTable *table, char *name, enum variableTypes type)
{
    struct variableEntry *newArgument = newVariableEntry();
    newArgument->isAssigned = 1;
    newArgument->stackOffset = table->argStackSize + 4;
    switch (type)
    {
    case vt_var:
        table->argStackSize += 2;
        break;

    default:
        break;
    }
    symTabInsert(table, name, newArgument, e_argument);
}

void symTab_insertFunction(struct symbolTable *table, char *name, struct symbolTable *subTable)
{
    struct functionEntry *newFunction = newFunctionEntry(subTable);
    symTabInsert(table, name, newFunction, e_function);
}

void printSymTabRec(struct symbolTable *it, int depth)
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
            printf("> argument [%s] - stack offset %d\n", it->entries[i]->name, theArgument->stackOffset);
        }
        break;

        case e_variable:
        {
            struct variableEntry *theVariable = it->entries[i]->entry;
            printf("> variable [%s] - stack offset %d\n", it->entries[i]->name, theVariable->stackOffset);
        }
        break;

        case e_function:
        {
            struct functionEntry *theFunction = it->entries[i]->entry;
            printf("> function [%s]: %d symbols\n", it->entries[i]->name, theFunction->table->size);
            printSymTabRec(theFunction->table, depth + 1);
            //printTACBlock(theFunction->table->codeBlock);
            printf("\n");
        }
        break;
        }
    }
    for (int i = 0; i < depth; i++)
        printf("\t");
    printf("\n");
}

void printSymTab(struct symbolTable *it)
{
    printSymTabRec(it, 0);
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
        //free(it->name);
        free(currentEntry);
    }
    freeTAC(it->codeBlock);

    if (it->tl != NULL)
    {
        freeTempList(it->tl);
    }
    
    free(it->entries);
    free(it);
}

// finds and returns the positive/negative offset of a variable
// value is byte offset relative to base pointer of provided function
int findInStack(char *var, struct symbolTable *table)
{
    struct symTabEntry *theEntry = symbolTableLookup(table, var);
    if (theEntry == NULL)
    {
        printf("UNABLE TO FIND VARIABLE %s IN SYMBOL TABLE\n", var);
        exit(0);
    }
    switch (theEntry->type)
    {
    case e_variable:
    case e_argument:
        return ((struct variableEntry *)theEntry->entry)->stackOffset;

    default:
        printf("Saw something illegal while trying to locate variable/argument %s on the stack\n", var);
        exit(1);
    }
}

/*
 * AST walk and symbol table generation functions
 */
void walkStatement(struct astNode *it, struct symbolTable *wip)
{
    switch (it->type)
    {
    case t_var:
        char *varName;
        if (it->child->type == t_assign)
            varName = it->child->child->value;
        else
            varName = it->child->value;

        // lookup the variable being assigned, only insert if unique
        // also covers modification of argument values
        if (!symbolTableContains(wip, varName))
        {
            symTab_insertVariable(wip, varName, vt_var);
        }

        break;

    case t_assign:
        if (!symbolTableContains(wip, it->child->value))
        {
            printf("Error - variable [%s] assigned before declaration\n", it->child->value);
            exit(1);
        }
        break;

    case t_if:
        // having fun yet?
        struct astNode *ifRunner = it->child->sibling->child;
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
        struct astNode *whileRunner = it->child->sibling->child;
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
        printf("Error walking AST for function %s - expected 'var', name, or function call, saw %s with value of [%s]\n", it->child->value, getTokenName(it->type), it->value);
        exit(1);
    }
}

void walkFunction(struct astNode *it, struct symbolTable *wip)
{
    struct astNode *functionRunner = it->child;
    char *functionName = functionRunner->value;
    struct symbolTable *subTable = newSymbolTable(functionName);
    while (functionRunner != NULL)
    {
        // printAST(functionRunner, 0);
        switch (functionRunner->type)
        {
        // looking at function name, which will have argument variables as children
        case t_name:
            printf("GOT A NAME\n");
            struct astNode *argumentRunner = functionRunner->child;
            printAST(argumentRunner, 0);
            while (argumentRunner != NULL)
            {
                enum variableTypes theType;
                switch (argumentRunner->type)
                {
                case t_var:
                    theType = vt_var;
                    break;

                default:
                    printf("Unexpected argument type while walking function!\n");
                    exit(1);
                    break;
                }
                symTab_insertArgument(subTable, argumentRunner->child->value, theType);

                argumentRunner = argumentRunner->sibling;
            }
            break;

        // function call/return can't create new symbols so ignore
        case t_call:
        case t_return:
            break;

        case t_if:
            // having fun yet?
            struct astNode *ifRunner = functionRunner->child->sibling->child;
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
            struct astNode *whileRunner = functionRunner->child->sibling->child;
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
struct symbolTable *walkAST(struct astNode *it)
{
    struct symbolTable *wip = newSymbolTable("Program");
    wip->tl = newTempList();
    struct astNode *runner = it;
    while (runner != NULL)
    {
        printf(".");
        switch (runner->type)
        {

        case t_fun:
            walkFunction(runner, wip);
            break;

        // ignore asm blocks
        case t_asm:
            break;

        default:
            printf("Error walking AST - expected 'v' or function declaration\n");
            exit(1);
            break;
        }
        runner = runner->sibling;
    }
    return wip;
}
