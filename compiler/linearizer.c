#include "linearizer.h"

/*
 * These functions walk the AST and convert it to three-address code
 */

struct TACLine *linearizeExpression(struct astNode *it, int *tempNum, struct tempList *tl);

struct TACLine *linearizeFunctionCall(struct astNode *it, int *tempNum, struct tempList *tl)
{
    struct astNode *runner = it->child->child;
    struct TACLine *calltac = newTACLine();
    calltac->operands[0] = getTempString(tl, *tempNum);
    calltac->operandTypes[0] = vt_temp;

    (*tempNum)++;
    calltac->operands[1] = it->child->value;
    calltac->operandTypes[1] = vt_returnval;
    calltac->operation = tt_call;

    while (runner != NULL)
    {
        struct TACLine *thisArgument = NULL;
        switch (runner->type)
        {
        case t_name:
            thisArgument = newTACLine();
            thisArgument->operandTypes[0] = vt_var;
        case t_literal:
            if (thisArgument == NULL)
            {
                thisArgument = newTACLine();
                thisArgument->operandTypes[0] = vt_literal;
            }
            thisArgument->operands[0] = runner->value;
            thisArgument->operation = tt_push;
            break;

        default:

            struct TACLine *pushOperation = newTACLine();
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
struct TACLine *linearizeExpression(struct astNode *it, int *tempNum, struct tempList *tl)
{
    struct TACLine *thisExpression = newTACLine();
    struct TACLine *returnExpression = NULL;

    // since 'cmp' doesn't generate a result, it just sets flags, no need to consume a temp
    if (it->type != t_compOp)
    {
        thisExpression->operands[0] = getTempString(tl, *tempNum);
        thisExpression->operandTypes[0] = vt_temp;
        // increment count of temp variables, the parse of this expression will be written to a temp
        (*tempNum)++;
    }

    switch (it->child->type)
    {
    case t_call:
        thisExpression->operandTypes[1] = vt_temp;
        thisExpression->operands[1] = getTempString(tl, *tempNum);
        returnExpression = prependTAC(thisExpression, linearizeFunctionCall(it->child, tempNum, tl));
        break;

    case t_compOp:
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

    default:
        printf("Unexpected type seen while linearizing expression! Expected variable or literal\n");
        exit(1);
    }

    switch (it->value[0])
    {
    case '+':
        thisExpression->reorderable = 1;
        thisExpression->operation = tt_add;
        break;

    case '-':
        thisExpression->operation = tt_subtract;
        break;

    case '>':
    case '<':
    case '!':
    case '=':
        thisExpression->operation = tt_cmp;
        break;
    }

    switch (it->child->sibling->type)
    {
    case t_call:
        thisExpression->operandTypes[2] = vt_temp;
        thisExpression->operands[2] = getTempString(tl, *tempNum);
        returnExpression = prependTAC(returnExpression, linearizeFunctionCall(it->child->sibling, tempNum, tl));
        break;

    case t_compOp:
    case t_unOp:
        thisExpression->operands[2] = getTempString(tl, *tempNum);
        thisExpression->operandTypes[2] = vt_temp;
        // when recursing, we need to prepend this line so things happen in the correct order
        // figure out what to prepend to, then set the return expression TACLine to the head of the block
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

    default:
        printf("Unexpected type seen while linearizing expression!\n Expected variable or literal\n");
        exit(1);
    }

    // if we have prepended, make sure to return the first line of TAC
    // otherwise there is only one line of TAC to return, so return that
    if (returnExpression == NULL)
        returnExpression = thisExpression;
    return returnExpression;
}

// given an AST node of an assignment, return the TAC block necessary to generate the correct value
struct TACLine *linearizeAssignment(struct astNode *it, int *tempNum, struct tempList *tl)
{
    struct TACLine *assignment;

    // if this assignment is simply setting one thing to another
    if (it->child->sibling->child == NULL)
    {
        // pull out the relevant values and generate a single line of TAC to return
        assignment = newTACLine();
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
        struct TACLine *lastLine = findLastTAC(assignment);

        // set the final line's operand 0 to the variable actually being assigned
        lastLine->operands[0] = it->child->value;
        lastLine->operandTypes[0] = vt_var;
    }

    return assignment;
}

// given the AST for a function, generate TAC and return a pointer to the head of the generated block
struct TACLine *linearizeStatementList(struct astNode *it, int *tempNum, int *labelCount, struct tempList *tl)
{
    // scrape along the function
    struct astNode *runner = it;
    struct TACLine *sltac = NULL;
    while (runner != NULL)
    {
        switch (runner->type)
        {

        // if we see a variable being declared and then assigned
        // generate the code and stick it on to the end of the block
        case t_var:
            if (runner->child->type == t_assign)
                sltac = appendTAC(sltac, linearizeAssignment(runner->child, tempNum, tl));

            break;

        // if we see an assignment, generate the code and stick it on to the end of the block
        case t_assign:
            sltac = appendTAC(sltac, linearizeAssignment(runner, tempNum, tl));
            break;

        case t_call:
            sltac = appendTAC(sltac, linearizeFunctionCall(runner, tempNum, tl));
            break;

        case t_return:
            struct TACLine *returnAssignment = linearizeAssignment(runner->child, tempNum, tl);
            // force the ".RETVAL" variable to a temp type since we don't care about its lifespan
            findLastTAC(returnAssignment)->operandTypes[0] = vt_temp;
            sltac = appendTAC(sltac, returnAssignment);

            struct TACLine *returnTac = newTACLine();
            returnTac->operands[0] = findLastTAC(sltac)->operands[0];
            returnTac->operation = tt_return;
            sltac = appendTAC(sltac, returnTac);
            // printf("return %s\n", findLastTAC(asttac)->operands[0]);
            break;

        case t_if:
            sltac = appendTAC(sltac, linearizeExpression(runner->child, tempNum, tl));
            
            struct TACLine *pushState = newTACLine();
            pushState->operation = tt_pushstate;
            appendTAC(sltac, pushState);
            
            struct TACLine *noifJump = newTACLine();

            // generate a label and figure out condition to jump when the if statement isn't executed
            char *cmpOp = runner->child->value;
            switch (cmpOp[0])
            {
            case '<':
                switch (cmpOp[1])
                {
                case '=':
                    noifJump->operation = tt_jg;
                    break;

                default:
                    noifJump->operation = tt_jge;
                    break;
                }
                break;

            case '>':
                switch (cmpOp[1])
                {
                case '=':
                    noifJump->operation = tt_jl;
                    break;

                default:
                    noifJump->operation = tt_jle;
                    break;
                }
                break;

            case '!':
                noifJump->operation = tt_je;
                break;

            case '=':
                noifJump->operation = tt_jne;
                break;
            }
            struct TACLine *noifLabel = newTACLine();
            noifLabel->operation = tt_label;
            appendTAC(sltac, noifJump);

            // bit hax (⌐▨_▨)
            // (store the label index using the char* for this TAC line)
            // (avoids having to use more fields in the struct)
            noifLabel->operands[0] = (char *)((long int)++(*labelCount));
            noifJump->operands[0] = (char *)(long int)(*labelCount);
            // printf("\n\n~~~\n");

            struct TACLine *ifBody = linearizeStatementList(runner->child->sibling->child, tempNum, labelCount, tl);

            appendTAC(sltac, ifBody);

            // if the if block ends in a return, no need to restore the state
            // since execution will jump directly to the end of the funciton from here
            if (findLastTAC(ifBody)->operation != tt_return)
            {
                struct TACLine *endIfRestore = newTACLine();
                endIfRestore->operation = tt_restorestate;
                appendTAC(sltac, endIfRestore);
            }

            // if there's an else to fall through to
            if (runner->child->sibling->sibling != NULL)
            {
                struct TACLine *ifBlockFinalTAC = findLastTAC(ifBody);
                // don't need this jump and label when the if statement ends in a return
                struct TACLine *endIfLabel;
                if (ifBlockFinalTAC->operation != tt_return)
                {
                    // generate a label for the absolute end of the if/else block
                    endIfLabel = newTACLine();
                    endIfLabel->operation = tt_label;
                    endIfLabel->operands[0] = (char *)((long int)++(*labelCount));

                    // jump to the end after the if part is done
                    struct TACLine *ifDoneJump = newTACLine();
                    ifDoneJump->operation = tt_jmp;
                    ifDoneJump->operands[0] = endIfLabel->operands[0];
                    appendTAC(sltac, ifDoneJump);
                }
                // make sure we are in a known state before the else statement
                struct TACLine *beforeElseRestore = newTACLine();
                beforeElseRestore->operation = tt_resetstate;
                appendTAC(noifLabel, beforeElseRestore);

                struct TACLine *elseBody = linearizeStatementList(runner->child->sibling->sibling->child->child, tempNum, labelCount, tl);

                // if the if won't be executed, so execution from the noif label goes to the else block
                appendTAC(noifLabel, elseBody);
                appendTAC(sltac, noifLabel);

                struct TACLine *endElseRestore = newTACLine();
                endElseRestore->operation = tt_restorestate;
                appendTAC(sltac, endElseRestore);

                if (ifBlockFinalTAC->operation != tt_return)
                {
                    appendTAC(sltac, endIfLabel);
                }
            }
            else
            {
                appendTAC(sltac, noifLabel);
            }

            struct TACLine *popStateLine = newTACLine();
            popStateLine->operation = tt_popstate;
            appendTAC(sltac, popStateLine);

            break;

        default:
            printf("Something broke :(\n");
            exit(1);
        }
        runner = runner->sibling;
    }
    return sltac;
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
            int tempNum = 0; // track the number of temporary variables used
            int labelCount = 0;
            struct functionEntry *theFunction = symbolTableLookup(table, runner->child->value)->entry;
            struct TACLine *generatedTAC = newTACLine();

            generatedTAC->operation = tt_label;
            generatedTAC->operands[0] = 0;
            appendTAC(generatedTAC, linearizeStatementList(runner->child->sibling, &tempNum, &labelCount, table->tl));
            theFunction->codeBlock = generatedTAC;
            break;
        }
        break;

        // ignore everything else (for now) - no global vars, etc...
        default:
            break;
        }
        runner = runner->sibling;
    }
}
