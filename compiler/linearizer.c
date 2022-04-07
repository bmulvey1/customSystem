#include "linearizer.h"

/*
 * These functions walk the AST and convert it to three-address code
 */

int linearizeASMBlock(int currentTACIndex, struct BasicBlock *currentBlock, struct astNode *it)
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

int linearizeDereference(int currentTACIndex, struct LinkedList *blockList, struct BasicBlock *currentBlock, struct astNode *it, int *tempNum, struct tempList *tl)
{
    struct TACLine *thisDereference = newTACLine(currentTACIndex++, tt_memr_1);

    thisDereference->operands[0] = getTempString(tl, *tempNum);
    thisDereference->operandTypes[0] = vt_temp;
    (*tempNum)++;

    switch (it->type)
    {
        // directly dereference variables
    case t_name:
        thisDereference->operands[1] = it->value;
        thisDereference->operandTypes[1] = vt_var;
        break;

        // recursively dereference nested dereferences
    case t_dereference:
        thisDereference->operands[1] = getTempString(tl, *tempNum);
        thisDereference->operandTypes[1] = vt_temp;
        currentTACIndex = linearizeDereference(currentTACIndex, blockList, currentBlock, it->child, tempNum, tl);
        break;

        // handle pointer arithmetic to evalute the correct adddress to dereference
    case t_un_add:
    case t_un_sub:
        thisDereference->operands[1] = getTempString(tl, *tempNum);
        thisDereference->operandTypes[1] = vt_temp;
        currentTACIndex = linearizePointerArithmetic(currentTACIndex, blockList, currentBlock, it, tempNum, tl, 0);
        break;

    default:
        printf("Malformed parse tree when linearizing dereference!\n");
        exit(1);
    }

    // printf("here's what we got:\n");
    // printTACBlock(returnLine, 0);

    BasicBlock_append(currentBlock, thisDereference);
    return currentTACIndex;
}

/*
 * TODO (when implementing types):
 * - proper type comparisons
 * - indirection level checks (int + char* vs. char* + char* - what needs scaling)
 * - similarly for assignment code, arithmetic results with type being assigned
 */

int linearizePointerArithmetic(int currentTACIndex, struct LinkedList *blockList, struct BasicBlock *currentBlock, struct astNode *it, int *tempNum, struct tempList *tl, int depth)
{
    // set as tt_assign, assign properly in switch body
    struct TACLine *thisOperation = newTACLine(currentTACIndex++, tt_assign);
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
            currentTACIndex = linearizePointerArithmetic(currentTACIndex, blockList, currentBlock, it->child, tempNum, tl, depth);
            break;

        // handle dereferences explicitly
        case t_dereference:
            thisOperation->operands[1] = getTempString(tl, *tempNum);
            thisOperation->operandTypes[1] = vt_temp;
            currentTACIndex = linearizeDereference(currentTACIndex, blockList, currentBlock, it->child->child, tempNum, tl);
            break;

        // handle variables directly
        case t_name:
            thisOperation->operands[1] = it->child->value;
            thisOperation->operandTypes[1] = vt_var;
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
                scaleMultiplication = newTACLine(currentTACIndex++, tt_mul);
                scaleMultiplication->operands[0] = getTempString(tl, *tempNum);
                scaleMultiplication->operandTypes[0] = vt_temp;

                thisOperation->operands[2] = getTempString(tl, *tempNum);
                thisOperation->operandTypes[2] = vt_temp;

                (*tempNum)++;

                scaleMultiplication->operands[1] = getTempString(tl, *tempNum);
                scaleMultiplication->operandTypes[1] = vt_temp;

                scaleMultiplication->operands[2] = "2";
                scaleMultiplication->operandTypes[2] = vt_literal;

                currentTACIndex = linearizePointerArithmetic(currentTACIndex, blockList, currentBlock, it->child->sibling, tempNum, tl, depth + 1);
            }
            else
            {
                thisOperation->operands[2] = getTempString(tl, *tempNum);
                thisOperation->operandTypes[2] = vt_temp;
                currentTACIndex = linearizePointerArithmetic(currentTACIndex, blockList, currentBlock, it->child->sibling, tempNum, tl, depth + 1);
            }
            break;

        case t_dereference:
            thisOperation->operands[2] = getTempString(tl, *tempNum);
            thisOperation->operandTypes[2] = vt_temp;
            (*tempNum)++;
            currentTACIndex = linearizeDereference(currentTACIndex, blockList, currentBlock, it->child->sibling->child, tempNum, tl);
            break;

        case t_name:
        {
            // scale iff depth 0
            if (depth == 0)
            {
                scaleMultiplication = newTACLine(currentTACIndex++, tt_mul);
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
            }
            else
            {
                thisOperation->operands[2] = it->child->sibling->value;
                thisOperation->operandTypes[2] = vt_var;
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
        }
        break;

        default:
            printf("Bad RHS of pointer assignment expression:\n");
            printAST(it, 0);
            exit(1);
        }
        break;

    case t_dereference:
        freeTAC(thisOperation);
        currentTACIndex = linearizeDereference(currentTACIndex, blockList, currentBlock, it->child, tempNum, tl);
        break;

    default:
        printf("malformed parse tree or bad call to linearize pointer arithmetic!\n");
        exit(1);
        break;
    }
    if (scaleMultiplication != NULL)
        BasicBlock_append(currentBlock, scaleMultiplication);

    BasicBlock_append(currentBlock, thisOperation);
    return currentTACIndex;
}

// given an AST node of a function call, generate TAC to evaluate and push the arguments, then call it
int linearizeFunctionCall(int currentTACIndex, struct LinkedList *blockList, struct BasicBlock *currentBlock, struct astNode *it, int *tempNum, struct tempList *tl)
{
    struct astNode *runner = it->child->child;
    char *operand0 = getTempString(tl, *tempNum);
    (*tempNum)++;

    while (runner != NULL)
    {
        struct TACLine *thisArgument = NULL;
        switch (runner->type)
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
            thisArgument->operands[0] = runner->value;
            break;

        default:
            char *pushOperand0 = getTempString(tl, *tempNum);

            currentTACIndex = linearizeExpression(currentTACIndex, blockList, currentBlock, runner, tempNum, tl);
            struct TACLine *pushOperation = newTACLine(currentTACIndex++, tt_push);
            pushOperation->operands[0] = pushOperand0;
            pushOperation->operandTypes[0] = vt_temp;
            BasicBlock_append(currentBlock, pushOperation);
        }
        if (thisArgument != NULL)
            BasicBlock_append(currentBlock, thisArgument);

        runner = runner->sibling;
    }

    struct TACLine *calltac = newTACLine(currentTACIndex++, tt_call);
    calltac->operands[0] = operand0;
    calltac->operandTypes[0] = vt_temp;
    calltac->operands[1] = it->child->value;
    calltac->operandTypes[1] = vt_returnval;

    BasicBlock_append(currentBlock, calltac);
    return currentTACIndex;
}

// given an AST node of an expression, figure out how to break it down into multiple lines of three address code
int linearizeExpression(int currentTACIndex, struct LinkedList *blockList, struct BasicBlock *currentBlock, struct astNode *it, int *tempNum, struct tempList *tl)
{
    // set to tt_assign, reassign in switch body based on operator later
    char *operand0 = NULL;
    char *operand1 = NULL;
    char *operand2 = NULL;
    enum TACType operation;
    char reorderable = 0;
    enum variableTypes operand0Type = vt_null;
    enum variableTypes operand1Type = vt_null;
    enum variableTypes operand2Type = vt_null;
    // since 'cmp' doesn't generate a result, it just sets flags, no need to consume a temp
    if (it->type != t_compOp)
    {
        operand0 = getTempString(tl, *tempNum);
        operand0Type = vt_temp;
        // increment count of temp variables, the parse of this expression will be written to a temp
        (*tempNum)++;
    }
    // support dereference and reference operations separately
    // since these have only one operand
    if (it->type == t_dereference)
    {
        operation = tt_memr_1;
        // if simply dereferencing a name
        if (it->child->type == t_name)
        {
            operand1 = it->child->value;
            operand1Type = vt_var;
        }
        // otherwise there's pointer arithmetic involved
        else
        {
            operand1 = getTempString(tl, *tempNum);
            operand1Type = vt_temp;

            currentTACIndex = linearizePointerArithmetic(currentTACIndex, blockList, currentBlock, it->child, tempNum, tl, 0);
        }
        struct TACLine *thisExpression = newTACLine(currentTACIndex++, tt_assign);
        thisExpression->operands[0] = operand0;
        thisExpression->operands[1] = operand1;
        thisExpression->operands[2] = operand2;
        thisExpression->operandTypes[0] = operand0Type;
        thisExpression->operandTypes[1] = operand1Type;
        thisExpression->operandTypes[2] = operand2Type;
        thisExpression->operation = operation;
        thisExpression->reorderable = reorderable;
        BasicBlock_append(currentBlock, thisExpression);
        return currentTACIndex;
    }

    /*
    TODO: handle referencing variables
    if (it->type == t_reference)
    {
        thisExpression->operandTypes[1] = vt_temp;
        thisExpression->operands[1] = getTempString(tl, *tempNum);
        thisExpression->operation = tt_reference;
        return prependTAC(thisExpression, linearizePointerArithmetic(it->child, tempNum, tl, 0));
    }*/

    // if we fall through to here, the expression is some sort of unary operation
    // handle the LHS of the operation
    switch (it->child->type)
    {

    case t_call:
        operand1 = getTempString(tl, *tempNum);
        operand1Type = vt_temp;

        currentTACIndex = linearizeFunctionCall(currentTACIndex, blockList, currentBlock, it->child, tempNum, tl);
        break;

    case t_compOp:
    case t_un_add:
    case t_un_sub:
        operand1 = getTempString(tl, *tempNum);
        operand1Type = vt_temp;

        currentTACIndex = linearizeExpression(currentTACIndex, blockList, currentBlock, it->child, tempNum, tl);
        break;

    case t_name:
        operand1 = it->child->value;
        operand1Type = vt_var;
        break;

    case t_literal:
        operand1 = it->child->value;
        operand1Type = vt_literal;
        break;

    case t_dereference:
        operand1 = getTempString(tl, *tempNum);
        operand1Type = vt_temp;

        currentTACIndex = linearizeDereference(currentTACIndex, blockList, currentBlock, it->child->child, tempNum, tl);
        break;

    default:
        printf("Unexpected type seen while linearizing expression! Expected variable or literal\n");
        exit(1);
    }

    // assign the TAC operation based on the operator at hand
    switch (it->value[0])
    {
    case '+':
        reorderable = 1;
        operation = tt_add;
        break;

    case '-':
        operation = tt_subtract;
        break;

    case '>':
    case '<':
    case '!':
    case '=':
        operation = tt_cmp;
        break;
    }

    // handle the RHS of the expression, same as LHS but at third operand of TAC
    switch (it->child->sibling->type)
    {
    case t_call:
        operand2 = getTempString(tl, *tempNum);
        operand2Type = vt_temp;

        currentTACIndex = linearizeFunctionCall(currentTACIndex, blockList, currentBlock, it->child->sibling, tempNum, tl);
        break;

    case t_compOp:
    case t_un_add:
    case t_un_sub:
        operand2 = getTempString(tl, *tempNum);
        operand2Type = vt_temp;
        // figure out what to prepend to, then set the return expression TACLine to the head of the block
        currentTACIndex = linearizeExpression(currentTACIndex, blockList, currentBlock, it->child->sibling, tempNum, tl);
        break;

    case t_name:
        operand2 = it->child->sibling->value;
        operand2Type = vt_var;
        break;

    case t_literal:
        operand2 = it->child->sibling->value;
        operand2Type = vt_literal;
        break;

    case t_dereference:
        operand2 = getTempString(tl, *tempNum);
        operand2Type = vt_temp;

        currentTACIndex = linearizeDereference(currentTACIndex, blockList, currentBlock, it->child->sibling->child, tempNum, tl);
        break;

    default:
        printf("Unexpected type seen while linearizing expression!\n Expected variable or literal\n");
        exit(1);
    }

    struct TACLine *thisExpression = newTACLine(currentTACIndex++, tt_assign);
    thisExpression->operands[0] = operand0;
    thisExpression->operands[1] = operand1;
    thisExpression->operands[2] = operand2;
    thisExpression->operandTypes[0] = operand0Type;
    thisExpression->operandTypes[1] = operand1Type;
    thisExpression->operandTypes[2] = operand2Type;
    thisExpression->operation = operation;
    thisExpression->reorderable = reorderable;
    BasicBlock_append(currentBlock, thisExpression);
    return currentTACIndex;
}

// given an AST node of an assignment, return the TAC block necessary to generate the correct value
int linearizeAssignment(int currentTACIndex, struct LinkedList *blockList, struct BasicBlock *currentBlock, struct astNode *it, int *tempNum, struct tempList *tl)
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
            currentTACIndex = linearizeDereference(currentTACIndex, blockList, currentBlock, it->child->sibling->child, tempNum, tl);
            break;

        case t_un_add:
        case t_un_sub:
            currentTACIndex = linearizeExpression(currentTACIndex, blockList, currentBlock, it->child->sibling, tempNum, tl);
            break;

        case t_call:
            currentTACIndex = linearizeFunctionCall(currentTACIndex, blockList, currentBlock, it->child->sibling, tempNum, tl);
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
            currentTACIndex = linearizeDereference(currentTACIndex, blockList, currentBlock, it->child->child->child, tempNum, tl);

            finalWrite = newTACLine(currentTACIndex++, tt_memw_1);
            finalWrite->operands[1] = lastLine->operands[0];
            finalWrite->operandTypes[1] = lastLine->operandTypes[0];

            finalWrite->operands[0] = getTempString(tl, *tempNum);
            finalWrite->operandTypes[0] = vt_temp;
            BasicBlock_append(currentBlock, finalWrite);
            break;

        case t_un_add:
        case t_un_sub:
            currentTACIndex = linearizePointerArithmetic(currentTACIndex, blockList, currentBlock, it->child->child, tempNum, tl, 0);

            finalWrite = newTACLine(currentTACIndex++, tt_memw_1);
            finalWrite->operands[1] = lastLine->operands[0];
            finalWrite->operandTypes[1] = lastLine->operandTypes[0];

            finalWrite->operands[0] = getTempString(tl, *tempNum);
            finalWrite->operandTypes[0] = vt_temp;

            BasicBlock_append(currentBlock, finalWrite);
            break;

        case t_name:
            LHS = newTACLine(currentTACIndex++, tt_memw_1);
            LHS->operands[0] = it->child->child->value;
            LHS->operandTypes[0] = vt_var;

            LHS->operands[1] = lastLine->operands[0];
            LHS->operandTypes[1] = lastLine->operandTypes[0];
            BasicBlock_append(currentBlock, LHS);

            break;

        default:
            printf("Malformed parse tree - expected unary operator or dereference as child of pointer write!\n");
            exit(1);
        }

        // struct TACLine *LHS = linearizePointerArithmetic(it->child, tempNum, tl, 0);
        // printf("linearized LHS - here's what we got\n");
        // printTACBlock(LHS, 0);
    }
    else
    {
        // set the final line's operand 0 to the variable actually being assigned
        lastLine->operands[0] = it->child->value;
        lastLine->operandTypes[0] = vt_var;
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

int linearizeDeclaration(int currentTACIndex, struct BasicBlock *currentBlock, struct astNode *it, enum token type)
{
    struct TACLine *declarationLine = newTACLine(currentTACIndex, tt_declare);
    declarationLine->operands[0] = it->value;
    enum variableTypes declaredType;
    switch (type)
    {
    case t_var:
        declaredType = vt_var;
        break;

    default:
        perror("Unexpected type seen while linearizing declaration!");
        exit(1);
    }
    declarationLine->operandTypes[0] = declaredType;
    BasicBlock_append(currentBlock, declarationLine);
    return currentTACIndex;
}

struct Stack *linearizeIfStatement(int currentTACIndex, struct LinkedList *blockList, struct BasicBlock *currentBlock, struct BasicBlock *afterIfBlock, struct astNode *it, int *tempNum, int *labelCount, struct tempList *tl)
{
    // a stack is overkill but allows to store either 1 or 2 resulting blocks depending on if there is or isn't an else block
    struct Stack *resultBlocks = Stack_new();

    // linearize the expression we will test
    currentTACIndex = linearizeExpression(currentTACIndex, blockList, currentBlock, it->child, tempNum, tl);

    // save state before branch
    struct TACLine *pushState = newTACLine(currentTACIndex, tt_pushstate);
    BasicBlock_append(currentBlock, pushState);

    // generate a label and figure out condition to jump when the if statement isn't executed
    struct TACLine *noifJump = linearizeConditionalJump(currentTACIndex++, it->child->value);

    BasicBlock_append(currentBlock, noifJump);

    // track the highest TAC index of both branches
    int ifTACIndex = currentTACIndex;
    int elseTACIndex = currentTACIndex;

    ifTACIndex = linearizeStatementList(ifTACIndex, blockList, currentBlock, afterIfBlock, it->child->sibling->child, tempNum, labelCount, tl);

    Stack_push(resultBlocks, currentBlock);

    // if there is an else statement to the if
    if (it->child->sibling->sibling != NULL)
    {
        // create a basicblock for the else statement
        struct BasicBlock *elseBlock = BasicBlock_new(++(*labelCount));
        LinkedList_append(blockList, elseBlock);
        Stack_push(resultBlocks, elseBlock);

        // need to ensure the else block starts from the same state as the if
        struct TACLine *resetLine = newTACLine(elseTACIndex, tt_resetstate);
        BasicBlock_append(elseBlock, resetLine);

        // bit hax (⌐▨_▨)
        // store the label index using the char* for this TAC line to avoid more fields in the struct
        noifJump->operands[0] = (char *)((long int)elseBlock->labelNum);

        // linearize the else block, it returns to the same afterIfBlock as the ifBlock does
        elseTACIndex = linearizeStatementList(elseTACIndex, blockList, elseBlock, afterIfBlock, it->child->sibling->sibling->child->child, tempNum, labelCount, tl);
    }

    return resultBlocks;
}

int linearizeWhileLoop(int currentTACIndex, struct LinkedList *blockList, struct BasicBlock *currentBlock, struct BasicBlock *afterWhileBlock, struct astNode *it, int *tempNum, int *labelCount, struct tempList *tl)
{
    int entryTACIndex = currentTACIndex;
    // save state before while block
    struct TACLine *pushState = newTACLine(currentTACIndex, tt_pushstate);
    BasicBlock_append(currentBlock, pushState);

    struct BasicBlock *whileBlock = BasicBlock_new(++(*labelCount));
    LinkedList_append(blockList, whileBlock);

    // linearize the expression we will test
    currentTACIndex = linearizeExpression(currentTACIndex, blockList, whileBlock, it->child, tempNum, tl);

    // restore state before the conditional jump that exits the loop
    struct TACLine *beforeNoWhileRestore = newTACLine(currentTACIndex, tt_restorestate);
    beforeNoWhileRestore->operands[0] = (char *)((long int)entryTACIndex);
    BasicBlock_append(whileBlock, beforeNoWhileRestore);

    struct TACLine *noWhileJump = linearizeConditionalJump(currentTACIndex++, it->child->value);
    BasicBlock_append(whileBlock, noWhileJump);

    currentTACIndex = linearizeStatementList(currentTACIndex, blockList, whileBlock, whileBlock, it->child->sibling->child, tempNum, labelCount, tl);
    for (struct LinkedListNode *runner = whileBlock->TACList->tail; runner != NULL; runner = runner->prev)
    {
        struct TACLine *examinedTAC = runner->data;
        if (examinedTAC->operation == tt_restorestate)
        {
            examinedTAC->operands[0] = (char *)((long int)entryTACIndex);
            break;
        }
    }

    return currentTACIndex;
}

// given the AST for a function, generate TAC and return a pointer to the head of the generated block
int linearizeStatementList(int currentTACIndex, struct LinkedList *blockList, struct BasicBlock *currentBlock, struct BasicBlock *controlConvergesTo, struct astNode *it, int *tempNum, int *labelCount, struct tempList *tl)
{
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
                currentTACIndex = linearizeAssignment(currentTACIndex, blockList, currentBlock, runner->child, tempNum, tl);
                break;

            case t_dereference:
                struct astNode *dereferenceScraper = runner->child;
                while (dereferenceScraper->type == t_dereference)
                    dereferenceScraper = dereferenceScraper->child;

                if (dereferenceScraper->type == t_assign)
                    currentTACIndex = linearizeAssignment(currentTACIndex, blockList, currentBlock, dereferenceScraper, tempNum, tl);
                else
                    currentTACIndex = linearizeDeclaration(currentTACIndex, currentBlock, runner->child, dereferenceScraper->type);

                break;

            // if just a declaration, do nothing
            case t_name:
                currentTACIndex = linearizeDeclaration(currentTACIndex, currentBlock, runner->child, runner->type);
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
            currentTACIndex = linearizeAssignment(currentTACIndex, blockList, currentBlock, runner, tempNum, tl);
            break;

        case t_call:
        {
            // for a raw call, return value is not used, null out that operand
            currentTACIndex = linearizeFunctionCall(currentTACIndex, blockList, currentBlock, runner, tempNum, tl);
            struct TACLine *lastLine = currentBlock->TACList->tail->data;
            lastLine->operands[0] = NULL;
        }
        break;

        case t_return:
        {
            currentTACIndex = linearizeAssignment(currentTACIndex, blockList, currentBlock, runner->child, tempNum, tl);
            struct TACLine *returnLine = currentBlock->TACList->tail->data;
            // force the ".RETVAL" variable to a temp type since we don't care about its lifespan
            returnLine->operandTypes[0] = vt_temp;

            struct TACLine *returnTac = newTACLine(currentTACIndex++, tt_return);
            returnTac->operands[0] = returnLine->operands[0];
            returnTac->operandTypes[0] = returnLine->operandTypes[0];
            BasicBlock_append(currentBlock, returnTac);
        }
        break;

        case t_if:
        {
            // this is the block that control will be passed to after the branch, regardless of what happens
            struct BasicBlock *afterIfBlock = BasicBlock_new(++(*labelCount));

            // linearize the if statement and attached else if it exists
            struct Stack *resultBlocks = linearizeIfStatement(currentTACIndex, blockList, currentBlock, afterIfBlock, runner, tempNum, labelCount, tl);
            LinkedList_append(blockList, afterIfBlock);

            struct Stack *beforeConvergeRestores = Stack_new();

            // grab all generated basic blocks generated by the if statement's linearization
            while (resultBlocks->size > 0)
            {
                struct BasicBlock *poppedBlock = Stack_pop(resultBlocks);
                // find last effective line, there will be state control
                struct TACLine *lastLine = poppedBlock->TACList->tail->data;

                for (struct LinkedListNode *TACRunner = poppedBlock->TACList->tail; TACRunner != NULL; TACRunner = TACRunner->prev)
                {
                    struct TACLine *examinedTAC = TACRunner->data;
                    if (examinedTAC->operation == tt_restorestate)
                    {
                        Stack_push(beforeConvergeRestores, examinedTAC);
                        break;
                    }
                }
                // skip the current TAC index as far forward as possible so code generated from here on out is always after previous code

                if (lastLine->index > currentTACIndex)
                    currentTACIndex = lastLine->index + 1;
            }

            Stack_free(resultBlocks);

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

            currentTACIndex = linearizeWhileLoop(currentTACIndex, blockList, currentBlock, afterWhileBlock, runner, tempNum, labelCount, tl);

            currentBlock = afterWhileBlock;

            /*
            // grab all generated basic blocks generated by the if statement's linearization
            char stillScraping = 1;
            for (struct LinkedListNode *TACRunner = condCheckBlock->TACList->tail; TACRunner != NULL; TACRunner = TACRunner->prev)
            {
                struct TACLine *examinedTAC = TACRunner->data;
                switch (examinedTAC->operation)
                {
                case tt_jg:
                case tt_jge:
                case tt_jl:
                case tt_jle:
                case tt_je:
                case tt_jne:
                case tt_jmp:
                    examinedTAC->operands[0] = (char *)((long int)afterWhileBlock->labelNum);
                    break;

                default:
                    break;
                }

                if (!stillScraping)
                    break;
            }*/
            // skip the current TAC index as far forward as possible so code generated from here on out is always after previous code
        }

        /*
        {
            // push state before entering the loop
            struct TACLine *pushState = newTACLine(currentTACIndex++, );
            pushState->operation = tt_pushstate;
            sltac = appendTAC(sltac, pushState);

            // establish a block during which any live variable's lifetime will be extended
            // since a variable could "die" during the while loop, then the loop could repeat
            // we need to extend all the lifetimes within the loop or risk overwriting and losing some variables

            // generate a label for the top of the while loop
            struct TACLine *beginWhile = newTACLine(currentTACIndex++, );
            beginWhile->operation = tt_label;
            beginWhile->operands[0] = (char *)((long int)++(*labelCount));
            appendTAC(sltac, beginWhile);

            // linearize the expression we will test
            appendTAC(sltac, linearizeExpression(runner->child, tempNum, tl));

            // generate a label and figure out condition to jump when the while loop isn't executed
            struct TACLine *noWhileJump = newTACLine(currentTACIndex++, );
            char *cmpOp = runner->child->value;
            switch (cmpOp[0])
            {
            case '<':
                switch (cmpOp[1])
                {
                case '=':
                    noWhileJump->operation = tt_jg;
                    break;

                default:
                    noWhileJump->operation = tt_jge;
                    break;
                }
                break;

            case '>':
                switch (cmpOp[1])
                {
                case '=':
                    noWhileJump->operation = tt_jl;
                    break;

                default:
                    noWhileJump->operation = tt_jle;
                    break;
                }
                break;

            case '!':
                noWhileJump->operation = tt_je;
                break;

            case '=':
                noWhileJump->operation = tt_jne;
                break;
            }
            appendTAC(sltac, noWhileJump);

            struct TACLine *noWhileLabel = newTACLine(currentTACIndex++, );
            noWhileLabel->operation = tt_label;
            // bit hax (⌐▨_▨)
            // (store the label index using the char* for this TAC line)
            // (avoids having to use more fields in the struct)
            noWhileLabel->operands[0] = (char *)((long int)++(*labelCount));
            noWhileJump->operands[0] = (char *)(long int)(*labelCount);

            // linearize the body of the loop
            struct TACLine *whileBody = linearizeStatementList(runner->child->sibling->child, tempNum, labelCount, tl);
            appendTAC(sltac, whileBody);

            // restore the registers to a known state at the end of every loop
            struct TACLine *restoreState = newTACLine(currentTACIndex++, );
            restoreState->operation = tt_restorestate;
            appendTAC(sltac, restoreState);

            // jump back to the condition test
            struct TACLine *loopJump = newTACLine(currentTACIndex++, );
            loopJump->operation = tt_jmp;
            loopJump->operands[0] = beginWhile->operands[0];
            appendTAC(sltac, loopJump);

            // at the very end, add the label we jump to if the condition test fails
            appendTAC(sltac, noWhileLabel);
            // throw away the saved state when done with the loop
            struct TACLine *endWhilePop = newTACLine(currentTACIndex++, );
            endWhilePop->operation = tt_popstate;
            appendTAC(sltac, endWhilePop);
        }
        */
        break;

        default:
            printf("Something broke :(\n");
            exit(1);
        }
        runner = runner->sibling;
    }

    if (controlConvergesTo != NULL)
    {
        struct TACLine *beforeConvergeRestore = newTACLine(currentTACIndex, tt_restorestate);
        // beforeConvergeRestore->operands[0] = (char *)((long int)((struct TACLine *)controlConvergesTo->TACList->head->data)->index);
        BasicBlock_append(currentBlock, beforeConvergeRestore);

        struct TACLine *convergeControlJump = newTACLine(currentTACIndex++, tt_jmp);
        convergeControlJump->operands[0] = (char *)((long int)controlConvergesTo->labelNum);
        BasicBlock_append(currentBlock, convergeControlJump);
    }
    return currentTACIndex;
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

            LinkedList_append(functionBlockList, functionBlock);
            linearizeStatementList(0, functionBlockList, functionBlock, NULL, runner->child->sibling, &funTempNum, &labelCount, table->tl);
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
                currentTACIndex = linearizeAssignment(currentTACIndex, globalBlockList, globalBlock, declarationScraper, &tempNum, table->tl);

            break;

        case t_assign:
            currentTACIndex = linearizeAssignment(currentTACIndex, globalBlockList, globalBlock, runner, &tempNum, table->tl);
            break;

        // ignore everything else (for now) - no global vars, etc...
        default:
            break;
        }
        runner = runner->sibling;
    }
}
