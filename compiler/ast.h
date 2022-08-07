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
	t_scope,
	t_EOF,
};

struct ASTNode
{
	char *value;
	enum token type;
	struct ASTNode *child;
	struct ASTNode *sibling;
	int sourceLine;
	int sourceCol;
};

struct ASTNode *ASTNode_new(enum token t, char *value);

void ASTNode_insertSibling(struct ASTNode *it, struct ASTNode *newSibling);

void ASTNode_insertChild(struct ASTNode *it, struct ASTNode *newChild);

void ASTNode_print(struct ASTNode *it, int depth);

void ASTNode_printHorizontal(struct ASTNode *it);

void ASTNode_free(struct ASTNode *it);