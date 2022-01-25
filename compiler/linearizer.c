#include "linearizer.h"


/*
 * These functions walk the AST and convert it to three-address code
 */

struct tacLine *linearizeExpression(struct astNode *it, int *tempNum, struct tempList *tl);

struct tacLine *linearizeFunctionCall(struct astNode *it, int *tempNum, struct tempList *tl)
{
    struct astNode *runner = it->child->child;
    struct tacLine *calltac = newtacLine();
    calltac->operands[0] = getTempString(tl, *tempNum);
    calltac->operandTypes[0] = vt_temp;

    (*tempNum)++;
    calltac->operands[1] = it->child->value;
    calltac->operandTypes[1] = vt_returnval;
    calltac->operation = tt_call;

    while (runner != NULL)
    {
        struct tacLine *thisArgument = NULL;
        switch (runner->type)
        {
        case t_name:
            thisArgument = newtacLine();
            thisArgument->operandTypes[0] = vt_var;
        case t_literal:
            if (thisArgument == NULL)
            {
                thisArgument = newtacLine();
                thisArgument->operandTypes[0] = vt_literal;
            }
            thisArgument->operands[0] = runner->value;
            thisArgument->operation = tt_push;
            break;

        default:

            struct tacLine *pushOperation = newtacLine();
            pushOperation->operands[0] = getTempString(tl, *tempNum);
            pushOperation->operandTypes[0] = vt_temp;
            pushOperation->operation = tt_push;

            thisArgument = linearizeExpression(runner, tempNum, tl);
            appendTAC(thisArgument, pushOperation);
        }

        calltac = prependTAC(calltac, thisArgument);
        runner = runner->sibling;
    }

    return calltac;
}

// given an AST node of an expression, figure out how to break it down into multiple lines of three address code
struct tacLine *linearizeExpression(struct astNode *it, int *tempNum, struct tempList *tl)
{
    struct tacLine *thisExpression = newtacLine();
    struct tacLine *returnExpression = NULL;

    thisExpression->operands[0] = getTempString(tl, *tempNum);
    thisExpression->operandTypes[0] = vt_temp;
    // increment count of temp variables, the parse of this expression will be written to a temp
    (*tempNum)++;

    switch (it->child->type)
    {
    case t_call:
        thisExpression->operandTypes[1] = vt_temp;
        thisExpression->operands[1] = getTempString(tl, *tempNum);
        returnExpression = prependTAC(thisExpression, linearizeFunctionCall(it->child, tempNum, tl));
        break;

    case t_unOp:
        thisExpression->operandTypes[1] = vt_temp;
        thisExpression->operands[1] = getTempString(tl, *tempNum);
        returnExpression = prependTAC(thisExpression, linearizeExpression(it->child, tempNum, tl));
        break;

    case t_name:
        thisExpression->operands[1] = it->child->value;
        thisExpression->operandTypes[1] = vt_var;
        break;

    case t_literal:
        thisExpression->operands[1] = it->child->value;
        thisExpression->operandTypes[1] = vt_literal;
        break;

        /*case t_call:
            thisExpression->operands[1] = getTempString(tl, *tempNum);
            thisExpression->operandTypes[1] = vt_temp;
            returnExpression = prependTAC(returnExpression, linearizeFunctionCall(it->child, tempNum, tl));
            break;*/

    default:
        printf("Unexpected type seen while linearizing expression! Expected variable or literal\n");
        exit(1);
    }
    /*// check left child
    if (it->child->type == t_unOp || it->child->type == t_call)
    {
        thisExpression->operandTypes[1] = vt_temp;
        thisExpression->operands[1] = getTempString(tl, *tempNum);
        returnExpression = prependTAC(thisExpression, linearizeExpression(it->child, tempNum, tl));
    }
    else
    {
        // otherwise we just have an existing variable, write it to the corresponding address in the TAC line
        thisExpression->operands[1] = it->child->value;
        switch (it->child->type)
        {
        case t_name:
            thisExpression->operandTypes[1] = vt_var;
            break;

        case t_literal:
            thisExpression->operandTypes[1] = vt_literal;
            break;

            case t_call:
            thisExpression->operands[1] = getTempString(tl, *tempNum);
            thisExpression->operandTypes[1] = vt_temp;
            returnExpression = prependTAC(returnExpression, linearizeFunctionCall(it->child, tempNum, tl));
            break;

default:
    printf("Unexpected type seen while linearizing expression! Expected variable or literal\n");
    exit(1);
}
}
*/

    switch (it->value[0])
    {
    case '+':
        thisExpression->reorderable = 1;
        thisExpression->operation = tt_add;
        break;

    case '-':
        thisExpression->operation = tt_subtract;
        break;
    }
    // thisExpression->operation = it->value;

    switch (it->child->sibling->type)
    {
    case t_call:
        thisExpression->operandTypes[2] = vt_temp;
        thisExpression->operands[2] = getTempString(tl, *tempNum);
        returnExpression = prependTAC(returnExpression, linearizeFunctionCall(it->child->sibling, tempNum, tl));
        break;

    case t_unOp:
        thisExpression->operands[2] = getTempString(tl, *tempNum);
        thisExpression->operandTypes[2] = vt_temp;
        // when recursing, we need to prepend this line so things happen in the correct order
        // figure out what to prepend to, then set the return expression TACline to the head of the block
        if (returnExpression != NULL)
            returnExpression = prependTAC(returnExpression, linearizeExpression(it->child->sibling, tempNum, tl));
        else
            returnExpression = prependTAC(thisExpression, linearizeExpression(it->child->sibling, tempNum, tl));
        break;

    case t_name:
        thisExpression->operands[2] = it->child->sibling->value;
        thisExpression->operandTypes[2] = vt_var;
        break;

    case t_literal:
        thisExpression->operands[2] = it->child->sibling->value;
        thisExpression->operandTypes[2] = vt_literal;
        break;

        /*case t_call:
            thisExpression->operands[2] = getTempString(tl, *tempNum);
            thisExpression->operandTypes[2] = vt_temp;
            returnExpression = prependTAC(returnExpression, linearizeFunctionCall(it->child->sibling, tempNum, tl));
            break;*/

    default:
        printf("Unexpected type seen while linearizing expression!\n Expected variable or literal\n");
        exit(1);
    }

    /*
    // check right child, same as left but with correct address index
    if (it->child->sibling->type == t_unOp || it->child->sibling->type == t_call)
    {
        thisExpression->operands[2] = getTempString(tl, *tempNum);
        thisExpression->operandTypes[2] = vt_temp;
        // when recursing, we need to prepend this line so things happen in the correct order
        // figure out what to prepend to, then set the return expression TACline to the head of the block
        if (returnExpression != NULL)
            returnExpression = prependTAC(returnExpression, linearizeExpression(it->child->sibling, tempNum, tl));
        else
            returnExpression = prependTAC(thisExpression, linearizeExpression(it->child->sibling, tempNum, tl));
    }
    else
    {
        thisExpression->operands[2] = it->child->sibling->value;
        switch (it->child->sibling->type)
        {
        case t_name:
            thisExpression->operandTypes[2] = vt_var;
            break;

        case t_literal:
            thisExpression->operandTypes[2] = vt_literal;
            break;

            case t_call:
            thisExpression->operands[2] = getTempString(tl, *tempNum);
            thisExpression->operandTypes[2] = vt_temp;
            returnExpression = prependTAC(returnExpression, linearizeFunctionCall(it->child->sibling, tempNum, tl));
            break;

        default:
            printf("Unexpected type seen while linearizing expression!\n Expected variable or literal\n");
            exit(1);
        }
    }
    */

    // if we have prepended, make sure to return the first line of TAC
    // otherwise there is only one line of TAC to return, so return that
    if (returnExpression == NULL)
        returnExpression = thisExpression;

    return returnExpression;
}

// given an AST node of an assignment, return the TAC block necessary to generate the correct value
struct tacLine *linearizeAssignment(struct astNode *it, int *tempNum, struct tempList *tl)
{
    struct tacLine *assignment;

    // if this assignment is simply setting one thing to another
    if (it->child->sibling->child == NULL)
    {
        // pull out the relevant values and generate a single line of TAC to return
        assignment = newtacLine();
        assignment->operands[0] = it->child->value;
        assignment->operands[1] = it->child->sibling->value;
        assignment->operandTypes[0] = vt_var;
        assignment->operandTypes[2] = vt_null;

        switch (it->child->sibling->type)
        {
        case t_literal:
            assignment->operandTypes[1] = vt_literal;
            break;

        case t_name:
            assignment->operandTypes[1] = vt_var;
            break;

        default:
            printf("Error parsing simple assignment - unexpected type on RHS\n");
            exit(1);
        }
    }
    // otherwise there is some sort of expression, which will need to be broken down into multiple lines of TAC
    else
    {
        switch (it->child->sibling->type)
        {
        case t_unOp:
            assignment = linearizeExpression(it->child->sibling, tempNum, tl);
            break;

        case t_call:
            assignment = linearizeFunctionCall(it->child->sibling, tempNum, tl);
            break;

        default:
            printf("Error linearizing expression - malformed parse tree (expected unOp or call)\n");
            exit(1);
        }
        struct tacLine *lastLine = findLastTAC(assignment);

        // set the final line's operand 0 to the variable actually being assigned
        lastLine->operands[0] = it->child->value;
        lastLine->operandTypes[0] = vt_var;
    }

    return assignment;
}


// given the AST for a function, generate TAC and return a pointer to the head of the generated block
struct tacLine *linearizeFunction(struct astNode *it, struct tempList *tl)
{
    // scrape along the function
    struct astNode *runner = it->child->sibling;
    struct tacLine *asttac = newtacLine();

    asttac->operation = tt_label;
    asttac->operands[0] = it->child->value;

    int tempNum = 0; // track the number of temporary variables used
    while (runner != NULL)
    {
        switch (runner->type)
        {

        // if we see a variable being declared and then assigned
        // generate the code and stick it on to the end of the block
        case t_var:
            if (runner->child->type == t_assign)
                asttac = appendTAC(asttac, linearizeAssignment(runner->child, &tempNum, tl));

            break;

        // if we see an assignment, generate the code and stick it on to the end of the block
        case t_assign:
            asttac = appendTAC(asttac, linearizeAssignment(runner, &tempNum, tl));
            break;

        case t_call:
            asttac = appendTAC(asttac, linearizeFunctionCall(runner, &tempNum, tl));
            break;

        case t_return:
            struct tacLine* returnAssignment = linearizeAssignment(runner->child, &tempNum, tl);
            // force the ".RETVAL" variable to a temp type since we don't care about its lifespan
            findLastTAC(returnAssignment)->operandTypes[0] = vt_temp;
            asttac = appendTAC(asttac, returnAssignment);

            struct tacLine* returnTac = newtacLine();
            returnTac->operands[0] = findLastTAC(asttac)->operands[0];
            returnTac->operation = tt_return;
            asttac = appendTAC(asttac, returnTac);
            //printf("return %s\n", findLastTAC(asttac)->operands[0]);
            break;

        default:
            printf("Something broke :(\n");
            exit(1);
        }
        runner = runner->sibling;
    }
    return asttac;
}

// given an AST and a populated symbol table, generate three address code for the function entries
void linearizeProgram(struct astNode *it, struct symbolTable *table)
{
    // scrape along the top level of the AST
    struct astNode *runner = it;
    while (runner != NULL)
    {
        printf(".");
        switch (runner->type)
        {
        // if we encounter a function, lookup its symbol table entry
        // then generate the TAC for it and add a reference to the start of the generated code to the function entry
        case t_fun:
        {
            struct functionEntry *theFunction = symbolTableLookup(table, runner->child->value)->entry;
            struct tacLine *generatedTAC = linearizeFunction(runner, table->tl);
            theFunction->codeBlock = generatedTAC;
            break;
        }
        break;

        // ignore everything else
        default:
            break;
        }
        runner = runner->sibling;
    }
}
