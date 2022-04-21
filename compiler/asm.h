#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#pragma once
struct ASMline
{
	char *data;
	struct ASMline *next;
};

struct ASMblock
{
	struct ASMline *head;
	struct ASMline *tail;
};

struct ASMblock *newASMblock();

void ASMblock_free(struct ASMblock* it);

void ASMblock_prepend(struct ASMblock *block, char *data);

void ASMblock_append(struct ASMblock *block, char *data);

void ASMblock_output(struct ASMblock *block, FILE *outFile);