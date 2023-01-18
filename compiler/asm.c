#include "asm.h"

struct ASMblock *newASMblock()
{
	struct ASMblock *wip = malloc(sizeof(struct ASMblock));
	wip->head = NULL;
	wip->tail = NULL;
	return wip;
}

void ASM_free(struct ASMblock *it)
{
	struct ASM *runner = it->head;
	while (runner != NULL)
	{
		struct ASM *old = runner;
		runner = runner->next;
		free(old->data);
		free(old);
	}
	free(it);
}

void ASM_prepend(struct ASMblock *block, char *data)
{
	struct ASM *newLine = malloc(sizeof(struct ASM));
	newLine->data = data;

	if (block->head != NULL)
	{
		newLine->next = block->head;
	}
	else
	{
		newLine->next = NULL;
		block->tail = newLine;
	}
	block->head = newLine;
}

void ASM_append(struct ASMblock *block, char *data)
{
	// printf("append [%s]\n", data);
	struct ASM *newLine = malloc(sizeof(struct ASM));
	newLine->data = data;
	newLine->next = NULL;

	if (block->tail != NULL)
	{
		block->tail->next = newLine;
	}
	else
	{
		block->head = newLine;
	}
	block->tail = newLine;
}

void ASM_output(struct ASMblock *block, FILE *outFile)
{
	for (struct ASM *runner = block->head; runner != NULL; runner = runner->next)
	{
		if (runner->data[strlen(runner->data) - 1] != ':')
		{
			fprintf(outFile, "\t");
		}
		fprintf(outFile, "%s\n", runner->data);
	}
}