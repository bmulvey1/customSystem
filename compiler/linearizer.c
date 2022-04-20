#include "linearizer.h"

/*
 * These functions walk the AST and convert it to three-address code
 */

int linearizeASMBlock(int currentTACIndex,
                      struct BasicBlock *currentBlock,
                      struct astNode *it)
{
    struct astNode *asmRunner = it->child;
    while (asmRunner != NULL)
    {
        struct TACLine *asmLine = newTACLine(currentTACIndex++, tt_asm);
        asmLine->operands[0] = asmRunner->value;
        BasicBlock_append(currentBlock, asmLine);
        asmRunner = asmRunner->sibling;
    }
    return currentTACIndex;
}
void bkpt(){}
int linearizeDereference(struct symbolTable *table,
                         int currentTACIndex,
                         struct LinkedList *blockList,
                         struct BasicBlock *currentBlock,
                         struct astNode *it,
                         int *tempNum,
                         struct tempList *tl)
{
    struct TACLine *thisDereference = newTACLine(currentTACIndex, tt_memr_1);

    thisDereference->operands[0] = getTempString(tl, *tempNum);
    thisDereference->operandTypes[0] = vt_temp;
    (*tempNum)++;

    switch (it->type)
    {
        // directly dereference variables
    case t_name:
        thisDereference->operands[1] = it->value;
        thisDereference->operandTypes[1] = vt_var;
        thisDereference->indirectionLevels[1] = symbolTableLookup_var(table, it->value)->indirectionLevel;
        break;

        // recursively dereference nested dereferences
    case t_dereference:
        thisDereference->operands[1] = getTempString(tl, *tempNum);
        thisDereference->operandTypes[1] = vt_temp;
        currentTACIndex = linearizeDereference(table, currentTACIndex, blockList, currentBlock, it->child, tempNum, tl);
        thisDereference->indirectionLevels[1] = ((struct TACLine *)currentBlock->TACList->tail->data)->indirectionLevels[0];

        break;

        // handle pointer arithmetic to evalute the correct adddress to dereference
    case t_un_add:
    case t_un_sub:
        thisDereference->operands[1] = getTempString(tl, *tempNum);
        thisDereference->operandTypes[1] = vt_temp;
        currentTACIndex = linearizePointerArithmetic(table, currentTACIndex, blockList, currentBlock, it, tempNum, tl, 0);
        thisDereference->indirectionLevels[1] = ((struct TACLine *)currentBlock->TACList->tail->data)->indirectionLevels[0];
        break;

    default:
        printf("Malformed parse tree when linearizing dereference!\n");
        exit(1);
    }

    // printf("here's what we got:\n");
    // printTACBlock(returnLine, 0);
    thisDereference->index = currentTACIndex++;
    int newIndirection = thisDereference->indirectionLevels[1];
    if (newIndirection > 0)
    {
        newIndirection--;
    }
    else
    {
        printf("Warning - dereference of non-indirect variable [%s]", thisDereference->operands[1]);
    }
    thisDereference->indirectionLevels[0] = newIndirection;
    BasicBlock_append(currentBlock, thisDereference);
    return currentTACIndex;
}

/*
 * TODO (when implementing types):
 * - proper type comparisons
 * - indirection level checks (int + char* vs. char* + char* - what needs scaling)
 * - similarly for assignment code, arithmetic results with type being assigned
 */

int linearizePointerArithmetic(struct symbolTable *table,
                               int currentTACIndex,
                               struct LinkedList *blockList,
                               struct BasicBlock *currentBlock,
                               struct astNode *it,
                               int *tempNum,
                               struct tempList *tl,
                               int depth)
{
    // set as tt_assign, assign properly in switch body
    struct TACLine *thisOperation = newTACLine(currentTACIndex, tt_assign);
    struct TACLine *scaleMultiplication = NULL;

    thisOperation->operands[0] = getTempString(tl, *tempNum);
    thisOperation->operandTypes[0] = vt_temp;
    (*tempNum)++;
    char fallingThrough = 0;
    switch (it->type)
    {
        // assign the correct operation based on the operator node
    case t_un_add:
        thisOperation->reorderable = 1;
        thisOperation->operation = tt_add;
        fallingThrough = 1;
    case t_un_sub:
        if (!fallingThrough)
        {
            thisOperation->operation = tt_subtract;
            fallingThrough = 1;
        }

        // see what the LHS of the tree is
        switch (it->child->type)
        {
        // recursively handle more operations
        case t_un_add:
        case t_un_sub:
            thisOperation->operands[1] = getTempString(tl, *tempNum);
            thisOperation->operandTypes[1] = vt_temp;
            currentTACIndex = linearizePointerArithmetic(table, currentTACIndex, blockList, currentBlock, it->child, tempNum, tl, depth);
            thisOperation->indirectionLevels[1] = ((struct TACLine *)currentBlock->TACList->tail->data)->indirectionLevels[0];
            break;

        // handle dereferences explicitly
        case t_dereference:
            thisOperation->operands[1] = getTempString(tl, *tempNum);
            thisOperation->operandTypes[1] = vt_temp;
            currentTACIndex = linearizeDereference(table, currentTACIndex, blockList, currentBlock, it->child->child, tempNum, tl);
            thisOperation->indirectionLevels[1] = ((struct TACLine *)currentBlock->TACList->tail->data)->indirectionLevels[0];
            break;

        // handle variables directly
        case t_name:
            thisOperation->operands[1] = it->child->value;
            thisOperation->operandTypes[1] = vt_var;
            struct variableEntry *e = symbolTableLookup_var(table, it->child->value);
            if(e == NULL){
                printf("Error - use of variable [%s] before declaration\n", it->child->value);
                exit(1);
            }
            thisOperation->indirectionLevels[1] = e->indirectionLevel;
            break;

        // disallow literals in the LHS of expressions - will never feasibly be doing something like *(1 - the_ptr)
        case t_literal:
            printf("Error - literal in LHS of pointer arithmetic expression!\n");
            exit(1);

        // die if anything else is seen
        default:
            printf("Error - unexpected LHS of pointer arithmetic!\n");
            printAST(it, 0);
            exit(1);

            break;
        }

        /* much the same as the LHS evaluation above
         * except for performing scaling multiplication operations when depth == 0 to account for type sizes
         * TODO:
         *  - explicit scaling multiplication could be handled instead with different addressing modes
         *  - size lookups using symbol table (when implementing types)
         */

        switch (it->child->sibling->type)
        {
        case t_un_add:
        case t_un_sub:
            // scale iff depth 0
            if (depth == 0)
            {
                scaleMultiplication = newTACLine(currentTACIndex, tt_mul);
                scaleMultiplication->operands[0] = getTempString(tl, *tempNum);
                scaleMultiplication->operandTypes[0] = vt_temp;

                thisOperation->operands[2] = getTempString(tl, *tempNum);
                thisOperation->operandTypes[2] = vt_temp;

                (*tempNum)++;

                scaleMultiplication->operands[1] = getTempString(tl, *tempNum);
                scaleMultiplication->operandTypes[1] = vt_temp;

                scaleMultiplication->operands[2] = "2";
                scaleMultiplication->operandTypes[2] = vt_literal;

                currentTACIndex = linearizePointerArithmetic(table, currentTACIndex, blockList, currentBlock, it->child->sibling, tempNum, tl, depth + 1);
                // int indirectionLevel = ((struct TACLine *)currentBlock->TACList->tail->data)->indirectionLevels[0];
                int indirectionLevel = thisOperation->indirectionLevels[1];
                thisOperation->indirectionLevels[2] = indirectionLevel;
                scaleMultiplication->indirectionLevels[0] = indirectionLevel;
                scaleMultiplication->indirectionLevels[1] = indirectionLevel;
                scaleMultiplication->indirectionLevels[2] = indirectionLevel;
            }
            else
            {
                thisOperation->operands[2] = getTempString(tl, *tempNum);
                thisOperation->operandTypes[2] = vt_temp;
                currentTACIndex = linearizePointerArithmetic(table, currentTACIndex, blockList, currentBlock, it->child->sibling, tempNum, tl, depth + 1);
                thisOperation->indirectionLevels[2] = ((struct TACLine *)currentBlock->TACList->tail->data)->indirectionLevels[0];
            }
            break;

        case t_dereference:
            thisOperation->operands[2] = getTempString(tl, *tempNum);
            thisOperation->operandTypes[2] = vt_temp;
            (*tempNum)++;
            currentTACIndex = linearizeDereference(table, currentTACIndex, blockList, currentBlock, it->child->sibling->child, tempNum, tl);
            thisOperation->indirectionLevels[2] = ((struct TACLine *)currentBlock->TACList->tail->data)->indirectionLevels[0];
            break;

        case t_name:
        {
            // scale iff depth 0
            if (depth == 0)
            {
                scaleMultiplication = newTACLine(currentTACIndex, tt_mul);
                scaleMultiplication->operands[0] = getTempString(tl, *tempNum);
                scaleMultiplication->operandTypes[0] = vt_temp;

                scaleMultiplication->operands[1] = it->child->sibling->value;
                scaleMultiplication->operandTypes[1] = vt_var;

                thisOperation->operands[2] = getTempString(tl, *tempNum);
                thisOperation->operandTypes[2] = vt_temp;

                // TODO - scale multiplication based on object size (when implementing types)
                scaleMultiplication->operands[2] = "2";
                scaleMultiplication->operandTypes[2] = vt_literal;
                (*tempNum)++;
                printf("\n%s\n", it->child->sibling->value);
                struct variableEntry *e = symbolTableLookup_var(table, it->child->sibling->value);
                if(e == NULL){
                    printf("Error - use of undeclared variable [%s]\n", it->child->sibling->value);
                    exit(1);
                }
                int indirectionLevel = e->indirectionLevel;
                thisOperation->indirectionLevels[2] = indirectionLevel + 1;
                scaleMultiplication->indirectionLevels[0] = indirectionLevel + 1;
                scaleMultiplication->indirectionLevels[1] = indirectionLevel;
                scaleMultiplication->indirectionLevels[2] = indirectionLevel;
            }
            else
            {
                thisOperation->operands[2] = it->child->sibling->value;
                thisOperation->operandTypes[2] = vt_var;
                thisOperation->indirectionLevels[2] = symbolTableLookup_var(table, it->child->sibling->value)->indirectionLevel;
            }
        }
        break;

        case t_literal:
        {
            // scale iff depth 0
            if (depth == 0)
            {
                // TODO: use the dict to store these multiplied literals
                // this quick and dirty method leaks memory!
                char *scaledLiteral = malloc(8 * sizeof(char));

                sprintf(scaledLiteral, "%d", 2 * atoi(it->child->sibling->value));
                thisOperation->operands[2] = scaledLiteral;
            }
            else
            {
                thisOperation->operands[2] = it->child->sibling->value;
            }
            thisOperation->operandTypes[2] = vt_literal;
            thisOperation->indirectionLevels[2] = thisOperation->indirectionLevels[1];
        }
        break;

        default:
            printf("Bad RHS of pointer assignment expression:\n");
            printAST(it, 0);
            exit(1);
        }
        break;

        /* this should be super impossible but leaving it in, in the case where I missed something and this accidentally breaks
        case t_dereference:
            freeTAC(thisOperation);
            currentTACIndex = linearizeDereference(table, currentTACIndex, blockList, currentBlock, it->child, tempNum, tl);
            break;
            */

    default:
        printf("malformed parse tree or bad call to linearize pointer arithmetic!\n");
        exit(1);
        break;
    }
    if (scaleMultiplication != NULL)
    {
        scaleMultiplication->index = currentTACIndex++;
        BasicBlock_append(currentBlock, scaleMultiplication);
    }

    if (thisOperation->indirectionLevels[1] != thisOperation->indirectionLevels[2])
    {
        printf("Warning - pointer arithmetic between different indirection levels!\n\tExpression Tree: ");
        printASTHorizontal(it);
        printf("\n\t");
        printTACLine(thisOperation);
        printf("\n\n");
    }

    thisOperation->indirectionLevels[0] = thisOperation->indirectionLevels[1];

    thisOperation->index = currentTACIndex++;
    BasicBlock_append(currentBlock, thisOperation);
    return currentTACIndex;
}

int linearizeArgumentPushes(struct symbolTable *table,
                            int currentTACIndex,
                            struct LinkedList *blockList,
                            struct BasicBlock *currentBlock,
                            struct astNode *argRunner,
                            int *tempNum,
                            struct tempList *tl)
{
    if (argRunner->sibling != NULL)
    {
        currentTACIndex = linearizeArgumentPushes(table, currentTACIndex, blockList, currentBlock, argRunner->sibling, tempNum, tl);
    }
    struct TACLine *thisArgument = NULL;
    switch (argRunner->type)
    {
    case t_name:
        thisArgument = newTACLine(currentTACIndex++, tt_push);
        thisArgument->operandTypes[0] = vt_var;
    case t_literal:
        if (thisArgument == NULL)
        {
            thisArgument = newTACLine(currentTACIndex++, tt_push);
            thisArgument->operandTypes[0] = vt_literal;
        }
        thisArgument->operands[0] = argRunner->value;
        break;

    default:
        char *pushOperand0 = getTempString(tl, *tempNum);

        currentTACIndex = linearizeExpression(table, currentTACIndex, blockList, currentBlock, argRunner, tempNum, tl);
        struct TACLine *thisArgument = newTACLine(currentTACIndex++, tt_push);
        thisArgument->operands[0] = pushOperand0;
        thisArgument->operandTypes[0] = vt_temp;
        BasicBlock_append(currentBlock, thisArgument);
        thisArgument = NULL;
    }
    if (thisArgument != NULL)
        BasicBlock_append(currentBlock, thisArgument);

    return currentTACIndex;
}

// given an AST node of a function call, generate TAC to evaluate and push the arguments, then call it
int linearizeFunctionCall(struct symbolTable *table,
                          int currentTACIndex,
                          struct LinkedList *blockList,
                          struct BasicBlock *currentBlock,
                          struct astNode *it,
                          int *tempNum,
                          struct tempList *tl)
{
    // struct astNode *runner = it->child->child;
    char *operand0 = getTempString(tl, *tempNum);
    (*tempNum)++;

    currentTACIndex = linearizeArgumentPushes(table, currentTACIndex, blockList, currentBlock, it->child->child, tempNum, tl);

    struct TACLine *calltac = newTACLine(currentTACIndex++, tt_call);
    calltac->operands[0] = operand0;
    calltac->operandTypes[0] = vt_temp;
    calltac->operands[1] = it->child->value;
    calltac->operandTypes[1] = vt_returnval;

    BasicBlock_append(currentBlock, calltac);
    return currentTACIndex;
}

// given an AST node of an expression, figure out how to break it down into multiple lines of three address code
int linearizeExpression(struct symbolTable *table,
                        int currentTACIndex,
                        struct LinkedList *blockList,
                        struct BasicBlock *currentBlock,
                        struct astNode *it,
                        int *tempNum,
                        struct tempList *tl)
{
    // set to tt_assign, reassign in switch body based on operator later
    // also set true TAC index at bottom of function, after child expression linearizations take place
    struct TACLine *thisExpression = newTACLine(currentTACIndex, tt_assign);

    // since 'cmp' doesn't generate a result, it just sets flags, no need to consume a temp
    if (it->type != t_compOp)
    {
        thisExpression->operands[0] = getTempString(tl, *tempNum);
        thisExpression->operandTypes[0] = vt_temp;
        // increment count of temp variables, the parse of this expression will be written to a temp
        (*tempNum)++;
    }
    // support dereference and reference operations separately
    // since these have only one operand
    if (it->type == t_dereference)
    {
        thisExpression->operation = tt_memr_1;
        // if simply dereferencing a name
        if (it->child->type == t_name)
        {
            thisExpression->operands[1] = it->child->value;
            thisExpression->operandTypes[1] = vt_var;
        }
        // otherwise there's pointer arithmetic involved
        else
        {
            thisExpression->operands[1] = getTempString(tl, *tempNum);
            thisExpression->operandTypes[1] = vt_temp;

            currentTACIndex = linearizePointerArithmetic(table, currentTACIndex, blockList, currentBlock, it->child, tempNum, tl, 0);
            thisExpression->indirectionLevels[1] = ((struct TACLine *)currentBlock->TACList->tail->data)->indirectionLevels[0];
        }
        thisExpression->index = currentTACIndex++;
        BasicBlock_append(currentBlock, thisExpression);
        return currentTACIndex;
    }

    // if we fall through to here, the expression is some sort of unary operation
    // handle the LHS of the operation
    switch (it->child->type)
    {

    // handle recording return types from function calls here
    case t_call:
        thisExpression->operands[1] = getTempString(tl, *tempNum);
        thisExpression->operandTypes[1] = vt_temp;

        currentTACIndex = linearizeFunctionCall(table, currentTACIndex, blockList, currentBlock, it->child, tempNum, tl);
        break;

    case t_compOp:
    case t_un_add:
    case t_un_sub:
        thisExpression->operands[1] = getTempString(tl, *tempNum);
        thisExpression->operandTypes[1] = vt_temp;

        currentTACIndex = linearizeExpression(table, currentTACIndex, blockList, currentBlock, it->child, tempNum, tl);
        thisExpression->indirectionLevels[1] = ((struct TACLine *)currentBlock->TACList->tail->data)->indirectionLevels[0];
        break;

    case t_name:
        thisExpression->operands[1] = it->child->value;
        thisExpression->operandTypes[1] = vt_var;
        thisExpression->indirectionLevels[1] = symbolTableLookup_var(table, it->child->value)->indirectionLevel;
        break;

    case t_literal:
        thisExpression->operands[1] = it->child->value;
        thisExpression->operandTypes[1] = vt_literal;
        // indirection levels set to 0 by default
        break;

    case t_dereference:
        thisExpression->operands[1] = getTempString(tl, *tempNum);
        thisExpression->operandTypes[1] = vt_temp;

        currentTACIndex = linearizeDereference(table, currentTACIndex, blockList, currentBlock, it->child->child, tempNum, tl);
        thisExpression->indirectionLevels[0] = ((struct TACLine *)currentBlock->TACList->tail->data)->indirectionLevels[0];
        break;

    default:
        printf("Unexpected type seen while linearizing expression! Expected variable or literal\n");
        exit(1);
    }

    // assign the TAC operation based on the operator at hand
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

    // handle the RHS of the expression, same as LHS but at third operand of TAC
    switch (it->child->sibling->type)
    {
    case t_call:
        thisExpression->operands[2] = getTempString(tl, *tempNum);
        thisExpression->operandTypes[2] = vt_temp;

        currentTACIndex = linearizeFunctionCall(table, currentTACIndex, blockList, currentBlock, it->child->sibling, tempNum, tl);
        break;

    case t_compOp:
    case t_un_add:
    case t_un_sub:
        thisExpression->operands[2] = getTempString(tl, *tempNum);
        thisExpression->operandTypes[2] = vt_temp;
        // figure out what to prepend to, then set the return expression TACLine to the head of the block
        currentTACIndex = linearizeExpression(table, currentTACIndex, blockList, currentBlock, it->child->sibling, tempNum, tl);
        thisExpression->indirectionLevels[2] = ((struct TACLine *)currentBlock->TACList->tail->data)->indirectionLevels[0];
        break;

    case t_name:
        thisExpression->operands[2] = it->child->sibling->value;
        thisExpression->operandTypes[2] = vt_var;
        thisExpression->indirectionLevels[2] = symbolTableLookup_var(table, it->child->sibling->value)->indirectionLevel;

        break;

    case t_literal:
        thisExpression->operands[2] = it->child->sibling->value;
        thisExpression->operandTypes[2] = vt_literal;
        // indirection levels set to 0 by default
        break;

    case t_dereference:
        thisExpression->operands[2] = getTempString(tl, *tempNum);
        thisExpression->operandTypes[2] = vt_temp;

        currentTACIndex = linearizeDereference(table, currentTACIndex, blockList, currentBlock, it->child->sibling->child, tempNum, tl);
        thisExpression->indirectionLevels[2] = ((struct TACLine *)currentBlock->TACList->tail->data)->indirectionLevels[0];
        break;

    default:
        printf("Unexpected type seen while linearizing expression!\n Expected variable or literal\n");
        exit(1);
    }

    if (thisExpression->indirectionLevels[1] != thisExpression->indirectionLevels[2])
    {
        printf("Warning - arithmetic between ");
        switch (thisExpression->operandTypes[1])
        {
        case vt_var:
            printf("var");
            break;

        default:
            printf("-");
        }
        for(int i = 0; i < thisExpression->indirectionLevels[1]; i++){
            printf("*");
        }

        printf(" and ");

        switch (thisExpression->operandTypes[2])
        {
        case vt_var:
            printf("var");
            break;

        default:
            printf("-");
        }
        for(int i = 0; i < thisExpression->indirectionLevels[2]; i++){
            printf("*");
        }
        printf("\n\t");
        printf("Expression Tree: ");
        printASTHorizontal(it);
        printf("\n\n");
    }

    if(thisExpression->indirectionLevels[0] != thisExpression->indirectionLevels[1]){
        printf("Warning - result of arithmetic (");
        switch (thisExpression->operandTypes[1])
        {
        case vt_var:
            printf("var");
            break;

        default:
            printf("-");
        }
        for(int i = 0; i < thisExpression->indirectionLevels[1]; i++){
            printf("*");
        }

        printf(") has different type than expected (");

        switch (thisExpression->operandTypes[2])
        {
        case vt_var:
            printf("var");
            break;

        default:
            printf("-");
        }
        for(int i = 0; i < thisExpression->indirectionLevels[2]; i++){
            printf("*");
        }
        printf(")\n");
    }

    thisExpression->index = currentTACIndex++;
    BasicBlock_append(currentBlock, thisExpression);
    return currentTACIndex;
}

// given an AST node of an assignment, return the TAC block necessary to generate the correct value
int linearizeAssignment(struct symbolTable *table,
                        int currentTACIndex,
                        struct LinkedList *blockList,
                        struct BasicBlock *currentBlock,
                        struct astNode *it,
                        int *tempNum,
                        struct tempList *tl)
{
    struct TACLine *assignment = NULL;

    // if this assignment is simply setting one thing to another
    if (it->child->sibling->child == NULL)
    {
        // pull out the relevant values and generate a single line of TAC to return
        assignment = newTACLine(currentTACIndex++, tt_assign);
        assignment->operands[1] = it->child->sibling->value;

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

        BasicBlock_append(currentBlock, assignment);
    }

    // otherwise there is some sort of expression, which will need to be broken down into multiple lines of TAC
    else
    {
        switch (it->child->sibling->type)
        {
        case t_dereference:
            currentTACIndex = linearizeDereference(table, currentTACIndex, blockList, currentBlock, it->child->sibling->child, tempNum, tl);
            break;

        case t_un_add:
        case t_un_sub:
            currentTACIndex = linearizeExpression(table, currentTACIndex, blockList, currentBlock, it->child->sibling, tempNum, tl);
            break;

        case t_call:
            currentTACIndex = linearizeFunctionCall(table, currentTACIndex, blockList, currentBlock, it->child->sibling, tempNum, tl);
            break;

        default:
            printf("Error linearizing expression - malformed parse tree (expected unOp or call)\n");
            exit(1);
        }
    }
    struct TACLine *lastLine = currentBlock->TACList->tail->data;
    if (it->child->type == t_dereference)
    {
        lastLine->operands[0] = getTempString(tl, *tempNum);
        lastLine->operandTypes[0] = vt_temp;
        (*tempNum)++;

        struct TACLine *LHS;
        struct TACLine *finalWrite;
        switch (it->child->child->type)
        {
        case t_dereference:
            currentTACIndex = linearizeDereference(table, currentTACIndex, blockList, currentBlock, it->child->child->child, tempNum, tl);

            finalWrite = newTACLine(currentTACIndex++, tt_memw_1);
            finalWrite->operands[1] = lastLine->operands[0];
            finalWrite->operandTypes[1] = lastLine->operandTypes[0];
            finalWrite->indirectionLevels[1] = lastLine->indirectionLevels[0];

            finalWrite->operands[0] = getTempString(tl, *tempNum);
            finalWrite->operandTypes[0] = vt_temp;
            BasicBlock_append(currentBlock, finalWrite);
            break;

        case t_un_add:
        case t_un_sub:
            currentTACIndex = linearizePointerArithmetic(table, currentTACIndex, blockList, currentBlock, it->child->child, tempNum, tl, 0);
            struct TACLine *lastDereferenceLine = currentBlock->TACList->tail->data;

            finalWrite = newTACLine(currentTACIndex++, tt_memw_1);
            finalWrite->operands[1] = lastLine->operands[0];
            finalWrite->operandTypes[1] = lastLine->operandTypes[0];
            finalWrite->indirectionLevels[1] = lastLine->indirectionLevels[0];

            finalWrite->operands[0] = lastDereferenceLine->operands[0];
            finalWrite->operandTypes[0] = vt_temp;

            BasicBlock_append(currentBlock, finalWrite);
            break;

        case t_name:
            LHS = newTACLine(currentTACIndex++, tt_memw_1);
            LHS->operands[0] = it->child->child->value;
            LHS->operandTypes[0] = vt_var;

            LHS->operands[1] = lastLine->operands[0];
            LHS->operandTypes[1] = lastLine->operandTypes[0];
            LHS->indirectionLevels[1] = lastLine->indirectionLevels[0];
            BasicBlock_append(currentBlock, LHS);

            break;

        default:
            printf("Malformed parse tree - expected unary operator or dereference as child of pointer write!\n");
            exit(1);
        }

        // struct TACLine *LHS = linearizePointerArithmetic(table, it->child, tempNum, tl, 0);
        // printf("linearized LHS - here's what we got\n");
        // printTACBlock(LHS, 0);
    }
    else
    {
        // set the final line's operand 0 to the variable actually being assigned
        lastLine->operands[0] = it->child->value;
        lastLine->operandTypes[0] = vt_var;
        struct variableEntry *e = symbolTableLookup_var(table, it->child->value);
        if(e == NULL){
            printf("Error - use of variable [%s] before assignment\n", it->child->value);
            exit(1);
        }
        lastLine->indirectionLevels[0] = e->indirectionLevel;
    }

    // if (assignment != NULL)
    // BasicBlock_append(currentBlock, assignment);
    return currentTACIndex;
}

struct TACLine *linearizeConditionalJump(int currentTACIndex, char *cmpOp)
{
    struct TACLine *notMetJump;
    switch (cmpOp[0])
    {
    case '<':
        switch (cmpOp[1])
        {
        case '=':
            notMetJump = newTACLine(currentTACIndex, tt_jg);
            break;

        default:
            notMetJump = newTACLine(currentTACIndex, tt_jge);
            break;
        }
        break;

    case '>':
        switch (cmpOp[1])
        {
        case '=':
            notMetJump = newTACLine(currentTACIndex, tt_jl);
            break;

        default:
            notMetJump = newTACLine(currentTACIndex, tt_jle);
            break;
        }
        break;

    case '!':
        notMetJump = newTACLine(currentTACIndex, tt_je);
        break;

    case '=':
        notMetJump = newTACLine(currentTACIndex, tt_jne);
        break;
    }

    return notMetJump;
}

int linearizeDeclaration(struct symbolTable *table,
                         int currentTACIndex,
                         struct BasicBlock *currentBlock,
                         struct astNode *it)
{
    printAST(it, 0);
    struct TACLine *declarationLine = newTACLine(currentTACIndex++, tt_declare);
    enum variableTypes declaredType;
    switch (it->type)
    {
    case t_var:
        declaredType = vt_var;
        break;

    default:
        perror("Unexpected type seen while linearizing declaration!");
        exit(1);
    }

    while (it->child->type == t_dereference)
    {
        it = it->child;
    }

    declarationLine->operands[0] = it->child->value;
    declarationLine->operandTypes[0] = declaredType;
    BasicBlock_append(currentBlock, declarationLine);
    return currentTACIndex;
}

struct Stack *linearizeIfStatement(struct symbolTable *table,
                                   int currentTACIndex,
                                   struct LinkedList *blockList,
                                   struct BasicBlock *currentBlock,
                                   struct BasicBlock *afterIfBlock,
                                   struct astNode *it,
                                   int *tempNum,
                                   int *labelCount,
                                   struct tempList *tl)
{
    // a stack is overkill but allows to store either 1 or 2 resulting blocks depending on if there is or isn't an else block
    struct Stack *results = Stack_new();

    // linearize the expression we will test
    currentTACIndex = linearizeExpression(table, currentTACIndex, blockList, currentBlock, it->child, tempNum, tl);

    // save state before branch
    struct TACLine *pushState = newTACLine(currentTACIndex, tt_pushstate);
    BasicBlock_append(currentBlock, pushState);

    // generate a label and figure out condition to jump when the if statement isn't executed
    struct TACLine *noifJump = linearizeConditionalJump(currentTACIndex++, it->child->value);

    BasicBlock_append(currentBlock, noifJump);

    // track the highest TAC index of both branches
    int ifTACIndex = currentTACIndex;
    int elseTACIndex = currentTACIndex;

    // struct TACLine *ifDo = newTACLine(ifTACIndex, tt_do);
    // BasicBlock_append(currentBlock, ifDo);

    struct LinearizationResult *r = linearizeStatementList(table, ifTACIndex, blockList, currentBlock, afterIfBlock, it->child->sibling->child, tempNum, labelCount, tl);

    // struct TACLine *ifEndDo = newTACLine(ifTACIndex, tt_enddo);
    // BasicBlock_append(currentBlock, ifEndDo);

    Stack_push(results, r);

    // if there is an else statement to the if
    if (it->child->sibling->sibling != NULL)
    {
        // create a basicblock for the else statement
        struct BasicBlock *elseBlock = BasicBlock_new(++(*labelCount));
        elseBlock->hintLabel = "else block";
        LinkedList_append(blockList, elseBlock);

        // need to ensure the else block starts from the same state as the if
        struct TACLine *resetLine = newTACLine(elseTACIndex, tt_resetstate);
        BasicBlock_append(elseBlock, resetLine);

        // struct TACLine *elseDo = newTACLine(elseTACIndex, tt_do);
        // BasicBlock_append(elseBlock, elseDo);

        // bit hax (⌐▨_▨)
        // store the label index using the char* for this TAC line to avoid more fields in the struct
        noifJump->operands[0] = (char *)((long int)elseBlock->labelNum);

        // linearize the else block, it returns to the same afterIfBlock as the ifBlock does
        r = linearizeStatementList(table, elseTACIndex, blockList, elseBlock, afterIfBlock, it->child->sibling->sibling->child->child, tempNum, labelCount, tl);

        Stack_push(results, r);

        // struct TACLine *elseEndDo = newTACLine(elseTACIndex, tt_enddo);
        // BasicBlock_append(elseBlock, elseEndDo);
    }
    else
    {
        noifJump->operands[0] = (char *)((long int)afterIfBlock->labelNum);
    }

    return results;
}

struct LinearizationResult *linearizeWhileLoop(struct symbolTable *table,
                                               int currentTACIndex,
                                               struct LinkedList *blockList,
                                               struct BasicBlock *currentBlock,
                                               struct BasicBlock *afterWhileBlock,
                                               struct astNode *it,
                                               int *tempNum,
                                               int *labelCount,
                                               struct tempList *tl)
{
    int entryTACIndex = currentTACIndex;
    // save state before while block
    struct TACLine *pushState = newTACLine(currentTACIndex, tt_pushstate);
    BasicBlock_append(currentBlock, pushState);

    struct BasicBlock *whileBlock = BasicBlock_new(++(*labelCount));
    whileBlock->hintLabel = "while block";
    LinkedList_append(blockList, whileBlock);

    struct TACLine *enterWhileJump = newTACLine(currentTACIndex++, tt_jmp);
    enterWhileJump->operands[0] = (char *)((long int)whileBlock->labelNum);
    BasicBlock_append(currentBlock, enterWhileJump);

    struct TACLine *whileDo = newTACLine(currentTACIndex, tt_do);
    BasicBlock_append(whileBlock, whileDo);

    // linearize the expression we will test
    currentTACIndex = linearizeExpression(table, currentTACIndex, blockList, whileBlock, it->child, tempNum, tl);

    // restore state before the conditional jump that exits the loop
    struct TACLine *beforeNoWhileRestore = newTACLine(currentTACIndex, tt_restorestate);
    beforeNoWhileRestore->operands[0] = (char *)((long int)entryTACIndex);
    BasicBlock_append(whileBlock, beforeNoWhileRestore);

    struct TACLine *noWhileJump = linearizeConditionalJump(currentTACIndex++, it->child->value);
    noWhileJump->operands[0] = (char *)((long int)afterWhileBlock->labelNum);
    BasicBlock_append(whileBlock, noWhileJump);

    struct LinearizationResult *r = linearizeStatementList(table, currentTACIndex, blockList, whileBlock, whileBlock, it->child->sibling->child, tempNum, labelCount, tl);

    for (struct LinkedListNode *TACRunner = r->block->TACList->tail; TACRunner != NULL; TACRunner = TACRunner->prev)
    {
        struct TACLine *examinedTAC = TACRunner->data;
        if (examinedTAC->operation == tt_restorestate)
        {
            examinedTAC->operands[0] = beforeNoWhileRestore->operands[0];
            break;
        }
    }

    /*struct TACLine *whileLoopJump = newTACLine(currentTACIndex++, tt_jmp);
    whileLoopJump->operands[0] = (char *)((long int)whileBlock->labelNum);
    BasicBlock_append(whileBlock, whileLoopJump);*/

    struct TACLine *whileEndDo = newTACLine(r->endingTACIndex, tt_enddo);
    BasicBlock_append(r->block, whileEndDo);

    return r;
}

// given the AST for a function, generate TAC and return a pointer to the head of the generated block
struct LinearizationResult *linearizeStatementList(struct symbolTable *table,
                                                   int currentTACIndex,
                                                   struct LinkedList *blockList,
                                                   struct BasicBlock *currentBlock,
                                                   struct BasicBlock *controlConvergesTo,
                                                   struct astNode *it,
                                                   int *tempNum,
                                                   int *labelCount,
                                                   struct tempList *tl)
{
    // int startingTACIndex = currentTACIndex;
    // scrape along the function
    struct astNode *runner = it;
    while (runner != NULL)
    {
        switch (runner->type)
        {
        case t_asm:
            currentTACIndex = linearizeASMBlock(currentTACIndex, currentBlock, runner);
            break;

        // if we see a variable being declared and then assigned
        // generate the code and stick it on to the end of the block
        case t_var:
        {
            switch (runner->child->type)
            {
            case t_assign:
                currentTACIndex = linearizeAssignment(table, currentTACIndex, blockList, currentBlock, runner->child, tempNum, tl);
                break;

            case t_dereference:
                struct astNode *dereferenceScraper = runner->child;
                while (dereferenceScraper->type == t_dereference)
                    dereferenceScraper = dereferenceScraper->child;

                if (dereferenceScraper->type == t_assign)
                    currentTACIndex = linearizeAssignment(table, currentTACIndex, blockList, currentBlock, dereferenceScraper, tempNum, tl);
                else
                    currentTACIndex = linearizeDeclaration(table, currentTACIndex, currentBlock, runner);

                break;

            // if just a declaration, do nothing
            case t_name:
                currentTACIndex = linearizeDeclaration(table, currentTACIndex, currentBlock, runner);
                break;

            default:
                printAST(runner, 0);
                printf("Error linearizing statement - malformed parse tree under 'var' node\n");
                exit(1);
            }
        }
        break;

        // if we see an assignment, generate the code and stick it on to the end of the block
        case t_assign:
            currentTACIndex = linearizeAssignment(table, currentTACIndex, blockList, currentBlock, runner, tempNum, tl);
            break;

        case t_call:
        {
            // for a raw call, return value is not used, null out that operand
            currentTACIndex = linearizeFunctionCall(table, currentTACIndex, blockList, currentBlock, runner, tempNum, tl);
            struct TACLine *lastLine = currentBlock->TACList->tail->data;
            lastLine->operands[0] = NULL;
            lastLine->operandTypes[0] = vt_null;
        }
        break;

        case t_return:
        {
            char *returned;
            enum variableTypes returnedType;

            switch (runner->child->type)
            {
            case t_name:
                returned = runner->child->value;
                returnedType = vt_var;
                break;

            case t_literal:
                returned = runner->child->value;
                returnedType = vt_literal;
                break;

            default:
                returned = getTempString(tl, *tempNum);
                returnedType = vt_temp;
                currentTACIndex = linearizeExpression(table, currentTACIndex, blockList, currentBlock, runner->child, tempNum, tl);
                break;
            }
            struct TACLine *returnTac = newTACLine(currentTACIndex++, tt_return);
            returnTac->operands[0] = returned;
            returnTac->operandTypes[0] = returnedType;
            BasicBlock_append(currentBlock, returnTac);
        }
        break;

        case t_if:
        {
            // this is the block that control will be passed to after the branch, regardless of what happens
            struct BasicBlock *afterIfBlock = BasicBlock_new(++(*labelCount));
            afterIfBlock->hintLabel = "after if block";

            // linearize the if statement and attached else if it exists
            struct Stack *results = linearizeIfStatement(table, currentTACIndex, blockList, currentBlock, afterIfBlock, runner, tempNum, labelCount, tl);
            LinkedList_append(blockList, afterIfBlock);

            struct Stack *beforeConvergeRestores = Stack_new();

            // grab all generated basic blocks generated by the if statement's linearization
            while (results->size > 0)
            {
                struct LinearizationResult *poppedResult = Stack_pop(results);
                for (struct LinkedListNode *TACRunner = poppedResult->block->TACList->tail; TACRunner != NULL; TACRunner = TACRunner->prev)
                {
                    struct TACLine *examinedTAC = TACRunner->data;
                    if (examinedTAC->operation == tt_restorestate)
                    {
                        Stack_push(beforeConvergeRestores, examinedTAC);
                        break;
                    }
                }
                // skip the current TAC index as far forward as possible so code generated from here on out is always after previous code

                if (poppedResult->endingTACIndex > currentTACIndex)
                    currentTACIndex = poppedResult->endingTACIndex + 1;

                free(poppedResult);
            }

            Stack_free(results);

            while (beforeConvergeRestores->size > 0)
            {
                struct TACLine *expireLine = Stack_pop(beforeConvergeRestores);
                expireLine->operands[0] = (char *)((long int)currentTACIndex);
            }

            Stack_free(beforeConvergeRestores);

            // we are now linearizing code into the block which the branches converge to
            currentBlock = afterIfBlock;
            struct TACLine *endPop = newTACLine(currentTACIndex, tt_popstate);
            BasicBlock_append(currentBlock, endPop);
        }
        break;

        case t_while:
        {
            struct BasicBlock *afterWhileBlock = BasicBlock_new(++(*labelCount));
            afterWhileBlock->hintLabel = "after while block";

            struct LinearizationResult *r = linearizeWhileLoop(table, currentTACIndex, blockList, currentBlock, afterWhileBlock, runner, tempNum, labelCount, tl);
            currentTACIndex = r->endingTACIndex;
            LinkedList_append(blockList, afterWhileBlock);
            free(r);

            currentBlock = afterWhileBlock;
            struct TACLine *endPop = newTACLine(currentTACIndex, tt_popstate);
            BasicBlock_append(currentBlock, endPop);
        }
        break;

        default:
            printf("Something broke :(\n");
            exit(1);
        }
        runner = runner->sibling;
    }

    if (controlConvergesTo != NULL)
    {
        struct TACLine *lastLine = currentBlock->TACList->tail->data;
        if (lastLine->operation != tt_return)
        {
            struct TACLine *beforeConvergeRestore = newTACLine(currentTACIndex, tt_restorestate);
            // beforeConvergeRestore->operands[0] = (char *)((long int)startingTACIndex - 1); // this might cause problems
            BasicBlock_append(currentBlock, beforeConvergeRestore);

            struct TACLine *convergeControlJump = newTACLine(currentTACIndex++, tt_jmp);
            convergeControlJump->operands[0] = (char *)((long int)controlConvergesTo->labelNum);
            BasicBlock_append(currentBlock, convergeControlJump);
        }
    }

    struct LinearizationResult *r = malloc(sizeof(struct LinearizationResult));
    r->block = currentBlock;
    r->endingTACIndex = currentTACIndex;
    return r;
}

// given an AST and a populated symbol table, generate three address code for the function entries
void linearizeProgram(struct astNode *it, struct symbolTable *table)
{
    struct LinkedList *globalBlockList = LinkedList_new();
    struct BasicBlock *globalBlock = BasicBlock_new(0);
    LinkedList_append(globalBlockList, globalBlock);
    // scrape along the top level of the AST
    table->BasicBlockList = globalBlockList;
    struct astNode *runner = it;
    int tempNum = 0;
    int currentTACIndex = 0;
    while (runner != NULL)
    {
        printf(".");
        switch (runner->type)
        {
        // if we encounter a function, lookup its symbol table entry
        // then generate the TAC for it and add a reference to the start of the generated code to the function entry
        case t_fun:
        {
            int funTempNum = 0; // track the number of temporary variables used
            int labelCount = 0;
            struct functionEntry *theFunction = symbolTableLookup(table, runner->child->value)->entry;
            struct LinkedList *functionBlockList = LinkedList_new();
            theFunction->table->BasicBlockList = functionBlockList;
            struct BasicBlock *functionBlock = BasicBlock_new(funTempNum);
            functionBlock->hintLabel = table->name;

            LinkedList_append(functionBlockList, functionBlock);
            struct LinearizationResult *r = linearizeStatementList(theFunction->table, 0, functionBlockList, functionBlock, NULL, runner->child->sibling, &funTempNum, &labelCount, table->tl);
            free(r);
            // theFunction->table->codeBlock = generatedTAC;
            break;
        }
        break;

        case t_asm:
            currentTACIndex = linearizeASMBlock(currentTACIndex, globalBlock, runner);
            break;

        case t_var:
            struct astNode *declarationScraper = runner;

            // scrape down all pointer levels if necessary, then linearize if the variable is actually assigned
            while (declarationScraper->child->type == t_dereference)
                declarationScraper = declarationScraper->child;

            declarationScraper = declarationScraper->child;
            if (declarationScraper->type == t_assign)
                currentTACIndex = linearizeAssignment(table, currentTACIndex, globalBlockList, globalBlock, declarationScraper, &tempNum, table->tl);

            break;

        case t_assign:
            currentTACIndex = linearizeAssignment(table, currentTACIndex, globalBlockList, globalBlock, runner, &tempNum, table->tl);
            break;

        // ignore everything else (for now) - no global vars, etc...
        default:
            break;
        }
        runner = runner->sibling;
    }
}
