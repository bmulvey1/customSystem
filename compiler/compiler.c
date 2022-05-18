#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "parser.h"
#include "tac.h"
#include "symtab.h"
#include "util.h"
#include "linearizer.h"
#include "asm.h"
#include "codegen.h"
#include "serialize.h"

int compareBasicBlockStartIndices(struct BasicBlock *a, struct BasicBlock *b)
{
	return ((struct TACLine *)a->TACList->head->data)->index < ((struct TACLine *)b->TACList->head->data)->index;
}

void printBasicBlocks(struct symbolTable *theTable)
{
	printf("Basic Blocks for %s\n", theTable->name);
	for (int i = 0; i < theTable->size; i++)
	{
		if (theTable->entries[i]->type == e_function)
		{
			struct functionEntry *thisEntry = theTable->entries[i]->entry;

			for (struct LinkedListNode *runner = thisEntry->table->BasicBlockList->head; runner != NULL; runner = runner->next)
			{
				printBasicBlock(runner->data, 0);
			}
		}
	}
}

void checkUninitializedUsage(struct symbolTable *table)
{

	for (struct LinkedListNode *r = table->BasicBlockList->head; r != NULL; r = r->next)
	{

		struct BasicBlock *b = r->data;
		struct LinkedListNode *tr = b->TACList->head;
		if (tr == NULL)
		{
			continue;
		}
		struct TACLine *ir = tr->data;

		for (int i = 0; i < table->size; i++)
		{
			switch (table->entries[i]->type)
			{
			case e_variable:
				struct variableEntry *theVar = table->entries[i]->entry;
				if (theVar->assignedAt >= ir->index)
					theVar->isAssigned = 0;
				break;

			// skip, arguments are entered into symtab as declared and assigned at index 0
			case e_argument:
				break;

			default:
			}
		}

		for (; tr != NULL; tr = tr->next)
		{
			ir = tr->data;
			// printTACLine(ir);
			// printf("\t%d\n", (ir->correspondingTree == NULL) ? -1 : (ir->correspondingTree->sourceLine));
			switch (ir->operation)
			{
			case tt_declare:

				struct variableEntry *declared = symbolTableLookup(table, ir->operands[0])->entry;
				if (declared->declaredAt > 0)
				{
					printf("Error - redeclaration of variable [%s]\n", ir->operands[0]);
					exit(1);
				}
				declared->declaredAt = ir->index;
				continue;

			case tt_push:
				if (ir->operandPermutations[0] == vp_standard)
				{
					switch (ir->operandTypes[0])
					{
					case vt_var:
						struct variableEntry *pushed = symbolTableLookup_var(table, ir->operands[0]);
						if (pushed == NULL)
						{
							printf("Error - use of undeclared variable [%s]\n", ir->operands[0]);
							// printf("\tLine %d, Col %d\n", ir->correspondingTree->sourceLine, ir->correspondingTree->sourceCol);
							exit(1);
						}
						break;

					default:
						perror("Unexpected type in IR line for push\n");
						exit(2);
					}
				}
				break;

			case tt_call:
				if (ir->operandTypes[0] != vt_null && ir->operandPermutations[0] == vp_standard)
				{
					struct variableEntry *callResult = symbolTableLookup_var(table, ir->operands[0]);
					if (!callResult->isAssigned)
					{
						callResult->assignedAt = ir->index;
						callResult->isAssigned = 1;
					}
				}
				break;

			default:
				break;
			}

			// check operands 2, 3, and 4
			for (int j = 3; j > 0; j--)
			{
				if (ir->operandTypes[j] != vt_null)
				{
					switch (ir->operandPermutations[j])
					{
					case vp_standard:
						struct symTabEntry *it = symbolTableLookup(table, ir->operands[j]);
						if (it == NULL)
						{
							struct ASTNode *n = ir->correspondingTree;
							printf("Error - use of undeclared variable [%s]\n\tLine %d, Col %d\n", ir->operands[0], n->sourceLine, n->sourceCol);
							exit(1);
						}
						if (!((struct variableEntry *)it->entry)->isAssigned)
						{
							struct ASTNode *n = ir->correspondingTree;
							// printTACLine(ir);
							// printf("\n");
							printf("Error - use of variable [%s] before assignment!\n\tLine %d, Col %d\n\n", ir->operands[j], n->sourceLine, n->sourceCol);
							exit(1);
						}
						break;
					default:
					}
				}
			}

			if (ir->operandTypes[0] != vt_null)
			{
				switch (ir->operandPermutations[0])
				{
				// check only standard type, not temp or literal
				case vp_standard:
					struct symTabEntry *it = symbolTableLookup(table, ir->operands[0]);
					if (it == NULL)
					{
						printf("Error - use of undeclared variable [%s]\n", ir->operands[0]);
						exit(1);
					}
					struct variableEntry *theVariable = it->entry;
					if (theVariable->declaredAt > ir->index || theVariable->declaredAt == -1)
					{
						struct ASTNode *n = ir->correspondingTree;
						if (n == NULL)
						{
							printf("Error - use of undeclared variable [%s]\n", ir->operands[0]);
							exit(1);
						}
						printf("Error - use of undeclared variable [%s]\n\tLine %d, Col %d\n", ir->operands[0], n->sourceLine, n->sourceCol);
						exit(1);
					}
					theVariable->isAssigned = 1;
					theVariable->assignedAt = ir->index;
					break;

				default:
				}
			}
		}
	}
}

void checkIRConsistency(struct LinkedList *blockList)
{
	for (struct LinkedListNode *br = blockList->head; br != NULL; br = br->next)
	{
		struct BasicBlock *b = br->data;
		for (struct LinkedListNode *tr = b->TACList->head; tr != NULL; tr = tr->next)
		{
			struct TACLine *t = tr->data;
			switch (t->operation)
			{
			case tt_add:
			case tt_subtract:
			case tt_mul:
			case tt_div:
				if (t->indirectionLevels[1] != t->indirectionLevels[2])
				{
					printf("Warning - arithmetic between ");
					switch (t->operandTypes[1])
					{
					case vt_var:
						printf("var");
						break;

					default:
						printf("-");
					}
					for (int i = 0; i < t->indirectionLevels[1]; i++)
					{
						printf("*");
					}

					printf(" and ");

					switch (t->operandTypes[2])
					{
					case vt_var:
						printf("var");
						break;

					default:
						printf("-");
					}
					for (int i = 0; i < t->indirectionLevels[2]; i++)
					{
						printf("*");
					}
					printf("\n\tLine %d, Col %d\n\tExpression Tree: ", t->correspondingTree->sourceLine, t->correspondingTree->sourceCol);
					ASTNode_printHorizontal(t->correspondingTree);
					printf("\n\n");
				}

				if (t->indirectionLevels[0] != t->indirectionLevels[1])
				{
					printf("Warning - result of arithmetic (");
					switch (t->operandTypes[0])
					{
					case vt_var:
						printf("var");
						break;

					default:
						printf("-");
					}
					for (int i = 0; i < t->indirectionLevels[0]; i++)
					{
						printf("*");
					}

					printf(") has different type than expected (");

					switch (t->operandTypes[2])
					{
					case vt_var:
						printf("var");
						break;

					case vt_null:
						printf("WTF\n");
						break;

					default:
						printf("-");
					}
					for (int i = 0; i < t->indirectionLevels[2]; i++)
					{
						printf("*");
					}
					printf("\n\tLine %d, Col %d\n\tExpression Tree: ", t->correspondingTree->sourceLine, t->correspondingTree->sourceCol);
					ASTNode_printHorizontal(t->correspondingTree);
					printf("\n\t");
					printTACLine(t);
					printf("\n\n");
				}
				break;

			case tt_reference:
				if (t->indirectionLevels[1] == 0)
				{
					printf("Warning - dereference of non-indirect variable [%s]\n\tLine %d, Col %d\n\tExpression Tree: ", t->operands[1], t->correspondingTree->sourceLine, t->correspondingTree->sourceCol);
					ASTNode_printHorizontal(t->correspondingTree);
					printf("\n\n");
				}

			case tt_assign:
				if (t->indirectionLevels[0] != t->indirectionLevels[1])
				{
					printf("Warning - assignment from (");
					switch (t->operandTypes[1])
					{
					case vt_var:
						printf("var");
						break;

					default:
						printf("-");
					}
					for (int i = 0; i < t->indirectionLevels[1]; i++)
					{
						printf("*");
					}

					printf(") to (");

					switch (t->operandTypes[0])
					{
					case vt_var:
						printf("var");
						break;

					default:
						printf("-");
					}
					for (int i = 0; i < t->indirectionLevels[0]; i++)
					{
						printf("*");
					}
					printf(")\n\tLine %d, Col %d\n\tExpression Tree: ", t->correspondingTree->sourceLine, t->correspondingTree->sourceCol);
					ASTNode_printHorizontal(t->correspondingTree);
					printf("\n\n");
				}

				break;

			default:
				break;
			}
		}
	}
}

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		printf("Error - please specify an input and output file!\n");
		exit(1);
	}
	else if (argc < 3)
	{
		printf("Error - please specify an output file!\n");
		exit(1);
	}
	printf("Parsing program from %s\n", argv[1]);

	printf("Generating output to %s\n", argv[2]);
	struct Dictionary *parseDict = newDictionary(10);
	struct ASTNode *program = parseProgram(argv[1], parseDict);

	serializeAST("astdump", program);
	printf("\n");

	ASTNode_print(program, 0);
	printf("Generating symbol table from AST");
	struct symbolTable *theTable = walkAST(program);
	printf("\n");

	printSymTab(theTable, 1);

	printf("Linearizing code to basic blocks\n");
	linearizeProgram(program, theTable);

	printBasicBlocks(theTable);
	printf("\n\n");

	FILE *outFile = fopen(argv[2], "wb");
	// struct Lifetime *theseLifetimes = findLifetimes(theTable);
	struct ASMblock *output;
	output = generateCode(theTable, outFile);
	ASMblock_output(output, outFile);
	ASMblock_free(output);

	// exit(1);
	for (int i = 0; i < theTable->size; i++)
	{
		if (theTable->entries[i]->type == e_function)
		{
			struct functionEntry *thisEntry = theTable->entries[i]->entry;
			checkUninitializedUsage(thisEntry->table);
			checkIRConsistency(thisEntry->table->BasicBlockList);
			fprintf(outFile, "#d \"%s\"\n", thisEntry->table->name);
			output = generateCode(thisEntry->table, outFile);
			// for (struct Lifetime *ltRunner = theseLifetimes; ltRunner != NULL; ltRunner = ltRunner->next)
			// {
			// printf("Var [%8s]: %2x - %2x\n", ltRunner->variable, ltRunner->start, ltRunner->end);
			// }

			// run along all the lines of asm output from this funtcion, printing and freeing as we go
			ASMblock_output(output, outFile);
			ASMblock_free(output);
		}
	}

	fprintf(outFile, "data:\n");

	fclose(outFile);
	freeDictionary(parseDict);
	ASTNode_free(program);
	freeSymTab(theTable);

	printf("done printing\n");
}
