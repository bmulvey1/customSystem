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

	// ASTNode_print(program, 0);
	printf("Generating symbol table from AST");
	struct SymbolTable *theTable = walkAST(program);
	printf("\n");


	printf("Linearizing code to basic blocks\n");
	linearizeProgram(program, theTable->globalScope, parseDict);

	// SymbolTable_print(theTable, 0);

	FILE *outFile = fopen(argv[2], "wb");

	struct Stack *outputBlocks;
	outputBlocks = generateCode(theTable, outFile);
	fprintf(outFile, "#include \"CPU.asm\"\n");
	for(int i = 0; i < outputBlocks->size; i++)
	{
		ASMblock_output(outputBlocks->data[i], outFile);
	}

	fclose(outFile);
	freeDictionary(parseDict);
	ASTNode_free(program);
	SymbolTable_free(theTable);

	printf("done printing\n");
}
