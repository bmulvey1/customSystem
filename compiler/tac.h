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
    tt_declare,
    tt_add,
    tt_subtract,
    tt_mul,
    tt_div,
    tt_dereference,
    tt_reference,
    tt_memw_1, // mov (reg), reg
    tt_memw_2, // mov offset(reg), reg
    tt_memw_3, // mov offset(reg, scale), reg
    tt_memr_1, // same addressing modes as write
    tt_memr_2,
    tt_memr_3,
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
    tt_popstate
};

struct TACLine
{
    char *operands[4];                  // track all 3 operands
    enum variableTypes operandTypes[4]; // track whether the left hand side operands are literals
    enum TACType operation;
    char reorderable;
    struct TACLine *nextLine;
    struct TACLine *prevLine;
};
char* getAsmOp(enum TACType t);

void printTACLine(struct TACLine *it);

char *sPrintTACLine(struct TACLine *it);

void printTACBlock(struct TACLine *it, int indentLevel);

struct TACLine *newTACLine();

struct TACLine *appendTAC(struct TACLine *before, struct TACLine *after);

struct TACLine *prependTAC(struct TACLine *after, struct TACLine *before);

struct TACLine *findLastTAC(struct TACLine *head);

void freeTAC(struct TACLine* it);