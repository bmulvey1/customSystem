#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"

enum production
{
    p_program,
    p_tldList,
    p_tld,
    p_variableInit,
    p_assignment,
    p_statementList,
    p_statement,
    p_expression,
    p_unOp
};

char *production_names[] = {
    "Program",
    "TLD List",
    "Top Level Definition",
    "Variable Initialization",
    "Assignment",
    "Statement List",
    "Statement",
    "Expression",
    "Unary Operator"

};

int main(int argc, char **argv)
{
    printf("%s\n", argv[1]);

    struct astNode *program = parseProgram(argv[1]);
    printf("DONE PARSING PROGRAM\n");
    printAST(program, 0);

    /*while (lookahead() != EOF)
    {
        scan();
        printf("Scanned %s with token of %s\n", &buffer, token_names[currentToken]);
    }*/
    printf("done printing\n");
}