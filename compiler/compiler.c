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
/*
void printBasicBlocks(struct SymbolTable *theTable)
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

void checkUninitializedUsage(struct SymbolTable *table)
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

				struct variableEntry *declared = SymbolTableLookup(table, ir->operands[0])->entry;
				if (declared->declaredAt > 0)
				{
					ErrorAndExit(ERROR_CODE, "Error - redeclaration of variable [%s]\n", ir->operands[0]);
				}
				declared->declaredAt = ir->index;
				continue;

			case tt_push:
				if (ir->operandPermutations[0] == vp_standard)
				{
					switch (ir->operandTypes[0])
					{
					case vt_var:
						// lookup_var breaks and exits if variable is undeclared
						//struct variableEntry *pushed =  
						SymbolTableLookup_var(table, ir->operands[0]);
						break;

					default:
						ErrorAndExit(ERROR_INTERNAL, "Unexpected type in IR line for push\n");
					}
				}
				break;

			case tt_call:
				if (ir->operandTypes[0] != vt_null && ir->operandPermutations[0] == vp_standard)
				{
					struct variableEntry *callResult = SymbolTableLookup_var(table, ir->operands[0]);
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
						struct variableEntry *it = SymbolTableLookup_var(table, ir->operands[j]);

						if (!it->isAssigned)
						{
							struct ASTNode *n = ir->correspondingTree;
							// printTACLine(ir);
							// printf("\n");
							ErrorAndExit(ERROR_CODE, "Error - use of variable [%s] before assignment!\n\tLine %d, Col %d\n\n", ir->operands[j], n->sourceLine, n->sourceCol);
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
					struct variableEntry *theVariable = SymbolTableLookup_var(table, ir->operands[0]);

					if (theVariable->declaredAt > ir->index || theVariable->declaredAt == -1)
					{
						struct ASTNode *n = ir->correspondingTree;
						if (n == NULL)
						{
							ErrorAndExit(1, "Error - use of undeclared variable [%s]\n", ir->operands[0]);
						}
						ErrorAndExit(1, "Error - use of undeclared variable [%s]\n\tLine %d, Col %d\n", ir->operands[0], n->sourceLine, n->sourceCol);
					}
					theVariable->isAssigned = 1;
					if (ir->index < theVariable->assignedAt)
					{
						theVariable->assignedAt = ir->index;
					}
					break;

				default:
				}
			}
		}
	}
}
*/
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
		ErrorAndExit(ERROR_INVOCATION, "Error - please specify an input and output file!\n");
	}
	else if (argc < 3)
	{
		ErrorAndExit(ERROR_INVOCATION, "Error - please specify an output file!\n");
	}
	printf("Parsing program from %s\n", argv[1]);

	printf("Generating output to %s\n", argv[2]);
	struct Dictionary *parseDict = newDictionary(10);
	struct ASTNode *program = parseProgram(argv[1], parseDict);

	serializeAST("astdump", program);
	printf("\n");

	ASTNode_print(program, 0);
	printf("Generating symbol table from AST");
	struct SymbolTable *theTable = walkAST(program);
	printf("\n");

	// printSymTab(theTable, 1);
	SymbolTable_print(theTable, 0);

	printf("Linearizing code to basic blocks\n");
	linearizeProgram(program, theTable->globalScope);

	// printBasicBlocks(theTable);
	// printf("\n\n");

	// FILE *outFile = fopen(argv[2], "wb");

	// struct Lifetime *theseLifetimes = findLifetimes(theTable);

	// struct ASMblock *output;
	// output = generateCode(theTable, outFile);
	// ASMblock_output(output, outFile);
	// ASMblock_free(output);

	// exit(1);
	/*
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
	*/

	// fprintf(outFile, "data:\n");

	// fclose(outFile);
	freeDictionary(parseDict);
	ASTNode_free(program);
	SymbolTable_free(theTable);

	printf("done printing\n");
}
