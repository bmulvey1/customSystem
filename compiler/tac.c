#include "tac.h"

char *getAsmOp(enum TACType t)
{
	switch (t)
	{
	case tt_asm:
		return "";

	case tt_assign:
		return "";

	case tt_declare:
		return "";

	case tt_add:
		return "add";

	case tt_subtract:
		return "sub";

	case tt_mul:
		return "mul";

	case tt_div:
		return "div";

	case tt_dereference:
		return "dereference";

	case tt_reference:
		return "reference";

	case tt_memw_1:
		return "mov (reg), reg";

	case tt_memw_2:
		return "mov offset(reg), reg";

	case tt_memw_3:
		return "mov offset(reg, scale), reg";

	case tt_memr_1:
		return "mov reg, (reg)";

	case tt_memr_2:
		return "mov reg, offset(reg)";

	case tt_memr_3:
		return "mov reg, offset(basereg, scale)";

	case tt_cmp:
		return "cmp";

	case tt_push:
		return "push";

	case tt_call:
		return "call";

	case tt_label:
		return ".";

	case tt_return:
		return "ret";

	case tt_do:
		return "do";

	case tt_enddo:
		return "end do";

	case tt_jg:
		return "jg";

	case tt_jge:
		return "jge";

	case tt_jl:
		return "jl";

	case tt_jle:
		return "jle";

	case tt_je:
		return "je";

	case tt_jne:
		return "jne";

	case tt_jmp:
		return "jmp";

	}
	return "";
}

struct TACLine *newTACLine(int index, enum TACType operation, struct ASTNode *correspondingTree)
{
	struct TACLine *wip = malloc(sizeof(struct TACLine));
	wip->correspondingTree = correspondingTree;
	wip->operands[0] = NULL;
	wip->operands[1] = NULL;
	wip->operands[2] = NULL;
	wip->operands[3] = NULL;
	wip->operandTypes[0] = vt_null;
	wip->operandTypes[1] = vt_null;
	wip->operandTypes[2] = vt_null;
	wip->operandTypes[3] = vt_null;
	wip->operandPermutations[0] = vp_standard;
	wip->operandPermutations[1] = vp_standard;
	wip->operandPermutations[2] = vp_standard;
	wip->operandPermutations[3] = vp_standard;
	wip->indirectionLevels[0] = 0;
	wip->indirectionLevels[1] = 0;
	wip->indirectionLevels[2] = 0;
	wip->indirectionLevels[3] = 0;
	// default type of a line of TAC is assignment
	wip->operation = operation;
	// by default operands are NOT reorderable
	wip->reorderable = 0;
	wip->index = index;
	return wip;
}

void printTACLine(struct TACLine *it)
{
	char *operationStr;
	char fallingThrough = 0;
	int width;
	if (TACLine_isEffective(it))
	{
		width = printf("%2x: ", it->index);
	}
	else
	{
		width = printf(" ~  ");
	}

	switch (it->operation)
	{
	case tt_asm:
		width += printf("ASM:%s", it->operands[0]);
		break;

	case tt_add:
		if (!fallingThrough)
			operationStr = "+";
		fallingThrough = 1;
	case tt_subtract:
		if (!fallingThrough)
			operationStr = "-";
		fallingThrough = 1;
	case tt_mul:
		if (!fallingThrough)
			operationStr = "*";
		fallingThrough = 1;
	case tt_div:
		if (!fallingThrough)
			operationStr = "/";
		fallingThrough = 1;

		width += printf("%s = %s %s %s", it->operands[0], it->operands[1], operationStr, it->operands[2]);
		break;

	case tt_dereference:
		width += printf("%s = *%s", it->operands[0], it->operands[1]);
		break;

	case tt_reference:
		width += printf("%s = &%s", it->operands[0], it->operands[1]);
		break;

	case tt_memw_1:
		// operands: base source
		width += printf("(%s) = %s", it->operands[0], it->operands[1]);
		break;

	case tt_memw_2:
		// operands: base offset source
		width += printf("(%s + %d) = %s", it->operands[0], (int)(long int)it->operands[1], it->operands[2]);
		break;

	case tt_memw_3:
		// operands base offset scale source
		width += printf("(%s + %s * %d) = %s", it->operands[0], it->operands[1], (int)(long int)it->operands[2], it->operands[3]);
		break;

	case tt_memr_1:
		// operands: dest base
		width += printf("%s = (%s)", it->operands[0], it->operands[1]);
		break;

	case tt_memr_2:
		// operands: dest base offset
		width += printf("%s = (%s + %d)", it->operands[0], it->operands[1], (int)(long int)it->operands[2]);
		break;

	case tt_memr_3:
		// operands: dest base offset scale
		width += printf("%s = (%s + %s * %d)", it->operands[0], it->operands[1], it->operands[2], (int)(long int)it->operands[3]);
		break;

	case tt_jg:
	case tt_jge:
	case tt_jl:
	case tt_jle:
	case tt_je:
	case tt_jne:
	case tt_jmp:
		width += printf("%s basicblock %ld", getAsmOp(it->operation), (long int)it->operands[0]);
		break;

	case tt_cmp:
		width += printf("cmp %s %s", it->operands[1], it->operands[2]);
		break;

	case tt_assign:
		width += printf("%s = %s", it->operands[0], it->operands[1]);
		break;

	case tt_declare:
		width += printf("declare %s", it->operands[0]);
		break;

	case tt_push:
		width += printf("push %s", it->operands[0]);
		break;

	case tt_call:
		if (it->operands[0] == NULL)
			width += printf("call %s", it->operands[1]);
		else
			width += printf("%s = call %s", it->operands[0], it->operands[1]);

		break;

	case tt_label:
		width += printf("~label %ld:", (long int)it->operands[0]);
		break;

	case tt_return:
		width += printf("ret %s", it->operands[0]);
		break;

	case tt_do:
		width += printf("do");
		break;

	case tt_enddo:
		width += printf("end do");
		break;

	}
	while (width++ < 24)
	{
		printf(" ");
	}
	printf("\t");
	for (int i = 0; i < 4; i++)
	{
		if (it->operandTypes[i] != vt_null)
		{
			printf("[%d", it->operandTypes[i]);
			switch (it->operandPermutations[i])
			{
			case vp_standard:
				printf("S");
				break;

			case vp_temp:
				printf("T");
				break;

			case vp_literal:
				printf("L");
				break;
			}
			printf(" %d*]", it->indirectionLevels[i]);
		}
		else
		{
			printf("[     ]");
		}
	}
	// printf("\t%d %d %d %d", it->operandTypes[0], it->operandTypes[1], it->operandTypes[2], it->operandTypes[3]);
	// printf("\t%d %d %d", it->indirectionLevels[0], it->indirectionLevels[1], it->indirectionLevels[2]);

	// width += printf("%s", it->reorderable ? " - Reorderable" : "");
}

char *sPrintTACLine(struct TACLine *it)
{
	char *operationStr;
	char *tacString = malloc(128);
	char fallingThrough = 0;
	int width = sprintf(tacString, "%2x:", it->index);
	switch (it->operation)
	{
	case tt_asm:
		width += sprintf(tacString, "ASM:%s", it->operands[0]);
		break;

	case tt_add:
		if (!fallingThrough)
			operationStr = "+";
		fallingThrough = 1;
	case tt_subtract:
		if (!fallingThrough)
			operationStr = "-";
		fallingThrough = 1;
	case tt_mul:
		if (!fallingThrough)
			operationStr = "*";
		fallingThrough = 1;
	case tt_div:
		if (!fallingThrough)
			operationStr = "/";

		width += sprintf(tacString, "%s = %s %s %s", it->operands[0], it->operands[1], operationStr, it->operands[2]);
		break;

	case tt_dereference:
		width += sprintf(tacString, "%s = *%s", it->operands[0], it->operands[1]);
		break;

	case tt_reference:
		width += sprintf(tacString, "%s = &%s", it->operands[0], it->operands[1]);
		break;

	case tt_memw_1:
		// operands: base source
		width += sprintf(tacString, "(%s) = %s", it->operands[0], it->operands[1]);
		break;

	case tt_memw_2:
		// operands: base offset source
		width += sprintf(tacString, "%s + %d = %s", it->operands[0], (int)(long int)it->operands[1], it->operands[2]);
		break;

	case tt_memw_3:
		// operands base offset scale source
		width += sprintf(tacString, "(%s + %s * %d) = %s", it->operands[0], it->operands[1], (int)(long int)it->operands[2], it->operands[3]);
		break;

	case tt_memr_1:
		// operands: dest base
		width += sprintf(tacString, "%s = (%s)", it->operands[0], it->operands[1]);
		break;

	case tt_memr_2:
		// operands: dest base offset
		width += sprintf(tacString, "%s = (%s + %d)", it->operands[0], it->operands[1], (int)(long int)it->operands[2]);
		break;

	case tt_memr_3:
		// operands: dest base offset scale
		width += sprintf(tacString, "%s = (%s + %s * %d)", it->operands[0], it->operands[1], it->operands[2], (int)(long int)it->operands[3]);
		break;

	case tt_jg:
	case tt_jge:
	case tt_jl:
	case tt_jle:
	case tt_je:
	case tt_jne:
	case tt_jmp:
		width += sprintf(tacString, "%s basicblock %ld", getAsmOp(it->operation), (long int)it->operands[0]);
		break;

	case tt_cmp:
		width += sprintf(tacString, "cmp %s %s", it->operands[1], it->operands[2]);
		break;

	case tt_assign:
		width += sprintf(tacString, "%s = %s", it->operands[0], it->operands[1]);
		break;

	case tt_declare:
		width += sprintf(tacString, "declare %s", it->operands[0]);
		break;

	case tt_push:
		width += sprintf(tacString, "push %s", it->operands[0]);
		break;

	case tt_call:
		if (it->operands[0] == NULL)
			width += sprintf(tacString, "call %s", it->operands[1]);
		else
			width += sprintf(tacString, "%s = call %s", it->operands[0], it->operands[1]);

		break;

	case tt_label:
		width += sprintf(tacString, "~label %ld:", (long int)it->operands[0]);
		break;

	case tt_return:
		width += sprintf(tacString, "ret %s", it->operands[0]);
		break;

	case tt_do:
		width += sprintf(tacString, "do");
		break;

	case tt_enddo:
		width += sprintf(tacString, "end do");
		break;

	}

	char *trimmedString = malloc(width + 1);
	sprintf(trimmedString, "%s", tacString);
	free(tacString);
	return trimmedString;
}

void freeTAC(struct TACLine *it)
{
	printTACLine(it);
	free(it);
}

char TACLine_isEffective(struct TACLine *it)
{
	switch (it->operation)
	{
	case tt_do:
	case tt_enddo:
		return 0;

	default:
		return 1;
	}
}

struct BasicBlock *BasicBlock_new(int labelNum)
{
	struct BasicBlock *wip = malloc(sizeof(struct BasicBlock));
	wip->TACList = LinkedList_new();
	wip->labelNum = labelNum;
	wip->containsEffectiveCode = 0;
	return wip;
}

void BasicBlock_free(struct BasicBlock *b)
{
	LinkedList_free(b->TACList, free);
	free(b);
}

void BasicBlock_append(struct BasicBlock *b, struct TACLine *l)
{
	b->containsEffectiveCode |= TACLine_isEffective(l);
	LinkedList_append(b->TACList, l);
}

void BasicBlock_prepend(struct BasicBlock *b, struct TACLine *l)
{
	b->containsEffectiveCode |= TACLine_isEffective(l);
	LinkedList_prepend(b->TACList, l);
}

struct TACLine *findLastEffectiveTAC(struct BasicBlock *b)
{
	struct LinkedListNode *runner = b->TACList->tail;
	while (runner != NULL && !TACLine_isEffective(runner->data))
	{
		runner = runner->prev;
	}
	if (runner == NULL)
	{
		return NULL;
	}
	return runner->data;
}

void printBasicBlock(struct BasicBlock *b, int indentLevel)
{
	for (int i = 0; i < indentLevel; i++)
	{
		printf("\t");
	}
	printf("BASIC BLOCK %d\n", b->labelNum);
	for (struct LinkedListNode *runner = b->TACList->head; runner != NULL; runner = runner->next)
	{
		struct TACLine *this = runner->data;
		for (int i = 0; i < indentLevel; i++)
		{
			printf("\t");
		}

		if (runner->data != NULL)
		{
			printTACLine(this);
			printf("\n");
		}
	}
	printf("\n\n");
}
