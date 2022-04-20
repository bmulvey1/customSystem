#include <stdlib.h>

#pragma once

enum token
{
    t_asm,
    t_var,
    t_fun,
    t_return,
    t_if,
    t_else,
    t_while,
    t_do,
    t_name,
    t_literal,
    t_un_add,
    t_un_sub,
    t_binOp,
    t_compOp,
    t_reference,
    t_dereference,
    t_assign,
    t_comma,
    t_semicolon,
    t_lParen,
    t_rParen,
    t_lCurly,
    t_rCurly,
    t_call,
    t_EOF,
};

struct astNode
{
    char *value;
    enum token type;
    struct astNode *child;
    struct astNode *sibling;
};

struct astNode *newastNode(enum token t, char *value);

void astNode_insertSibling(struct astNode *it, struct astNode *newSibling);

void astNode_insertChild(struct astNode *it, struct astNode *newChild);

void printAST(struct astNode *it, int depth);

void printASTHorizontal(struct astNode *it);

void freeAST(struct astNode *it);