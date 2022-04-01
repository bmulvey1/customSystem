#include "tac.h"

char *getAsmOp(enum TACType t)
{
    switch (t)
    {
    case tt_asm:
        return "";

    case tt_assign:
        return "";

    case tt_declare:
        return "";

    case tt_add:
        return "add";

    case tt_subtract:
        return "sub";

    case tt_mul:
        return "mul";

    case tt_div:
        return "div";

    case tt_dereference:
        return "dereference";

    case tt_reference:
        return "reference";

    case tt_memw_1:
        return "mov (reg), reg";

    case tt_memw_2:
        return "mov offset(reg), reg";

    case tt_memw_3:
        return "mov offset(reg, scale), reg";

    case tt_memr_1:
        return "mov reg, (reg)";

    case tt_memr_2:
        return "mov reg, offset(reg)";

    case tt_memr_3:
        return "mov reg, offset(reg, scale)";

    case tt_cmp:
        return "cmp";

    case tt_push:
        return "push";

    case tt_call:
        return "call";

    case tt_label:
        return ".";

    case tt_return:
        return "ret";

    case tt_jg:
        return "jg";

    case tt_jge:
        return "jge";

    case tt_jl:
        return "jl";

    case tt_jle:
        return "jle";

    case tt_je:
        return "je";

    case tt_jne:
        return "jne";

    case tt_jmp:
        return "jmp";

    case tt_pushstate:
        return "PUSHSTATE";

    case tt_restorestate:
        return "RESTORESTATE";

    case tt_resetstate:
        return "RESETSTATE";

    case tt_popstate:
        return "POPSTATE";
    }
    return "";
}

struct TACLine *newTACLine(int index, enum TACType operation)
{
    struct TACLine *wip = malloc(sizeof(struct TACLine));
    wip->operands[0] = NULL;
    wip->operands[1] = NULL;
    wip->operands[2] = NULL;
    wip->operands[3] = NULL;
    wip->operandTypes[0] = vt_null;
    wip->operandTypes[1] = vt_null;
    wip->operandTypes[2] = vt_null;
    wip->operandTypes[3] = vt_null;
    // default type of a line of TAC is assignment
    wip->operation = operation;
    // by default operands are NOT reorderable
    wip->reorderable = 0;
    wip->index = index;
    return wip;
}

void printTACLine(struct TACLine *it)
{
    char *operationStr;
    char fallingThrough = 0;
    int width;
    if (TACLine_isEffective(it))
    {
        width = printf("%2x: ", it->index);
    }
    else
    {
        width = printf(" ~  ");
    }

    switch (it->operation)
    {
    case tt_asm:
        width += printf("ASM:%s", it->operands[0]);
        break;

    case tt_add:
        if (!fallingThrough)
            operationStr = "+";
        fallingThrough = 1;
    case tt_subtract:
        if (!fallingThrough)
            operationStr = "-";
        fallingThrough = 1;
    case tt_mul:
        if (!fallingThrough)
            operationStr = "*";
        fallingThrough = 1;
    case tt_div:
        if (!fallingThrough)
            operationStr = "/";
        fallingThrough = 1;

        width += printf("%s = %s %s %s", it->operands[0], it->operands[1], operationStr, it->operands[2]);
        break;

    case tt_dereference:
        width += printf("%s = *%s", it->operands[0], it->operands[1]);
        break;

    case tt_reference:
        width += printf("%s = &%s", it->operands[0], it->operands[1]);
        break;

    case tt_memw_1:
        width += printf("(%s) = %s", it->operands[0], it->operands[1]);
        break;

    case tt_memw_2:
        width += printf("%d(%s) = %s", (int)(long int)it->operands[0], it->operands[1], it->operands[2]);
        break;

    case tt_memw_3:
        width += printf("%s(%s, %d) = %s", it->operands[0], it->operands[1], (int)(long int)it->operands[2], it->operands[3]);
        break;

    case tt_memr_1:
        width += printf("%s = (%s)", it->operands[0], it->operands[1]);
        break;

    case tt_memr_2:
        width += printf("%s = %d(%s)", it->operands[0], (int)(long int)it->operands[1], it->operands[2]);
        break;

    case tt_memr_3:
        width += printf("%s = %s(%s, %d)", it->operands[0], it->operands[1], it->operands[2], (int)(long int)it->operands[3]);
        break;

    case tt_jg:
    case tt_jge:
    case tt_jl:
    case tt_jle:
    case tt_je:
    case tt_jne:
    case tt_jmp:
        width += printf("%s basicblock %ld", getAsmOp(it->operation), (long int)it->operands[0]);
        break;

    case tt_cmp:
        width += printf("cmp %s %s", it->operands[1], it->operands[2]);
        break;

    case tt_assign:
        width += printf("%s = %s", it->operands[0], it->operands[1]);
        break;

    case tt_declare:
        width += printf("declare %s", it->operands[0]);
        break;

    case tt_push:
        width += printf("push %s", it->operands[0]);
        break;

    case tt_call:
        if (it->operands[0] == NULL)
            width += printf("call %s", it->operands[1]);
        else
            width += printf("%s = call %s", it->operands[0], it->operands[1]);

        break;

    case tt_label:
        width += printf("~label %ld:", (long int)it->operands[0]);
        break;

    case tt_return:
        width += printf("ret %s", it->operands[0]);
        break;

    case tt_pushstate:
        width += printf("PUSHSTATE");
        break;

    case tt_restorestate:
        width += printf("RESTORESTATE");
        break;

    case tt_resetstate:
        width += printf("RESETSTATE");
        break;

    case tt_popstate:
        width += printf("POPSTATE");
        break;
    }
    while (width++ < 24)
    {
        printf(" ");
    }
    // printf("\t%d %d %d", it->operandTypes[0], it->operandTypes[1], it->operandTypes[2]);
    // width += printf("%s", it->reorderable ? " - Reorderable" : "");
}

char *sPrintTACLine(struct TACLine *it)
{
    char *operationStr;
    char *tacString = malloc(128);
    char fallingThrough = 0;
    int width = sprintf(tacString, "%2x:", it->index);
    switch (it->operation)
    {
    case tt_asm:
        width += sprintf(tacString, "ASM:%s", it->operands[0]);
        break;

    case tt_add:
        if (!fallingThrough)
            operationStr = "+";
        fallingThrough = 1;
    case tt_subtract:
        if (!fallingThrough)
            operationStr = "-";
        fallingThrough = 1;
    case tt_mul:
        if (!fallingThrough)
            operationStr = "*";
        fallingThrough = 1;
    case tt_div:
        if (!fallingThrough)
            operationStr = "/";
        fallingThrough = 1;

        width += sprintf(tacString, "%s = %s %s %s", it->operands[0], it->operands[1], operationStr, it->operands[2]);
        break;

    case tt_dereference:
        width += sprintf(tacString, "%s = *%s", it->operands[0], it->operands[1]);
        break;

    case tt_reference:
        width += sprintf(tacString, "%s = &%s", it->operands[0], it->operands[1]);
        break;

    case tt_memw_1:
        width += sprintf(tacString, "(%s) = %s", it->operands[0], it->operands[1]);
        break;

    case tt_memw_2:
        width += sprintf(tacString, "%d(%s) = %s", (int)(long int)it->operands[0], it->operands[1], it->operands[2]);
        break;

    case tt_memw_3:
        width += sprintf(tacString, "%s(%s, %d) = %s", it->operands[0], it->operands[1], (int)(long int)it->operands[2], it->operands[3]);
        break;

    case tt_memr_1:
        width += sprintf(tacString, "%s = (%s)", it->operands[0], it->operands[1]);
        break;

    case tt_memr_2:
        width += sprintf(tacString, "%s = %d(%s)", it->operands[0], (int)(long int)it->operands[1], it->operands[2]);
        break;

    case tt_memr_3:
        width += sprintf(tacString, "%s = %s(%s, %d)", it->operands[0], it->operands[1], it->operands[2], (int)(long int)it->operands[3]);
        break;

    case tt_jg:
    case tt_jge:
    case tt_jl:
    case tt_jle:
    case tt_je:
    case tt_jne:
    case tt_jmp:
        width += sprintf(tacString, "%s basicblock %ld", getAsmOp(it->operation), (long int)it->operands[0]);
        break;

    case tt_cmp:
        width += sprintf(tacString, "cmp %s %s", it->operands[1], it->operands[2]);
        break;

    case tt_assign:
        width += sprintf(tacString, "%s = %s", it->operands[0], it->operands[1]);
        break;

    case tt_declare:
        width += sprintf(tacString, "declare %s", it->operands[0]);
        break;

    case tt_push:
        width += sprintf(tacString, "push %s", it->operands[0]);
        break;

    case tt_call:
        if (it->operands[0] == NULL)
            width += sprintf(tacString, "call %s", it->operands[1]);
        else
            width += sprintf(tacString, "%s = call %s", it->operands[0], it->operands[1]);

        break;

    case tt_label:
        width += sprintf(tacString, "~label %ld:", (long int)it->operands[0]);
        break;

    case tt_return:
        width += sprintf(tacString, "ret %s", it->operands[0]);
        break;

    case tt_pushstate:
        width += sprintf(tacString, "PUSHSTATE");
        break;

    case tt_restorestate:
        width += sprintf(tacString, "RESTORESTATE");
        break;

    case tt_resetstate:
        width += sprintf(tacString, "RESETSTATE");
        break;

    case tt_popstate:
        width += sprintf(tacString, "POPSTATE");
        break;
    }

    char *trimmedString = malloc(width + 1);
    sprintf(trimmedString, "%s", tacString);
    free(tacString);
    return trimmedString;
}

void freeTAC(struct TACLine *it)
{
    free(it);
}

char TACLine_isEffective(struct TACLine *it)
{
    switch (it->operation)
    {
    case tt_pushstate:
    case tt_popstate:
    case tt_restorestate:
    case tt_resetstate:
    case tt_declare:
        return 0;
        break;

    default:
        return 1;
        break;
    }
}

struct BasicBlock *BasicBlock_new(int labelNum)
{
    struct BasicBlock *wip = malloc(sizeof(struct BasicBlock));
    wip->TACList = LinkedList_new();
    wip->labelNum = labelNum;
    wip->containsEffectiveCode = 0;
    return wip;
}

void BasicBlock_free(struct BasicBlock *b)
{
    LinkedList_free(b->TACList, &freeTAC);
    free(b);
}

void BasicBlock_append(struct BasicBlock *b, struct TACLine *l)
{
    b->containsEffectiveCode |= TACLine_isEffective(l);
    LinkedList_append(b->TACList, l);
}

void BasicBlock_prepend(struct BasicBlock *b, struct TACLine *l)
{
    b->containsEffectiveCode |= TACLine_isEffective(l);
    LinkedList_prepend(b->TACList, l);
}

struct TACLine *findLastEffectiveTAC(struct BasicBlock *b)
{
    struct LinkedListNode *runner = b->TACList->tail;
    while(runner != NULL && !TACLine_isEffective(runner->data)){
        runner = runner->prev;
    }
    if(runner == NULL){
        return NULL;
    }
    return runner->data;
}

void printBasicBlock(struct BasicBlock *b, int indentLevel)
{
    printf("BASIC BLOCK %d\n", b->labelNum);
    for (struct LinkedListNode *runner = b->TACList->head; runner != NULL; runner = runner->next)
    {
        struct TACLine *this = runner->data;
        for (int i = 0; i < indentLevel; i++)
        {
            printf("\t");
        }

        if (runner->data != NULL)
        {
            printTACLine(this);
            printf("\n");
        }
    }
    printf("\n\n");
}
