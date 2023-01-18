#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#pragma once
// a linkedlist of strings - should probably use the linkedlist implementaion in util
struct ASM
{
	char *data;
	struct ASM *next;
};

struct ASMblock
{
	struct ASM *head;
	struct ASM *tail;
};

struct ASMblock *newASMblock();

void ASM_free(struct ASMblock* it);

void ASM_prepend(struct ASMblock *block, char *data);

void ASM_append(struct ASMblock *block, char *data);

void ASM_output(struct ASMblock *block, FILE *outFile);