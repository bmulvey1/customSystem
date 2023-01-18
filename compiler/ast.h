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
	t_lBracket,
	t_rBracket,
	t_array,
	t_call,
	t_scope,
	t_EOF,
};

struct AST
{
	char *value;
	enum token type;
	struct AST *child;
	struct AST *sibling;
	int sourceLine;
	int sourceCol;
};

struct AST *AST_new(enum token t, char *value);

void AST_InsertSibling(struct AST *it, struct AST *newSibling);

void AST_InsertChild(struct AST *it, struct AST *newChild);

void AST_Print(struct AST *it, int depth);

void AST_PrintHorizontal(struct AST *it);

void AST_Free(struct AST *it);