#include "serialize.h"

FILE *outFile = NULL;

void serializeASTRec(struct AST *n){
	fprintf(outFile, "%p\n%s\n%d\n%p\n%p\n", n, n->value, n->type, n->child, n->sibling);

	if(n->child != NULL)
		serializeASTRec(n->child);


	if(n->sibling != NULL)
		serializeASTRec(n->sibling);
	
}

void serializeAST(char *outFileName, struct AST *root){
	outFile = fopen(outFileName, "wb");
	serializeASTRec(root);
	fclose(outFile);
}
