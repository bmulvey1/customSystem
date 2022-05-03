#include <stdio.h>
#include <string.h>

#include "ast.h"

struct ASTNode *ASTNode_new(enum token t, char *value)
{
	struct ASTNode *wip = malloc(sizeof(struct ASTNode));
	wip->child = NULL;
	wip->sibling = NULL;
	wip->type = t;
	wip->value = value;
	wip->sourceLine = 0;
	wip->sourceCol = 0;
	return wip;
}

void ASTNode_insertSibling(struct ASTNode *it, struct ASTNode *newSibling)
{
	struct ASTNode *runner = it;
	while (runner->sibling != NULL)
		runner = runner->sibling;

	runner->sibling = newSibling;
}

void ASTNode_insertChild(struct ASTNode *it, struct ASTNode *newChild)
{
	if (it->child == NULL)
		it->child = newChild;
	else
		ASTNode_insertSibling(it->child, newChild);
}

void ASTNode_print(struct ASTNode *it, int depth)
{
	if (it->sibling != NULL)
		ASTNode_print(it->sibling, depth);

	for (int i = 0; i < depth; i++)
		printf("\t");

	printf("%s\n", it->value);
	if (it->child != NULL)
		ASTNode_print(it->child, depth + 1);
}

void ASTNode_printHorizontalRec(struct ASTNode *it){
	if (it->sibling != NULL)
	{
		ASTNode_printHorizontalRec(it->sibling);
		printf(" ");
	}

	printf("(");
	printf("%s", it->value);

	if (it->child != NULL)
	{
		printf(" ");
		ASTNode_printHorizontalRec(it->child);
	}

	printf(")");
}

void ASTNode_printHorizontal(struct ASTNode *it)
{

	printf("(");
	printf("%s", it->value);

	if (it->child != NULL)
	{
		printf(" ");
		ASTNode_printHorizontalRec(it->child);
	}

	printf(")");
}

void ASTNode_free(struct ASTNode *it)
{
	struct ASTNode *runner = it;
	while (runner != NULL)
	{
		if (runner->child != NULL)
		{
			ASTNode_free(runner->child);
		}
		struct ASTNode *old = runner;
		runner = runner->sibling;
		free(old);
	}
}