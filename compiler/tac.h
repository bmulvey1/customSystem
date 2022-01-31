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

enum tacType
{
    tt_assign,
    tt_add,
    tt_subtract,
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
    tt_popstate
};

struct tacLine
{
    char *operands[3];                  // track all 3 operands
    enum variableTypes operandTypes[3]; // track whether the left hand side operands are literals
    enum tacType operation;
    char reorderable;
    struct tacLine *nextLine;
    struct tacLine *prevLine;
};
char* getAsmOp(enum tacType t);

void printTacLine(struct tacLine *it);

struct tacLine *newtacLine();

struct tacLine *appendTAC(struct tacLine *before, struct tacLine *after);

struct tacLine *prependTAC(struct tacLine *after, struct tacLine *before);

struct tacLine *findLastTAC(struct tacLine *head);

void freeTAC(struct tacLine* it);