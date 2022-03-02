#include <stdio.h>
#include <stdlib.h>

#include "ast.h"

#pragma once

enum variableTypes
{
    vt_null,
    vt_var,
    vt_temp,
    vt_returnval,
    vt_literal
};

enum TACType
{
    tt_asm,
    tt_assign,
    tt_add,
    tt_subtract,
    tt_dereference,
    tt_reference,
    tt_cmp,
    tt_jg,
    tt_jge,
    tt_jl,
    tt_jle,
    tt_je,
    tt_jne,
    tt_jmp,
    tt_push,
    tt_call,
    tt_label,
    tt_return,
    tt_pushstate,
    tt_restorestate,
    tt_resetstate,
    tt_popstate,
    tt_do,
    tt_enddo
};

struct TACLine
{
    char *operands[3];                  // track all 3 operands
    enum variableTypes operandTypes[3]; // track whether the left hand side operands are literals
    enum TACType operation;
    char reorderable;
    struct TACLine *nextLine;
    struct TACLine *prevLine;
};
char* getAsmOp(enum TACType t);

void printTACLine(struct TACLine *it);

void printTACBlock(struct TACLine *it, int indentLevel);

struct TACLine *newTACLine();

struct TACLine *appendTAC(struct TACLine *before, struct TACLine *after);

struct TACLine *prependTAC(struct TACLine *after, struct TACLine *before);

struct TACLine *findLastTAC(struct TACLine *head);

void freeTAC(struct TACLine* it);